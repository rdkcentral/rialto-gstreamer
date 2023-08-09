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

import sys

def main():
    if len(sys.argv) < 3:
        write_output("Can't compare coverage stats - Wrong number of script arguments")
        sys.exit("Wrong number of script arguments")
    master_stats = parse_statistics(sys.argv[1])
    current_stats = parse_statistics(sys.argv[2])
    comparison_output = compare_coverage(master_stats, current_stats)
    write_output(comparison_output)

def parse_statistics(file_path):
    try:
        file = open(file_path, "r")
        lines = file.readlines()
        covered_lines_line = [i for i in lines if "lines......" in i][0]
        covered_functions_line = [i for i in lines if "functions.." in i][0]
        covered_lines_str = covered_lines_line[15:covered_lines_line.find('%')]
        covered_functions_str = covered_functions_line[15:covered_functions_line.find('%')]
        file.close()
        return (float(covered_lines_str), float(covered_functions_str))
    except:
        write_output("Can't compare coverage stats - Could not open statistics file")
        return (0.0, 0.0)

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
