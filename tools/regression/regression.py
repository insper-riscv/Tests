"""Regressão completa dos programas de teste e verificação cruzada com o Spike.

Roda a suíte de programas (``asm/`` e ``c/``) em cada plataforma do plano
(``plan.json``), confere cada programa também no Spike e emite um relatório
único em Markdown e em JSON. Nenhum programa fica de fora sem uma justificativa
escrita no plano, e o relatório lista cada exclusão com o motivo.
"""

import argparse
import json
import re
import subprocess
import sys
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any

PLAN_PATH = Path(__file__).with_name("plan.json")
REPORT_DIR = Path("build/regression")
# Linha do resumo que o riscv-tools imprime para cada programa.
_VERDICT = re.compile(r"^\s*(PASS|FAIL)\s+(\S+)")
_MIN_REASON_CHARS = 10


@dataclass
class StageResult:
    """Resultado de uma etapa da regressão.

    Attributes
    ----------
    name : str
        Nome da etapa.
    verdicts : dict of {str: bool}
        Se cada programa passou.
    exit_code : int
        Código de saída do último comando da etapa.
    excluded : dict of {str: str}
        Programa excluído e a justificativa.
    """

    name: str
    verdicts: dict[str, bool] = field(default_factory=dict)
    exit_code: int = 0
    excluded: dict[str, str] = field(default_factory=dict)

    @property
    def passed(self) -> bool:
        """Se a etapa terminou sem falha."""
        return self.exit_code == 0 and all(self.verdicts.values())


def load_plan(path: Path = PLAN_PATH) -> dict[str, Any]:
    """Carrega o plano e confere que toda exclusão tem justificativa.

    Parameters
    ----------
    path : Path, optional
        Arquivo do plano.

    Returns
    -------
    dict of {str: Any}
        O plano.

    Raises
    ------
    ValueError
        Uma exclusão ou divergência sem justificativa escrita.
    """
    plan: dict[str, Any] = json.loads(path.read_text(encoding="utf-8"))
    stages = {s["name"] for s in plan["stages"]}
    for item in plan["exclusions"]:
        if item["stage"] not in stages:
            raise ValueError(f"exclusão em etapa desconhecida: {item}")
        _require_reason(item)
    for item in plan["spike"]["divergences"]:
        _require_reason(item)
    return plan


def _require_reason(item: dict[str, str]) -> None:
    """Exige uma justificativa de tamanho mínimo.

    Parameters
    ----------
    item : dict of {str: str}
        A exclusão ou divergência.

    Raises
    ------
    ValueError
        A justificativa falta ou é curta demais.
    """
    if len(item.get("reason", "").strip()) < _MIN_REASON_CHARS:
        raise ValueError(f"sem justificativa escrita: {item}")


def parse_verdicts(output: str) -> dict[str, bool]:
    """Extrai o veredito de cada programa do resumo do riscv-tools.

    Parameters
    ----------
    output : str
        Saída de ``sim`` ou ``spike-run``.

    Returns
    -------
    dict of {str: bool}
        Se cada programa passou.
    """
    verdicts: dict[str, bool] = {}
    for line in output.splitlines():
        match = _VERDICT.match(line)
        if match:
            verdicts[match.group(2)] = match.group(1) == "PASS"
    return verdicts


def classify_divergences(
    sim: dict[str, bool],
    spike: dict[str, bool],
    known: dict[str, str],
) -> tuple[dict[str, str], list[str], list[str]]:
    """Compara os vereditos da simulação e do Spike.

    Parameters
    ----------
    sim : dict of {str: bool}
        Vereditos da simulação.
    spike : dict of {str: bool}
        Vereditos do Spike.
    known : dict of {str: str}
        Divergências classificadas, com o motivo.

    Returns
    -------
    tuple
        As divergências classificadas, as não classificadas e as
        classificações que já não divergem.
    """
    differing = {n for n in sim if n in spike and sim[n] != spike[n]}
    differing |= {n for n in spike if n not in sim}
    classified = {n: known[n] for n in sorted(differing) if n in known}
    unclassified = sorted(differing - set(known))
    stale = sorted(set(known) - differing)
    return classified, unclassified, stale


def _run(cmd: list[str], log: Path) -> tuple[int, str]:
    """Roda um comando, copia a saída para o log e para o terminal.

    Parameters
    ----------
    cmd : list of str
        O comando.
    log : Path
        Arquivo que recebe a saída completa.

    Returns
    -------
    tuple of (int, str)
        O código de saída e a saída.
    """
    print("+", " ".join(cmd), flush=True)
    proc = subprocess.run(  # noqa: S603 - comando montado pelo plano versionado
        cmd, capture_output=True, text=True, check=False
    )
    out = proc.stdout + proc.stderr
    log.parent.mkdir(parents=True, exist_ok=True)
    with log.open("a", encoding="utf-8") as handle:
        handle.write(f"$ {' '.join(cmd)}\n{out}\n")
    return proc.returncode, out


def run_sim_stage(
    stage: dict[str, str], excluded: dict[str, str], report_dir: Path
) -> StageResult:
    """Compila e simula os programas de uma plataforma.

    Parameters
    ----------
    stage : dict of {str: str}
        A etapa do plano.
    excluded : dict of {str: str}
        Programas excluídos e justificativas.
    report_dir : Path
        Pasta dos logs.

    Returns
    -------
    StageResult
        O resultado da etapa.
    """
    base = ["riscv-tools", "--config", stage["config"]]
    log = report_dir / f"{stage['name']}.log"
    result = StageResult(stage["name"], excluded=excluded)
    for step in (["generate-header"], ["compile", "--emit", "hex"]):
        code, _ = _run([*base, *step], log)
        if code != 0:
            result.exit_code = code
            return result
    sim = [*base, "sim"]
    if excluded:
        sim += ["--exclude", ",".join(sorted(excluded))]
    result.exit_code, out = _run(sim, log)
    result.verdicts = parse_verdicts(out)
    return result


def run_spike_stage(
    config: str, skip: set[str], report_dir: Path
) -> StageResult:
    """Roda os programas no Spike.

    Parameters
    ----------
    config : str
        Configuração da plataforma de referência.
    skip : set of str
        Programas que a simulação não rodou.
    report_dir : Path
        Pasta dos logs.

    Returns
    -------
    StageResult
        Os vereditos do Spike; ``exit_code`` só indica falha de preparo,
        pois falhas de programa são tratadas pela classificação.
    """
    base = ["riscv-tools", "--config", config]
    log = report_dir / "spike.log"
    result = StageResult("spike")
    code, _ = _run([*base, "compile", "--emit", "mif"], log)
    if code != 0:
        result.exit_code = code
        return result
    _, out = _run([*base, "spike-run"], log)
    result.verdicts = {
        n: ok for n, ok in parse_verdicts(out).items() if n not in skip
    }
    if not result.verdicts:
        result.exit_code = 1
    return result


def render_report(
    plan: dict[str, Any],
    stages: list[StageResult],
    crosscheck: tuple[dict[str, str], list[str], list[str]] | None,
) -> str:
    """Monta o relatório único em Markdown.

    Parameters
    ----------
    plan : dict of {str: Any}
        O plano.
    stages : list of StageResult
        Os resultados das etapas de simulação.
    crosscheck : tuple or None
        O resultado de ``classify_divergences``, ou None sem Spike.

    Returns
    -------
    str
        O relatório.
    """
    desc = {s["name"]: s["description"] for s in plan["stages"]}
    lines = ["# Relatório de regressão", "", "## 1. Etapas", ""]
    lines += ["| Etapa | Programas | Passaram | Falharam | Excluídos | Resultado |"]
    lines += ["| :--- | ---: | ---: | ---: | ---: | :--- |"]
    for st in stages:
        ok = sum(st.verdicts.values())
        row = (
            f"| {st.name} | {len(st.verdicts)} | {ok} | "
            f"{len(st.verdicts) - ok} | {len(st.excluded)} | "
            f"{'verde' if st.passed else 'VERMELHO'} |"
        )
        lines.append(row)
    lines += ["", "## 2. Exclusões justificadas", ""]
    lines += ["| Etapa | Programa | Justificativa |", "| :--- | :--- | :--- |"]
    for st in stages:
        for name, reason in sorted(st.excluded.items()):
            lines.append(f"| {st.name} | {name} | {reason} |")
    lines += ["", "## 3. Descrição das etapas", ""]
    lines += [f"- {n}: {d}" for n, d in desc.items()]
    for st in stages:
        failed = sorted(n for n, ok in st.verdicts.items() if not ok)
        if failed or st.exit_code:
            lines += ["", f"### Falhas em {st.name}", ""]
            lines += [f"- {n}" for n in failed] or ["- falha de preparo"]
    lines += ["", "## 4. Verificação cruzada com o Spike", ""]
    if crosscheck is None:
        lines.append("Não executada.")
    else:
        classified, unclassified, stale = crosscheck
        lines += ["| Programa | Classificação |", "| :--- | :--- |"]
        lines += [f"| {n} | {r} |" for n, r in classified.items()]
        lines += ["", f"Divergências sem classificação: {unclassified or 'nenhuma'}."]
        lines += [f"Classificações que já não divergem: {stale or 'nenhuma'}."]
    return "\n".join(lines) + "\n"


def regression(skip_spike: bool, report_dir: Path) -> int:
    """Executa a regressão completa e escreve o relatório.

    Parameters
    ----------
    skip_spike : bool
        Pula a verificação cruzada.
    report_dir : Path
        Pasta dos logs e do relatório.

    Returns
    -------
    int
        0 se tudo passou e nenhuma divergência ficou sem classificação.
    """
    plan = load_plan()
    report_dir.mkdir(parents=True, exist_ok=True)
    for old in report_dir.glob("*.log"):
        old.unlink()
    stages: list[StageResult] = []
    for stage in plan["stages"]:
        excluded = {
            i["test"]: i["reason"]
            for i in plan["exclusions"]
            if i["stage"] == stage["name"]
        }
        stages.append(run_sim_stage(stage, excluded, report_dir))
    crosscheck = None
    spike_failed = False
    if not skip_spike:
        sim = stages[0].verdicts
        spike = run_spike_stage(plan["spike"]["config"], set(), report_dir)
        spike_failed = spike.exit_code != 0
        known = {d["test"]: d["reason"] for d in plan["spike"]["divergences"]}
        crosscheck = classify_divergences(sim, spike.verdicts, known)
    (report_dir / "report.md").write_text(
        render_report(plan, stages, crosscheck), encoding="utf-8"
    )
    (report_dir / "report.json").write_text(
        json.dumps({s.name: s.verdicts for s in stages}, indent=2) + "\n",
        encoding="utf-8",
    )
    print((report_dir / "report.md").read_text(encoding="utf-8"))
    bad = not all(s.passed for s in stages) or spike_failed
    if crosscheck is not None and crosscheck[1]:
        bad = True
    return 1 if bad else 0


def main(argv: list[str] | None = None) -> int:
    """Ponto de entrada.

    Parameters
    ----------
    argv : list of str, optional
        Argumentos; padrão ``sys.argv``.

    Returns
    -------
    int
        Código de saída.
    """
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--skip-spike", action="store_true")
    parser.add_argument("--report-dir", type=Path, default=REPORT_DIR)
    args = parser.parse_args(argv)
    return regression(args.skip_spike, args.report_dir)


if __name__ == "__main__":
    sys.exit(main())
