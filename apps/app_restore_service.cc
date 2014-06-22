// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/app_restore_service.h"

#include "apps/app_lifetime_monitor_factory.h"
#include "apps/app_restore_service_factory.h"
#include "apps/app_window.h"
#include "apps/launcher.h"
#include "apps/saved_files_service.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/profile.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"

using extensions::Extension;
using extensions::ExtensionHost;
using extensions::ExtensionPrefs;
using extensions::ExtensionRegistry;

namespace apps {

// static
bool AppRestoreService::ShouldRestoreApps(bool is_browser_restart) {
  bool should_restore_apps = is_browser_restart;
#if defined(OS_CHROMEOS)
  // Chromeos always restarts apps, even if it was a regular shutdown.
  should_restore_apps = true;
#endif
  return should_restore_apps;
}

AppRestoreService::AppRestoreService(Profile* profile)
    : profile_(profile) {
  StartObservingAppLifetime();
}

void AppRestoreService::HandleStartup(bool should_restore_apps) {
  const extensions::ExtensionSet& extensions =
      ExtensionRegistry::Get(profile_)->enabled_extensions();
  ExtensionPrefs* extension_prefs = ExtensionPrefs::Get(profile_);

  for (extensions::ExtensionSet::const_iterator it = extensions.begin();
      it != extensions.end(); ++it) {
    const Extension* extension = it->get();
    if (extension_prefs->IsExtensionRunning(extension->id())) {
      RecordAppStop(extension->id());
      // If we are not restoring apps (e.g., because it is a clean restart), and
      // the app does not have retain permission, explicitly clear the retained
      // entries queue.
      if (should_restore_apps) {
        RestoreApp(it->get());
      } else {
        SavedFilesService::Get(profile_)->ClearQueueIfNoRetainPermission(
            extension);
      }
    }
  }
}

bool AppRestoreService::IsAppRestorable(const std::string& extension_id) {
  return ExtensionPrefs::Get(profile_)->IsExtensionRunning(extension_id);
}

// static
AppRestoreService* AppRestoreService::Get(Profile* profile) {
  return apps::AppRestoreServiceFactory::GetForProfile(profile);
}

void AppRestoreService::OnAppStart(Profile* profile,
                                   const std::string& app_id) {
  RecordAppStart(app_id);
}

void AppRestoreService::OnAppActivated(Profile* profile,
                                       const std::string& app_id) {
  RecordAppActiveState(app_id, true);
}

void AppRestoreService::OnAppDeactivated(Profile* profile,
                                         const std::string& app_id) {
  RecordAppActiveState(app_id, false);
}

void AppRestoreService::OnAppStop(Profile* profile, const std::string& app_id) {
  RecordAppStop(app_id);
}

void AppRestoreService::OnChromeTerminating() {
  // We want to preserve the state when the app begins terminating, so stop
  // listening to app lifetime events.
  StopObservingAppLifetime();
}

void AppRestoreService::Shutdown() {
  StopObservingAppLifetime();
}

void AppRestoreService::RecordAppStart(const std::string& extension_id) {
  ExtensionPrefs::Get(profile_)->SetExtensionRunning(extension_id, true);
}

void AppRestoreService::RecordAppStop(const std::string& extension_id) {
  ExtensionPrefs::Get(profile_)->SetExtensionRunning(extension_id, false);
}

void AppRestoreService::RecordAppActiveState(const std::string& id,
                                             bool is_active) {
  ExtensionPrefs* extension_prefs = ExtensionPrefs::Get(profile_);

  // If the extension isn't running then we will already have recorded whether
  // it is active or not.
  if (!extension_prefs->IsExtensionRunning(id))
    return;

  extension_prefs->SetIsActive(id, is_active);
}

void AppRestoreService::RestoreApp(const Extension* extension) {
  RestartPlatformApp(profile_, extension);
}

void AppRestoreService::StartObservingAppLifetime() {
  AppLifetimeMonitor* app_lifetime_monitor =
      AppLifetimeMonitorFactory::GetForProfile(profile_);
  DCHECK(app_lifetime_monitor);
  app_lifetime_monitor->AddObserver(this);
}

void AppRestoreService::StopObservingAppLifetime() {
  AppLifetimeMonitor* app_lifetime_monitor =
      AppLifetimeMonitorFactory::GetForProfile(profile_);
  // This might be NULL in tests.
  if (app_lifetime_monitor)
    app_lifetime_monitor->RemoveObserver(this);
}

}  // namespace apps
