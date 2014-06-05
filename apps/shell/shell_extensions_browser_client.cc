// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/shell/shell_extensions_browser_client.h"

#include "apps/shell/shell_app_sorting.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/pref_service_factory.h"
#include "base/prefs/testing_pref_store.h"
#include "chrome/browser/extensions/extension_prefs.h"
#include "components/user_prefs/pref_registry_syncable.h"
#include "components/user_prefs/user_prefs.h"
#include "extensions/browser/app_sorting.h"

using content::BrowserContext;

namespace {

// See chrome::RegisterProfilePrefs() in chrome/browser/prefs/browser_prefs.cc
void RegisterPrefs(user_prefs::PrefRegistrySyncable* registry) {
  extensions::ExtensionPrefs::RegisterProfilePrefs(registry);
}

}  // namespace

namespace apps {

ShellExtensionsBrowserClient::ShellExtensionsBrowserClient(
    BrowserContext* context)
    : browser_context_(context) {
  // Set up the preferences service.
  base::PrefServiceFactory factory;
  factory.set_user_prefs(new TestingPrefStore);
  factory.set_extension_prefs(new TestingPrefStore);
  // app_shell should not require syncable preferences, but for now we need to
  // recycle some of the RegisterProfilePrefs() code in Chrome.
  // TODO(jamescook): Convert this to user_prefs::PrefRegistrySimple.
  user_prefs::PrefRegistrySyncable* pref_registry =
      new user_prefs::PrefRegistrySyncable;
  // Prefs should be registered before the PrefService is created.
  RegisterPrefs(pref_registry);
  prefs_ = factory.Create(pref_registry).Pass();
  user_prefs::UserPrefs::Set(browser_context_, prefs_.get());
}

ShellExtensionsBrowserClient::~ShellExtensionsBrowserClient() {}

bool ShellExtensionsBrowserClient::IsShuttingDown() {
  return false;
}

bool ShellExtensionsBrowserClient::AreExtensionsDisabled(
    const CommandLine& command_line,
    BrowserContext* context) {
  return false;
}

bool ShellExtensionsBrowserClient::IsValidContext(BrowserContext* context) {
  return context == browser_context_;
}

bool ShellExtensionsBrowserClient::IsSameContext(BrowserContext* first,
                                                 BrowserContext* second) {
  return first == second;
}

bool ShellExtensionsBrowserClient::HasOffTheRecordContext(
    BrowserContext* context) {
  return false;
}

BrowserContext* ShellExtensionsBrowserClient::GetOffTheRecordContext(
    BrowserContext* context) {
  // app_shell only supports a single context.
  return NULL;
}

BrowserContext* ShellExtensionsBrowserClient::GetOriginalContext(
    BrowserContext* context) {
  return context;
}

PrefService* ShellExtensionsBrowserClient::GetPrefServiceForContext(
    BrowserContext* context) {
  return prefs_.get();
}

bool ShellExtensionsBrowserClient::DeferLoadingBackgroundHosts(
    BrowserContext* context) const {
  return false;
}

bool ShellExtensionsBrowserClient::IsBackgroundPageAllowed(
    BrowserContext* context) const {
  return true;
}

bool ShellExtensionsBrowserClient::DidVersionUpdate(BrowserContext* context) {
  // TODO(jamescook): We might want to tell extensions when app_shell updates.
  return false;
}

scoped_ptr<extensions::AppSorting>
ShellExtensionsBrowserClient::CreateAppSorting() {
  return scoped_ptr<extensions::AppSorting>(new ShellAppSorting).Pass();
}

bool ShellExtensionsBrowserClient::IsRunningInForcedAppMode() {
  return false;
}

content::JavaScriptDialogManager*
ShellExtensionsBrowserClient::GetJavaScriptDialogManager() {
  // TODO(jamescook): Create a JavaScriptDialogManager or reuse the one from
  // content_shell.
  NOTREACHED();
  return NULL;
}

}  // namespace apps
