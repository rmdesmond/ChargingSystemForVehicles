# CI metrics

One row per week, or whenever the pipeline changes. The rule: a pull request
gets its answer in under 10 minutes, and the coverage numbers only go up.
Pipeline time is the wall-clock time of the slowest job, from the run page.
| Date       | Change               | Pipeline | build | lint | test | Lines | Branches |
|------------|----------------------|----------|-------|------|------|-------|----------|
| 2026-10-15 | First pipeline (v1)  | 1m40s | 1m40s | 1m7s | 1m14s | 95.6% | 86.7% |
| 2026-10-16 | Intro daemon code    | 1m10s | 1m08s | 1m01s | 1m10s | 96.8% | 81.5% |

## Notes

- 2026-10-15: every job rebuilds the Docker image from scratch. If image build time dominates, the next step is to publish the image to a registry and pull it by digest. Measure first. 

- 2026-10-16: Branches coverage percentage is lower due to untestable socket error branches without fault injection, but if we looked at the raw numbers, more branches were tested because of introduced code increase.
