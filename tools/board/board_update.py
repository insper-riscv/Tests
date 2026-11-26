"""Roda na placa DE0-CV os programas de atualização da FLASH e o de PLIC da FLASH, com um veredito por programa.

Os programas ``flash-update`` e ``flash-update-interrupted`` (``c-sdram-update/``) recebem pela UART sobre
JTAG a imagem da banca B, atualizam a FLASH pelo controlador e reiniciam pela memória de boot. O
``update-new`` (``c-sdram-bank-b/``) é o programa dessa imagem e roda sozinho, gravado na banca B. O
``riscv-tools run`` não envia bytes pela UART, então este roteiro faz a sequência de cada programa:

1. zera a palavra de sobrevivência da SDRAM (a placa não zera a SDRAM entre programas) e o ``tohost``;
2. grava a FLASH inteira pelo carregador JTAG (a banca B sai zerada, portanto inválida) e solta o núcleo;
3. espera o programa chegar à leitura da UART e envia a imagem da banca B (``riscv-tools flash-image send``);
4. espera o ``tohost`` e confere o texto que o programa imprimiu.

O roteiro roda onde estão o Quartus e o cabo, sob o lock da placa, e escreve ``results-update-<placa>.json``.
"""

import argparse
import json
import os
import sys
import time
from pathlib import Path
from typing import Any, NotRequired, TypedDict

from board_run import board_session
from riscv_tools import (
    flash_image,
    flash_loader,
    htif,
    ram_target,
    rom_writer,
    uart_console,
)
from riscv_tools.cli.common import jtag_link
from riscv_tools.jtag import JtagLink
from riscv_tools.settings import load_config

TESTS_ROOT = Path(__file__).resolve().parents[2]
BUILD = TESTS_ROOT / "tools/riscv_build"

# Endereço, relativo ao início da SDRAM, da palavra que os programas de atualização usam para saber que já
# tentaram atualizar (SURVIVOR em src.c).
SURVIVOR_OFFSET = 0x03000000
FLASH_WORDS = 65536


class Case(TypedDict):
    """Como um programa de atualização roda na placa.

    Attributes
    ----------
    config : str
        Config do Tests que o compilou.
    build : str
        Diretório de build desse config.
    source : str
        Diretório do fonte do programa.
    send : bool
        Se a imagem da banca B é enviada pela UART.
    truncate : NotRequired[bool]
        Se só as palavras que cabem na FLASH são gravadas.
    expected_from : NotRequired[str]
        Diretório de onde vem o texto esperado, quando não é o do próprio programa.
    expected : NotRequired[str]
        O texto esperado, quando não vem de um arquivo.
    """

    config: str
    build: str
    source: str
    send: bool
    truncate: NotRequired[bool]
    expected_from: NotRequired[str]
    expected: NotRequired[str]


# Cada programa: config, diretório do fonte, origem do que se envia pela UART e fim esperado.
CASES: dict[str, Case] = {
    "flash-update": {
        "config": "config.sdram-flash-update.yaml",
        "build": "build-sdram-flash-update",
        "source": "c-sdram-update/flash-update",
        "send": True,
    },
    "flash-update-interrupted": {
        "config": "config.sdram-flash-update.yaml",
        "build": "build-sdram-flash-update",
        "source": "c-sdram-update/flash-update-interrupted",
        "send": True,
    },
    "update-new": {
        "config": "config.sdram-flash-bank-b.yaml",
        "build": "build-bank-b",
        "source": "c-sdram-bank-b/update-new",
        "send": False,
        # o mif deste programa tem mais palavras do que a FLASH tem: só as da FLASH valem
        "truncate": True,
        "expected_from": "c-sdram-update/flash-update",
    },
    # Os eventos do controlador de FLASH e da UART sobre JTAG pelo PLIC; o console fica ligado para a UART esvaziar.
    "plic-flash": {
        "config": "config.plic-de0-cv.yaml",
        "build": "build-plic-de0-cv",
        "source": "asm-plic-de0-cv/plic-flash",
        "send": False,
        "expected": "",
    },
}


def stdout_buffer(link: JtagLink, cfg: dict[str, Any], target: ram_target.RamTarget) -> str:
    """Lê o que o programa imprimiu no buffer de ``stdout`` da SDRAM (a cópia que o teste confere).

    Parameters
    ----------
    link : JtagLink
        Qual cabo e chip.
    cfg : dict
        O config carregado.
    target : RamTarget
        Como a RAM é alcançada.

    Returns
    -------
    str
        O texto impresso, vazio se o buffer está vazio.
    """
    memory = cfg["memory"]
    first = htif.word_offset(memory["ram_base"], memory["tohost_addr"] - 0x408)  # stdout: 1 KiB + 8 bytes below tohost
    length = ram_target.read_words(link, target, first, 1)[0]
    if length == 0 or length > 1024:
        return ""
    words = ram_target.read_words(link, target, first + 2, (length + 3) // 4)
    data = b"".join(w.to_bytes(4, "little") for w in words)
    return data[:length].decode(errors="replace")


def expected_text(case: Case) -> str:
    """Devolve o texto que o programa tem de imprimir.

    Parameters
    ----------
    case : Case
        O programa.

    Returns
    -------
    str
        O campo ``expected`` ou o conteúdo de ``uart_expected.txt`` do diretório de origem.
    """
    expected = case.get("expected")
    if expected is not None:
        return expected
    directory = TESTS_ROOT / case.get("expected_from", case["source"])
    return (directory / "uart_expected.txt").read_text(errors="replace")


def wait_for_verdict(link: JtagLink, cfg: dict[str, Any], printed: bytearray, finish_s: float) -> str:
    """Espera o ``tohost`` e acumula o que a UART imprime enquanto isso.

    Parameters
    ----------
    link : JtagLink
        Qual cabo e chip.
    cfg : dict
        O config carregado.
    printed : bytearray
        Recebe os bytes lidos da UART.
    finish_s : float
        Quanto esperar o ``tohost``.

    Returns
    -------
    str
        PASS, FAIL com o código ou TIMEOUT.
    """
    deadline = time.monotonic() + finish_s
    while time.monotonic() < deadline:
        uart_console.read_console(link, printed.extend, seconds=3)
        value = htif.read_tohost_configured(link, cfg)
        if htif.is_done(value):
            return "PASS" if value == 1 else f"FAIL({htif.fail_code(value)})"
    return "TIMEOUT"


def run_case(name: str, wait_s: float, finish_s: float) -> tuple[str, str]:
    """Roda um programa na placa e devolve o veredito e uma observação.

    Parameters
    ----------
    name : str
        Nome do programa, uma chave de ``CASES``.
    wait_s : float
        Quanto esperar entre soltar o núcleo e enviar a imagem.
    finish_s : float
        Quanto esperar o ``tohost`` depois do envio.

    Returns
    -------
    tuple of (str, str)
        PASS, FAIL ou TIMEOUT e o texto impresso pelo programa.
    """
    case = CASES[name]
    cfg = load_config(BUILD / case["config"])
    link = jtag_link(cfg)
    target = ram_target.target_from_config(cfg, link)
    mif = TESTS_ROOT / case["build"] / "real" / f"{name}.mif"
    expected = expected_text(case)

    survivor = htif.word_offset(cfg["memory"]["ram_base"], cfg["memory"]["ram_base"] + SURVIVOR_OFFSET)
    ram_target.write_word(link, target, survivor, 0)
    htif.clear_tohost_configured(link, cfg)

    printed = bytearray()
    if case.get("truncate"):
        words = rom_writer.mif_words(mif)[:FLASH_WORDS]
        flash_loader.load_image(link, words)
    else:
        rom_writer.write_rom_configured(link, cfg, mif)

    if case["send"]:
        time.sleep(wait_s)
        image = (TESTS_ROOT / case["source"] / "uart_input.txt").read_bytes()
        flash_image.send_image(link, image, printed.extend)
    verdict = wait_for_verdict(link, cfg, printed, finish_s)
    text = stdout_buffer(link, cfg, target) or printed.decode(errors="replace")
    if verdict == "PASS" and expected.strip() not in text:
        verdict = "FAIL(output)"
    return verdict, text


def run_cases(names: list[str], wait_s: float, finish_s: float) -> dict[str, str]:
    """Roda os programas pedidos, um a um, e devolve o veredito de cada um.

    Parameters
    ----------
    names : list of str
        Nomes de programas, chaves de ``CASES``.
    wait_s : float
        Quanto esperar entre soltar o núcleo e enviar a imagem.
    finish_s : float
        Quanto esperar o ``tohost`` depois do envio.

    Returns
    -------
    dict of {str: str}
        PASS, FAIL ou TIMEOUT por programa.
    """
    results: dict[str, str] = {}
    for name in names:
        verdict, text = run_case(name, wait_s, finish_s)
        print(f"{verdict} {name}: {text!r}", flush=True)
        results[name] = verdict
    return results


def host_label(value: str) -> str:
    """Valida o rótulo do host, que vira parte do nome de um arquivo em ``/tmp``.

    Parameters
    ----------
    value : str
        O texto recebido em ``--host``.

    Returns
    -------
    str
        O próprio texto, se for aceito.

    Raises
    ------
    argparse.ArgumentTypeError
        Se o texto for vazio ou tiver ``/`` ou espaço em branco.
    """
    if not value or "/" in value or any(c.isspace() for c in value):
        raise argparse.ArgumentTypeError(f"rótulo de host inválido: {value!r} (sem '/' nem espaço em branco)")
    return value


def main() -> None:
    """Lê a linha de comando, toma a trava da placa e roda os programas."""
    parser = argparse.ArgumentParser(description=(__doc__ or "").split("\n", maxsplit=1)[0])
    parser.add_argument(
        "--host",
        "--board",
        dest="host",
        type=host_label,
        required=True,
        help=(
            "rótulo do host da placa, o hostname curto da máquina (hostname -s) ou laptop, "
            "no nome da trava e do arquivo de resultado"
        ),
    )
    parser.add_argument("--cases", nargs="+", default=list(CASES))
    parser.add_argument("--wait", type=float, default=4.0)
    parser.add_argument("--finish", type=float, default=60.0)
    parser.add_argument("--out", default="build/board")
    parser.add_argument(
        "--skip-lock",
        action="store_true",
        help="o chamador já segura a trava e exporta RISCV_TOOLS_CABLE",
    )
    args = parser.parse_args()
    out = Path(args.out)
    out.mkdir(parents=True, exist_ok=True)
    if args.skip_lock:
        results = run_cases(args.cases, args.wait, args.finish)
    else:
        with board_session(args.host) as board:
            # o riscv-tools que jtag_link() consulta usa o cabo da placa travada
            os.environ["RISCV_TOOLS_CABLE"] = board["cable"]
            results = run_cases(args.cases, args.wait, args.finish)
    (out / f"results-update-{args.host}.json").write_text(json.dumps(results, indent=1, sort_keys=True))
    sys.exit(0 if all(v == "PASS" for v in results.values()) else 1)


if __name__ == "__main__":
    main()
