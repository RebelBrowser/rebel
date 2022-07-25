#!/usr/bin/env python3

# Copyright 2022 Viasat Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import pathlib
import os
import xml.etree.ElementTree as ET

XML_HEADER = '''<?xml version="1.0" encoding="UTF-8"?>
<!-- This file was created by //rebel/branding/create_channel_constants.py. -->
'''


def main():
    parser = argparse.ArgumentParser(
        formatter_class=argparse.RawDescriptionHelpFormatter,
        description=__doc__)

    req = parser.add_argument_group('required arguments')

    req.add_argument(
        '-n', '--browser-name', dest='browser_name', required=True,
        help='The name of the Rebel-branded browser')
    req.add_argument(
        '-i', '--input-xml', dest='input_xml', required=True,
        help='The path to the Chromium-branded channel_constants.xml')
    req.add_argument(
        '-o', '--output-xml', dest='output_xml', required=True,
        help='The path to generate the Rebel-branded channel_constants.xml')

    args = parser.parse_args()

    pathlib.Path(os.path.dirname(args.output_xml)).mkdir(
        parents=True, exist_ok=True)

    input_xml = ET.parse(args.input_xml)

    for element in input_xml.iter():
        if element.tag == 'string':
            element.text = element.text.replace('Chromium', args.browser_name)

    output_xml = ET.tostring(
        input_xml.getroot(), method='xml', encoding='unicode')

    with open(args.output_xml, 'w', encoding='utf8') as output_xml_file:
        output_xml_file.write(XML_HEADER)
        output_xml_file.write(output_xml)


if __name__ == '__main__':
    main()
