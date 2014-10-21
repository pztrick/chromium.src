// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_APP_VIEW_APP_VIEW_INTERNAL_API_H_
#define EXTENSIONS_BROWSER_API_APP_VIEW_APP_VIEW_INTERNAL_API_H_

#include "extensions/browser/extension_function.h"

namespace extensions {

class AppViewInternalAttachFrameFunction : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("appViewInternal.attachFrame",
                             APPVIEWINTERNAL_ATTACHFRAME);
  AppViewInternalAttachFrameFunction();

 protected:
  ~AppViewInternalAttachFrameFunction() override {}
  bool RunAsync() final;

 private:
  DISALLOW_COPY_AND_ASSIGN(AppViewInternalAttachFrameFunction);
};

class AppViewInternalDenyRequestFunction : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("appViewInternal.denyRequest",
                             APPVIEWINTERNAL_DENYREQUEST);
  AppViewInternalDenyRequestFunction();

 protected:
  ~AppViewInternalDenyRequestFunction() override {}
  bool RunAsync() final;

 private:
  DISALLOW_COPY_AND_ASSIGN(AppViewInternalDenyRequestFunction);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_APP_VIEW_APP_VIEW_INTERNAL_API_H_
