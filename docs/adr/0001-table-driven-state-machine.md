# ADR 0001: Table-driven state machine

Status: accepted, 2026-10-14

## Context

The charger's behaviour is a state machine with six states and seven events.
It decides when the contactor closes, so it has to be easy to review and easy
to test completely.

## Options

1. Nested `switch` statements: state outside, event inside.
2. The State pattern: one class per state with virtual event handlers.
3. A transition table: an array of {from, event, to, action} rows and one
lookup loop.

## Decision

Option 3. The table in the code has the same shape as the table in
`docs/design.md`, so a reviewer compares them row by row. Unlisted pairs are
ignored by construction, which makes "all 42 state/event pairs are accounted
for" a countable claim that a test checks.

## Consequences

- Adding behaviour is adding a row, plus a row in the design and in the test.
- Guards and timeouts do not fit in a plain row. The two we have (REQ-006,
REQ-007) live in `tick()` and `enter()`; if more appear, revisit this.
- The linear lookup is 17 comparisons at most. Not worth optimising.