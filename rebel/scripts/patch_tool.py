#!/usr/bin/env python3

# Copyright 2023 Viasat Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import argparse
import os
import subprocess
import shutil
import sys

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

def get_upstream_revision(upstream_directory):
  """
  Retrieve the currently checked out upstream revision.
  """
  with ChangeDirectory(upstream_directory):
    command = ['git', 'rev-parse', 'HEAD']

    revision = subprocess.check_output(command)
    return revision.strip()

def get_last_patched_revision(revision_file):
  """
  Retrieve a tuple (revision, patch) containing the last synced upstream
  revision and last applied patch number from the revision file.
  """
  if os.path.isfile(revision_file):
    with open(revision_file, 'r') as opened_revision_file:
      contents = opened_revision_file.read()
      return contents.split(':')

  return (None, None)

def write_new_patched_revision(revision_file, revision, patch_number):
  """
  Write the newly synced upstream revision and newly applied patch number to the
  revision file.
  """
  with open(revision_file, 'w') as opened_revision_file:
    contents = '%s:%s' % (revision, patch_number)
    opened_revision_file.write(contents)

def copy_upstream_source(upstream_directory, source_directory):
  """
  Remove the copied upstream source to be used for patching and copy the
  original source to the patched location.
  """
  if os.path.isdir(source_directory):
    shutil.rmtree(source_directory)

  shutil.copytree(upstream_directory, source_directory)

def copy_new_and_overridden_files(source_directory, files_directory):
  """
  Copy Viasat-created files and files that should be overridden to the patched
  upstream source directory.
  """
  source_directory_absolute = os.path.abspath(source_directory)

  with ChangeDirectory(files_directory):
    for (path, _, files) in os.walk('.'):
      for file in files:
        src_path = os.path.join(path, file)
        dst_path = os.path.join(source_directory_absolute, path, file)

        shutil.copy2(src_path, dst_path)

def apply_patches(source_directory, patches_directory, patches):
  """
  Apply each patch in order. Using -C1 helps avoid merge conflicts by reducing
  the required surrounding context around each change.
  """
  patches_directory_absolute = os.path.abspath(patches_directory)

  with ChangeDirectory(source_directory):
    for patch in patches:
      patch_path = os.path.join(patches_directory_absolute, patch)
      command = ['git', 'apply', '-C1', patch_path]

      if subprocess.check_call(command) != 0:
        return False

  return True

def main():
  parser = argparse.ArgumentParser()
  req = parser.add_argument_group('required arguments')

  req.add_argument(
      '-b', '--upstream', dest='upstream', required=True,
      help='Directory containing the upstream, unmodified source.')
  req.add_argument(
      '-s', '--source', dest='source', required=True,
      help='Directory to create the patched source.')
  req.add_argument(
      '-p', '--patches', dest='patches', required=True,
      help='Directory containing the patches to apply. Each patch should be'
           ' named: "<patch_number>_<description>.patch".')

  parser.add_argument(
      '-f', '--files', dest='files', required=False,
      help='Directory containing files be copied directly to the patched source'
           ' location. The structure of subdirectories will be retained, and'
           ' existing files will be overridden.')

  args = parser.parse_args()

  sorted_patches = sorted(os.listdir(args.patches))
  patch_number = sorted_patches[-1].split('_')[0]

  revision_file = os.path.join(args.source, 'REVISION')
  revision = get_upstream_revision(args.upstream)

  (last_revision, last_patch_number) = get_last_patched_revision(revision_file)

  if (revision != last_revision) or (patch_number != last_patch_number):
    copy_upstream_source(args.upstream, args.source)

    if args.files:
      copy_new_and_overridden_files(args.source, args.files)

    if not apply_patches(args.source, args.patches, sorted_patches):
      return 1

    write_new_patched_revision(revision_file, revision, patch_number)

  return 0

if __name__ == '__main__':
  sys.exit(main())
