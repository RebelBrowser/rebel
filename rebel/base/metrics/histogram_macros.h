// Copyright 2023 Viasat Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REBEL_BASE_METRICS_HISTOGRAM_MACROS_H_
#define REBEL_BASE_METRICS_HISTOGRAM_MACROS_H_

#include <string>
#include <unordered_map>

#include "base/check_op.h"
#include "base/command_line.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/histogram_macros_internal.h"
#include "base/metrics/histogram_macros_local.h"
#include "base/no_destructor.h"
#include "base/strings/string_piece.h"
#include "base/time/time.h"

// NOTE: Do not use this macro directly!
// This is a helper macro used by other macros and shouldn't be used directly.
// Defines a static map of atomic histogram pointers, and forwards to
// HISTOGRAM_POINTER_USE.
#define STATIC_HISTOGRAM_POINTER_MAP_BLOCK(dynamic_histogram_name,            \
                                           histogram_add_method_invocation,   \
                                           histogram_factory_get_invocation)  \
  do {                                                                        \
    /*                                                                        \
     * The pointer's presence indicates that the initialization is complete.  \
     * Initialization is idempotent, so it can safely be atomically repeated. \
     */                                                                       \
    static base::NoDestructor<std::unordered_map<                             \
        base::StringPiece, std::atomic_uintptr_t, base::StringPieceHash>>     \
        atomic_histogram_pointer_map;                                         \
    std::atomic_uintptr_t& atomic_histogram_pointer =                         \
        (*atomic_histogram_pointer_map)[dynamic_histogram_name];              \
    HISTOGRAM_POINTER_USE(                                                    \
        std::addressof(atomic_histogram_pointer), dynamic_histogram_name,     \
        histogram_add_method_invocation, histogram_factory_get_invocation);   \
  } while (0)

// This is the dynamic version of UMA_HISTOGRAM_CUSTOM_TIMES, where the name of
// the histogram is dynamic, which means it can be defined at runtime, and
// doesn't have to be the same name every time. However, each unique name will
// always refer to a unique histogram of that name, i.e two different names will
// refer to two to different histograms.
//
// Sample usage:
//   UMA_HISTOGRAM_CUSTOM_TIMES_DYNAMIC(dynamic_histogram_name, time_delta,
//       base::Seconds(1), base::Days(1), 100);
#define UMA_HISTOGRAM_CUSTOM_TIMES_DYNAMIC(name, sample, min, max, \
                                           bucket_count)           \
  do {                                                             \
    STATIC_HISTOGRAM_POINTER_MAP_BLOCK(                            \
        name, AddTimeMillisecondsGranularity(sample),              \
        base::Histogram::FactoryTimeGet(                           \
            name, min, max, bucket_count,                          \
            base::HistogramBase::kUmaTargetedHistogramFlag));      \
  } while (0)

#endif  // REBEL_BASE_METRICS_HISTOGRAM_MACROS_H_
