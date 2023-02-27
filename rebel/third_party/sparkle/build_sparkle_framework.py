#!/usr/bin/env python3

# Copyright 2023 Viasat Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import os
import subprocess
import sys

# In-order targets to build
TARGETS = ['bsdiff', 'ed25519', 'Sparkle']

class ChangeDirectory(object):
  """
  Context manager for temporarily changing the working directory.
  """
  def __init__(self, working_dir):
    self.saved_working_dir = os.getcwd()
    self.working_dir = working_dir

  def __enter__(self):
    os.chdir(self.working_dir)

  def __exit__(self, *args, **kwargs):
    os.chdir(self.saved_working_dir)

def main(args):
  build_dir = 'CONFIGURATION_BUILD_DIR=' + os.getcwd()

  sparkle_dir = os.path.join(
    os.path.dirname(os.path.realpath(__file__)),
    'src'
  )

  with ChangeDirectory(sparkle_dir):
    for target in TARGETS:
      command = [
        'xcodebuild',
        '-target',
        target,
        '-configuration',
        'Release',
        build_dir,
        'build',
      ]

      with open(os.devnull, 'w') as dev_null:
        if subprocess.check_call(command, stdout=dev_null) != 0:
          return 1

  return 0

if __name__ == '__main__':
  sys.exit(main(sys.argv))
