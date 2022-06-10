#!/usr/bin/env python3

# Copyright 2022 Viasat Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import collections
import os
import pathlib
import re
import sys
import xml.etree.ElementTree as ET


GRIT_DIR = os.path.join(os.path.dirname(__file__), '..', '..', 'tools', 'grit')
sys.path.append(GRIT_DIR)

from grit import tclib  # nopep8

# List of placeholder tags to skip for branding.
PLACEHOLDER_TAGS_TO_SKIP = [
    'BEGIN_LINK_CHROMIUM',
]

# We cannot wholesale replace "Google" with another company name because then
# we'd replace instances like "Google Docs". Instead, we can keep a list of the
# instances we want to brand.
#
# Note: entries in this list are lowercase, and may be a substring of the full
# message, which lets us catch minor differences in otherwise duplicate strings.
GOOGLE_MESSAGES_TO_BRAND = [
    'send feedback to google',
    'file will be sent to google for debugging',
    'usage statistics and crash reports to google',
    'google terms of service',
    'google privacy policy',
]

GRD_HEADER = '''<?xml version="1.0" encoding="UTF-8"?>
<!-- This file was created by //rebel/branding/create_branded_grd.py. -->
'''

XTB_HEADER = '''<?xml version="1.0" ?>
<!DOCTYPE translationbundle>
<translationbundle lang="{language}">
'''

XTB_MESSAGE = '<translation id="{id}">{message}</translation>\n'

XTB_FOOTER = '</translationbundle>'

PLACEHOLDER_REGEX = re.compile(r'\$+(\d+)')


class Browser(object):
    """
    Structure to hold information about the Rebel-branded browser.

    Attributes
    ----------
    name : str
        The name of the Rebel-branded browser.
    company : str
        The name of the company owning the Rebel-branded browser.
    path_component : str
        The path-component of the Rebel-branded browser.
    schema : str
        The internal WebUI schema of the Rebel-branded browser.
    messages_to_brand : list of str
        List of messages to brand. Each entry is a tuple where the first element
        is a list of unbranded strings, and the second element is the string to
        replace the unbranded message with.
    messages_to_unbrand : list of str
        List of messages to undo branding changes after applying the list of
        messages_to_brand.
    """

    def __init__(self, name, company, path_component, schema):
        self.name = name
        self.company = company
        self.path_component = path_component
        self.schema = schema

        self.messages_to_brand = [
            (['Chromium', 'Google Chrome', 'Chrome'], self.name),
            (['Google LLC'], self.company),
            (['chrome://'], self.schema + '://'),
            (['You and Google'], 'You'),
            (['Sync and Google services', 'Other Google services'], 'Services'),
        ]

        self.messages_to_unbrand = [
            (f'{self.name} OS', 'Chrome OS'),
            (f'{self.name} Web Store', 'Chrome Web Store'),
            (f'{self.name}book', 'Chromebook'),
            (f'{self.name}OS', 'ChromeOS'),
            (f'{self.name}Vox', 'ChromeVox'),
        ]


class ResourceGroup(object):
    """
    Structure to hold information about a GRD file to be branded.

    Attributes
    ----------
    grd_file : str
        The path to the Chromium version of the top-level GRD file.
    grdp_files : list of str, optional
        A list of paths to Chromium GRDP files that should also be branded.
    grdp_extras : list of str, optional
        A list of Rebel-owned GRDP files to insert into branded GRD file.
    xtb_extras : list of str, optional
        A list of Rebel-owned XTB files to insert into branded GRD file.
    """

    def __init__(self, base_path, grd_file, grdp_files=[], grdp_extras=[], xtb_extras=[]):
        self.grd_file = grd_file
        self.grdp_files = grdp_files
        self.grdp_extras = grdp_extras
        self.xtb_extras = xtb_extras

        if not os.path.isdir(base_path):
            raise Exception(f'Could not find path: {base_path}')
        if not os.path.isfile(os.path.join(base_path, self.grd_file)):
            raise Exception(f'Could not find file: {self.grd_file}')

        for grdp_file in self.grdp_files:
            if not os.path.isfile(os.path.join(base_path, grdp_file)):
                raise Exception(f'Could not find file: {grdp_file}')

        for grdp_extra in self.grdp_extras:
            if not os.path.isfile(grdp_extra):
                raise Exception(f'Could not find file: {grdp_extra}')

        for xtb_extra in self.xtb_extras:
            if not os.path.isfile(xtb_extra):
                raise Exception(f'Could not find file: {xtb_extra}')


def remove_triple_quotes(val):
    """
    Some resource strings are surrounded with triple quotes, but the quotes and
    the whitespace inside them should be ingored for creating translation IDs.
    """
    val = val.strip()

    if val.startswith("'''"):
        val = val[3:]
    if val.endswith("'''"):
        val = val[:-3]

    return val.strip()


def textify(element):
    """
    Obtain the entire raw text of an XML node, including any children.
    """
    val = ET.tostring(element, method='xml', encoding='unicode')
    val = val[val.index('>') + 1: val.rindex('<')]

    return remove_triple_quotes(val)


def branded_string(browser, message, force_google_substitution, escape_dollar_signs):
    """
    Peform branding substitutions on a resource string. We replace "Google" in
    only an explicit list of resource strings. Return a tuple with the branded
    resource string and a boolean indicating whether "Google" was replaced.
    """
    did_google_substitution = False

    if message is None:
        return (None, did_google_substitution)

    for google_message in GOOGLE_MESSAGES_TO_BRAND:
        lowercase_message = message.strip().lower()

        if force_google_substitution or google_message in lowercase_message:
            message = message.replace('Google', browser.company)
            did_google_substitution = True

    for (unbranded_messages, branded_message) in browser.messages_to_brand:
        for unbranded_message in unbranded_messages:
            message = message.replace(unbranded_message, branded_message)

    for (branded_message, unbranded_message) in browser.messages_to_unbrand:
        message = message.replace(branded_message, unbranded_message)

    # Unescaped dollar signs result in the following error from grit:
    #
    #     Placeholder formatter found outside of <ph> tag
    #
    # We must re-encode any dollar sign that was decoded by xml.etree.ElementTree.
    # Unfortunately, we cannot encode the dollar sign as '&#36;', because ET will
    # re-encode the ampersand itself, leaving us with '&amp;#36;'. So for now, we
    # use the fullwidth dollar sign (U+FF04).
    if escape_dollar_signs:
        message = PLACEHOLDER_REGEX.sub(r'＄\1', message)

    return (message, did_google_substitution)


def branded_element(browser, element, force_google_substitution=False):
    """
    Peform branding substitutions on a XML resource element. Return a tuple with
    the branded element and a boolean indicating whether "Google" was replaced
    on any child of the element.
    """
    did_google_substitution = False

    (element.text, google) = branded_string(
        browser, element.text, force_google_substitution, True)
    did_google_substitution = did_google_substitution or google

    (element.tail, google) = branded_string(
        browser, element.tail, force_google_substitution, True)
    did_google_substitution = did_google_substitution or google

    for placeholder in element.findall('ph'):
        name = placeholder.get('name')

        if (name is not None) and (name in PLACEHOLDER_TAGS_TO_SKIP):
            continue

        (placeholder.tail, google) = branded_string(
            browser, placeholder.tail, force_google_substitution, False)
        did_google_substitution = did_google_substitution or google

    return (element, did_google_substitution)


def get_translation_id(element):
    """
    Calculate the translation ID of a <message> tag.
    """
    if element.text is None:
        return -1

    text = element.text
    meaning = element.get('meaning')

    for placeholder in element.findall('ph'):
        placeholder_name = placeholder.get('name').upper()
        placeholder_text = placeholder.tail or ''

        text += placeholder_name + placeholder_text

    msg = tclib.Message(text=remove_triple_quotes(text), meaning=meaning)
    return msg.GetId()


def parse_xtb_file(xtb_file):
    """
    Parse an XTB file. Return a tuple containing the language of the XTB file
    and a dictionary mapping translation IDs to the translation XML element.
    """
    tree = ET.parse(xtb_file)

    language = tree.getroot().get('lang')
    translations = {}

    for element in tree.findall('.//translation'):
        message_id = element.get('id')
        translations[message_id] = element

    return (language, translations)


def get_localized_strings(browser, xtb_files, translation_ids):
    """
    Parse the Chromium version of a list of XTB files. Perform Rebel branding
    on every translated message and match the result with the translation ID of
    the non-translated message. Return a dictionary, keyed by locale, containing
    a map of the new translations IDs to the Rebel-branded translations.
    """
    localizations = {}

    for (grd_language, (xtb_path, _)) in xtb_files.items():
        (xtb_language, unbranded_translations) = parse_xtb_file(xtb_path)

        # In some cases, the language parsed from the GRD file differs from the
        # language parsed from the XTB file (e.g. 'he' and 'iw' for Hebrew). We
        # need them both for different contexts.
        language = (grd_language, xtb_language)
        branded_translations = {}

        for (chromium_id, element) in unbranded_translations.items():
            if chromium_id not in translation_ids:
                continue

            (rebel_id, did_google_substitution) = translation_ids[chromium_id]
            (element, _) = branded_element(
                browser, element, did_google_substitution)

            branded_translations[rebel_id] = textify(element)

        localizations[language] = collections.OrderedDict(
            sorted(branded_translations.items()))

    return localizations


def generate_grd_file(browser, base_path, output_path, grd_file, grdp_files, grdp_extras, xtb_extras):
    """
    Create a Rebel-branded GRD file. Performs Rebel branding on each <message>
    tag, replaces any included GRDP / XTB files names with branded versions of
    those file names, and injects any Rebel-owned GRDP / XTB files. As a message
    is parsed, the translation IDs of the original Chromium-branded string and
    the modified Rebel-branded string are computed for later.

    Returns a tuple containing two dictionaries. The first is a mapping of
    languages to the path of XTB file for those languages. The second is a
    mapping of Chromium to Rebel translation IDs.
    """
    chromium_grd_path = os.path.join(base_path, grd_file)
    rebel_grd_path = os.path.join(output_path, grd_file)

    chromium_grd = ET.parse(chromium_grd_path)

    xtb_files = {}
    translation_ids = {}

    # Step 1: Iteratate over every tag in the GRD file and perform branding
    # replacements on specific tags.
    for element in chromium_grd.iter():
        # Step 1a: Replace any <part> tags with the Rebel version of the GRDP
        # file, which must be provided in |grdp_files|.
        if element.tag == 'part':
            grdp_file = element.get('file')

            assert grdp_file in grdp_files, \
                f'GRDP file "{grdp_file}" must be added to //rebel/branding/BUILD.gn'
            element.set('file', os.path.join(output_path, grdp_file))

        # Step 1b: Replace any <file> tags with the Rebel version of the XTB
        # file for every locale. Store the language and path of the XTB file.
        elif element.tag == 'file' and element.get('lang') is not None:
            language = element.get('lang')
            xtb_file = element.get('path')

            xtb_input = os.path.join(base_path, xtb_file)
            xtb_files[language] = (xtb_input, xtb_file)

        # Step 1c: Perform Rebel branding on the contents of all <message> tags
        # (and any children <ph> tags). Compute the translation IDs of the
        # original and modified strings.
        elif element.tag == 'message':
            chromium_id = get_translation_id(element)
            (element, did_google_substitution) = branded_element(browser, element)
            rebel_id = get_translation_id(element)

            if element.get('translateable') != 'false':
                if chromium_id != -1 and rebel_id != -1:
                    translation_ids[chromium_id] = (
                        rebel_id, did_google_substitution)

    # Step 2: Append any Rebel-owned GRDP files to the the <messages> tag.
    messages_tag = chromium_grd.find('.//messages')

    for grdp_extra in grdp_extras:
        part_tag = ET.Element('part', file=grdp_extra)
        messages_tag.append(part_tag)

    # Step 3: Append any Rebel-owned XTB files to the <translations> tag.
    translations_tag = chromium_grd.find('.//translations')

    for xtb_extra in xtb_extras:
        xtb_path = os.path.join(base_path, xtb_extra)

        xtb_tree = ET.parse(xtb_path)
        language = xtb_tree.getroot().get('lang')

        file_tag = ET.Element('file', path=xtb_extra, lang=language)
        translations_tag.append(file_tag)

    # Step 4: Write out the new GRD file!
    rebel_grd = ET.tostring(chromium_grd.getroot(),
                            method='xml', encoding='unicode')

    with open(rebel_grd_path, 'w', encoding='utf8') as rebel_grd_file:
        rebel_grd_file.write(GRD_HEADER)
        rebel_grd_file.write(rebel_grd)

    return (xtb_files, translation_ids)


def generate_xtb_files(output_path, xtb_files, localizations):
    """
    Create Rebel-branded XTB files.
    """
    for ((grd_language, xtb_language), translations) in localizations.items():
        xtb_file_path = os.path.join(output_path, xtb_files[grd_language][1])

        with open(xtb_file_path, 'w', encoding='utf8') as xtb_file:
            xtb_file.write(XTB_HEADER.format(language=xtb_language))

            for (message_id, translation) in translations.items():
                message = XTB_MESSAGE.format(id=message_id, message=translation)
                xtb_file.write(message)

            xtb_file.write(XTB_FOOTER)


def main():
    parser = argparse.ArgumentParser(
        formatter_class=argparse.RawDescriptionHelpFormatter,
        description=__doc__)

    req = parser.add_argument_group('required arguments')

    req.add_argument(
        '-n', '--browser-name', dest='browser_name', required=True,
        help='The name of the Rebel-branded browser')
    req.add_argument(
        '-c', '--browser-company', dest='browser_company', required=True,
        help='The name of the company owning the Rebel-branded browser')
    req.add_argument(
        '-p', '--browser-path-component', dest='browser_path_component', required=True,
        help='The path-component of the Rebel-branded browser')
    req.add_argument(
        '-s', '--browser-schema', dest='browser_schema', required=True,
        help='The internal WebUI schema of the Rebel-branded browser')
    req.add_argument(
        '-o', '--output-path', dest='output_path', required=True,
        help='The directory to generate the Rebel-branded files')
    req.add_argument(
        '-x', '--xtb-relative-path', dest='xtb_relative_path',
        help='The path relative to --output-path to generate XTB files')
    req.add_argument(
        '-g', '--grd-file', dest='grd_file', required=True,
        help='The path to the GRD file to brand')
    req.add_argument(
        '-r', '--grdp-files', dest='grdp_files', nargs='*',
        help='Optional list of paths to GRDP files to brand')
    req.add_argument(
        '-e', '--grdp-extras', dest='grdp_extras', nargs='*',
        help='Optional list of Rebel-owned GRDP files to insert')

    args = parser.parse_args()

    pathlib.Path(args.output_path).mkdir(parents=True, exist_ok=True)

    browser = Browser(
        name=args.browser_name,
        company=args.browser_company,
        path_component=args.browser_path_component,
        schema=args.browser_schema
    )

    base_path = os.path.dirname(args.grd_file)

    if args.xtb_relative_path:
        xtb_path = os.path.join(args.output_path, args.xtb_relative_path)
        pathlib.Path(xtb_path).mkdir(parents=True, exist_ok=True)

    resource_group = ResourceGroup(
        base_path=base_path,
        grd_file=os.path.basename(args.grd_file),
        grdp_files=[os.path.basename(g) for g in args.grdp_files or []],
        grdp_extras=args.grdp_extras or []
    )

    (xtb_files, translation_ids) = generate_grd_file(
        browser,
        base_path,
        args.output_path,
        resource_group.grd_file,
        resource_group.grdp_files,
        resource_group.grdp_extras,
        resource_group.xtb_extras
    )

    for grdp_file in resource_group.grdp_files:
        (_, grdp_ids) = generate_grd_file(
            browser, base_path, args.output_path, grdp_file,
            resource_group.grdp_files, [], [])
        translation_ids.update(grdp_ids)

    localizations = get_localized_strings(browser, xtb_files, translation_ids)
    generate_xtb_files(args.output_path, xtb_files, localizations)


if __name__ == '__main__':
    main()
