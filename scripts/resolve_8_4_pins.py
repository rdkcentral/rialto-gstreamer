#!/usr/bin/env python3

# Copyright (C) 2026 Sky UK
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation;
# version 2.1 of the License.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

"""Resolve the dependency pins used by release/v0.14.0 CI builds."""

import os
import re
import urllib.request


MANIFEST_URL = (
    "https://raw.githubusercontent.com/rdkcentral/"
    "meta-middleware-generic-support/support/8.4.4.0/conf/include/generic-srcrev.inc"
)


def resolve_pins(manifest):
    pins = {}
    for package, output in (
        ("rialto", "RIALTO_REF"),
        ("rialto-ocdm", "RIALTO_OCDM_REF"),
    ):
        matches = re.findall(
            r'^\s*SRCREV:pn-' + re.escape(package)
            + r'\s*=\s*"([0-9a-f]{40})"\s*(?:#.*)?$',
            manifest,
            re.MULTILINE,
        )
        if len(matches) != 1:
            raise ValueError("Expected one 40-character SRCREV for " + package)
        pins[output] = matches[0]
    return pins


if __name__ == "__main__":
    with urllib.request.urlopen(MANIFEST_URL, timeout=30) as response:
        pins = resolve_pins(response.read().decode("utf-8"))
    with open(os.environ["GITHUB_ENV"], "a") as output:
        for name, ref in pins.items():
            print("{}={}".format(name, ref))
            output.write("{}={}\n".format(name, ref))
