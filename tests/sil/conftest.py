"""Fixtures for the SIL suite: a real daemon binary, started fresh for every test.

Software-in-the-loop: the product binary is real, everything around it is
simulated. Each test gets its own daemon with its own socket paths in a
temporary directory, so tests cannot see each other and can run in parallel.

The binary is found through CHARGER_DAEMON, or defaults to the native build.
The daemon's stderr goes to a file next to the sockets; its last lines are
printed when a test fails, so the log is in the pytest output.
"""
from __future__ import annotations

#from concurrent.futures import process
#from importlib.resources import path
import os
import signal
import socket
import subprocess
import time
from dataclasses import dataclass, field
from pathlib import Path
#from urllib import request

import pytest

from virtual_vehicle import VirtualVehicle

REPO = Path(__file__).resolve().parents[2]
DEFAULT_BINARY = REPO / "build-native" / "app" / "charger-daemon"

@dataclass
class Daemon:
    """A running charger-daemon and the paths the harness needs to talk to it."""
    
    process: subprocess.Popen
    vehicle_socket: str
    status_socket: str
    log_path: Path
    extra_args: list[str] = field(default_factory=list)

    def stop(self, timeout: float = 3.0) -> int:
        """SIGTERM, as systemd would; returns the exit status."""
        if self.process.poll() is None:
            self.process.send_signal(signal.SIGTERM)
            try:
                self.process.wait(timeout)
            except subprocess.TimeoutExpired:
                self.process.kill()
                self.process.wait()
        return self.process.returncode

    def log(self) -> str:
        return self.log_path.read_text()

    def log_lines(self) -> list[str]:
        return self.log().splitlines()

def _wait_for_socket(path: str, timeout: float) -> None:
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as probe:
            try:
                probe.connect(path)
                return
            except OSError:
                time.sleep(0.02)
    raise TimeoutError(f"daemon did not open {path} within {timeout} s")

@pytest.fixture
def start_daemon(tmp_path: Path, request: pytest.FixtureRequest):
    """A factory: start_daemon("--sim", env={...}) -> Daemon. Stops it after the test."""
    started: list[Daemon] = []
    binary = Path(os.environ.get("CHARGER_DAEMON", DEFAULT_BINARY))
    if not binary.exists():
        pytest.fail(f"no daemon binary at {binary}; build the native preset or set CHARGER_DAEMON")

    def _start(*extra_args: str, env: dict[str, str] | None = None) -> Daemon:
        vehicle_socket = str(tmp_path / "vehicle.sock")
        status_socket = str(tmp_path / "status.sock")
        log_path = tmp_path / f"daemon-{len(started)}.log"
        command = [
            str(binary),
            "--vehicle-socket", vehicle_socket,
            "--status-socket", status_socket,
            *extra_args,
        ]
        with log_path.open("w") as log:
            process = subprocess.Popen(
                command, stderr=log, env={**os.environ, **(env or {})}
            )
        daemon = Daemon(process, vehicle_socket, status_socket, log_path, list(extra_args))
        started.append(daemon)
        try:
            _wait_for_socket(status_socket, timeout=5.0)
        except TimeoutError:
            daemon.stop()
            pytest.fail(f"daemon did not start:\n{daemon.log()}")
        return daemon

    yield _start

    for daemon in started:
        status = daemon.stop()
        failed = request.node.rep_call.failed if hasattr(request.node, "rep_call") else False
        if failed or status != 0:
            print(f"\n--- daemon log ({daemon.log_path}) ---")
            print("\n".join(daemon.log_lines()[-30:]))
        # REQ-009: a daemon that was asked to stop exits with status 0.
        assert status == 0, f"daemon exited with status {status}"

@pytest.fixture
def daemon(start_daemon) -> Daemon:
    """A daemon with the default options: logging contactor, real time."""
    return start_daemon()

@pytest.fixture
def vehicle(daemon: Daemon):
    """A virtual vehicle connected to the default daemon."""
    with VirtualVehicle(daemon.vehicle_socket, daemon.status_socket) as connection:
        yield connection

@pytest.hookimpl(hookwrapper=True)
def pytest_runtest_makereport(item, call):
    """Remembers each test's outcome on the item, so fixtures can print logs on failure."""
    outcome = yield
    report = outcome.get_result()
    setattr(item, f"rep_{report.when}", report)