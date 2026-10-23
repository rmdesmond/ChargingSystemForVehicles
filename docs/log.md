# Daily log

**10/12/26 - Initial GitHub repo and local environment setup (not part of this repo)**
  Initial repo includes skeletal:
  .clang-format
  .clang-tidy
  .editorconfig
  .gitignore
  LICENSE
  README.md
  docs/log.md (this file)
      /safety-notes.md (reflections on standards, e.g., ISO 26262, etc.)
      /stories.md  (teachable moments / learnings)
      /testing.md
  Also, ordered parts for this exploration (CAN related hardware and some microcontrollers and breakout boards).

**10/13/26 - Build system - CMake, etc. verified with scripts/verify.sh**
  == native: configure, build, test
  100% tests passed, 0 tests failed out of 1
  == cross-x86_64: configure, build, test
  100% tests passed, 0 tests failed out of 1
  == smoke-run the cross-x86_64 daemon under QEMU
  charger-daemon: hello
  == OK: native and cross-x86_64 built and tested from a clean clone in 20 s

**10/14/26 - First ADR (0001) and subsequent update to design.md (6 states, 7 events, 17 rows, REQ-001..007), three interfaces, table-driven StateMachine, 50 GTest/GMock tests on native + QEMU, 5 mutations caught.**
  - Started Quality Gates in VM.
    - 100% tests passed, 0 tests failed out of 51
    - Total Test time (real) =   0.10 sec
    - Filter suppressions 'branch':
        - 4 instances
        - 4 coverpoints
    - Overall coverage rate:
      - lines......: 95.6% (87 of 91 lines)
      - functions......: 76.2% (16 of 21 functions)
      - branches......: 86.7% (39 of 45 branches)
   - clang-tidy target,  
   - clang-format scripts + pre-commit hook.

**10/15/26 - Docker, CI pipeline taking shape**
  'time scripts/build-in-docker.sh scripts/ci.sh all' yielded:
   - 3m30 real on a clean run (rm -rf build-native build-cross-* build-coverage)
   - 0m5 on a subsequent, cached, run
   - After implementing the .github/workflows/ci.yml:
     - **All checks were successful**
     - 0 cancelled, 0 failing, 3 successful, 0 skipped, and 0 pending checks
    |   |    NAME                  | ELAPSED  |
    |---|--------------------------|----------|                                                                                         
    | ✓ | ci/build (pull_request)  | 1m40s    |   
    | ✓ | ci/lint (pull_request)   | 1m7s     |
    | ✓ | ci/test (pull_request)   | 1m13s    |
   - Proved CI worked on GitHub to prevent a bad branch from merging via
     - .github/workflows/ci.yml
     - .githooks/pre-commit

**10/16/26 - Bringing up hardware**
Diagnosed an SD card failure after configuring the serial port
and the Raspberry Pi incessantly cycled on a file system check and never
recovered and never booted. Power supply looked fine. The SD card is a
generic brand; we'll get a quality brand and try again on Monday.

**10/19/26 - Bringing up hardware, Running initial daemon on hardware**
SD card verdict: It wasn't the power supply. It wasn't the
configuration and updates applied to the Pi.  Replaced the SD
card with one from a quality manufacturer and all is well.

Proved the daemon ran on the target Raspberry Pi and would restart
when it died. Tested with forced kills (sudo kill -STOP/-SEGV charger-daemon)
and a clean stop (systemctl stop charger-daemon) and verified restarts via 
(systemctl show -p NRestarts -p Result charger-daemon).

Also measured security: systemd-analyze security charger-daemon
Rating: 8.5 EXPOSED, as expected at this point

**10/22/26 - Vehicle socket & status socked added, SIL introduced**
Updated design.md with the vehicle socket protocol. Python based SIL
implemented.