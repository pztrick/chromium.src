// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/screenlock_private/screenlock_private_api.h"

#include "base/lazy_instance.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/screenlock_private.h"
#include "extensions/browser/event_router.h"

namespace screenlock = extensions::api::screenlock_private;

namespace extensions {

namespace {

const char kNotLockedError[] = "Screen is not currently locked.";

screenlock::AuthType FromLockHandlerAuthType(
    ScreenlockBridge::LockHandler::AuthType auth_type) {
  switch (auth_type) {
    case ScreenlockBridge::LockHandler::OFFLINE_PASSWORD:
      return screenlock::AUTH_TYPE_OFFLINEPASSWORD;
    case ScreenlockBridge::LockHandler::NUMERIC_PIN:
      return screenlock::AUTH_TYPE_NUMERICPIN;
    case ScreenlockBridge::LockHandler::USER_CLICK:
      return screenlock::AUTH_TYPE_USERCLICK;
    case ScreenlockBridge::LockHandler::ONLINE_SIGN_IN:
      // Apps should treat forced online sign in same as system password.
      return screenlock::AUTH_TYPE_OFFLINEPASSWORD;
    case ScreenlockBridge::LockHandler::EXPAND_THEN_USER_CLICK:
      // This type is used for public sessions, which do not support screen
      // locking.
      NOTREACHED();
      return screenlock::AUTH_TYPE_NONE;
    case ScreenlockBridge::LockHandler::FORCE_OFFLINE_PASSWORD:
      return screenlock::AUTH_TYPE_OFFLINEPASSWORD;
  }
  NOTREACHED();
  return screenlock::AUTH_TYPE_OFFLINEPASSWORD;
}

}  // namespace

ScreenlockPrivateGetLockedFunction::ScreenlockPrivateGetLockedFunction() {}

ScreenlockPrivateGetLockedFunction::~ScreenlockPrivateGetLockedFunction() {}

bool ScreenlockPrivateGetLockedFunction::RunAsync() {
  SetResult(new base::FundamentalValue(ScreenlockBridge::Get()->IsLocked()));
  SendResponse(error_.empty());
  return true;
}

ScreenlockPrivateSetLockedFunction::ScreenlockPrivateSetLockedFunction() {}

ScreenlockPrivateSetLockedFunction::~ScreenlockPrivateSetLockedFunction() {}

bool ScreenlockPrivateSetLockedFunction::RunAsync() {
  scoped_ptr<screenlock::SetLocked::Params> params(
      screenlock::SetLocked::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  if (params->locked)
    ScreenlockBridge::Get()->Lock(GetProfile());
  else
    ScreenlockBridge::Get()->Unlock(GetProfile());
  SendResponse(error_.empty());
  return true;
}

ScreenlockPrivateAcceptAuthAttemptFunction::
    ScreenlockPrivateAcceptAuthAttemptFunction() {}

ScreenlockPrivateAcceptAuthAttemptFunction::
    ~ScreenlockPrivateAcceptAuthAttemptFunction() {}

bool ScreenlockPrivateAcceptAuthAttemptFunction::RunAsync() {
  scoped_ptr<screenlock::AcceptAuthAttempt::Params> params(
      screenlock::AcceptAuthAttempt::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  ScreenlockBridge::LockHandler* locker =
      ScreenlockBridge::Get()->lock_handler();
  if (locker) {
    if (params->accept) {
      locker->Unlock(ScreenlockBridge::GetAuthenticatedUserEmail(GetProfile()));
    } else {
      locker->EnableInput();
    }
  } else {
    SetError(kNotLockedError);
  }
  SendResponse(error_.empty());
  return true;
}

ScreenlockPrivateEventRouter::ScreenlockPrivateEventRouter(
    content::BrowserContext* context)
    : browser_context_(context) {
  ScreenlockBridge::Get()->AddObserver(this);
}

ScreenlockPrivateEventRouter::~ScreenlockPrivateEventRouter() {}

void ScreenlockPrivateEventRouter::OnScreenDidLock() {
  DispatchEvent(screenlock::OnChanged::kEventName,
      new base::FundamentalValue(true));
}

void ScreenlockPrivateEventRouter::OnScreenDidUnlock() {
  DispatchEvent(screenlock::OnChanged::kEventName,
      new base::FundamentalValue(false));
}

void ScreenlockPrivateEventRouter::OnFocusedUserChanged(
    const std::string& user_id) {
}

void ScreenlockPrivateEventRouter::DispatchEvent(
    const std::string& event_name,
    base::Value* arg) {
  scoped_ptr<base::ListValue> args(new base::ListValue());
  if (arg)
    args->Append(arg);
  scoped_ptr<extensions::Event> event(new extensions::Event(
      event_name, args.Pass()));
  extensions::EventRouter::Get(browser_context_)->BroadcastEvent(event.Pass());
}

static base::LazyInstance<extensions::BrowserContextKeyedAPIFactory<
    ScreenlockPrivateEventRouter> > g_factory = LAZY_INSTANCE_INITIALIZER;

// static
extensions::BrowserContextKeyedAPIFactory<ScreenlockPrivateEventRouter>*
ScreenlockPrivateEventRouter::GetFactoryInstance() {
  return g_factory.Pointer();
}

void ScreenlockPrivateEventRouter::Shutdown() {
  ScreenlockBridge::Get()->RemoveObserver(this);
}

void ScreenlockPrivateEventRouter::OnAuthAttempted(
    ScreenlockBridge::LockHandler::AuthType auth_type,
    const std::string& value) {
  scoped_ptr<base::ListValue> args(new base::ListValue());
  args->AppendString(screenlock::ToString(FromLockHandlerAuthType(auth_type)));
  args->AppendString(value);

  scoped_ptr<extensions::Event> event(new extensions::Event(
      screenlock::OnAuthAttempted::kEventName, args.Pass()));
  extensions::EventRouter::Get(browser_context_)->BroadcastEvent(event.Pass());
}

}  // namespace extensions
