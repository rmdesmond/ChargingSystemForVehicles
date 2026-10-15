# Daily log

10/12/26 - Initial GitHub repo and local environment setup (not part of this repo). Initial repo includes skeletal:
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


10/13/26 - Build system - CMake, etc. verified with scripts/verify.sh 
  == native: configure, build, test
  100% tests passed, 0 tests failed out of 1
  == cross-x86_64: configure, build, test
  100% tests passed, 0 tests failed out of 1
  == smoke-run the cross-x86_64 daemon under QEMU
  charger-daemon: hello
  == OK: native and cross-x86_64 built and tested from a clean clone in 20 s

10/14/26 - First ADR (0001) and subsequent update to design.md (6 states, 7 events, 17 rows, REQ-001..007), three interfaces, table-driven StateMachine, 50 GTest/GMock tests on native + QEMU, 5 mutations caught.
  Started Quality Gates in VM.
    100% tests passed, 0 tests failed out of 51
    Total Test time (real) =   0.10 sec
    Filter suppressions 'branch':
        4 instances
        4 coverpoints
    Overall coverage rate:
      lines......: 95.6% (87 of 91 lines)
      functions......: 76.2% (16 of 21 functions)
      branches......: 86.7% (39 of 45 branches)
   clang-tidy target,  
   clang-format scripts + pre-commit hook.