# Copyright 2023 Viasat Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import re

from . import signing
from .chromium_config import ChromiumCodeSignConfig
from .model import CodeSignOptions, CodeSignedProduct, VerifyOptions

# FIXME: Generate this configuration at build time.
class InternalCodeSignConfig(ChromiumCodeSignConfig):
    """Rebel codesign configuration object."""

    @property
    def product(self):
        return 'RB'

    @property
    def provisioning_profile_basename(self):
        return '/Users/buildbot/Viasat_Browser_Distro'

    @property
    def run_spctl_assess(self):
        return True

    @property
    def sparkle_framework_dir(self):
        return '{0.framework_dir}/Frameworks/Sparkle.framework'.format(self)

    @property
    def autoupdate_app_dir(self):
        return '{0.sparkle_framework_dir}/Versions/Current/Resources/Autoupdate.app'.format(self)

    def override_parts_for_rebel(self, parts):
        """
        Override signing configuration for macOS packaging.
        """
        skip_library_hardened_runtime_options = (
            CodeSignOptions.HARDENED_RUNTIME | CodeSignOptions.RESTRICT |
            CodeSignOptions.KILL)

        # Widevine is signed by Google using their TeamID, which differs from
        # Rebel's TeamID. To be able to load the Widevine dylib, disable
        # library validation in the Helper app which loads Widevine. See:
        # https://developer.apple.com/documentation/bundleresources/entitlements/com_apple_security_cs_disable-library-validation
        parts['helper-app'].options = skip_library_hardened_runtime_options
        parts['helper-app'].entitlements = 'helper-entitlements.plist'

    def sign_rebel_parts(self, paths, config):
        """
        Sign components added by Rebel to the macOS package. These are signed
        here because they need to be signed in a specific order, which the
        caller (sign_chrome) does not support.
        """
        full_hardened_runtime_options = (
            CodeSignOptions.HARDENED_RUNTIME | CodeSignOptions.RESTRICT |
            CodeSignOptions.LIBRARY_VALIDATION | CodeSignOptions.KILL)
        verify_options = VerifyOptions.DEEP | VerifyOptions.STRICT

        sparkle = CodeSignedProduct(
            self.sparkle_framework_dir,
            'org.sparkle-project.Sparkle',
            verify_options=verify_options)
        sparkle_fileop = CodeSignedProduct(
            '{0.autoupdate_app_dir}/Contents/MacOS/fileop'.format(self),
            'fileop',
            options=full_hardened_runtime_options,
            verify_options=verify_options)
        sparkle_autoupdate = CodeSignedProduct(
            '{0.autoupdate_app_dir}/Contents/MacOS/Autoupdate'.format(self),
            'Autoupdate',
            options=full_hardened_runtime_options,
            verify_options=verify_options)

        signing.sign_part(paths, config, sparkle_fileop)
        signing.sign_part(paths, config, sparkle_autoupdate)
        signing.sign_part(paths, config, sparkle)
