#!/usr/bin/env python3

# Copyright 2024 Viasat Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import pathlib
import os

def main():
    parser = argparse.ArgumentParser(
        formatter_class=argparse.RawDescriptionHelpFormatter,
        description=__doc__)

    req = parser.add_argument_group('required arguments')

    req.add_argument(
        '-n', '--browser-name', dest='browser_name', required=True,
        help='The name of the Rebel-branded browser')
    req.add_argument(
        '-p', '--browser-path', dest='browser_path', required=True,
        help='The path component of the Rebel-branded browser')
    req.add_argument(
        '-c', '--browser-company', dest='browser_company', required=True,
        help='The company name of the Rebel-branded browser')
    req.add_argument(
        '-e', '--browser-email', dest='browser_email', required=True,
        help='The email address of the Rebel-branded browser')
    req.add_argument(
        '-w', '--browser-website', dest='browser_website', required=True,
        help='The homepage URL of the Rebel-branded browser')
    req.add_argument(
        '-H', '--browser-help', dest='browser_help', required=True,
        help='The help URL of the Rebel-branded browser')
    req.add_argument(
        '-a', '--input-appdata', dest='input_appdata', required=True,
        help='The path to the chromium-browser.appdata.xml template')
    req.add_argument(
        '-i', '--input-info', dest='input_info', required=True,
        help='The path to the chromium-browser.info template')
    req.add_argument(
        '-A', '--output-appdata', dest='output_appdata', required=True,
        help='The path to generate the Rebel-branded chromium-browser.appdata.xml')
    req.add_argument(
        '-I', '--output-info', dest='output_info', required=True,
        help='The path to generate the Rebel-branded chromium-browser.info')

    args = parser.parse_args()

    pathlib.Path(os.path.dirname(args.output_appdata)).mkdir(
        parents=True, exist_ok=True)
    pathlib.Path(os.path.dirname(args.output_info)).mkdir(
        parents=True, exist_ok=True)

    with open(args.input_appdata, 'r') as input_appdata_file:
        input_appdata = input_appdata_file.read()
    with open(args.input_info, 'r') as input_info_file:
        input_info = input_info_file.read()

    package_name = args.browser_name.lower().replace(' ', '-')

    output_appdata = input_appdata.format(
        browser_name=args.browser_name,
        browser_company=args.browser_company,
        browser_email=args.browser_email,
        browser_help=args.browser_help)

    output_info = input_info.format(
        browser_name=args.browser_name,
        browser_path=args.browser_path,
        browser_company=args.browser_company,
        browser_email=args.browser_email,
        browser_website=args.browser_website,
        package_name=package_name)

    with open(args.output_appdata, 'w', encoding='utf8') as output_appdata_file:
        output_appdata_file.write(output_appdata)
    with open(args.output_info, 'w', encoding='utf8') as output_info_file:
        output_info_file.write(output_info)


if __name__ == '__main__':
    main()
