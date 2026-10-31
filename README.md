# rialto-gstreamer

## Project webpage:
https://rdkcentral.github.io/rialto-gstreamer/

## Coding Guidelines:
https://wiki.rdkcentral.com/display/ASP/Rialto+Coding+Guidelines

## Building and Running Unit Tests:
https://wiki.rdkcentral.com/display/ASP/Unit+Tests

### CI coverage comparisons
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

## Building Rialto using Yocto:
https://wiki.rdkcentral.com/display/ASP/Building+Rialto+using+Yocto

## Running Rialto:
https://wiki.rdkcentral.com/display/ASP/Running+YouTube+with+Rialto

## Main (Rialto) project webpage:
https://rdkcentral.github.io/rialto/
