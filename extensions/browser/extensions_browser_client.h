// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_EXTENSIONS_BROWSER_CLIENT_H_
#define EXTENSIONS_BROWSER_EXTENSIONS_BROWSER_CLIENT_H_

#include "base/memory/scoped_ptr.h"

class CommandLine;
class PrefService;

namespace content {
class BrowserContext;
class JavaScriptDialogManager;
}

namespace extensions {

class AppSorting;

// Interface to allow the extensions module to make browser-process-specific
// queries of the embedder. Should be Set() once in the browser process.
//
// NOTE: Methods that do not require knowledge of browser concepts should be
// added in ExtensionsClient (extensions/common/extensions_client.h) even if
// they are only used in the browser process.
class ExtensionsBrowserClient {
 public:
  virtual ~ExtensionsBrowserClient() {}

  // Returns true if the embedder has started shutting down.
  virtual bool IsShuttingDown() = 0;

  // Returns true if extensions have been disabled (e.g. via a command-line flag
  // or preference).
  virtual bool AreExtensionsDisabled(const CommandLine& command_line,
                                     content::BrowserContext* context) = 0;

  // Returns true if the |context| is known to the embedder.
  virtual bool IsValidContext(content::BrowserContext* context) = 0;

  // Returns true if the BrowserContexts could be considered equivalent, for
  // example, if one is an off-the-record context owned by the other.
  virtual bool IsSameContext(content::BrowserContext* first,
                             content::BrowserContext* second) = 0;

  // Returns true if |context| has an off-the-record context associated with it.
  virtual bool HasOffTheRecordContext(content::BrowserContext* context) = 0;

  // Returns the off-the-record context associated with |context|. If |context|
  // is already off-the-record, returns |context|.
  // WARNING: This may create a new off-the-record context. To avoid creating
  // another context, check HasOffTheRecordContext() first.
  virtual content::BrowserContext* GetOffTheRecordContext(
      content::BrowserContext* context) = 0;

  // Return the original "recording" context. This method returns |context| if
  // |context| is not incognito.
  virtual content::BrowserContext* GetOriginalContext(
      content::BrowserContext* context) = 0;

  // Returns the PrefService associated with |context|.
  virtual PrefService* GetPrefServiceForContext(
      content::BrowserContext* context) = 0;

  // Returns true if loading background pages should be deferred.
  virtual bool DeferLoadingBackgroundHosts(
      content::BrowserContext* context) const = 0;

  virtual bool IsBackgroundPageAllowed(
      content::BrowserContext* context) const = 0;

  // Returns true if the client version has updated since the last run. Called
  // once each time the extensions system is loaded per browser_context. The
  // implementation may wish to use the BrowserContext to record the current
  // version for later comparison.
  virtual bool DidVersionUpdate(content::BrowserContext* context) = 0;

  // Creates a new AppSorting instance.
  virtual scoped_ptr<AppSorting> CreateAppSorting() = 0;

  // Return true if the system is run in forced app mode.
  virtual bool IsRunningInForcedAppMode() = 0;

  // Returns the embedder's JavaScriptDialogManager or NULL if the embedder
  // does not support JavaScript dialogs.
  virtual content::JavaScriptDialogManager* GetJavaScriptDialogManager() = 0;

  // Returns the single instance of |this|.
  static ExtensionsBrowserClient* Get();

  // Initialize the single instance.
  static void Set(ExtensionsBrowserClient* client);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_EXTENSIONS_BROWSER_CLIENT_H_
