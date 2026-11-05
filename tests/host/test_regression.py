"""Testes do plano e do relatório da regressão."""
import json
import tempfile
import unittest
from pathlib import Path

from tools.regression import regression as reg


class RegressionTests(unittest.TestCase):
    def test_plan_is_valid(self) -> None:
        plan = reg.load_plan()
        self.assertTrue(plan["stages"])

    def test_exclusion_without_reason_is_rejected(self) -> None:
        plan = json.loads(reg.PLAN_PATH.read_text(encoding="utf-8"))
        plan["exclusions"][0]["reason"] = ""
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "plan.json"
            path.write_text(json.dumps(plan), encoding="utf-8")
            with self.assertRaises(ValueError):
                reg.load_plan(path)

    def test_parse_verdicts(self) -> None:
        out = "=== Summary ===\n  PASS  a  [ASM]\n  FAIL  b  [C]\nnoise\n"
        self.assertEqual(reg.parse_verdicts(out), {"a": True, "b": False})

    def test_classify_divergences(self) -> None:
        sim = {"a": True, "b": True, "c": True}
        spike = {"a": True, "b": False, "c": False}
        classified, unclassified, stale = reg.classify_divergences(
            sim, spike, {"b": "motivo", "z": "motivo"}
        )
        self.assertEqual(classified, {"b": "motivo"})
        self.assertEqual(unclassified, ["c"])
        self.assertEqual(stale, ["z"])

    def test_report_lists_exclusions(self) -> None:
        plan = reg.load_plan()
        st = reg.StageResult("de0-cv-sdram", {"x": True}, 0, {"hex-id": "razão longa"})
        text = reg.render_report(plan, [st], ({}, [], []))
        self.assertIn("| de0-cv-sdram | hex-id | razão longa |", text)
        self.assertIn("verde", text)
