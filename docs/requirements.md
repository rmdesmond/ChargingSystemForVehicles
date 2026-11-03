# Requirements

The single source of the requirement ids. Every id here must have at least one
test that carries it; `scripts/trace.py` checks that on every pull request and
writes the result to [trace-matrix.md](trace-matrix.md). Design background is
in [design.md](design.md); the numbers are never reused or renumbered.

How a test carries an id:

| Test kind | Convention | Example |
|-----------|------------|---------|
| GoogleTest | `REQ_nnn` tokens in the test name | `TEST_F(StateMachineTest, REQ_006_PreparingTimesOutAt60Seconds)` |
| CTest (`add_test`) | `REQ_nnn` tokens in the test name | `add_test(NAME REQ_009_daemon_service_stops_on_sigterm ...)` |
| pytest (SIL) | one `@pytest.mark.req("REQ-nnn")` per id | `@pytest.mark.req("REQ-006")` |

Types: **functional** (what the controller does), **safety** (what it must
never do; these survive into the upcoming safety concept), **interface** (what
another program or person can rely on), **service** (the contract with the
service manager).

| Id | Type | Requirement |
|----|------|-------------|
| REQ-001 | functional | The controller shall take exactly the transitions in the design table and ignore every other event. |
| REQ-002 | functional | The contactor shall be commanded closed only on entering Charging. |
| REQ-003 | functional | The contactor shall be commanded open on every transition out of Charging. |
| REQ-004 | safety | A Fault event in any state shall command the contactor open and enter Faulted. |
| REQ-005 | safety | While Faulted, the contactor shall not be commanded closed; only Clear leaves Faulted. |
| REQ-006 | functional | If Preparing lasts 60 s without authorization, the controller shall return to Available. |
| REQ-007 | safety | The contactor shall not be commanded closed unless the vehicle interface reports plugged in and ready; otherwise the controller shall open the contactor and enter Faulted. |
| REQ-008 | service | The daemon shall report readiness once start-up is complete, and shall report liveness from its main loop at intervals of at most half the watchdog timeout it was given. |
| REQ-009 | service | On SIGTERM the daemon shall leave its main loop within one loop period (1 s) and exit with status 0. |
| REQ-010 | service | If the daemon reports no liveness for 10 s, or exits with a failure, the service manager shall restart it. |
| REQ-011 | safety | After commanding the contactor closed, if the contactor does not report closed, the controller shall command it open and enter Faulted. |
| REQ-012 | interface | When started with `--vehicle-socket PATH`, the daemon shall accept newline-delimited commands on that socket, apply them in order, and answer every line with `STATE <state>` or `ERR <reason>`. Malformed or unknown lines shall change nothing. |
| REQ-013 | interface | When started with `--status-socket PATH`, the daemon shall write `STATE <state>` to every connection on that socket and close it. |
| REQ-014 | interface | The vehicle socket shall be created with mode 0600 and the status socket with mode 0666; a leftover socket file at either path shall be replaced at start-up. |
| REQ-015 | interface | A command line longer than 256 bytes shall be answered with `ERR line too long` and shall not be applied. |

Reserved: REQ-016 (upcoming, CAN power module), REQ-017 (upcoming, 500 ms
heartbeat loss opens the contactor).

Notes on verification:

- REQ-010 is met by `systemd/charger-daemon.service`, not by code. Its test
  reads the unit file (`WatchdogSec=10`, `Restart=on-failure`); the live check
  (`kill -SEGV`, `NRestarts` climbing) was done by hand on the Pi on 2026-10-19
  and belongs to the HIL level once that exists.
- REQ-006 under SIL runs at `--time-scale 20`: three real seconds stand for
  the 60 s. The unit test holds the boundary at exactly 60 s.
