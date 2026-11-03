# CI metrics

One row per week, or whenever the pipeline changes. The rule: a pull request
gets its answer in under 10 minutes, and the coverage numbers only go up.
Pipeline time is the wall-clock time of the slowest job, from the run page.

| Date       | Change               | Pipeline | build | lint  | test  |  sil  | trace | Lines | Branches |
|------------|----------------------|----------|-------|-------|-------|-------|-------|-------|----------|
| 2026-10-15 | First pipeline (v1)  |  1m40s   | 1m40s | 1m07s | 1m14s |  n/a  |  n/a  | 95.6% |   86.7%  |
| 2026-10-16 | Intro daemon code    |  1m10s   | 1m08s | 1m01s | 1m10s |  n/a  |  n/a  | 96.8% |   81.5%  |
| 2026-10-26 | Intro SIL            |  2m04s   | 2m04s | 1m31s | 1m18s | 1m17s |  n/a  | skip  |   skip   |
| 2026-11-03 | Coverage Gate & Trace|  1m40s   | 1m40s | 1m19s | 1m30s | 1m12s | 0m09s | 96.8% |   82.7%  |

## Notes

- 2026-10-15: every job rebuilds the Docker image from scratch. If image build time dominates, the next step is to publish the image to a registry and pull it by digest. Measure first. 

- 2026-10-16: Branches coverage percentage is lower due to untestable socket error branches without fault injection, but if we looked at the raw numbers, more branches were tested because of introduced code increase.

- 2026-10-26: Added SIL, skipped lines and branches coverage due to code changes (this is where straight percentages are weak indicators)

- 2026-11-03: Added Coverage Gate and Traceability Matrix
