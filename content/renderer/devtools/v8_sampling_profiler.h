// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_DEVTOOLS_V8_SAMPLING_PROFILER_H_
#define CONTENT_RENDERER_DEVTOOLS_V8_SAMPLING_PROFILER_H_

#include "base/debug/trace_event_impl.h"
#include "base/synchronization/waitable_event.h"
#include "content/common/content_export.h"

namespace content {

// The class monitors enablement of V8 CPU profiler and
// spawns a sampling thread when needed.
class CONTENT_EXPORT V8SamplingProfiler final
    : public base::debug::TraceLog::EnabledStateObserver {
 public:
  V8SamplingProfiler();
  ~V8SamplingProfiler();

  // Implementation of TraceLog::EnabledStateObserver
  void OnTraceLogEnabled() override;
  void OnTraceLogDisabled() override;

  void EnableSamplingEventForTesting();
  void WaitSamplingEventForTesting();

 private:
  scoped_ptr<base::WaitableEvent> waitable_event_for_testing_;
  scoped_ptr<class V8SamplingThread> sampling_thread_;

  DISALLOW_COPY_AND_ASSIGN(V8SamplingProfiler);
};

}  // namespace content

#endif  // CONTENT_RENDERER_DEVTOOLS_V8_SAMPLING_PROFILER_H_
