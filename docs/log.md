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