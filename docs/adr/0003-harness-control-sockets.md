# ADR 0003: Two unix sockets as the harness's channel to the daemon

Status: accepted, 2026-10-19

## Context

Week 2 Day 1 makes the daemon testable as a black box: a Python harness must
play the vehicle and the operator, drive a session through the real binary and
read back the state, locally and in CI. Until Week 4 there is no vehicle
hardware at all. Three questions had to be settled, and two of them looked
like departures from the sprint checklist until traced through Weeks 3 and 4.

1. The checklist's happy path is `PLUG -> READY -> CURRENT -> UNPLUG`, but the
    Day 3 transition table (ADR 0001) leaves Preparing only on `Authorize`,
    which is also how a session is stopped. Week 3's OCPP client hangs
    `StartTransaction` and `StopTransaction` on exactly those transitions.
2. The checklist asks for a vehicle socket and a separate status socket.
3. The checklist's "vehicle timeout" fault case reads "no READY for N s".

## Options

1. Protocol: (a) drop `Authorize` from the table so the checklist's happy path
    works unchanged; (b) keep the table and add an `AUTH` command to the socket
    protocol, i.e. let the harness play the operator as well as the car.
2. Sockets: (a) one socket with a `STATE` query; (b) two sockets, one for
    commands and one read-only.
3. Timeout: (a) invent a SuspendedEV timeout; (b) realise the case with the
    existing Preparing timeout (REQ-006) under a fast clock.

## Decision

- **1(b)**. The table stays as it is; `AUTH` and `CLEAR` are commands. They
    are not things a vehicle does, so the socket is defined as the harness's
    channel to the *whole outside world*, not as a vehicle emulator. This is
    also why it survives Week 4: with `--mcu`, plug and ready sensing move to
    the Pico, and `AUTH`, `CLEAR` and `STATE` stay on this socket.
- **2(b)**. Two sockets, as the checklist says. The vehicle socket is a
    command channel, created only with `--vehicle-socket`, mode 0600. The status
    socket answers every connection with one `STATE` line and hangs up, mode
    0666, and is enabled in the production unit file: it is the health probe
    that Thursday's RAUC mark-good service and any `nc -U` on the device can
    use, without being able to inject events.
- **3(b)**. No new requirement. OCPP's own timeout (`ConnectionTimeOut`) runs
    during Preparing, and a car may sit in SuspendedEV indefinitely.
- One thread. The sockets join the existing service loop through `poll()`,
    with SIGTERM/SIGINT read from a `signalfd` instead of `sigtimedwait`. The
    loop still ticks the state machine once a second and feeds the watchdog.

## Consequences
- The transition table, `docs/design.md` and the Day 3 tests are unchanged.
    The Week 2 Monday checklist's happy-path line is read as including `AUTH`.
- `--time-scale` (BUILD 3) is applied through the `Clock` the state machine
    sees, never to the service loop's watchdog pings nor, in Week 4, to the
    200 ms heartbeat: those are real-time contracts with another process.
- The vehicle socket is not a safety boundary. Anything that can connect to
    it can command a session; this is the harness's channel and is never
    enabled in the production unit file.
- Week 4 needs an authorize source on the bench. The socket is one; OCPP
    `RemoteStartTransaction` would be another. Noted for the Week 4 companion.