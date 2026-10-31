# rialto-gstreamer

## Project webpage:
https://rdkcentral.github.io/rialto-gstreamer/

## Coding Guidelines:
https://wiki.rdkcentral.com/display/ASP/Rialto+Coding+Guidelines

## Building and Running Unit Tests:
https://wiki.rdkcentral.com/display/ASP/Unit+Tests

### CI coverage comparisons
Pull requests targeting `master`, `release/*`, or `support/*` compare line and
function coverage against the PR's exact target commit (`pull_request.base.sha`).
The workflow builds that commit in a separate directory with the same Rialto
dependency ref selected for the PR build, then compares the two coverage reports
from the current run. No previous coverage artifacts or manual baseline runs are
required.

A decrease in either metric fails the comparison. Baseline build failures,
missing statistics, or invalid statistics fail explicitly; they are not treated
as zero coverage. PR checks perform an additional build to generate the baseline;
push and manual runs only build the selected branch.

### Release 8.4 CI dependencies

For PRs targeting `release/v0.14.0`, unit-test, Valgrind, and native-build
workflows resolve Rialto from `meta-middleware-generic-support` at
`support/8.4.4.0`, using `conf/include/generic-srcrev.inc`. Unit-test and
Valgrind builds also use that manifest's OCDM pin for metadata files.
These builds intentionally use platform pins instead of a same-named Rialto
feature branch. Missing or invalid pins fail the job.

The shared resolver is [scripts/resolve_8_4_pins.py](scripts/resolve_8_4_pins.py).
Other target branches retain their existing unit-test and Valgrind selection.
Native builds try the full Rialto feature-branch name, then the target branch,
and fail if neither exists. Native prerequisites include `libyaml-cpp-dev`.

## Building Rialto using Yocto:
https://wiki.rdkcentral.com/display/ASP/Building+Rialto+using+Yocto

## Running Rialto:
https://wiki.rdkcentral.com/display/ASP/Running+YouTube+with+Rialto

## Main (Rialto) project webpage:
https://rdkcentral.github.io/rialto/
