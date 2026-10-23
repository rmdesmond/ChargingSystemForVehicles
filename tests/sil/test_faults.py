"""Fault injection through the real binary.

Three ways a session goes wrong, each ending in Faulted with the contactor
open: a fault reported mid-charge, a contactor that does not close when told
to, and a vehicle that plugs in and then never does anything.
"""

import pytest

def contactor_log(daemon) -> list[str]:
    return [line.strip() for line in daemon.log_lines() if line.startswith(" contactor:")]

@pytest.mark.req("REQ-004")
@pytest.mark.req("REQ-005")
def test_fault_mid_charge_opens_contactor_and_only_clear_recovers(daemon, vehicle):
    vehicle.plug()
    vehicle.auth()
    assert vehicle.ready() == "Charging"
    
    assert vehicle.fault() == "Faulted"
    assert vehicle.state() == "Faulted"

    # Nothing the vehicle does gets it out; the contactor stays open.
    assert vehicle.unplug() == "Faulted"
    assert vehicle.plug() == "Faulted"
    assert vehicle.ready() == "Faulted"
    
    assert vehicle.clear() == "Available"
    daemon.stop()
    assert contactor_log(daemon) == ["contactor: CLOSED", "contactor: OPEN"]