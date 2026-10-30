# rialto-gstreamer

## Project webpage:
https://rdkcentral.github.io/rialto-gstreamer/

## Coding Guidelines:
https://wiki.rdkcentral.com/display/ASP/Rialto+Coding+Guidelines

## Building and Running Unit Tests:
https://wiki.rdkcentral.com/display/ASP/Unit+Tests

### CI coverage comparisons
Pull requests targeting `master`, `release/*`, or `support/*` compare line and
function coverage against a successful push run of `build_ut.yml` on the PR's
target branch. A decrease in either metric fails the comparison.

The target branch must have a successful push build with an unexpired
`coverage_report_stats` artifact. Missing baseline artifacts or invalid statistics
fail explicitly; they are not treated as zero coverage. If a baseline is missing
or expired, generate a new successful push build on the target branch before
rerunning the PR check.

## Building Rialto using Yocto:
https://wiki.rdkcentral.com/display/ASP/Building+Rialto+using+Yocto

## Running Rialto:
https://wiki.rdkcentral.com/display/ASP/Running+YouTube+with+Rialto

## Main (Rialto) project webpage:
https://rdkcentral.github.io/rialto/
