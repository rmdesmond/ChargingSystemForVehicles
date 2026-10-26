# Testing

## Unit

`tests/*.cpp`, GoogleTest with GoogleMock. The expected behaviour is written
down independently of the code (the transition table is transcribed from
`docs/design.md`, not read from the product's table), so a test is a second
opinion rather than a mirror. Time is a dependency (`Clock`), so a 60 s
timeout is tested in microseconds with `FakeClock`.

## SIL

`tests/sil/`, pytest. Each test starts a fresh daemon with private socket
paths in a temporary directory and talks to it the way the product's
surroundings will: commands in, state out, contactor actuations in the log.
Nothing in the harness reaches inside the process, which is what makes it a
system test; it would run unchanged against a daemon built by someone else.
Two switches exist for the harness only, both documented as test-only:
`--sim` lets the environment inject faults (`CHARGER_SIM_CONTACTOR=stuck`),
and `--time-scale N` speeds up the state machine's clock so timeouts take
seconds instead of minutes. The time scale is applied through the `Clock`
interface, never to real-time contracts with other processes (the systemd
watchdog, week 4's heartbeat to the MCU).
What the virtual vehicle fakes is deliberately thin: a plug, a ready signal,
a current request and an operator. It does not model a battery or a pilot
waveform. A plant model earns its place when a test needs it, not before.

## HIL

The bench: Pi 5, Pico, the CrowPi board's buttons and LEDs. It exists to
catch what simulation cannot: UART timing, the watchdog chain, the A/B boot
path, a daemon that runs for days. It never gates a pull request. Hardware
flakes, and a flaky required check teaches a team to ignore red; so HIL runs
nightly and on release tags, captures the journal and the serial console as
artifacts, and a failure opens a ticket rather than blocking a merge.

## Flakiness

A test that fails without a code change is quarantined within the hour:
marked (`@pytest.mark.skip(reason=...)` with a ticket), removed from the
required set, and fixed or deleted within the week. The PR pipeline stays
under ten minutes and stays trustworthy; those two properties are worth
more than any single test.