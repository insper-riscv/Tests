"""Testes do rótulo de host que ``board_update`` aceita em ``--host``."""

import argparse
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools" / "board"))

import board_update


class HostLabelTests(unittest.TestCase):
    """A validação do rótulo que vira parte do nome da trava."""

    def test_accepts_a_short_hostname_and_laptop(self) -> None:
        self.assertEqual(board_update.host_label("D-20003267-LINUX"), "D-20003267-LINUX")
        self.assertEqual(board_update.host_label("laptop"), "laptop")

    def test_rejects_a_slash_whitespace_or_empty(self) -> None:
        for bad in ("a/b", "../x", "a b", "a\tb", ""):
            with self.subTest(bad=bad), self.assertRaises(argparse.ArgumentTypeError):
                board_update.host_label(bad)


if __name__ == "__main__":
    unittest.main()
