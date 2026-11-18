"""Roda numa placa (DE0-CV ou DE2-115) os programas do Tests e os ELFs do ACT4, com um veredito por item.

O runner tem três subcomandos.

``prepare``
    Compila cada etapa para a placa (``compile --emit mif``). Roda onde está o GCC
    RISC-V, isto é, dentro da imagem ``ci-elf``, e não precisa de placa.
``run``
    Escolhe a placa pelo JTAG e toma a trava dela (``riscv-tools board lock``), programa o
    bitstream com o ``quartus_pgm`` (a menos que ``--skip-program``), roda as etapas pedidas
    com ``riscv-tools run`` e escreve ``results-<host>.json`` com PASS, FAIL ou TIMEOUT por
    programa. Roda onde estão o Quartus e o cabo.
``act``
    Escolhe e trava a placa e roda os ELFs do ACT4 de um config gerado por
    ``gen_profiles.py --board`` (``riscv-tools certify-board --skip-build``).

A placa é achada pelo IDCODE da cadeia e pela porta USB do cabo, nunca pelo nome do cabo,
que muda, e a trava tem o nome ``de0cv-<host>.lock``. Duas placas rodam em paralelo
repartindo a lista com ``--part I/N``: a placa ``I`` (de 0 a N-1) roda os itens de índice
``I`` módulo ``N`` da lista ordenada. Com ``--part auto`` o runner toma uma placa livre e
reparte a lista entre todas as placas presentes do host, na ordem do identificador delas.
"""

import argparse
import json
import os
import re
import shlex
import subprocess
import sys
import time
from collections.abc import Generator
from contextlib import contextmanager
from pathlib import Path
from typing import NotRequired, TypedDict

TESTS_ROOT = Path(__file__).resolve().parents[2]
BUILD = "tools/riscv_build"

# Cada etapa é um config do Tests e os programas que a placa não roda, com o motivo.
# Extensões da plataforma completa: um programa vale numa plataforma se o cabeçalho `RV32_EXT` cita só
# extensões da lista dela (a letra de uma extensão de uma letra, como ``m``, inclusive).
FULL_EXTENSIONS = (
    "m",
    "zicsr",
    "zicntr",
    "zihpm",
    "zifencei",
    "zicclsm",
    "zimop",
    "zicond",
    "zihintntl",
    "zihintpause",
)


class Stage(TypedDict):
    """Uma etapa da placa.

    Attributes
    ----------
    config : str
        Config do Tests que compila a etapa.
    exclude : dict of {str: str}
        Programas que a placa não roda, com o motivo.
    timeouts : NotRequired[dict[str, int]]
        Timeout, em segundos, dos programas lentos.
    board : NotRequired[str]
        Pacote de placa da etapa; o padrão é a DE0-CV.
    extensions : NotRequired[tuple[str, ...]]
        Extensões que a plataforma liga, quando a etapa filtra os programas por elas.
    sequence : NotRequired[list[str]]
        Programas rodados um por vez, nesta ordem e com repetição, para provar que a BOOT aceita o teste
        seguinte depois de um que passa, de um que falha e de um que trava.
    expect : NotRequired[dict[str, str]]
        O veredito esperado de cada programa da sequência, quando não é PASS.
    """

    config: str
    exclude: dict[str, str]
    timeouts: NotRequired[dict[str, int]]
    board: NotRequired[str]
    extensions: NotRequired[tuple[str, ...]]
    sequence: NotRequired[list[str]]
    expect: NotRequired[dict[str, str]]


# A sequência de dez testes da espera entre testes: um FAIL e um travamento no meio, e o teste seguinte de cada um
# tem de rodar. O veredito esperado dos que não passam fica ao lado.
BOOT_SEQUENCE = [
    "boot-seq-pass",
    "boot-seq-pass",
    "boot-seq-pass",
    "boot-seq-fail",
    "boot-seq-pass",
    "boot-seq-hang",
    "boot-seq-pass",
    "boot-seq-pass",
    "boot-seq-fail",
    "boot-seq-pass",
]
BOOT_EXPECT = {"boot-seq-fail": "FAIL", "boot-seq-hang": "TIMEOUT"}

STAGES: dict[str, Stage] = {
    "regression": {
        "config": "config.sdram-flash-regression.yaml",
        "exclude": {
            "uart-echo": "pinos físicos da UART, que as placas não têm ligados",
            "uart-tx": "pinos físicos da UART, que as placas não têm ligados",
            "uart-errors": "pinos físicos da UART, que as placas não têm ligados",
        },
    },
    "sdram": {
        "config": "config.sdram-flash.yaml",
        "exclude": {},
        # hex-id pausa para uma pessoa ler o display e passa de 15 s
        "timeouts": {"hex-id": 90},
    },
    "irq": {
        "config": "config.sdram-flash-irq.yaml",
        "exclude": {},
    },
    # Os programas de boot (c-boot/): o estado que a BOOT deixa na main e a espera entre testes. O núcleo é o
    # do bitstream que a placa tem, então o `boot-state` confere as extensões que esse núcleo liga.
    "boot": {
        "config": "config.boot-de0-cv.yaml",
        "exclude": {},
    },
    # Dez testes seguidos com um FAIL e um travamento no meio: o teste seguinte de cada um tem de rodar.
    "boot-seq": {
        "config": "config.boot-seq-de0-cv.yaml",
        "exclude": {},
        "sequence": BOOT_SEQUENCE,
        "expect": BOOT_EXPECT,
    },
    # A DE2-115: a FLASH NOR e a SDRAM de 32 bits (N4) e a versão com as memórias emuladas (N2).
    "de2-115": {
        "config": "config.de2-115.yaml",
        "exclude": {},
        "board": "de2-115",
        "timeouts": {"hex-id": 90},
    },
    "de2-115-n2": {
        "config": "config.de2-115-n2.yaml",
        # a RAM emulada tem 128 KiB e estes programas testam a janela de 16 MB da SDRAM
        "exclude": {
            name: "usa a janela da SDRAM, que o nível N2 não tem"
            for name in (
                "cache-sweep",
                "cpi-kernels",
                "flash-const",
                "sdram-bytes",
                "sdram-copy",
                "sdram-loop",
                "sdram-memtest",
                "sdram-muldiv",
                "sdram-refresh",
            )
        },
        "board": "de2-115",
        "timeouts": {"hex-id": 90},
    },
    # Os programas de boot e a sequência da espera entre testes na DE2-115, os mesmos da DE0-CV.
    "boot-de2-115": {
        "config": "config.boot-de2-115.yaml",
        "exclude": {},
        "board": "de2-115",
    },
    "boot-seq-de2-115": {
        "config": "config.boot-seq-de2-115.yaml",
        "exclude": {},
        "board": "de2-115",
        "sequence": BOOT_SEQUENCE,
        "expect": BOOT_EXPECT,
    },
    # A DE0-Nano (RV32I com Zicsr) e a DE10-Lite (perfil completo): `extensions` lista as extensões que a
    # plataforma liga, e o `prepare` só compila os programas cujo cabeçalho `RV32_EXT` cita extensões dessa lista.
    "de0-nano-regression": {
        "config": "config.nano-regression.yaml",
        "exclude": {
            "uart-echo": "pinos físicos da UART, que as placas não têm ligados",
            "uart-tx": "pinos físicos da UART, que as placas não têm ligados",
            "uart-errors": "pinos físicos da UART, que as placas não têm ligados",
        },
        "board": "de0-nano",
        "extensions": ("zicsr", "zifencei"),
    },
    "de0-nano-sdram": {
        "config": "config.nano-sdram.yaml",
        "exclude": {
            "monitor": "espera alguém digitar",
            "hex-id": "depende das chaves e do display da placa",
            "code-sweep": "o programa de 17 KiB não cabe no banco A de 12 KiB",
            "flash-const": "precisa da falha de acesso (extra AccessFaults), que a plataforma não liga",
        },
        "board": "de0-nano",
        "extensions": ("zicsr", "zifencei"),
    },
    "de10-lite-regression": {
        "config": "config.lite-regression.yaml",
        "exclude": {
            "uart-echo": "pinos físicos da UART, que as placas não têm ligados",
            "uart-tx": "pinos físicos da UART, que as placas não têm ligados",
            "uart-errors": "pinos físicos da UART, que as placas não têm ligados",
        },
        "board": "de10-lite",
        "extensions": FULL_EXTENSIONS,
    },
    "de10-lite-sdram": {
        "config": "config.lite-sdram.yaml",
        "exclude": {
            "monitor": "espera alguém digitar",
            "hex-id": "depende das chaves e do display da placa",
        },
        "board": "de10-lite",
        "extensions": FULL_EXTENSIONS,
    },
    "de10-lite-irq": {
        "config": "config.lite-irq.yaml",
        "exclude": {},
        "board": "de10-lite",
        "extensions": FULL_EXTENSIONS,
    },
}

# A placa da plataforma das etapas; `main` troca pela do pacote da etapa pedida (`board` da etapa).
PLATFORM_BOARD = "de0-cv"
# As etapas que rodam sem `--stages`: as da DE0-CV, porque uma sessão trava uma placa só.
DEFAULT_STAGES = [name for name, stage in STAGES.items() if stage.get("board", "de0-cv") == "de0-cv"]
_VERDICT = re.compile(r"^\s*(PASS|FAIL|TIMEOUT)\s+(\S+)")
LOCK_WAIT_S = 14400


def declared_extensions(folder: Path) -> set[str]:
    """Lê as extensões que o cabeçalho ``RV32_EXT`` de um programa cita.

    Parameters
    ----------
    folder : Path
        A pasta do programa.

    Returns
    -------
    set of str
        As extensões, em minúsculas; vazio quando o programa não cita nenhuma.
    """
    source = "".join(f.read_text() for f in folder.glob("*.[cS]"))
    match = re.search(r"RV32_EXT:\s*(.+)", source)
    if not match:
        return set()
    return {e.strip().lower() for e in str(match.group(1)).split(",")}


def applicable(stage: str) -> list[str]:
    """Lista os programas de uma etapa que a plataforma consegue rodar.

    Parameters
    ----------
    stage : str
        Nome da etapa, com a lista ``extensions``.

    Returns
    -------
    list of str
        Os programas sem extensão fora da lista e fora dos excluídos da etapa, ordenados.
    """
    allowed = set(STAGES[stage].get("extensions", ()))
    text = (TESTS_ROOT / BUILD / STAGES[stage]["config"]).read_text()
    folders = re.findall(r"^\s*(?:c_dir|asm_dir):\s*(\S+)", text, re.MULTILINE)
    found = {
        path.name
        for folder in dict.fromkeys(folders)
        for path in sorted((TESTS_ROOT / folder).iterdir())
        if declared_extensions(path) <= allowed
    }
    return sorted(found - set(STAGES[stage]["exclude"]))


def tools_command() -> list[str]:
    """Devolve o comando do ``riscv-tools``.

    Returns
    -------
    list of str
        O valor de ``RISCV_TOOLS`` repartido como no shell, ou ``["riscv-tools"]``.
    """
    return shlex.split(os.environ.get("RISCV_TOOLS", "riscv-tools"))


def board_args() -> list[str]:
    """Diz ao ``riscv-tools board`` qual placa procurar.

    A placa é a de ``PLATFORM_BOARD``, do pacote ``Insper-RISC-V/boards/<placa>``, lido pelo
    catálogo do Tools.

    Returns
    -------
    list of str
        A opção ``--board``.

    Raises
    ------
    RuntimeError
        Se o ponteiro do submódulo Insper-RISC-V ainda não traz o pacote da placa.
    """
    package = TESTS_ROOT / "Insper-RISC-V" / "boards" / PLATFORM_BOARD / "board.yaml"
    if not package.is_file():
        raise RuntimeError(
            f"{package} não existe: atualize o submódulo Insper-RISC-V para um commit com os pacotes de placa"
        )
    return ["--board", PLATFORM_BOARD]


def board_command(host: str, sub: str, *extra: str) -> list[str]:
    """Monta o comando ``riscv-tools board <sub>`` da placa do Tests.

    Parameters
    ----------
    host : str
        O rótulo do host (``laptop`` ou o hostname curto da máquina), que entra no nome da trava.
    sub : str
        ``select`` ou ``lock``.
    *extra : str
        Opções a mais do subcomando.

    Returns
    -------
    list of str
        O comando.
    """
    return [
        *tools_command(),
        "--root",
        str(TESTS_ROOT),
        "board",
        sub,
        "--host",
        host,
        *board_args(),
        *extra,
    ]


def present_boards(host: str) -> list[dict[str, str]]:
    """Lista as placas da plataforma presentes no host.

    Parameters
    ----------
    host : str
        O rótulo do host.

    Returns
    -------
    list of dict
        Uma seleção (``id``, ``cable``, ``lock_file``...) por placa, na ordem das portas.
    """
    out = subprocess.run(
        board_command(host, "select", "--all"),
        check=True,
        capture_output=True,
        text=True,
        cwd=TESTS_ROOT,
    ).stdout
    return json.loads(out)


@contextmanager
def board_session(host: str, wait_s: int = LOCK_WAIT_S, free: bool = False) -> Generator[dict[str, str]]:
    """Escolhe a placa e segura a trava dela durante toda a sessão.

    A trava é do ``riscv-tools board lock --hold``, que a segura enquanto a entrada padrão
    dele estiver aberta, e por isso ela se solta se o runner morrer.

    Parameters
    ----------
    host : str
        O rótulo do host, ``laptop`` ou o hostname curto da máquina.
    wait_s : int
        Quanto esperar pela trava antes de desistir.
    free : bool
        Toma a primeira placa livre entre as iguais, em vez de falhar por ambiguidade.

    Yields
    ------
    dict
        A seleção: ``id``, ``cable``, ``device``, ``lock_file``.

    Raises
    ------
    RuntimeError
        Se a placa não foi achada ou a trava não ficou livre a tempo.
    """
    extra = ["--wait", str(wait_s), "--hold"] + (["--free"] if free else [])
    with subprocess.Popen(
        board_command(host, "lock", *extra),
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        text=True,
        cwd=TESTS_ROOT,
    ) as lock:
        line = lock.stdout.readline() if lock.stdout else ""
        if not line:
            lock.wait()
            raise RuntimeError(f"riscv-tools board lock falhou (status {lock.returncode})")
        try:
            yield json.loads(line)
        finally:
            if lock.stdin:
                lock.stdin.close()
            lock.wait(timeout=60)


def part_of(part: str, host: str, selection: dict[str, str]) -> str:
    """Resolve ``auto`` na fatia ``I/N`` da placa travada.

    Parameters
    ----------
    part : str
        ``I/N`` ou ``auto``.
    host : str
        O rótulo do host.
    selection : dict
        A placa travada.

    Returns
    -------
    str
        A fatia ``I/N``; com ``auto``, ``N`` é o número de placas presentes do host e ``I``
        a posição do identificador da placa travada entre elas.
    """
    if part != "auto":
        return part
    ids = sorted(b["id"] for b in present_boards(host))
    return f"{ids.index(selection['id'])}/{len(ids)}"


def program(sof: Path, cable: str) -> None:
    """Programa o bitstream na placa.

    Parameters
    ----------
    sof : Path
        O arquivo ``.sof``.
    cable : str
        O nome do cabo que o ``riscv-tools board`` escolheu para esta sessão.
    """
    print(f"programando {sof} pelo cabo {cable}", flush=True)
    subprocess.run(
        ["quartus_pgm", "-c", cable, "-m", "jtag", "-o", f"p;{sof}"],
        check=True,
    )
    time.sleep(3)


def manifest_names(stage: str) -> list[str]:
    """Lista os programas compilados de uma etapa.

    Parameters
    ----------
    stage : str
        Nome da etapa.

    Returns
    -------
    list of str
        Os nomes do manifesto, ordenados.
    """
    config = yaml_build_dir(STAGES[stage]["config"])
    manifest = TESTS_ROOT / config / "real" / "manifest.json"
    return sorted(e["name"] for e in json.loads(manifest.read_text()))


def yaml_build_dir(config: str) -> str:
    """Lê o ``paths.build_dir`` de um config do Tests sem carregar o YAML inteiro.

    Parameters
    ----------
    config : str
        Nome do config em ``tools/riscv_build``.

    Returns
    -------
    str
        O diretório de build, relativo à raiz do Tests.
    """
    text = (TESTS_ROOT / BUILD / config).read_text()
    found = re.search(r"^\s*build_dir:\s*(\S+)", text, re.MULTILINE)
    if not found:
        raise RuntimeError(f"{config} não declara paths.build_dir")
    return found.group(1)


def partition(names: list[str], part: str) -> list[str]:
    """Pega a fatia ``I/N`` de uma lista.

    Parameters
    ----------
    names : list of str
        A lista ordenada.
    part : str
        ``I/N``.

    Returns
    -------
    list of str
        Os itens de índice ``I`` módulo ``N``.
    """
    index, count = (int(x) for x in part.split("/"))
    return names[index::count]


def run_sequence(stage: str, host: str, part: str, out: Path, cable: str) -> dict[str, str]:
    """Roda os programas de uma etapa de sequência, um por vez e na ordem da lista.

    Cada programa é uma execução própria do ``riscv-tools run``, que grava a imagem na FLASH e toca a
    campainha, como acontece entre dois testes de uma suíte. O resultado de cada posição é PASS quando o
    veredito é o esperado para o programa (PASS, ou o da chave ``expect``) e traz os dois vereditos quando não é.

    Parameters
    ----------
    stage : str
        Nome da etapa, com a lista ``sequence``.
    host : str
        ``laptop`` ou o hostname curto da máquina, só para o nome do log.
    part : str
        Fatia ``I/N`` da lista; a sequência inteira roda na fatia 0 e as outras não têm o que rodar.
    out : Path
        Onde o log da etapa é escrito.
    cable : str
        O cabo da placa travada.

    Returns
    -------
    dict of {str: str}
        Uma entrada ``NN-programa`` por posição da sequência.
    """
    if int(part.split("/")[0]) != 0:
        return {}
    config = STAGES[stage]["config"]
    expect = STAGES[stage].get("expect", {})
    known = set(manifest_names(stage))
    log = out / f"{stage}-{host}.log"
    log.write_text("", encoding="utf-8")
    results: dict[str, str] = {}
    for index, name in enumerate(STAGES[stage].get("sequence", []), start=1):
        if name not in known:
            raise RuntimeError(f"{name} não está no manifesto da etapa {stage}")
        done = subprocess.run(
            tools_command()
            + ["--cable", cable, "--config", f"{BUILD}/{config}", "run", "--skip-reconfigure", "--skip-recompile"]
            + ["--only", name],
            cwd=TESTS_ROOT,
            capture_output=True,
            text=True,
            check=False,
        )
        with open(log, "a", encoding="utf-8") as handle:
            handle.write(f"## {index:02d} {name}\n{done.stdout}{done.stderr}")
        results[f"{index:02d}-{name}"] = sequence_verdict(
            parse_verdicts(done.stdout).get(name, "TIMEOUT"), expect.get(name, "PASS")
        )
    return results


def sequence_verdict(found: str, wanted: str) -> str:
    """Compara o veredito de um programa da sequência com o esperado.

    Parameters
    ----------
    found : str
        PASS, FAIL ou TIMEOUT, o que a placa deu.
    wanted : str
        O veredito esperado do programa.

    Returns
    -------
    str
        PASS quando os dois coincidem, senão ``<achado> (esperado <esperado>)``.
    """
    return "PASS" if found == wanted else f"{found} (esperado {wanted})"


def run_stage(stage: str, host: str, part: str, out: Path, cable: str) -> dict[str, str]:
    """Roda uma etapa na placa e devolve o veredito de cada programa.

    Parameters
    ----------
    stage : str
        Nome da etapa.
    host : str
        ``laptop`` ou o hostname curto da máquina, só para o nome do log.
    part : str
        Fatia ``I/N`` da lista.
    out : Path
        Onde o log da etapa é escrito.
    cable : str
        O cabo da placa travada, que o ``riscv-tools`` filho usa em vez de escolher outro.

    Returns
    -------
    dict of {str: str}
        PASS, FAIL ou TIMEOUT por programa.
    """
    if "sequence" in STAGES[stage]:
        return run_sequence(stage, host, part, out, cable)
    config = STAGES[stage]["config"]
    excluded = set(STAGES[stage]["exclude"])
    names = [n for n in manifest_names(stage) if n not in excluded]
    wanted = partition(names, part)
    if not wanted:
        return {}
    set_timeouts(stage, wanted)
    command = tools_command() + [
        "--cable",
        cable,
        "--config",
        f"{BUILD}/{config}",
        "run",
        "--skip-reconfigure",
        "--skip-recompile",
        "--only",
        ",".join(wanted),
    ]
    log = out / f"{stage}-{host}.log"
    with open(log, "w", encoding="utf-8") as handle:
        subprocess.run(
            command,
            cwd=TESTS_ROOT,
            stdout=handle,
            stderr=subprocess.STDOUT,
            check=False,
        )
    verdicts = parse_verdicts(log.read_text())
    return {name: verdicts.get(name, "TIMEOUT") for name in wanted}


def set_timeouts(stage: str, wanted: list[str]) -> None:
    """Aumenta no manifesto o timeout dos programas lentos da etapa.

    Parameters
    ----------
    stage : str
        Nome da etapa.
    wanted : list of str
        Os programas que a placa vai rodar.
    """
    timeouts = STAGES[stage].get("timeouts", {})
    mine = {n: t for n, t in timeouts.items() if n in wanted}
    if not mine:
        return
    config = yaml_build_dir(STAGES[stage]["config"])
    path = TESTS_ROOT / config / "real" / "manifest.json"
    entries = json.loads(path.read_text())
    for entry in entries:
        entry["timeout_s"] = max(entry["timeout_s"], mine.get(entry["name"], 0))
    path.write_text(json.dumps(entries, indent=1))


def parse_verdicts(text: str) -> dict[str, str]:
    """Lê as linhas ``PASS nome`` do resumo do ``riscv-tools``.

    Parameters
    ----------
    text : str
        A saída do comando.

    Returns
    -------
    dict of {str: str}
        O veredito de cada nome que aparece no resumo.
    """
    found: dict[str, str] = {}
    for line in text.splitlines():
        match = _VERDICT.match(line)
        if match:
            found[match.group(2)] = match.group(1)
    return found


def cmd_prepare(args: argparse.Namespace) -> None:
    """Compila as etapas (``compile --emit mif``)."""
    for stage in args.stages:
        config = f"{BUILD}/{STAGES[stage]['config']}"
        only = ["--only", ",".join(applicable(stage))] if "extensions" in STAGES[stage] else []
        for sub in ("generate-header", "compile --emit mif"):
            subprocess.run(
                tools_command() + ["--config", config] + sub.split() + (only if "compile" in sub else []),
                cwd=TESTS_ROOT,
                check=True,
            )


def cmd_run(args: argparse.Namespace) -> None:
    """Escolhe e trava a placa, programa e roda as etapas sob a trava."""
    out = Path(args.out)
    out.mkdir(parents=True, exist_ok=True)
    results: dict[str, dict[str, str]] = {}
    with board_session(args.host, free=args.part == "auto") as board:
        part = part_of(args.part, args.host, board)
        if not args.skip_program:
            program(Path(args.sof), board["cable"])
        for stage in args.stages:
            results[stage] = run_stage(stage, args.host, part, out, board["cable"])
    (out / f"results-{args.host}.json").write_text(json.dumps(results, indent=1, sort_keys=True))
    failed = [f"{s}/{n}" for s, r in results.items() for n, v in r.items() if v != "PASS"]
    print(f"{sum(len(r) for r in results.values())} programas, falhas: {failed}")
    sys.exit(1 if failed else 0)


def cmd_act(args: argparse.Namespace) -> None:
    """Roda os ELFs do ACT4 de um config gerado, sob a trava da placa."""
    out = Path(args.out)
    out.mkdir(parents=True, exist_ok=True)
    root = Path(args.root)
    work = root / "vendor/riscv-arch-test/work"
    elfs = sorted({p.stem for p in work.glob(f"{args.target}*/elfs/**/*.elf")})
    log = out / f"act-{args.host}.log"
    with board_session(args.host, free=args.part == "auto") as board:
        wanted = partition(elfs, part_of(args.part, args.host, board))
        if not args.skip_program:
            program(Path(args.sof), board["cable"])
        with open(log, "w", encoding="utf-8") as handle:
            subprocess.run(
                tools_command()
                + [
                    "--cable",
                    board["cable"],
                    "--config",
                    args.config,
                    "--root",
                    str(root),
                    "certify-board",
                    "--skip-build",
                    "--only",
                    ",".join(wanted),
                ],
                stdout=handle,
                stderr=subprocess.STDOUT,
                check=False,
            )
    verdicts = parse_verdicts(log.read_text())
    results = {n: verdicts.get(n, "TIMEOUT") for n in wanted}
    (out / f"act-{args.host}.json").write_text(json.dumps(results, indent=1, sort_keys=True))
    failed = sorted(n for n, v in results.items() if v != "PASS")
    print(f"{len(results)} ELFs, falhas: {failed}")
    sys.exit(1 if failed else 0)


def main() -> None:
    """Lê a linha de comando e despacha."""
    parser = argparse.ArgumentParser(description=(__doc__ or "").split("\n", maxsplit=1)[0])
    sub = parser.add_subparsers(dest="cmd", required=True)
    prep = sub.add_parser("prepare", help="compila as etapas")
    prep.add_argument("--stages", nargs="+", default=DEFAULT_STAGES)
    prep.set_defaults(func=cmd_prepare)
    for name, func in (("run", cmd_run), ("act", cmd_act)):
        p = sub.add_parser(name)
        p.add_argument(
            "--host",
            "--board",
            dest="host",
            required=True,
            help="rótulo do host da placa (laptop ou o hostname curto da máquina), no nome da trava e dos arquivos",
        )
        p.add_argument("--part", default="0/1", help="fatia I/N da lista, ou auto")
        p.add_argument("--sof", help="bitstream a programar")
        p.add_argument("--skip-program", action="store_true")
        p.add_argument(
            "--platform-board",
            help="pacote de placa (boards/<placa>); o padrão é o da etapa, ou de0-cv",
        )
        p.add_argument("--out", default="build/board")
        p.set_defaults(func=func)
        if name == "run":
            p.add_argument("--stages", nargs="+", default=DEFAULT_STAGES)
        else:
            p.add_argument("--config", required=True)
            p.add_argument("--root", required=True)
            p.add_argument("--target", required=True, help="pasta de work do ACT4")
    args = parser.parse_args()
    boards = {STAGES[s].get("board", PLATFORM_BOARD) for s in getattr(args, "stages", [])}
    if len(boards) > 1:
        parser.error(f"as etapas pedidas são de placas diferentes: {sorted(boards)}")
    if getattr(args, "platform_board", None):
        globals()["PLATFORM_BOARD"] = args.platform_board
    elif boards:
        globals()["PLATFORM_BOARD"] = boards.pop()
    if args.cmd != "prepare" and not args.skip_program and not args.sof:
        parser.error("--sof é obrigatório sem --skip-program")
    args.func(args)


if __name__ == "__main__":
    main()
