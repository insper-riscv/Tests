"""Testes do roteiro que roda os programas de boot por perfil (``tools/boot/boot_profile.py``)."""

import unittest
from typing import Any

from tools.boot import boot_profile as boot

# Um disassembly de BOOT com o que um perfil completo tem: CSR e fence.i.
FULL_TEXT = "  10: 30005073  csrwi mstatus,0\n  20: 0000100f  fence.i\n"
# Um disassembly de BOOT sem nenhuma extensão.
BASE_TEXT = "   0: 0540006f  j 54 <boot_start>\n"


class CombinationTests(unittest.TestCase):
    """O nome do perfil dá as extensões e a base."""

    def test_the_minimal_profile_has_no_extension(self) -> None:
        self.assertEqual(boot.COMBINATIONS["rv32i"].extensions, frozenset())

    def test_letters_and_z_extensions_are_split(self) -> None:
        found = boot.COMBINATIONS["caches"].extensions
        self.assertEqual(found, frozenset({"m", "zicclsm", "zicntr", "zihpm", "zifencei"}))

    def test_the_e_base_is_recognised(self) -> None:
        self.assertTrue(boot.COMBINATIONS["rv32e"].is_e)
        self.assertFalse(boot.COMBINATIONS["rv32i"].is_e)

    def test_the_cache_combination_needs_the_instruction_fence(self) -> None:
        self.assertIn("zifencei", boot.COMBINATIONS["caches"].extensions)
        self.assertIn("icache=on", boot.COMBINATIONS["caches"].memory)

    def test_counters_imply_the_csr_even_without_zicsr_in_the_name(self) -> None:
        self.assertTrue(boot.COMBINATIONS["caches"].has_zicsr)
        self.assertTrue(boot.COMBINATIONS["rv32im_zicsr"].has_zicsr)
        self.assertFalse(boot.COMBINATIONS["rv32i"].has_zicsr)
        self.assertFalse(boot.COMBINATIONS["rv32e"].has_zicsr)


class BootProblemTests(unittest.TestCase):
    """O que a BOOT montada tem de mostrar para cada perfil."""

    def test_a_correct_minimal_boot_has_no_problem(self) -> None:
        found = boot.boot_problems(boot.COMBINATIONS["rv32i"], 452, BASE_TEXT, 0x100)
        self.assertEqual(found, [])

    def test_a_correct_full_boot_has_no_problem(self) -> None:
        found = boot.boot_problems(boot.COMBINATIONS["caches"], 2000, FULL_TEXT, 0x100)
        self.assertEqual(found, [])

    def test_a_boot_over_4_kib_is_reported(self) -> None:
        found = boot.boot_problems(boot.COMBINATIONS["rv32i"], 4100, BASE_TEXT, 0x100)
        self.assertEqual(len(found), 1)
        self.assertIn("4100", found[0])

    def test_a_moved_wait_point_is_reported(self) -> None:
        self.assertIn("0x100", boot.boot_problems(boot.COMBINATIONS["rv32i"], 452, BASE_TEXT, 0x180)[0])

    def test_a_missing_wait_point_is_reported(self) -> None:
        self.assertIn("None", boot.boot_problems(boot.COMBINATIONS["rv32i"], 452, BASE_TEXT, None)[0])

    def test_fence_i_without_zifencei_is_reported(self) -> None:
        found = boot.boot_problems(boot.COMBINATIONS["rv32im_zicsr"], 600, "csrwi mstatus,0\n fence.i\n", 0x100)
        self.assertEqual(len(found), 1)
        self.assertIn("fence.i presente", found[0])

    def test_a_missing_fence_i_with_zifencei_is_reported(self) -> None:
        found = boot.boot_problems(boot.COMBINATIONS["caches"], 600, "csrwi mstatus,0\n", 0x100)
        self.assertIn("fence.i ausente", found[0])

    def test_a_csr_instruction_without_zicsr_is_reported(self) -> None:
        found = boot.boot_problems(boot.COMBINATIONS["rv32e"], 600, "  csrw mtvec,t0\n", 0x100)
        self.assertEqual(len(found), 1)
        self.assertIn("CSR", found[0])

    def test_csr_names_inside_other_words_do_not_count(self) -> None:
        self.assertEqual(boot.boot_problems(boot.COMBINATIONS["rv32i"], 452, "  j csr_free_label\n", 0x100), [])


class PlatformTests(unittest.TestCase):
    """O ajuste do mapa da plataforma na cópia de trabalho."""

    @staticmethod
    def platform() -> dict[str, Any]:
        """Um mapa mínimo com o que depende das interrupções.

        Returns
        -------
        dict of {str: Any}
            O trecho do ``platform.yaml`` modular que o ajuste lê.
        """
        return {
            "core": {"extensions": ["M", "Zicsr", "Interrupts"]},
            "interrupts": {"clock_hz": 10_000_000},
            "peripherals": {"TIMER": {}, "CLINT": {}, "PLIC": {}},
        }

    def test_dropping_interrupts_removes_the_extra_the_blocks_and_the_windows(self) -> None:
        platform = self.platform()
        boot.drop_interrupts(platform)
        self.assertEqual(platform["core"]["extensions"], ["M", "Zicsr"])
        self.assertNotIn("interrupts", platform)
        self.assertEqual(list(platform["peripherals"]), ["TIMER"])

    def test_dropping_twice_is_harmless(self) -> None:
        platform = self.platform()
        boot.drop_interrupts(platform)
        boot.drop_interrupts(platform)
        self.assertEqual(platform["core"]["extensions"], ["M", "Zicsr"])


class ConfigTests(unittest.TestCase):
    """O config do Tests escrito sobre o config gerado da plataforma."""

    def test_the_c_config_points_at_the_c_programs(self) -> None:
        text = boot.config_text("c-boot", cxx=False)
        self.assertIn("c_dir: c-boot", text)
        self.assertNotIn("cxx", text)

    def test_the_cxx_config_turns_the_driver_on(self) -> None:
        text = boot.config_text("cxx-boot", cxx=True)
        self.assertIn("c_dir: cxx-boot", text)
        self.assertIn("cxx: true", text)


class VerdictTests(unittest.TestCase):
    """A leitura do resumo da simulação."""

    def test_verdict_lines_are_read(self) -> None:
        text = "=== Summary ===\n  PASS  boot-state\n  FAIL  boot-wait-restart\nruído\n"
        self.assertEqual(boot.parse_verdicts(text), {"boot-state": "PASS", "boot-wait-restart": "FAIL"})

    def test_a_log_without_summary_has_no_verdict(self) -> None:
        self.assertEqual(boot.parse_verdicts("analyze x.vhd\n"), {})


if __name__ == "__main__":
    unittest.main()
