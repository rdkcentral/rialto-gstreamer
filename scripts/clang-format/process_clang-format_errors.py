#!/usr/bin/env python3

 #
 # Copyright (C) 2022 Sky UK
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
 #

# Processes the clang-format_errors.log file and construct junit xml

import os, sys
import argparse
import xml.etree.ElementTree as ET
import csv

# Default variables
clangFile = "clang-format_errors"

def main ():
    # Check log file
    errorLogFile = clangFile + ".log"
    if not os.path.isfile(errorLogFile) or os.stat(errorLogFile).st_size == 0:
        # No errors to process
        return

    # Remove xml if exists
    xmlFile = clangFile + ".xml"
    if os.path.isfile(xmlFile):
        os.remove(xmlFile)

    list = createErrorList(errorLogFile)

    # Construct xml
    testsuiteElement = ET.Element("testsuite")
    testsuiteElement.set('errors', str(0))
    testsuiteElement.set('failures', str(len(list)))
    testsuiteElement.set('name', 'clang-format')
    testsuiteElement.set('tests', str(len(list)))

    for error in list:
        testcaseElement = ET.SubElement(testsuiteElement, "testcase")
        testcaseElement.set('name', error['fileName'])
        failureElement = ET.SubElement(testcaseElement, "failure")
        failureElement.text = error['errorInfo']

    tree = ET.ElementTree(testsuiteElement)
    tree.write(xmlFile, encoding='UTF-8', xml_declaration = True)

    sys.exit("Failure detected, code has not been correctly formatted")

# Processes the clang-format_errors.log and returns a list of all the errors
def createErrorList(errorLogFile):
    errorList = []

    with open(errorLogFile) as f:
        lines = f.readlines()
        for line in lines:
            if line.find(': error:') != -1 and line.find('[-Wclang-format-violations]') != -1:
                errorList.append({'fileName' : line[:line.find(': error:')], 'errorInfo' : ''})
            else:
                errorList[-1].update({'errorInfo' : errorList[-1]['errorInfo'] + line})

    return errorList

if __name__ == "__main__":
    main()
