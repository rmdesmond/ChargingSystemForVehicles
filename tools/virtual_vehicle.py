"""The virtual vehicle: the SIL harness's hand on the daemon.

It connects to the daemon's vehicle socket and sends the line protocol from
docs/design.md, one command per call, returning the daemon's reply. It also
reads the status socket, which is how a test (or a person) asks "what state is
it in?" without being able to change anything.

Used from pytest (tests/sil/) and from a shell:

    python3 tools/virtual_vehicle.py /tmp/v.sock /tmp/s.sock plug auth ready state
"""

from __future__ import annotations

import socket
import sys
import time

# Every state the daemon can report; must match docs/design.md.
STATES = ("Available", "Preparing", "SuspendedEV", "Charging", "Finishing", "Faulted")

class ProtocolError(RuntimeError):
    """The daemon answered with ERR, or did not answer at all."""

class VirtualVehicle:
    """One connection to the vehicle socket, kept open for the whole session."""

    def __init__(self, vehicle_socket: str, status_socket: str, timeout: float = 2.0):
        self.status_socket = status_socket
        self._sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self._sock.settimeout(timeout)
        self._sock.connect(vehicle_socket)
        self._reader = self._sock.makefile("r", encoding="ascii", newline="\n")

    def close(self) -> None:
        self._reader.close()
        self._sock.close()

    def __enter__(self) -> "VirtualVehicle":
        return self

    def __exit__(self, *exc) -> None:
        self.close()

    # -- the protocol, one method per command -----------------------------
    def plug(self) -> str:
        return self.send("PLUG")

    def unplug(self) -> str:
        return self.send("UNPLUG")

    def auth(self) -> str:
        return self.send("AUTH")

    def ready(self) -> str:
        return self.send("READY")

    def suspend(self) -> str:
        return self.send("SUSPEND")

    def fault(self) -> str:
        return self.send("FAULT")

    def clear(self) -> str:
        return self.send("CLEAR")

    def current(self, amperes: float) -> str:
        return self.send(f"CURRENT {amperes:g}")

    def state(self) -> str:
        """The state as the status socket reports it: a fresh connection each time."""
        return read_status(self.status_socket)

    # -- plumbing ---------------------------------------------------------

    def send(self, line: str) -> str:
        """Sends one line, returns the state named in the reply, raises on ERR."""
        self._sock.sendall((line + "\n").encode("ascii"))
        reply = self._reader.readline().rstrip("\n")
        if reply.startswith("STATE "):
            return reply[len("STATE "):]
        raise ProtocolError(f"{line!r} -> {reply!r}")

    def send_raw(self, line: str) -> str:
        """Sends one line and returns the whole reply; for testing the ERR path."""
        self._sock.sendall((line + "\n").encode("ascii"))
        return self._reader.readline().rstrip("\n")

def read_status(status_socket: str, timeout: float = 2.0) -> str:
    """Connects to the status socket, reads the one line it sends, returns the state."""
    with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as sock:
        sock.settimeout(timeout)
        sock.connect(status_socket)
        reply = sock.makefile("r", encoding="ascii").readline().rstrip("\n")
    if not reply.startswith("STATE "):
        raise ProtocolError(f"status socket said {reply!r}")
    return reply[len("STATE "):]

def wait_for_state(status_socket: str, expected: str, timeout: float) -> str:
    """Polls the status socket until it reports `expected`; returns the last state seen."""
    deadline = time.monotonic() + timeout
    seen = read_status(status_socket)
    while seen != expected and time.monotonic() < deadline:
        time.sleep(0.05)
        seen = read_status(status_socket)
    return seen

def main(argv: list[str]) -> int:
    if len(sys.argv) < 3:
        print(__doc__)
        return 2
    vehicle_socket, status_socket, *commands = argv
    with VirtualVehicle(vehicle_socket, status_socket) as vehicle:
        for command in commands:
            name, _, argument = command.partition("=")
            method = getattr(vehicle, name)
            result = method(float(argument)) if argument else method()
            print(f"{command:>12} -> {result}")
    return 0

if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))