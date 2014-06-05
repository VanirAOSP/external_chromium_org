// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/extensions/extension_pref_value_map.h"
#include "chrome/browser/extensions/extension_pref_value_map_factory.h"
#include "chrome/browser/extensions/extension_prefs.h"
#include "chrome/browser/extensions/extension_prefs_factory.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "components/browser_context_keyed_service/browser_context_dependency_manager.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/common/constants.h"

namespace extensions {

// static
ExtensionPrefs* ExtensionPrefsFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<ExtensionPrefs*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
ExtensionPrefsFactory* ExtensionPrefsFactory::GetInstance() {
  return Singleton<ExtensionPrefsFactory>::get();
}

void ExtensionPrefsFactory::SetInstanceForTesting(
    content::BrowserContext* context, ExtensionPrefs* prefs) {
  Associate(context, prefs);
}

ExtensionPrefsFactory::ExtensionPrefsFactory()
    : BrowserContextKeyedServiceFactory(
        "ExtensionPrefs",
        BrowserContextDependencyManager::GetInstance()) {
}

ExtensionPrefsFactory::~ExtensionPrefsFactory() {
}

BrowserContextKeyedService* ExtensionPrefsFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  ExtensionsBrowserClient* client = ExtensionsBrowserClient::Get();
  return ExtensionPrefs::Create(
      client->GetPrefServiceForContext(context),
      context->GetPath().AppendASCII(extensions::kInstallDirectoryName),
      ExtensionPrefValueMapFactory::GetForBrowserContext(context),
      client->CreateAppSorting().Pass(),
      client->AreExtensionsDisabled(
          *CommandLine::ForCurrentProcess(), context));
}

content::BrowserContext* ExtensionPrefsFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return ExtensionsBrowserClient::Get()->GetOriginalContext(context);
}

}  // namespace extensions
