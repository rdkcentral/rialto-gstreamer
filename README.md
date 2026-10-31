# rialto-gstreamer

## Project webpage:
https://rdkcentral.github.io/rialto-gstreamer/

## Coding Guidelines:
https://wiki.rdkcentral.com/display/ASP/Rialto+Coding+Guidelines

## Building and Running Unit Tests:
https://wiki.rdkcentral.com/display/ASP/Unit+Tests

### CI coverage comparisons
Rialto branch selections are resolved once to a commit SHA before the coverage
builds. PR and automatic baseline builds reuse that SHA. The coverage check runs
even when its prerequisite build fails and explicitly rejects that failure.

The 8.4 baseline's historical CMake does not read the OCDM pin environment
variable. CI downloads both metadata files at the selected OCDM pin before
building the fresh baseline checkout, without cleaning away those files.

PRs targeting `master` retain the existing baseline lookup: coverage statistics
are downloaded from a successful `master` workflow run. They do not perform an
additional baseline build.

PRs targeting `release/*` or `support/*` automatically build the exact target
commit (`pull_request.base.sha`) in a separate directory, using the same selected
Rialto dependency ref as the PR build. Both reports come from the current run;
no previous baseline artifacts or manual baseline runs are required.

A decrease in line or function coverage fails the comparison. Missing or invalid
statistics and baseline build/download failures also fail explicitly. Push and
manual runs only build the selected branch and do not compare coverage.

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
