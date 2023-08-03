#!/usr/bin/env python3

# Copyright (C) 2023 Sky UK
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

# Processes any valgrind xml files in /build and summarises the results in a csv

import os
import argparse
import xml.etree.ElementTree as ET
import csv

# Default variables
valgrindOutput = "valgrind_report"
outputDir = "build"
possibleErrors = ['Leak_PossiblyLost', 'Leak_IndirectlyLost', 'Leak_DefinitelyLost', 'UninitCondition', 'InvalidWrite', 'InvalidRead', 'UnknownError']

def main ():
    # Create csv
    csv_file = valgrindOutput + ".csv"
    if os.path.isfile(csv_file):
        os.remove(csv_file)
    f = open(csv_file, 'w')
    errorWriter = csv.writer(f, delimiter=',')
    errorWriter.writerow(["Suites"] + possibleErrors)

    # Find all the valgrind xml files and add the errors to the csv
    directory = os.fsencode(outputDir)
    for file in os.listdir(directory):
        filename = os.fsdecode(file)
        if filename.endswith(valgrindOutput + ".xml"):
            hasFailed, results = ParseGtestXml(outputDir + "/" + filename)

            # if failed write the results to the csv
            if hasFailed:
                # Get suite
                prefixLen = len("_" + valgrindOutput + ".xml")
                suite = filename[:-prefixLen]

                errorWriter.writerow([suite] + results)
            continue
        else:
            continue

    f.close()

# Parses the xml and returns an array of the number of failures
def ParseGtestXml(xml):
    hasFailed = False
    results = [0 for x in range(len(possibleErrors))]

    # Parse the xml for all the errors
    root = ET.parse(xml).getroot()
    for errors in root.findall('error'):

        # Leak_StillReachable expected, accounts for static and global objects
        # Do not add to the error table
        kind = errors.find('kind').text
        if kind == "Leak_StillReachable":
            continue

        hasFailed = True

        # Find which error to increment
        errorWritten = False
        for i in range(len(possibleErrors)):
            if possibleErrors[i] == kind:
                results[i] += 1
                errorWritten = True
                break

        # Add as unknown error if not known
        if not errorWritten:
            print ("Unknown error '" + kind + "'")
            results[-1] += 1

    return hasFailed, results

if __name__ == "__main__":
    main()
