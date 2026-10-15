# CI metrics

One row per week, or whenever the pipeline changes. The rule: a pull request
gets its answer in under 10 minutes, and the coverage numbers only go up.
Pipeline time is the wall-clock time of the slowest job, from the run page.
| Date       | Change               | Pipeline | build | lint | test | Lines | Branches |
|------------|----------------------|----------|-------|------|------|-------|----------|
| 2026-10-15 | First pipeline (v1)  |  | 1m40 | 1m7 | 1m14 | 87/91 | 39/45 |

## Notes

- 2026-10-15: every job rebuilds the Docker image from scratch. If image build
time dominates, the next step is to publish the image to a registry and pull
it by digest. Measure first.