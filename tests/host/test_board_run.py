"""Testes do runner de placa: seleção da placa, trava e partição da lista de programas."""

import os
import re
import stat
import subprocess
import tempfile
import unittest
from pathlib import Path
from unittest import mock

from tools.board import board_run

FAKE_TOOLS = """#!/usr/bin/env python3
import json, sys
args = sys.argv[1:]
if "lock" in args:
    print(json.dumps({"id": "de0-cv-ws", "cable": "USB-Blaster [1-2]",
                      "device": "@1: x (0x02B050DD)", "lock_file": "/tmp/x.lock"}), flush=True)
    sys.stdin.read()
    open(sys.argv[0] + ".released", "w").write("released")
elif "--all" in args:
    print(json.dumps([{"id": "de0-cv-ws"}, {"id": "de0-cv-ws-2"}]))
"""


class BoardRunUnitTests(unittest.TestCase):
    """As funções do runner que não precisam de placa."""

    def test_the_runner_names_neither_a_lock_nor_an_idcode(self) -> None:
        source = Path(board_run.__file__).read_text(encoding="utf-8")
        self.assertNotIn("LOCKS", source)
        self.assertNotIn("de0cv_cable", source)
        self.assertNotIn("/tmp/de0cv", source)
        self.assertIsNone(re.search(r'"[0-9A-F]{8}"', source))

    def test_partition_takes_every_nth_item(self) -> None:
        self.assertEqual(board_run.partition(list("abcde"), "1/2"), ["b", "d"])

    def test_part_auto_is_the_position_among_the_present_boards(self) -> None:
        boards = [{"id": "de0-cv-ws"}, {"id": "de0-cv-ws-2"}]
        with mock.patch.object(board_run, "present_boards", return_value=boards):
            self.assertEqual(board_run.part_of("auto", "ws", {"id": "de0-cv-ws-2"}), "1/2")
            self.assertEqual(board_run.part_of("0/2", "ws", {"id": "x"}), "0/2")

    def test_the_board_comes_from_the_package_when_it_exists(self) -> None:
        with tempfile.TemporaryDirectory() as root:
            package = Path(root) / "Insper-RISC-V/boards/de0-cv"
            package.mkdir(parents=True)
            (package / "board.yaml").write_text("board: de0-cv\n")
            with mock.patch.object(board_run, "TESTS_ROOT", Path(root)):
                self.assertEqual(board_run.board_args(), ["--board", "de0-cv"])

    def test_the_stages_of_the_de2_115_choose_its_package(self) -> None:
        self.assertEqual(board_run.STAGES["de2-115"].get("board"), "de2-115")
        self.assertEqual(board_run.STAGES["de2-115-n2"].get("board"), "de2-115")
        with tempfile.TemporaryDirectory() as root:
            package = Path(root) / "Insper-RISC-V/boards/de2-115"
            package.mkdir(parents=True)
            (package / "board.yaml").write_text("board: de2-115\n")
            with (
                mock.patch.object(board_run, "TESTS_ROOT", Path(root)),
                mock.patch.object(board_run, "PLATFORM_BOARD", "de2-115"),
            ):
                self.assertEqual(board_run.board_args(), ["--board", "de2-115"])

    def test_without_the_package_the_runner_says_what_to_update(self) -> None:
        with (
            tempfile.TemporaryDirectory() as root,
            mock.patch.object(board_run, "TESTS_ROOT", Path(root)),
            self.assertRaisesRegex(RuntimeError, "Insper-RISC-V"),
        ):
            board_run.board_args()

    def test_the_session_holds_the_lock_until_it_ends(self) -> None:
        with tempfile.TemporaryDirectory() as root:
            fake = Path(root) / "fake-tools"
            fake.write_text(FAKE_TOOLS)
            fake.chmod(fake.stat().st_mode | stat.S_IXUSR)
            with (
                mock.patch.dict(os.environ, {"RISCV_TOOLS": str(fake)}),
                mock.patch.object(board_run, "board_args", return_value=["--board", "de0-cv"]),
            ):
                with board_run.board_session("ws") as board:
                    self.assertEqual(board["cable"], "USB-Blaster [1-2]")
                    self.assertFalse(Path(f"{fake}.released").exists())
                self.assertTrue(Path(f"{fake}.released").exists())

    def test_a_failed_lock_raises(self) -> None:
        with (
            mock.patch.dict(os.environ, {"RISCV_TOOLS": "true"}),
            mock.patch.object(board_run, "board_args", return_value=["--board", "de0-cv"]),
            self.assertRaises(RuntimeError),
            board_run.board_session("ws"),
        ):
            pass


class BootSequenceTests(unittest.TestCase):
    """A etapa que roda os testes um por vez, com FAIL e travamento no meio."""

    def test_the_sequence_has_ten_tests_with_a_fail_and_a_hang_in_the_middle(self) -> None:
        sequence = board_run.BOOT_SEQUENCE
        self.assertEqual(len(sequence), 10)
        self.assertIn("boot-seq-fail", sequence[1:-1])
        self.assertIn("boot-seq-hang", sequence[1:-1])
        self.assertEqual(sequence[-1], "boot-seq-pass")

    def test_every_non_passing_program_has_an_expected_verdict(self) -> None:
        for name in set(board_run.BOOT_SEQUENCE):
            if name != "boot-seq-pass":
                self.assertIn(name, board_run.BOOT_EXPECT)

    def test_the_boot_stages_exist_for_both_boards(self) -> None:
        self.assertEqual(board_run.STAGES["boot-seq"].get("sequence"), board_run.BOOT_SEQUENCE)
        self.assertEqual(board_run.STAGES["boot-seq-de2-115"].get("sequence"), board_run.BOOT_SEQUENCE)
        self.assertEqual(board_run.STAGES["boot-de2-115"].get("board"), "de2-115")
        self.assertNotIn("board", board_run.STAGES["boot"])

    def test_every_boot_stage_points_at_a_config_that_exists(self) -> None:
        for name in ("boot", "boot-seq", "boot-de2-115", "boot-seq-de2-115"):
            config = board_run.TESTS_ROOT / board_run.BUILD / board_run.STAGES[name]["config"]
            self.assertTrue(config.is_file(), config)

    def test_the_verdict_matches_the_expected_one(self) -> None:
        self.assertEqual(board_run.sequence_verdict("PASS", "PASS"), "PASS")
        self.assertEqual(board_run.sequence_verdict("FAIL", "FAIL"), "PASS")

    def test_a_different_verdict_carries_both(self) -> None:
        self.assertEqual(board_run.sequence_verdict("TIMEOUT", "PASS"), "TIMEOUT (esperado PASS)")

    def test_only_the_first_part_runs_the_sequence(self) -> None:
        with tempfile.TemporaryDirectory() as root:
            self.assertEqual(board_run.run_sequence("boot-seq", "ws", "1/2", Path(root), "cable"), {})

    def test_each_position_is_one_run_judged_against_its_expected_verdict(self) -> None:
        verdicts = {"boot-seq-pass": "PASS", "boot-seq-fail": "FAIL", "boot-seq-hang": ""}

        def fake_run(command: list[str], **_: object) -> subprocess.CompletedProcess[str]:
            name = command[-1]
            line = f"  {verdicts[name]}  {name}\n" if verdicts[name] else ""
            return subprocess.CompletedProcess(command, 0, line, "")

        with (
            tempfile.TemporaryDirectory() as root,
            mock.patch.object(board_run, "manifest_names", return_value=sorted(verdicts)),
            mock.patch.object(board_run.subprocess, "run", side_effect=fake_run),
        ):
            found = board_run.run_sequence("boot-seq", "ws", "0/1", Path(root), "cable")
        self.assertEqual(len(found), 10)
        self.assertEqual(set(found.values()), {"PASS"})
        self.assertEqual(found["06-boot-seq-hang"], "PASS")

    def test_a_program_that_never_reports_is_a_timeout(self) -> None:
        def fake_run(command: list[str], **_: object) -> subprocess.CompletedProcess[str]:
            return subprocess.CompletedProcess(command, 0, "", "")

        with (
            tempfile.TemporaryDirectory() as root,
            mock.patch.object(board_run, "manifest_names", return_value=sorted(set(board_run.BOOT_SEQUENCE))),
            mock.patch.object(board_run.subprocess, "run", side_effect=fake_run),
        ):
            found = board_run.run_sequence("boot-seq", "ws", "0/1", Path(root), "cable")
        self.assertEqual(found["01-boot-seq-pass"], "TIMEOUT (esperado PASS)")
        self.assertEqual(found["06-boot-seq-hang"], "PASS")

    def test_a_program_missing_from_the_manifest_stops_the_stage(self) -> None:
        with (
            tempfile.TemporaryDirectory() as root,
            mock.patch.object(board_run, "manifest_names", return_value=["boot-seq-pass"]),
            self.assertRaisesRegex(RuntimeError, "boot-seq-fail"),
        ):
            board_run.run_sequence("boot-seq", "ws", "0/1", Path(root), "cable")


if __name__ == "__main__":
    unittest.main()
