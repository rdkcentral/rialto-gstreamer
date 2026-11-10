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

import re
import sys

def main():
    if len(sys.argv) < 3:
        write_output("Can't compare coverage stats - Wrong number of script arguments")
        sys.exit("Wrong number of script arguments")
    try:
        master_stats = parse_statistics(sys.argv[1])
        current_stats = parse_statistics(sys.argv[2])
    except (OSError, ValueError) as error:
        message = f"Can't compare coverage stats - {error}"
        write_output(message)
        sys.exit(message)
    comparison_output = compare_coverage(master_stats, current_stats)
    write_output(comparison_output)

    # If line coverage, funtion coverage individually or both decrease, it should exit
    if current_stats[0] < master_stats[0] and current_stats[1] < master_stats[1]:
        sys.exit("Line coverage and function coverage have both decreased. Exiting...")
    elif current_stats[0] < master_stats[0]:
        sys.exit("Line coverage has decreased. Exiting...")
    elif current_stats[1] < master_stats[1]:
        sys.exit("Function coverage has decreased. Exiting...")
        
def parse_statistics(file_path):
    with open(file_path, "r") as file:
        contents = file.read()
    statistics = []
    for metric in ("lines", "functions"):
        match = re.search(r"^\s*" + metric + r"\.+:\s*(\d+(?:\.\d+)?)%", contents, re.MULTILINE)
        if match is None or not 0 <= float(match.group(1)) <= 100:
            raise ValueError(f"Missing or invalid {metric} coverage in {file_path}")
        statistics.append(float(match.group(1)))
    return tuple(statistics)

def compare_coverage(master_stats, current_stats):
    output_text = "Coverage statistics of your commit:\n"
    if current_stats[0] < master_stats[0]:
        output_text += "WARNING: Lines coverage decreased from: " + str(master_stats[0]) + "% to "
        output_text += str(current_stats[0]) + "%\n"

    elif current_stats[0] == master_stats[0]:
        output_text += "Lines coverage stays unchanged and is: " + str(current_stats[0]) + "%\n"
    else:
        output_text += "Congratulations, your commit improved lines coverage from: " + str(master_stats[0])
        output_text += "% to " + str(current_stats[0]) + "%\n"

    if current_stats[1] < master_stats[1]:
        output_text += "WARNING: Functions coverage decreased from: " + str(master_stats[1]) + "% to "
        output_text += str(current_stats[1]) + "%\n"
    elif current_stats[1] == master_stats[1]:
        output_text += "Functions coverage stays unchanged and is: " + str(current_stats[1]) + "%\n"
    else:
        output_text += "Congratulations, your commit improved functions coverage from: " + str(master_stats[1])
        output_text += "% to " + str(current_stats[1]) + "%\n"
    return output_text

def write_output(output_text):
    output_file = open("comparison_output.txt", "w")
    output_file.write(output_text)
    output_file.close()

if __name__ == "__main__":
    main()
