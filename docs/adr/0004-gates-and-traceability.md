# ADR 0004: Coverage gate, traceability, and the shape of the pull-request pipeline

Status: accepted, 2026-10-26

## Context

Day 7 makes the pipeline say no to three new kinds of mistake: a
change that breaks the daemon as a black box (SIL), a change that quietly
removes test coverage, and an untested requirement. Five choices had
to be made, and each has a cheaper-looking alternative.

## Options and decisions

1. **What coverage measures.** (a) Unit tests only: 89.7 % lines, 72.0 %
   branches on 2026-10-25, because `main.cpp`'s option parsing and socket
   service loop are run only by the SIL suite. (b) Unit tests plus the SIL
   suite, both against the instrumented binaries: 96.8 % / 82.7 %.
   **Decision: (b).** The number should describe what the tests exercise,
   and `main.cpp` is product code. Cost: the test job runs the SIL suite a
   second time (3 s). The `sil` job still runs the release build, which is
   what ships.

2. **Threshold.** A committed file, `.coverage-threshold`, with separate
   line and branch minimums, start at measured minus five (91 / 77), raised
   on Fridays, never lowered on a pull request. Measured on 2026-10-25:
   deleting the whole 55-test state-machine suite moves lines by 0.5 points
   and branches by 1.4, because the SIL suite walks the same code. The gate
   therefore stops erosion over months, not the loss of one test file; the
   trace matrix (4) is what notices a deleted test, because the requirement
   it carried becomes untested. Branches move more than lines and are the
   more useful of the two numbers here.

3. **Where the SIL job gets its binary.** (a) `needs: build`, upload the
   binary as an artifact, download it. (b) Rebuild in the `sil` job from the
   compiler cache. **Decision: (b).** A rebuild from ccache takes seconds,
   the job stays independent and parallel, and it runs the same
   `scripts/ci.sh sil` as a developer does. (a) is the right pattern when a
   build is long or a binary must be bit-identical across jobs; neither holds
   yet. Revisit when the Yocto image arrives.

4. **How a test carries its requirement.** `REQ_nnn` tokens in the GoogleTest
   and CTest names, `@pytest.mark.req("REQ-nnn")` in pytest; several ids per
   test allowed. The tokens are visible in every ctest and pytest listing and
   filterable (`ctest -R REQ_006`). `scripts/trace.py` reads the test sources,
   not a built binary, so the `trace` job needs no compiler and no image. It
   fails on an untested requirement, an unknown id, or a stale committed
   matrix. `docs/requirements.md` becomes the single source of the ids;
   `design.md` keeps the tables and points at it.

5. **Badge.** JSON in the shields.io endpoint format, written by the gate,
   published to a `gh-pages` branch by a job that runs only on a push to
   `main` and is the only job with `contents: write`. Alternatives: a gist
   (needs a personal token as a secret) or a third-party badge action (one
   more pin to maintain). Plain `git push` with the workflow token is enough.

## Consequences

- The pull-request pipeline is five parallel jobs, all required: build, lint,
  test, sil, trace. Nothing waits for anything, so pipeline time stays the
  time of the slowest job (the build at about 1m40).
- `scripts/ci.sh all` works again inside the build image: python3-venv is in
  the image and the pinned pytest and pyflakes are installed on first use.
- A requirement cannot be added without a test, and a test cannot be deleted
  without its requirement showing as untested. REQ-016 and REQ-017 are
  reserved for Weeks 3 and 4 and are not in the table until their tests are.
- The coverage threshold is a ratchet, not a detector; the Friday raise is
  what keeps it meaningful. The honest statement for an interview is "the
  pipeline blocks a drop below the floor and a requirement without a test",
  not "the pipeline blocks any deleted test".
