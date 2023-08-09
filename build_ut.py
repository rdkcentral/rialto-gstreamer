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

# Build script for running rialto unittests

import subprocess
import os
import argparse
import multiprocessing
import sys

# Default variables
resultOutput = "gtest_result"
valgrindOutput = "valgrind_report"
valgrindErrorCode = 101
valgrindIgnore = "rialto-gstreamer.supp"


def runcmd(*args, **kwargs):
    status = subprocess.run(*args, **kwargs)
    if status.returncode == 0 or status.returncode == valgrindErrorCode:
        return status
    else:
        args = ' '.join(status.args) if type(status.args) == list else status.args
        sys.exit(f'Command: "{args}" returned with {status.returncode} error code!')


def main ():
    # Get arguments
    argParser = argparse.ArgumentParser(description='Run the rialto unittests.', formatter_class=argparse.RawTextHelpFormatter)
    argParser.add_argument("-o", "--output", default="build",
                        help="Location to write the build files to (default 'build').")
    argParser.add_argument("-f", "--file", nargs='?', const="",
                        help="Write the build and test output to a file (default '" + resultOutput + ".log') \n" \
                             + "Valgrind output also written to file, default file name only \n" \
                             + "'*suite_name*_" + valgrindOutput + ".txt' (if -xml not specified).")
    argParser.add_argument("-xml", "--xml", nargs='?', const="",
                        help="Convert the test results to xml (default '*suite_name*_" + resultOutput + ".xml') \n" \
                             + "Valgrind output also converted to xml, default file name only \n" \
                             + "'*suite_name*_" + valgrindOutput + ".xml'.")
    argParser.add_argument("-gf", "--googletestFilter",
                        help="The filter to apply when running the googletest. \n" \
                             + "Use filter option to run specific tests or groups of tests. \n" \
                             + "E.g. FooTest* - runs all tests prefixed with FooTest \n" \
                             + "FooTest.* - runs all FooTest test cases \n" \
                             + "-FooTest.* - runs all tests apart from the FooTest cases \n" \
                             + "FooTest.*:BarTest.* - runs all FooTest and BarTest cases")
    argParser.add_argument("-l", "--listTests", action='store_true', help="Lists the tests in the given suites")
    argParser.add_argument("-c", "--clean", action='store_true', help="Clean the directory before running")
    argParser.add_argument("-nb", "--noBuild", action='store_true', help="Do not build")
    argParser.add_argument("-nt", "--noTest", action='store_true', help="Do not test")
    argParser.add_argument("-val", "--valgrind", action='store_true', help="Run the googletest with valgrind \n" \
                             + "Output written to file if -f option specified '*suite_name*_" + valgrindOutput + ".log' \n" \
                             + "Output written to xml if -xml option specified '*suite_name*_" + valgrindOutput + ".xml' \n" \
                             + "Note: Valgrind can only write output to one source (log or xml). \n" \
                             + "Note: Requires version valgrind 3.17.0+ installed. \n")
    argParser.add_argument("-cov", "--coverage", action='store_true', help="Generates UT coverage report")
    args = vars(argParser.parse_args())

    # Rialto Component Tests & Paths
    # {Component Name : {Test Suite, Test Path}}
    suitesToRun = {"gst" : {"suite" : "GstRialtoUnitTests", "path" : "/tests/ut/"}}

    # Clean if required
    if args['clean'] == True:
        executeCmd = ["rm", "-rf", args['output'], resultOutput + ".log", valgrindOutput + ".log"]
        runcmd(executeCmd, cwd=os.getcwd())

    # Get output file name if any
    if args['file'] != None:
        if args['file'] == "":
            f = open(resultOutput + ".log", "w")
        else:
            f = open(args['file'], "w")
    else:
        f = None

    # Get xml output file name if any
    if args['xml'] != None:
        if args['xml'] == "":
            xml = resultOutput + ".xml"
        else:
            xml = args['xml']
    else:
        xml = None

    # Build the test executables
    if args['noBuild'] == False:
        buildTargets(suitesToRun, args['output'], f, args['valgrind'], args['coverage'])

    # Run the tests with the optional settings
    if args['noTest'] == False:
        runTests(suitesToRun, args['listTests'], args['googletestFilter'], args['output'], f, xml, args['valgrind'],
                 args['coverage'])

# Build the target executables
def buildTargets (suites, outputDir, resultsFile, debug, coverage):
    # Run cmake
    cmakeCmd = ["cmake", "-B", outputDir , "-DCMAKE_BUILD_FLAG=UnitTests"]
    # Coverage
    if coverage:
        cmakeCmd.append("-DCOVERAGE_ENABLED=1")
    runcmd(cmakeCmd, cwd=os.getcwd())

    # Make targets
    jarg = "-j" + str(multiprocessing.cpu_count())
    makeCmd = ["make", jarg]
    for key in suites:
        makeCmd.append(suites[key]["suite"])

    print(f"+ {' '.join(makeCmd)}")
    if resultsFile != None:
        runcmd(makeCmd, cwd=os.getcwd() + '/' + outputDir, stdout=resultsFile, stderr=subprocess.STDOUT)
    else:
        runcmd(makeCmd, cwd=os.getcwd() + '/' + outputDir )

# Run the googletests
def runTests (suites, doListTests, gtestFilter, outputDir, resultsFile, xmlFile, valgrind, coverage):
    hasFailed = False

    for key in suites:
        executeCmd = []

        # Valgrind command must come before the test executable
        if valgrind == True:
            executeCmd.extend(AddValgrind(key, resultsFile != None, xmlFile != None))

        # Add test executable + any optionals
        executeCmd.append("." + suites[key]["path"] + suites[key]["suite"])
        if  doListTests == True:
            executeCmd.append('--gtest_list_tests')
        elif gtestFilter != None:
            executeCmd.append('--gtest_filter=' + gtestFilter)

        # Dont output as xml for valgrind
        # Googletest xml output flags an UninitCondition error when converting to xml
        if xmlFile != None and valgrind == False:
            executeCmd.append('--gtest_output=xml:' + key + "_" + xmlFile)

        # Run the command
        if resultsFile != None:
            status = runcmd(executeCmd, cwd=os.getcwd() + '/' + outputDir, stdout=resultsFile, stderr=subprocess.STDOUT)
        else:
            status = runcmd(executeCmd, cwd=os.getcwd() + '/' + outputDir, stderr=subprocess.STDOUT)

        # Check for error code
        if status.returncode == valgrindErrorCode:
            print("Valgrind failed for suite '" + key +"'")
            hasFailed = True
        elif status.returncode != 0:
            print("Command failed for suite '" + key +"' with error code '" + str(status.returncode) + "'")
            hasFailed = True

    if hasFailed:
        raise Exception( f"runTests has failed, exiting" )
    elif coverage:
        generateCoverageReport(outputDir, resultsFile)

# Returns the valgrind command arguments
def AddValgrind(suite, outputToFile, outputToXml):
    executeCmd = ["valgrind", "--leak-check=full", "--show-leak-kinds=all", "--track-origins=yes", "--verbose", "--error-exitcode=" + str(valgrindErrorCode)]

    # Xml redirects the output to xml, cannot output to logfile at the same time
    if outputToXml:
        executeCmd.extend(["--xml=yes", "--xml-file=" + suite + "_" + valgrindOutput + ".xml"])
    elif outputToFile:
        executeCmd.append("--log-file=" + suite + "_" + valgrindOutput + ".log")

    # Some shared libraries throw errors for 'possible leaks', these usually occur when
    # incrementing and decrementing pointers as valgrind thinks you have lost the start of
    # the buffer. Supress these errors.
    #executeCmd.append("--gen-suppressions=all")
    filePath = os.path.realpath(os.path.dirname(__file__))
    executeCmd.append("--suppressions=" + filePath + "/" + valgrindIgnore)

    return executeCmd

def generateCoverageReport(outputDir, resultsFile):
    lcovBaseCmd = ["lcov", "-c", "-i", "-d", ".", "-o", "coverage_base.info", "--exclude", "/usr/*", "--exclude",
                   "*build/*", "--exclude", "*tests/*", "--filter", "brace,function,trivial"]
    if resultsFile:
        lcovBaseStatus = runcmd(lcovBaseCmd, cwd=os.getcwd() + '/' + outputDir, stdout=resultsFile, stderr=subprocess.STDOUT)
    else:
        lcovBaseStatus = runcmd(lcovBaseCmd, cwd=os.getcwd() + '/' + outputDir, stderr=subprocess.STDOUT)
    if not lcovBaseStatus:
        return False
    lcovTestCmd = ["lcov", "-c", "-d", ".", "-o", "coverage_test.info", "--exclude",
               "/usr/*", "--exclude", "*build/*", "--exclude", "*tests/*", "--filter", "brace,function,trivial"]
    if resultsFile:
        lcovTestStatus = runcmd(lcovTestCmd, cwd=os.getcwd() + '/' + outputDir, stdout=resultsFile, stderr=subprocess.STDOUT)
    else:
        lcovTestStatus = runcmd(lcovTestCmd, cwd=os.getcwd() + '/' + outputDir, stderr=subprocess.STDOUT)
    if not lcovTestStatus:
        return False
    lcovCombineCmd = ["lcov", "-a", "coverage_base.info", "-a", "coverage_test.info", "-o", "coverage.info", "--filter",
                      "brace,function,trivial"]
    if resultsFile:
        lcovCombineStatus = runcmd(lcovCombineCmd, cwd=os.getcwd() + '/' + outputDir, stdout=resultsFile, stderr=subprocess.STDOUT)
    else:
        lcovCombineStatus = runcmd(lcovCombineCmd, cwd=os.getcwd() + '/' + outputDir, stderr=subprocess.STDOUT)
    if not lcovCombineStatus:
        return False
    genHtmlCmd = ["genhtml", "coverage.info", "--output-directory", "gh_pages/coverage_report", "--filter", "brace,function,trivial"]
    if resultsFile:
        genHtmlStatus = runcmd(genHtmlCmd, cwd=os.getcwd() + '/' + outputDir, stdout=resultsFile, stderr=subprocess.STDOUT)
    else:
        genHtmlStatus = runcmd(genHtmlCmd, cwd=os.getcwd() + '/' + outputDir, stderr=subprocess.STDOUT)
    genStatsCmd = ["lcov", "--summary", "coverage.info", "--filter", "brace,function,trivial"]
    statsFile = open(os.getcwd() + '/' + outputDir + '/' + "coverage_statistics.txt", "w")
    if statsFile:
        genStatsStatus = runcmd(genStatsCmd, cwd=os.getcwd() + '/' + outputDir, stdout=statsFile, stderr=subprocess.STDOUT)
    else:
        genStatsStatus = False
    statsFile.close()
    return genHtmlStatus and genStatsStatus


if __name__ == "__main__":
    main()
