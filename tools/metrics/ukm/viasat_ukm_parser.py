#!/usr/bin/env python
# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import hashlib
import struct

from codegen import EventInfo
from codegen import MetricInfo
from builders_template import HEADER as BUILDERS_HEADER_TEMPLATE
from builders_template import IMPL as BUILDERS_IMPL_TEMPLATE
from decode_template import HEADER as DECODE_HEADER_TEMPLATE
from decode_template import IMPL as DECODE_IMPL_TEMPLATE
import ukm_model
import gen_builders
import traceback

def HashName(name):
  # This must match the hash function in //base/metrics/metrics_hashes.cc.
  # >Q: 8 bytes, big endian.
  return struct.unpack('>Q', hashlib.md5(name.encode()).digest()[:8])[0]


def main():
    relpath = '.'
    with open('../../tools/metrics/ukm/ukm.xml') as f:
      data = ukm_model.UKM_XML_TYPE.Parse(f.read())

    event_hash_dict = {}
    metric_hash_dict = {}

    event_list = data[ukm_model._EVENT_TYPE.tag]
    for event in event_list:
      event_name = event["name"]
      event_hash_dict[HashName(event_name)] = event_name
      for metric_itme in event[ukm_model._METRIC_TYPE.tag]:
        metric_name = metric_itme['name']
        metric_hash_dict[HashName(metric_name)] = metric_name

    print("total event: %d" % len(event_list))
    print("event hash dict size: %d" % len(event_hash_dict))
    print("metric hash dict size: %d" % len(metric_hash_dict))

    with open("viasat_ukm_event_hash.json", "w") as f1:
      json.dump(event_hash_dict, f1)

    with open("viasat_ukm_metric_hash.json", "w") as f2:
      json.dump(metric_hash_dict, f2)

    # The followings are two test cases
    if (3632945013237863528 in event_hash_dict):
      print("hash 3632945013237863528 -> %s" % event_hash_dict[3632945013237863528])
    else:
      print("Event hash 3632945013237863528 does not exist!")


    if (2884111971077692445 in metric_hash_dict):
      print("hash 2884111971077692445 -> %s" % metric_hash_dict[2884111971077692445])
    else:
      print("Metric hash 2884111971077692445 does not exist!")

    return


if __name__ == '__main__':
  # to run this script:
  # cd rebel/src/tools/metrics
  # ./ukm/viasat_ukm_parser.py
  # two output files, viasat_ukm_event_hash.json and viasat_ukm_metric_hash.json will be generated 
  # under rebel/src/tools/metrics
  main()
