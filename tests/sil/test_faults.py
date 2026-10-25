"""Fault injection through the real binary.

Three ways a session goes wrong, each ending in Faulted with the contactor
open: a fault reported mid-charge, a contactor that does not close when told
to, and a vehicle that plugs in and then never does anything.
"""

import time

import pytest

##from tests.sil.conftest import daemon, start_daemon, vehicle
#from tools.virtual_vehicle import wait_for_state
from virtual_vehicle import VirtualVehicle, wait_for_state

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

@pytest.mark.req("REQ-011")
def test_stuck_contactor_faults_instead_of_charging(start_daemon):
    daemon = start_daemon("--sim", env={"CHARGER_SIM_CONTACTOR": "stuck"})
    with VirtualVehicle(daemon.vehicle_socket, daemon.status_socket) as vehicle:
        vehicle.plug()
        vehicle.auth()
        # The close is commanded, the device does not move, the daemon notices.
        assert vehicle.ready() == "Faulted"
        assert vehicle.state() == "Faulted"
    daemon.stop()
    assert contactor_log(daemon) == ["contactor: CLOSE FAILED (stuck)", "contactor: OPEN"]

@pytest.mark.req("REQ-006")
def test_vehicle_that_never_authorizes_times_out_to_available(start_daemon):
    # 60 s of the state machine's time in 3 s of ours.
    daemon = start_daemon("--time-scale", "20")
    with VirtualVehicle(daemon.vehicle_socket, daemon.status_socket) as vehicle:
        assert vehicle.plug() == "Preparing"
        # Still waiting well inside the timeout...
        time.sleep(1.5)
        assert vehicle.state() == "Preparing"
        # ...and given back once it expires. The tick runs every 50 ms at this
        # scale, so the state flips within 3.05 s of PLUG; allow some slack.
        assert wait_for_state(daemon.status_socket, "Available", timeout=2.5) == "Available"
        # A new session is possible without any operator action.
        assert vehicle.plug() == "Preparing"
    daemon.stop()
    assert contactor_log(daemon) == []