// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_SIGNIN_GAIA_AUTH_EXTENSION_LOADER_H_
#define CHROME_BROWSER_EXTENSIONS_SIGNIN_GAIA_AUTH_EXTENSION_LOADER_H_

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/extensions/api/profile_keyed_api_factory.h"

class Profile;

namespace extensions {

// Manages and registers the gaia auth extension with the extension system.
class GaiaAuthExtensionLoader : public ProfileKeyedAPI {
 public:
  explicit GaiaAuthExtensionLoader(Profile* profile);
  virtual ~GaiaAuthExtensionLoader();

  // Load the gaia auth extension if the extension is not loaded yet.
  void LoadIfNeeded();
  // Unload the gaia auth extension if no pending reference.
  void UnloadIfNeeded();

  static GaiaAuthExtensionLoader* Get(Profile* profile);

  // ProfileKeyedAPI implementation.
  static ProfileKeyedAPIFactory<GaiaAuthExtensionLoader>* GetFactoryInstance();

 private:
  friend class ProfileKeyedAPIFactory<GaiaAuthExtensionLoader>;

  // BrowserContextKeyedService overrides:
  virtual void Shutdown() OVERRIDE;

  // ProfileKeyedAPI implementation.
  static const char* service_name() {
    return "GaiaAuthExtensionLoader";
  }
  static const bool kServiceHasOwnInstanceInIncognito = true;

  Profile* profile_;
  int load_count_;

  DISALLOW_COPY_AND_ASSIGN(GaiaAuthExtensionLoader);
};

} // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_SIGNIN_GAIA_AUTH_EXTENSION_LOADER_H_
