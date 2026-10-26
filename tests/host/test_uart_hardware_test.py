import argparse
import fcntl
import os
import pty
import tempfile
import termios
import threading
import time
import unittest
from pathlib import Path
from unittest import mock

from riscv_tools import mailbox

from tools.riscv_build import uart_hardware_test as uart_hw


class UartHardwareTestUnitTests(unittest.TestCase):
    def test_default_payload_is_exact_firmware_contract(self) -> None:
        self.assertEqual(uart_hw.DEFAULT_PAYLOAD, b"RX test\r\n")
        self.assertEqual(len(uart_hw.DEFAULT_PAYLOAD), uart_hw.ECHO_BYTE_COUNT)
        self.assertEqual(uart_hw.UART_TX_EXPECTED, b"UART OK\r\n")
        self.assertEqual(uart_hw.UART_ERRORS_PREFIX, b"ATR")
        self.assertEqual(uart_hw.UART_ERRORS_READY, b"OB")
        self.assertEqual(uart_hw.UART_ERRORS_RX_INPUT, b"\x12\x34")
        self.assertEqual(uart_hw.UART_BURST_PAYLOAD, bytes(range(32)))
        self.assertEqual(uart_hw.UART_ERRORS_FINAL, b"P")

    def test_cli_defaults_to_both_uart_tests(self) -> None:
        args = uart_hw.build_parser().parse_args([])
        self.assertEqual(args.test, uart_hw.ALL_TESTS)

    def test_parse_payload_hex_preserves_cr_lf(self) -> None:
        payload = uart_hw.parse_payload_hex("52 58 20 74 65 73 74 0d 0a")
        self.assertEqual(payload, b"RX test\r\n")

    def test_parse_payload_hex_rejects_wrong_length(self) -> None:
        with self.assertRaises(argparse.ArgumentTypeError):
            uart_hw.parse_payload_hex("00 01")

    def test_timeout_must_be_positive_and_finite(self) -> None:
        self.assertEqual(uart_hw.positive_float("2.5"), 2.5)
        for value in ("0", "-1", "nan", "inf", "-inf"):
            with (
                self.subTest(value=value),
                self.assertRaises(argparse.ArgumentTypeError),
            ):
                uart_hw.positive_float(value)

    def test_serial_session_sets_raw_115200_and_restores_settings(self) -> None:
        master, slave = pty.openpty()
        path = Path(os.ttyname(slave))
        original = termios.tcgetattr(slave)
        try:
            with uart_hw.serial_session(path) as fd:
                active = termios.tcgetattr(fd)
                self.assertEqual(active[0], 0)
                self.assertEqual(active[1], 0)
                self.assertEqual(active[3], 0)
                self.assertEqual(active[4], termios.B115200)
                self.assertEqual(active[5], termios.B115200)
            self.assertEqual(termios.tcgetattr(slave), original)
        finally:
            os.close(master)
            os.close(slave)

    def test_serial_session_refuses_a_locked_port(self) -> None:
        master, slave = pty.openpty()
        path = Path(os.ttyname(slave))
        fcntl.flock(slave, fcntl.LOCK_EX | fcntl.LOCK_NB)
        try:
            with self.assertRaises(BlockingIOError), uart_hw.serial_session(path):
                pass
        finally:
            os.close(master)
            os.close(slave)

    def test_write_and_fragmented_read_are_lossless(self) -> None:
        payload = uart_hw.DEFAULT_PAYLOAD

        read_end, write_end = os.pipe()
        collected = bytearray()

        def collect_write() -> None:
            while len(collected) < len(payload):
                collected.extend(os.read(read_end, len(payload) - len(collected)))

        collector = threading.Thread(target=collect_write)
        collector.start()
        try:
            uart_hw.write_all(write_end, payload, 1.0)
        finally:
            collector.join(timeout=1.0)
            os.close(read_end)
            os.close(write_end)
        self.assertFalse(collector.is_alive())
        self.assertEqual(bytes(collected), payload)

        read_end, write_end = os.pipe()

        def echo_fragmented() -> None:
            for chunk in (payload[:2], payload[2:7], payload[7:]):
                os.write(write_end, chunk)
                time.sleep(0.005)

        worker = threading.Thread(target=echo_fragmented)
        worker.start()
        try:
            received = uart_hw.read_exact(read_end, len(payload), 1.0)
        finally:
            worker.join(timeout=1.0)
            os.close(read_end)
            os.close(write_end)

        self.assertFalse(worker.is_alive())
        self.assertEqual(received, payload)

    def test_read_exact_returns_partial_data_at_timeout(self) -> None:
        read_end, write_end = os.pipe()
        try:
            os.write(write_end, b"RX")
            received = uart_hw.read_exact(read_end, 9, 0.03)
        finally:
            os.close(read_end)
            os.close(write_end)
        self.assertEqual(received, b"RX")

    def test_read_exact_reports_disconnect_instead_of_spinning(self) -> None:
        read_end, write_end = os.pipe()
        os.close(write_end)
        try:
            with self.assertRaisesRegex(uart_hw.UartHardwareTestError, "disconnected"):
                uart_hw.read_exact(read_end, 1, 1.0)
        finally:
            os.close(read_end)

    def test_validate_echo_reports_partial_and_mismatch(self) -> None:
        with self.assertRaisesRegex(uart_hw.UartHardwareTestError, "received 2/9"):
            uart_hw.validate_echo(uart_hw.DEFAULT_PAYLOAD, b"RX")
        with self.assertRaisesRegex(uart_hw.UartHardwareTestError, "byte 8"):
            uart_hw.validate_echo(uart_hw.DEFAULT_PAYLOAD, b"RX test\rX")
        with self.assertRaisesRegex(uart_hw.UartHardwareTestError, "extra bytes"):
            uart_hw.validate_echo(
                uart_hw.DEFAULT_PAYLOAD, uart_hw.DEFAULT_PAYLOAD, b"!"
            )

    def test_validate_mailbox_accepts_only_pass(self) -> None:
        uart_hw.validate_mailbox(mailbox.PASS)
        with self.assertRaisesRegex(uart_hw.UartHardwareTestError, "FAIL"):
            uart_hw.validate_mailbox(mailbox.FAIL)
        with self.assertRaisesRegex(uart_hw.UartHardwareTestError, "did not finish"):
            uart_hw.validate_mailbox(0)

    def test_wait_for_mailbox_retries_a_running_firmware(self) -> None:
        link = mock.sentinel.link
        cfg = mock.sentinel.cfg
        with (
            mock.patch.object(
                uart_hw,
                "read_result_mailbox",
                side_effect=[0, mailbox.PASS],
            ) as read_mailbox,
            mock.patch.object(uart_hw.time, "sleep") as sleep,
        ):
            value = uart_hw.wait_for_result_mailbox(link, cfg, 1.0)

        self.assertEqual(value, mailbox.PASS)
        self.assertEqual(read_mailbox.call_count, 2)
        sleep.assert_called_once()

    def test_load_and_start_flushes_before_restart_pulse(self) -> None:
        events = []
        link = mock.sentinel.link
        mif = Path("uart-tx.mif")
        cfg = {
            "quartus": {
                "rom_mem_instances": [1, 3],
                "ram_mem_instance": 2,
            },
            "memory": {
                "ram_base": 0x8000,
                "go_flag_addr": 0x2FFF8,
            },
        }
        with (
            mock.patch.object(
                uart_hw.rom_writer,
                "write_rom",
                side_effect=lambda *_: events.append("write"),
            ) as write_rom,
            mock.patch.object(
                uart_hw.termios,
                "tcflush",
                side_effect=lambda *_: events.append("flush"),
            ),
            mock.patch.object(
                uart_hw.mailbox,
                "pulse_go_flag",
                side_effect=lambda *_: events.append("pulse"),
            ) as pulse,
        ):
            uart_hw.load_and_start_firmware(7, link, cfg, mif)

        self.assertEqual(events, ["write", "flush", "pulse"])
        write_rom.assert_called_once_with(link, [1, 3], mif)
        pulse.assert_called_once_with(link, 2, 0x8000, 0x2FFF8)

    def test_uart_tx_bytes_emitted_during_restart_are_not_lost(self) -> None:
        read_end, write_end = os.pipe()
        cfg = {
            "quartus": {
                "rom_mem_instances": [1, 3],
                "ram_mem_instance": 2,
            },
            "memory": {
                "ram_base": 0x8000,
                "go_flag_addr": 0x2FFF8,
            },
        }

        def pulse_while_returning(*_args: object) -> None:
            os.write(write_end, uart_hw.UART_TX_EXPECTED)

        try:
            with (
                mock.patch.object(uart_hw.rom_writer, "write_rom"),
                mock.patch.object(uart_hw.termios, "tcflush"),
                mock.patch.object(
                    uart_hw.mailbox,
                    "pulse_go_flag",
                    side_effect=pulse_while_returning,
                ),
            ):
                uart_hw.load_and_start_firmware(
                    read_end,
                    mock.sentinel.link,
                    cfg,
                    Path("uart-tx.mif"),
                )
                received = uart_hw.receive_expected(
                    read_end,
                    uart_hw.UART_TX_EXPECTED,
                    1.0,
                    "UART TX",
                )
        finally:
            os.close(read_end)
            os.close(write_end)

        self.assertEqual(received, uart_hw.UART_TX_EXPECTED)

    def test_error_and_burst_exchange_follows_firmware_protocol(self) -> None:
        fd = 7
        timeout = 1.5
        expected_final = uart_hw.UART_BURST_PAYLOAD + uart_hw.UART_ERRORS_FINAL
        with (
            mock.patch.object(
                uart_hw,
                "receive_expected",
                side_effect=[
                    uart_hw.UART_ERRORS_PREFIX,
                    uart_hw.UART_ERRORS_READY,
                    expected_final,
                ],
            ) as receive,
            mock.patch.object(uart_hw, "write_all") as write,
        ):
            responses = uart_hw.exchange_errors_and_burst(fd, timeout)

        self.assertEqual(
            responses,
            (
                uart_hw.UART_ERRORS_PREFIX,
                uart_hw.UART_ERRORS_READY,
                expected_final,
            ),
        )
        self.assertEqual(
            receive.call_args_list,
            [
                mock.call(fd, uart_hw.UART_ERRORS_PREFIX, timeout, "UART TX_DROPPED"),
                mock.call(
                    fd,
                    uart_hw.UART_ERRORS_READY,
                    timeout,
                    "UART RX_OVERRUN/W1C",
                ),
                mock.call(fd, expected_final, timeout, "UART burst"),
            ],
        )
        self.assertEqual(
            write.call_args_list,
            [
                mock.call(fd, uart_hw.UART_ERRORS_RX_INPUT, timeout),
                mock.call(fd, uart_hw.UART_BURST_PAYLOAD, timeout),
            ],
        )

    def test_validate_artifacts_requires_matching_fresh_image(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            port = root / "ttyACM0"
            test_mif = root / "build/real/uart-echo.mif"
            tx_mif = root / "build/real/uart-tx.mif"
            errors_mif = root / "build/real/uart-errors.mif"
            active_mif = root / "quartus/init.mif"
            sof = root / "quartus/output/core.sof"
            for path in (port, test_mif, active_mif, sof):
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(b"echo-image")
            tx_mif.write_bytes(b"tx-image")
            errors_mif.write_bytes(b"errors-image")
            now_ns = time.time_ns()
            os.utime(active_mif, ns=(now_ns, now_ns))
            os.utime(sof, ns=(now_ns + 1_000_000, now_ns + 1_000_000))

            cfg = {
                "paths": {"build_dir": "build"},
                "quartus": {
                    "project_dir": "quartus",
                    "rom_mif_target": "init.mif",
                    "sof_file": "output/core.sof",
                },
            }
            artifacts = uart_hw.validate_artifacts(root, cfg, port)
            self.assertEqual(artifacts.echo_mif, test_mif)
            self.assertIsNone(artifacts.tx_mif)
            self.assertIsNone(artifacts.errors_mif)

            artifacts = uart_hw.validate_artifacts(root, cfg, port, uart_hw.ALL_TESTS)
            self.assertEqual(artifacts.tx_mif, tx_mif)
            self.assertEqual(artifacts.errors_mif, errors_mif)

            tx_mif.unlink()
            with self.assertRaisesRegex(
                uart_hw.UartHardwareTestError, "compiled uart-tx MIF"
            ):
                uart_hw.validate_artifacts(root, cfg, port, uart_hw.UART_TX_TEST)

            errors_mif.unlink()
            with self.assertRaisesRegex(
                uart_hw.UartHardwareTestError, "compiled uart-errors MIF"
            ):
                uart_hw.validate_artifacts(
                    root,
                    cfg,
                    port,
                    uart_hw.UART_ERRORS_TEST,
                )

            active_mif.write_bytes(b"different")
            with self.assertRaisesRegex(
                uart_hw.UartHardwareTestError, "is not uart-echo.mif"
            ):
                uart_hw.validate_artifacts(root, cfg, port)

    def test_missing_port_fails_before_config_or_jtag(self) -> None:
        args = argparse.Namespace(
            root=Path("/tmp/unused-root"),
            config=Path("/tmp/unused-config"),
            port=Path("/definitely/missing/tty"),
            payload=uart_hw.DEFAULT_PAYLOAD,
            timeout=1.0,
            skip_program=False,
            test=uart_hw.ALL_TESTS,
        )
        cfg = {"paths": {"build_dir": "build"}, "quartus": {}}
        with (
            mock.patch.object(uart_hw, "load_config", return_value=cfg),
            mock.patch.object(uart_hw, "detect_jtag_hardware") as detect,
            self.assertRaisesRegex(uart_hw.UartHardwareTestError, "serial port"),
        ):
            uart_hw.run_hardware_test(args)
        detect.assert_not_called()


if __name__ == "__main__":
    unittest.main()
