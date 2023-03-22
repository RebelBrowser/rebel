#!/usr/bin/env python3

# Copyright 2023 Viasat Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import datetime
import os
import shutil
import subprocess

GN_ARGS_TEMPLATE = """
target_cpu = "{target_cpu}"

is_debug = false
is_component_build = false
is_official_build = true
dcheck_always_on = false
symbol_level = 1
enable_widevine = true
enable_plugins = true
proprietary_codecs = true

use_goma = true
use_system_xcode = false
mac_sdk_official_version = "12.3"

rebel_browser_name = "Viasat Browser"
rebel_browser_name_path_component = "viasat"
rebel_browser_company = "Viasat Inc."
rebel_browser_company_path_component = "Viasat"
rebel_browser_abbreviation = "VB"
rebel_browser_schema = "viasat"
rebel_package = "com.viasat.browser"
rebel_new_tab_page_url = "https://browser.viasat.com/viasat_ntp/index.html"
rebel_default_sites_url = "https://browser.viasat.com/viasat_ntp/default_sites.json"
rebel_browser_api_allow_list_url = "https://browser.viasat.com/viasat_ntp/remote_ntp_allow_list.json"
rebel_help_url = "https://browser.viasat.com/faq"

rebel_omaha_public_url = "https://omaha.ihs.viasat.io"
rebel_omaha_private_url = "https://omahainternal.ihs.viasat.io"

rebel_crash_report_enabled = true
rebel_crash_report_url = "$rebel_omaha_public_url/service/crash_report/"

rebel_sparkle_enabled = true
rebel_sparkle_product_name = "ViasatBrowser"
"""


def log(*args):
  now = datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S')
  print(f'\x1b[1;32m>> {now}\x1b[0m:', *args)


def remove_path(path):
  log('Removing:', path)

  if os.path.isdir(path):
    shutil.rmtree(path)


def run_command(*command):
  log('Running:', *command)
  subprocess.check_call(command)


def build_architechture(target_cpu):
  out_path = os.path.join('out', f'Release-{target_cpu}')
  gn_args_file = os.path.join(out_path, 'args.gn')

  os.makedirs(out_path, exist_ok=True)

  with open(gn_args_file, 'w') as file:
    file.write(GN_ARGS_TEMPLATE.format(target_cpu=target_cpu))

  run_command('gn', 'gen', out_path)
  run_command('ninja', '-j', '150', '-C', out_path, 'rebel_all')

  return out_path


def create_universal_build(x64_path, arm_path):
  out_path = os.path.join('out', f'Release-universal')

  remove_path(out_path)
  os.makedirs(out_path, exist_ok=True)

  universalizer = os.path.join('chrome', 'installer', 'mac', 'universalizer.py')

  browser_app = 'Viasat Browser.app'
  x64_app = os.path.join(x64_path, browser_app)
  arm_app = os.path.join(arm_path, browser_app)
  universal_app = os.path.join(out_path, browser_app)

  run_command('python3', universalizer, x64_app, arm_app, universal_app)

  return out_path


def codesign_universal_build(x64_path, universal_path):
  browser_packaging = 'VB Packaging'
  x64_packaging = os.path.join(x64_path, browser_packaging)
  universal_packaging = os.path.join(universal_path, browser_packaging)

  shutil.copytree(x64_packaging, universal_packaging)

  sign_chrome = os.path.join(x64_path, browser_packaging, 'sign_chrome.py')

  run_command(sign_chrome,
    '--identity', 'TYV58F58C9',
    '--input', universal_path,
    '--output', universal_path,
    '--notarize',
    '--notary-user', 'sparrow@viasat.com',
    '--notary-password', '@keychain:Viasat Browser Notary')


def main():
  x64_path = build_architechture('x64')
  arm_path = build_architechture('arm64')

  # Sparkle is already built as a Universal binary. Remove it from either arch,
  # otherwise the Universal script does not know how to combine them.
  arm_sparkle_path = os.path.join(
    arm_path, 'Viasat Browser.app', 'Contents', 'Frameworks',
    'VB Framework.framework', 'Frameworks', 'Sparkle.framework')
  remove_path(arm_sparkle_path)

  universal_path = create_universal_build(x64_path, arm_path)
  codesign_universal_build(x64_path, universal_path)


if __name__ == '__main__':
  main()
