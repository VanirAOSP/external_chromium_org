// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APPS_SHELL_SHELL_EXTENSIONS_CLIENT_H_
#define APPS_SHELL_SHELL_EXTENSIONS_CLIENT_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "extensions/common/extensions_client.h"

namespace apps {

// The app_shell implementation of ExtensionsClient.
class ShellExtensionsClient : public extensions::ExtensionsClient {
 public:
  ShellExtensionsClient();
  virtual ~ShellExtensionsClient();

  // extensions::ExtensionsClient overrides:
  virtual void Initialize() OVERRIDE;
  virtual const extensions::PermissionsProvider& GetPermissionsProvider() const
      OVERRIDE;
  virtual const extensions::PermissionMessageProvider&
      GetPermissionMessageProvider() const OVERRIDE;
  virtual extensions::FeatureProvider* GetFeatureProviderByName(
      const std::string& name) const OVERRIDE;
  virtual void FilterHostPermissions(
      const extensions::URLPatternSet& hosts,
      extensions::URLPatternSet* new_hosts,
      std::set<extensions::PermissionMessage>* messages) const OVERRIDE;
  virtual void SetScriptingWhitelist(const ScriptingWhitelist& whitelist)
      OVERRIDE;
  virtual const ScriptingWhitelist& GetScriptingWhitelist() const OVERRIDE;
  virtual extensions::URLPatternSet GetPermittedChromeSchemeHosts(
      const extensions::Extension* extension,
      const extensions::APIPermissionSet& api_permissions) const OVERRIDE;
  virtual bool IsScriptableURL(const GURL& url, std::string* error) const
      OVERRIDE;

 private:
  ScriptingWhitelist scripting_whitelist_;

  DISALLOW_COPY_AND_ASSIGN(ShellExtensionsClient);
};

}  // namespace apps

#endif  // APPS_SHELL_SHELL_EXTENSIONS_CLIENT_H_
