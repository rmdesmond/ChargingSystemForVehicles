"""The happy path, end to end through the real binary.

A session is PLUG -> AUTH -> READY -> CURRENT 16 -> (stop) AUTH -> UNPLUG, with
a pause requested by the vehicle in the middle. The assertions are on what the
daemon *says* (the reply to every command and the status socket) and on what
it *does* (the contactor lines in its log), not on anything inside the process.
"""

import time

import pytest

from virtual_vehicle import ProtocolError, read_status

SESSION = [
    # (command, state the daemon must report afterwards)
    ("plug", "Preparing"),
    ("auth", "SuspendedEV"),
    ("ready", "Charging"),
    ("suspend", "SuspendedEV"),
    ("ready", "Charging"),
    ("auth", "Finishing"),
    ("unplug", "Available"),
]

@pytest.mark.req("REQ-012")
@pytest.mark.req("REQ-001")
def test_full_session_reports_every_state_in_order(daemon, vehicle):
    assert vehicle.state() == "Available"
    vehicle.current(16)

    for command, expected in SESSION:
        started = time.monotonic()
        reported = getattr(vehicle, command)()
        elapsed = time.monotonic() - started
        assert reported == expected, f"{command}: got {reported}, wanted {expected}"
        # The status socket agrees with the command reply.
        assert vehicle.state() == expected
        # A reply is a local round trip, not a tick: well under the 1 s tick period.
        assert elapsed < 0.2, f"{command} took {elapsed:.3f} s"

@pytest.mark.req("REQ-002")
@pytest.mark.req("REQ-003")
def test_contactor_closes_once_per_charging_entry_and_opens_on_leaving(daemon, vehicle):
    for command, _ in SESSION:
        getattr(vehicle, command)()
    daemon.stop()

    actuations = [line.strip() for line in daemon.log_lines() if line.startswith(" contactor:")]
    # Charging was entered twice (around the suspend) and left twice.
    assert actuations == [
        "contactor: CLOSED",
        "contactor: OPEN",
        "contactor: CLOSED",
        "contactor: OPEN",
    ]

@pytest.mark.req("REQ-013")
def test_status_socket_is_read_only_and_reusable(daemon, vehicle):
    # Many readers, no effect on the session.
    for _ in range(5):
        assert read_status(daemon.status_socket) == "Available"
    vehicle.plug()
    for _ in range(5):
        assert read_status(daemon.status_socket) == "Preparing"

@pytest.mark.req("REQ-012")
def test_malformed_lines_are_refused_and_change_nothing(daemon, vehicle):
    vehicle.plug()
    assert vehicle.send_raw("plug") == "ERR unknown command"
    assert vehicle.send_raw("") == "ERR empty line"
    assert vehicle.send_raw("CURRENT 999") == "ERR CURRENT value out of range 0 to 80"
    with pytest.raises(ProtocolError):
        vehicle.send("BOGUS")
    assert vehicle.state() == "Preparing"

@pytest.mark.req("REQ-001")
def test_events_out_of_order_are_ignored(daemon, vehicle):
    # READY before AUTH: not a row in the table, so nothing happens.
    assert vehicle.plug() == "Preparing"
    assert vehicle.ready() == "Preparing"
    assert vehicle.auth() == "SuspendedEV"
    # ...and the earlier READY was not remembered as an event.
    assert vehicle.state() == "SuspendedEV"

@pytest.mark.req("REQ-009")
def test_daemon_stops_cleanly_on_sigterm_mid_session(daemon, vehicle):
    vehicle.plug()
    vehicle.auth()
    vehicle.ready()
    assert daemon.stop() == 0
    assert "signal 15, stopping" in daemon.log()