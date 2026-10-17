# ADR 0002: Speak the sd_notify protocol directly

Status: accepted, 2026-10-16

## Context

Under systemd with `Type=notify` and `WatchdogSec=`, the daemon has to tell the
service manager when it is ready and that it is still alive. The usual way is
`sd_notify()` from libsystemd.

## Options

1. Link libsystemd, behind a CMake option so builds without it still work.
2. Implement the protocol: send a text datagram such as `READY=1` to the unix
socket named in `NOTIFY_SOCKET`.

## Decision

Option 2. The protocol is documented as stable and is about forty lines of
socket code in `ServiceNotifier`. In exchange:
- No new dependency. The Docker image, the CI pipeline and both cross presets
are unchanged. Option 1 needs libsystemd for every target architecture, which
on Ubuntu means multiarch packages and a pkg-config set up for cross builds.
- It is unit-tested against a real socket, on both architectures.
- The daemon behaves the same outside systemd: with no socket, calls do nothing.

## Consequences

- We own those forty lines, including the abstract-socket rule.
- Nothing else from libsystemd is available. If the daemon later needs socket
activation or structured journal fields, revisit this decision.