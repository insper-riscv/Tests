"""Run the integrated RV32 UART tests on the physical FPGA.

The script deliberately uses only the Python standard library for the serial
side.  It verifies that the existing Quartus bitstream was built from the
``uart-echo`` image, opens the Pico/Debug Probe UART in raw 115200 8N1 mode,
reprograms that bitstream to start from a clean state, and checks the exact
echo plus the JTAG mailbox. It can then replace FLASH over JTAG with
``uart-tx`` and ``uart-errors``, restart through the boot ROM, and validate
TX/RX, sticky error flags, W1C clearing and a raw byte burst without another
Quartus compile.
"""

from __future__ import annotations

import argparse
import errno
import fcntl
import hashlib
import math
import os
import select
import subprocess
import sys
import termios
import time
from collections.abc import Generator
from contextlib import contextmanager
from dataclasses import dataclass
from pathlib import Path
from typing import Any

from riscv_tools import mailbox, quartus_program, rom_writer
from riscv_tools.jtag import JtagLink, detect_jtag_hardware
from riscv_tools.settings import load_config

REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
DEFAULT_CONFIG = REPOSITORY_ROOT / "tools/riscv_build/config.yaml"
DEFAULT_PORT = Path("/dev/ttyACM0")
DEFAULT_PAYLOAD = b"RX test\r\n"
UART_TX_EXPECTED = b"UART OK\r\n"
UART_ERRORS_PREFIX = b"ATR"
UART_ERRORS_READY = b"OB"
UART_ERRORS_RX_INPUT = bytes((0x12, 0x34))
UART_BURST_PAYLOAD = bytes(range(32))
UART_ERRORS_FINAL = b"P"
DEFAULT_TIMEOUT_SECONDS = 2.0
STARTUP_DELAY_SECONDS = 0.20
QUIET_WINDOW_SECONDS = 0.10
MAILBOX_POLL_SECONDS = 0.05
BAUD_RATE = 115_200
ECHO_BYTE_COUNT = 9
UART_ECHO_TEST = "uart-echo"
UART_TX_TEST = "uart-tx"
UART_ERRORS_TEST = "uart-errors"
ALL_TESTS = "all"


class UartHardwareTestError(RuntimeError):
    """A failed precondition or an observed UART/mailbox mismatch."""


@dataclass(frozen=True)
class Artifacts:
    """Validated files needed to run the selected UART hardware tests."""

    project_dir: Path
    sof_file: str
    sof_path: Path
    echo_mif: Path
    tx_mif: Path | None
    errors_mif: Path | None
    active_mif: Path
    echo_mif_sha256: str
    tx_mif_sha256: str | None
    errors_mif_sha256: str | None


def format_bytes(payload: bytes) -> str:
    """Return a stable hexadecimal representation for diagnostics."""
    return payload.hex(" ") if payload else "<none>"


def parse_payload_hex(value: str) -> bytes:
    """Parse exactly nine hexadecimal bytes for the fixed-size firmware."""
    try:
        payload = bytes.fromhex(value)
    except ValueError as exc:
        raise argparse.ArgumentTypeError(f"invalid hexadecimal payload: {exc}") from exc
    if len(payload) != ECHO_BYTE_COUNT:
        raise argparse.ArgumentTypeError(
            f"uart-echo requires exactly {ECHO_BYTE_COUNT} bytes, got {len(payload)}"
        )
    return payload


def positive_float(value: str) -> float:
    """Parse a strictly positive command-line duration."""
    try:
        parsed = float(value)
    except ValueError as exc:
        raise argparse.ArgumentTypeError(f"not a number: {value}") from exc
    if not math.isfinite(parsed) or parsed <= 0:
        raise argparse.ArgumentTypeError("value must be finite and greater than zero")
    return parsed


def _required_file(path: Path, label: str) -> None:
    if not path.is_file():
        raise UartHardwareTestError(f"{label} not found: {path}")


def validate_artifacts(
    root: Path,
    cfg: dict[str, Any],
    port: Path,
    selected_test: str = UART_ECHO_TEST,
) -> Artifacts:
    """Validate every local precondition before touching JTAG hardware."""
    if not port.exists():
        raise UartHardwareTestError(
            f"serial port not found: {port} (check /dev/ttyACM* and the Pico USB cable)"
        )

    build_dir = root / cfg["paths"]["build_dir"]
    echo_mif = build_dir / "real/uart-echo.mif"
    tx_mif = build_dir / "real/uart-tx.mif"
    errors_mif = build_dir / "real/uart-errors.mif"
    project_dir = (root / cfg["quartus"]["project_dir"]).resolve()
    active_mif = project_dir / cfg["quartus"]["rom_mif_target"]
    sof_file = cfg["quartus"]["sof_file"]
    sof_path = project_dir / sof_file

    _required_file(echo_mif, "compiled uart-echo MIF")
    if selected_test in (UART_TX_TEST, ALL_TESTS):
        _required_file(tx_mif, "compiled uart-tx MIF")
    else:
        tx_mif = None
    if selected_test in (UART_ERRORS_TEST, ALL_TESTS):
        _required_file(errors_mif, "compiled uart-errors MIF")
    else:
        errors_mif = None
    _required_file(active_mif, "Quartus active MIF")
    _required_file(sof_path, "Quartus SOF")

    echo_image = echo_mif.read_bytes()
    if echo_image != active_mif.read_bytes():
        raise UartHardwareTestError(
            "the Quartus init.mif is not uart-echo.mif; run a full Quartus "
            "compile with build/real/uart-echo.mif before this test"
        )
    if sof_path.stat().st_mtime_ns < active_mif.stat().st_mtime_ns:
        raise UartHardwareTestError(
            f"{sof_path} is older than {active_mif}; recompile Quartus before testing"
        )

    return Artifacts(
        project_dir=project_dir,
        sof_file=sof_file,
        sof_path=sof_path,
        echo_mif=echo_mif,
        tx_mif=tx_mif,
        errors_mif=errors_mif,
        active_mif=active_mif,
        echo_mif_sha256=hashlib.sha256(echo_image).hexdigest(),
        tx_mif_sha256=(
            hashlib.sha256(tx_mif.read_bytes()).hexdigest()
            if tx_mif is not None
            else None
        ),
        errors_mif_sha256=(
            hashlib.sha256(errors_mif.read_bytes()).hexdigest()
            if errors_mif is not None
            else None
        ),
    )


def configure_serial(fd: int) -> list[Any]:
    """Put a TTY in raw 115200 8N1 mode and return its original settings."""
    original = termios.tcgetattr(fd)
    attrs = termios.tcgetattr(fd)
    attrs[0] = 0
    attrs[1] = 0
    attrs[2] = termios.CS8 | termios.CREAD | termios.CLOCAL
    attrs[3] = 0
    attrs[4] = termios.B115200
    attrs[5] = termios.B115200
    attrs[6][termios.VMIN] = 0
    attrs[6][termios.VTIME] = 0
    termios.tcsetattr(fd, termios.TCSANOW, attrs)
    termios.tcflush(fd, termios.TCIFLUSH)
    return original


@contextmanager
def serial_session(port: Path) -> Generator[int]:
    """Open a serial port exclusively and always restore its prior settings."""
    fd = os.open(port, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
    original: list[Any] | None = None
    try:
        # picocom on this workstation uses flock(2); matching it gives a clear
        # busy error instead of allowing two programs to consume the same RX.
        fcntl.flock(fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
        tioc_excl = getattr(termios, "TIOCEXCL", None)
        if tioc_excl is not None:
            fcntl.ioctl(fd, tioc_excl)
        original = configure_serial(fd)
        yield fd
    finally:
        active_error = sys.exc_info()[0] is not None
        try:
            if original is not None:
                termios.tcsetattr(fd, termios.TCSANOW, original)
        except OSError:
            if not active_error:
                raise
        finally:
            os.close(fd)


def write_all(fd: int, payload: bytes, timeout_s: float) -> None:
    """Write all bytes using one absolute deadline and partial-write handling."""
    deadline = time.monotonic() + timeout_s
    remaining_payload = memoryview(payload)
    while remaining_payload:
        remaining_time = deadline - time.monotonic()
        if remaining_time <= 0:
            raise UartHardwareTestError(
                f"timeout while sending {len(payload)} UART bytes"
            )
        _, writable, _ = select.select([], [fd], [], remaining_time)
        if not writable:
            continue
        try:
            count = os.write(fd, remaining_payload)
        except BlockingIOError:
            continue
        if count == 0:
            raise UartHardwareTestError("serial port accepted zero bytes while writing")
        remaining_payload = remaining_payload[count:]


def read_exact(fd: int, size: int, timeout_s: float) -> bytes:
    """Read up to ``size`` bytes using one absolute deadline."""
    deadline = time.monotonic() + timeout_s
    received = bytearray()
    while len(received) < size:
        remaining_time = deadline - time.monotonic()
        if remaining_time <= 0:
            break
        readable, _, _ = select.select([fd], [], [], remaining_time)
        if not readable:
            break
        try:
            chunk = os.read(fd, size - len(received))
        except BlockingIOError:
            continue
        if not chunk:
            raise UartHardwareTestError(
                "serial port disconnected while waiting for UART data"
            )
        received.extend(chunk)
    return bytes(received)


def read_for(fd: int, duration_s: float) -> bytes:
    """Collect any unexpected bytes received during a quiet window."""
    deadline = time.monotonic() + duration_s
    received = bytearray()
    while True:
        remaining_time = deadline - time.monotonic()
        if remaining_time <= 0:
            return bytes(received)
        readable, _, _ = select.select([fd], [], [], remaining_time)
        if not readable:
            return bytes(received)
        try:
            chunk = os.read(fd, 4096)
        except BlockingIOError:
            continue
        if not chunk:
            raise UartHardwareTestError(
                "serial port disconnected during the UART quiet window"
            )
        received.extend(chunk)


def validate_uart_output(
    label: str,
    expected: bytes,
    received: bytes,
    extra: bytes = b"",
) -> None:
    """Raise a byte-precise diagnostic for partial, wrong, or extra output."""
    if len(received) != len(expected):
        raise UartHardwareTestError(
            f"{label} timeout: received {len(received)}/{len(expected)} bytes\n"
            f"  expected: {format_bytes(expected)}\n"
            f"  received: {format_bytes(received)}"
        )
    if received != expected:
        mismatch = next(
            index
            for index, (actual, wanted) in enumerate(
                zip(received, expected, strict=True)
            )
            if actual != wanted
        )
        raise UartHardwareTestError(
            f"{label} mismatch at byte {mismatch}: "
            f"expected 0x{expected[mismatch]:02x}, "
            f"received 0x{received[mismatch]:02x}\n"
            f"  expected: {format_bytes(expected)}\n"
            f"  received: {format_bytes(received)}"
        )
    if extra:
        raise UartHardwareTestError(
            f"{label} returned unexpected extra bytes: {format_bytes(extra)}"
        )


def validate_echo(expected: bytes, received: bytes, extra: bytes = b"") -> None:
    """Backward-compatible echo-specific validation wrapper."""
    validate_uart_output("UART echo", expected, received, extra)


def exchange_echo(fd: int, payload: bytes, timeout_s: float) -> bytes:
    """Send one fixed-size payload and require exactly the same bytes back."""
    write_all(fd, payload, timeout_s)
    received = read_exact(fd, len(payload), timeout_s)
    extra = read_for(fd, QUIET_WINDOW_SECONDS)
    validate_echo(payload, received, extra)
    return received


def receive_expected(
    fd: int,
    expected: bytes,
    timeout_s: float,
    label: str,
) -> bytes:
    """Receive exactly one expected message and reject trailing bytes."""
    received = read_exact(fd, len(expected), timeout_s)
    extra = read_for(fd, QUIET_WINDOW_SECONDS)
    validate_uart_output(label, expected, received, extra)
    return received


def exchange_errors_and_burst(
    fd: int,
    timeout_s: float,
) -> tuple[bytes, bytes, bytes]:
    """Run the uart-errors serial protocol and return its three responses."""
    prefix = receive_expected(
        fd,
        UART_ERRORS_PREFIX,
        timeout_s,
        "UART TX_DROPPED",
    )

    write_all(fd, UART_ERRORS_RX_INPUT, timeout_s)
    ready = receive_expected(
        fd,
        UART_ERRORS_READY,
        timeout_s,
        "UART RX_OVERRUN/W1C",
    )

    write_all(fd, UART_BURST_PAYLOAD, timeout_s)
    burst_and_final = receive_expected(
        fd,
        UART_BURST_PAYLOAD + UART_ERRORS_FINAL,
        timeout_s,
        "UART burst",
    )
    return prefix, ready, burst_and_final


def read_result_mailbox(link: JtagLink, cfg: dict[str, Any]) -> int:
    """Read the project's PASS/FAIL word once after the serial exchange."""
    return mailbox.read_mailbox(
        link,
        cfg["quartus"]["ram_mem_instance"],
        cfg["memory"]["ram_base"],
        cfg["memory"]["mailbox_addr"],
    )


def wait_for_result_mailbox(
    link: JtagLink,
    cfg: dict[str, Any],
    timeout_s: float,
) -> int:
    """Poll until firmware reports PASS/FAIL or the deadline expires."""
    deadline = time.monotonic() + timeout_s
    last_value = 0
    while True:
        last_value = read_result_mailbox(link, cfg)
        if last_value in (mailbox.PASS, mailbox.FAIL):
            return last_value
        remaining_time = deadline - time.monotonic()
        if remaining_time <= 0:
            return last_value
        time.sleep(min(MAILBOX_POLL_SECONDS, remaining_time))


def validate_mailbox(value: int) -> None:
    """Require the firmware's explicit PASS result."""
    if value == mailbox.PASS:
        return
    if value == mailbox.FAIL:
        raise UartHardwareTestError("firmware reported MAILBOX=2 (FAIL)")
    raise UartHardwareTestError(
        f"firmware did not finish: MAILBOX={value} (expected {mailbox.PASS})"
    )


def load_and_start_firmware(
    fd: int,
    link: JtagLink,
    cfg: dict[str, Any],
    mif_path: Path,
) -> None:
    """Replace both FLASH copies and restart a core parked in boot ROM."""
    rom_writer.write_rom(link, cfg["quartus"]["rom_mem_instances"], mif_path)

    # Flush before the restart pulse. A UART firmware may start transmitting
    # while quartus_stp is still returning, so never flush afterward.
    termios.tcflush(fd, termios.TCIFLUSH)
    mailbox.pulse_go_flag(
        link,
        cfg["quartus"]["ram_mem_instance"],
        cfg["memory"]["ram_base"],
        cfg["memory"]["go_flag_addr"],
    )


def run_hardware_test(args: argparse.Namespace) -> None:
    """Execute the selected UART programs and verify serial plus mailbox."""
    root = args.root.resolve()
    cfg = load_config(args.config.resolve())
    artifacts = validate_artifacts(root, cfg, args.port, args.test)

    print(
        "Artifacts: PASS\n"
        f"  SOF: {artifacts.sof_path}\n"
        f"  uart-echo MIF: {artifacts.echo_mif}\n"
        f"  uart-echo SHA256: {artifacts.echo_mif_sha256}"
    )
    if artifacts.tx_mif is not None:
        print(
            f"  uart-tx MIF: {artifacts.tx_mif}\n"
            f"  uart-tx SHA256: {artifacts.tx_mif_sha256}"
        )
    if artifacts.errors_mif is not None:
        print(
            f"  uart-errors MIF: {artifacts.errors_mif}\n"
            f"  uart-errors SHA256: {artifacts.errors_mif_sha256}"
        )

    with serial_session(args.port) as fd:
        print(f"UART: {args.port}, {BAUD_RATE} 8N1 raw")
        hardware_name = detect_jtag_hardware()
        link = JtagLink(
            hardware_name=hardware_name,
            device_name=cfg["quartus"]["jtag_device"],
        )

        if args.skip_program:
            print("FPGA programming: SKIPPED (firmware must be freshly waiting for RX)")
        else:
            print(f"Programming FPGA through {hardware_name}...")
            quartus_program.program_only(
                hardware_name=hardware_name,
                project_dir=artifacts.project_dir,
                sof_file=artifacts.sof_file,
            )
            time.sleep(STARTUP_DELAY_SECONDS)

        # FPGA reconfiguration can create transient edges on UART_TX. The
        # baked-in echo firmware is silent before RX, so this flush is safe.
        termios.tcflush(fd, termios.TCIFLUSH)
        print(f"TX[{len(args.payload)}]: {format_bytes(args.payload)}")
        received = exchange_echo(fd, args.payload, args.timeout)
        print(f"RX[{len(received)}]: {format_bytes(received)}")
        print("UART ECHO: PASS")

        echo_mailbox = wait_for_result_mailbox(link, cfg, args.timeout)
        validate_mailbox(echo_mailbox)
        print(f"uart-echo MAILBOX={echo_mailbox}: PASS")

        completed_tests = [UART_ECHO_TEST]
        if args.test in (UART_TX_TEST, ALL_TESTS):
            if artifacts.tx_mif is None:
                raise UartHardwareTestError(
                    "uart-tx was selected without a validated uart-tx MIF"
                )
            print(f"Loading {UART_TX_TEST} through JTAG...")
            load_and_start_firmware(fd, link, cfg, artifacts.tx_mif)
            tx_received = receive_expected(
                fd,
                UART_TX_EXPECTED,
                args.timeout,
                "UART TX",
            )
            print(f"RX[{len(tx_received)}]: {format_bytes(tx_received)}")
            print("UART TX: PASS")

            tx_mailbox = wait_for_result_mailbox(link, cfg, args.timeout)
            validate_mailbox(tx_mailbox)
            print(f"uart-tx MAILBOX={tx_mailbox}: PASS")
            completed_tests.append(UART_TX_TEST)

        if args.test in (UART_ERRORS_TEST, ALL_TESTS):
            if artifacts.errors_mif is None:
                raise UartHardwareTestError(
                    "uart-errors was selected without a validated uart-errors MIF"
                )
            print(f"Loading {UART_ERRORS_TEST} through JTAG...")
            load_and_start_firmware(fd, link, cfg, artifacts.errors_mif)
            prefix, ready, burst_and_final = exchange_errors_and_burst(
                fd,
                args.timeout,
            )
            print(f"RX[{len(prefix)}] TX_DROPPED/W1C: {format_bytes(prefix)}")
            print(
                f"TX[{len(UART_ERRORS_RX_INPUT)}] RX_OVERRUN stimulus: "
                f"{format_bytes(UART_ERRORS_RX_INPUT)}"
            )
            print(f"RX[{len(ready)}] RX_OVERRUN/W1C: {format_bytes(ready)}")
            print(
                f"TX[{len(UART_BURST_PAYLOAD)}] burst: "
                f"{format_bytes(UART_BURST_PAYLOAD)}"
            )
            print(
                f"RX[{len(burst_and_final)}] burst + PASS marker: "
                f"{format_bytes(burst_and_final)}"
            )
            print("UART ERRORS, W1C AND BURST: PASS")

            errors_mailbox = wait_for_result_mailbox(link, cfg, args.timeout)
            validate_mailbox(errors_mailbox)
            print(f"uart-errors MAILBOX={errors_mailbox}: PASS")
            completed_tests.append(UART_ERRORS_TEST)

    print(f"INTEGRATED UART HARDWARE TEST: PASS ({', '.join(completed_tests)})")


def build_parser() -> argparse.ArgumentParser:
    """Build the command-line parser."""
    parser = argparse.ArgumentParser(
        description=(
            "Validate uart-echo, uart-tx and uart-errors on the physical UART "
            "plus the RV32 PASS mailbox. Close picocom before running."
        )
    )
    parser.add_argument("--port", type=Path, default=DEFAULT_PORT)
    parser.add_argument("--config", type=Path, default=DEFAULT_CONFIG)
    parser.add_argument("--root", type=Path, default=REPOSITORY_ROOT)
    parser.add_argument(
        "--test",
        choices=(ALL_TESTS, UART_ECHO_TEST, UART_TX_TEST, UART_ERRORS_TEST),
        default=ALL_TESTS,
        help=(
            "test to validate (default: all); every selection first runs the "
            "baked-in uart-echo so the core reaches its safe restart loop"
        ),
    )
    parser.add_argument(
        "--payload-hex",
        dest="payload",
        type=parse_payload_hex,
        default=DEFAULT_PAYLOAD,
        metavar='"52 58 20 74 65 73 74 0d 0a"',
        help="exactly nine bytes (default: RX test followed by CR LF)",
    )
    parser.add_argument(
        "--timeout",
        type=positive_float,
        default=DEFAULT_TIMEOUT_SECONDS,
        help=(
            f"serial read/write timeout in seconds (default: {DEFAULT_TIMEOUT_SECONDS})"
        ),
    )
    parser.add_argument(
        "--skip-program",
        action="store_true",
        help=(
            "do not reprogram the SOF; only safe immediately after programming, "
            "while the baked-in uart-echo is still waiting for its first byte"
        ),
    )
    return parser


def _print_os_error(exc: OSError, port: Path) -> None:
    if isinstance(exc, PermissionError):
        print(
            f"FAIL: permission denied opening {port}; check membership in the "
            "dialout group",
            file=sys.stderr,
        )
    elif exc.errno in (errno.EBUSY, errno.EAGAIN):
        print(
            f"FAIL: {port} is busy; close picocom or another serial monitor",
            file=sys.stderr,
        )
    else:
        print(f"FAIL: {exc}", file=sys.stderr)


def main(argv: list[str] | None = None) -> int:
    """CLI entry point with concise hardware-oriented error messages."""
    args = build_parser().parse_args(argv)
    try:
        run_hardware_test(args)
    except UartHardwareTestError as exc:
        print(f"FAIL: {exc}", file=sys.stderr)
        return 1
    except subprocess.CalledProcessError as exc:
        print(
            f"FAIL: Quartus/JTAG command exited with status {exc.returncode}",
            file=sys.stderr,
        )
        return 1
    except OSError as exc:
        _print_os_error(exc, args.port)
        return 1
    except RuntimeError as exc:
        print(f"FAIL: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
