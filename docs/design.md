# Design: charging state machine

The daemon's core is one state machine per connector. It is written as a
transition table so that this document, the code and the tests can be compared
row by row.

## States

The names are the OCPP 1.6 connector statuses, so week 3's status reporting is
a direct mapping.

| State      | Meaning                                                    | Contactor |
-------------|------------------------------------------------------------|-----------|
| Available  | Idle, nothing connected                                    | open      |
| Preparing | Cable connected, waiting for the user to authorize          | open      |
| SuspendedEV | Authorized; the charger is ready but the vehicle is not   | open      |
| Charging | Energy is flowing                                            | closed    |
| Finishing | Session stopped by the user; waiting for the cable to go    | open      |
| Faulted | A fault was reported; nothing happens until it is cleared     | open      |

## Events

| Event     | Meaning                                                        |
|-----------|----------------------------------------------------------------|
| PlugIn    | The connector was mated with the vehicle                       |
| Authorize | A valid credential was presented: once to start, again to stop |
| EvReady   | The vehicle signalled it is ready to accept energy             |
| EvSuspend | The vehicle paused the session (battery full, preconditioning) |
| Unplug    | The connector was removed                                      |
| Fault     | Any monitor reported a fault                                   |
| Clear     | An operator cleared the fault                                  |

## Transition table

| #  | From         | Event      | To          | Contactor |
|----|--------------|------------|-------------|-----------|
| 1  | Available    | Plugin     | Preparing   | -         |
| 2  | Preparing    | Authorize  | SuspendedEV | -         |  // Suspended because EV must also be ready
| 3  | Preparing    | Unplug     | Available   | -         |
| 4  | SuspendedEV  | EvReady    | Charging    | close     |  // Ok. EV is ready. Charge!
| 5  | SuspendedEV  | Authorize  | Finishing   | -         |
| 6  | SuspendedEV  | Unplug     | Available   | -         |
| 7  | Charging     | EvSuspend  | SuspendedEV | open      |
| 8  | Charging     | Authorize  | Finishing   | open      |
| 9  | Charging     | Unplug     | Available   | open      |
| 10 | Finishing    | Unplug     | Available   | -         |
| 11 | Available    | Fault      | Faulted     | open      |
| 12 | Preparing    | Fault      | Faulted     | open      |
| 13 | Charging     | Fault      | Faulted     | open      |
| 14 | SuspendedEV  | Fault      | Faulted     | open      |
| 15 | Finishing    | Fault      | Faulted     | open      |
| 16 | Faulted      | Fault      | Faulted     | open      |
| 17 | Faulted      | Clear      | Available   | -         |

Six states and seven events give 42 pairs. The 17 above cause a transition; the
other 25 are ignored: the state does not change and the contactor is not touched.

## Requirements

- **REQ-001** The controller shall take exactly the transitions in the table and ignore every other event.
- **REQ-002** The contactor shall be commanded closed only on entering Charging.
- **REQ-003** The contactor shall be commanded open on every transition out of Charging.
- **REQ-004** A Fault event in any state shall command the contactor open and enter Faulted.
- **REQ-005** While Faulted, the contactor shall not be commanded closed; only Clear leaves Faulted.
- **REQ-006** If Preparing lasts 60 s without authorization, the controller shall return to Available.
- **REQ-007** The contactor shall not be commanded closed unless the vehicle interface reports plugged in and ready; otherwise the controller shall open the contactor and enter Faulted.

## Running as a service

The daemon is started by systemd. These requirements describe how the two
cooperate; REQ-010 is met by the unit file, not by code.

- **REQ-008** The daemon shall report readiness once start-up is complete, and shall report liveness from its main loop at intervals of at most half the watchdog timeout it was given.
- **REQ-009** On SIGTERM the daemon shall leave its main loop within one loop period (1 s) and exit with status 0.
- **REQ-010** If the daemon reports no liveness for 10 s, or exits with a failure, the service manager shall restart it.

## Contactor feedback

The contactor interface reports whether the device is actually closed. From
Week 2 the controller uses that report; in Week 4 the report comes from the
safety MCU.

- **REQ-011** After commanding the contactor closed, if the contactor does
  not report closed, the controller shall command it open and enter Faulted.

## Harness control sockets

The daemon has no vehicle hardware until Week 4. Until then (and in CI for
ever after) the vehicle, and the operator, are played by a test harness over
two unix sockets. The vehicle socket is a command channel; the status socket
is a read-only observation channel that anything on the device may use (an
operator with `nc`, a health check, a HIL runner).

- **REQ-012** When started with `--vehicle-socket PATH`, the daemon shall
  accept newline-delimited commands on that socket, apply them in order, and
  answer every line with `STATE <state>` or `ERR <reason>`. Malformed or
  unknown lines shall change nothing.
- **REQ-013** When started with `--status-socket PATH`, the daemon shall
  write `STATE <state>` to every connection on that socket and close it.

### Vehicle socket protocol

One command per line, upper case, terminated by `\n` (a trailing `\r` is
ignored). The reply to every line is one line.

| Command         | Effect on the vehicle interface      | Event raised | Reply           | 
|-----------------|--------------------------------------|--------------|-----------------|
| `PLUG`          | plugged in = true                    | PlugIn       | `STATE <state>` |
| `UNPLUG`        | plugged in = false, ready = false    | Unplug       | `STATE <state>` |
| `AUTH`          | -                                    | Authorize    | `STATE <state>` |
| `READY`         | ready = true                         | EvReady      | `STATE <state>` |
| `SUSPEND`       | ready = false                        | EvSuspend    | `STATE <state>` |
| `FAULT`         | -                                    | Fault        | `STATE <state>` |
| `CLEAR`         | -                                    | Clear        | `STATE <state>` |
| `CURRENT <A>`   | requested current = A (0 to 80)      | -            | `STATE <state>` |
| `STATE`         | -                                    | -            | `STATE <state>` |
| anything else   | -                                    | -            | `ERR <reason>`  |

`AUTH` and `CLEAR` are not things a vehicle does; they are the operator (a
card, a backend, a technician). The socket is the harness's channel to the
whole outside world, which is why it survives the Week 4 `--mcu` mode: plug
and ready sensing move to the MCU, `AUTH`, `CLEAR` and `STATE` stay here.
See `docs/adr/0003-harness-control-sockets.md`.

## Diagram

```mermaid
stateDiagram-v2
    [*] --> Available
    Available --> Preparing: PlugIn
    Preparing --> SuspendedEV: Authorize
    Preparing --> Available: Unplug or 60 s timeout
    SuspendedEV --> Charging: EvReady (close)
    Charging --> SuspendedEV: EvSuspend (open)
    SuspendedEV --> Finishing: Authorize
    Charging --> Finishing: Authorize (open)
    SuspendedEV --> Available: Unplug
    Charging --> Available: Unplug (open)
    Finishing --> Available: Unplug
    Faulted --> Available: Clear
    note right of Faulted
        Fault in any state:
        open contactor, enter Faulted
    end note
```
