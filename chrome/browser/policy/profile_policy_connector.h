// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_PROFILE_POLICY_CONNECTOR_H_
#define CHROME_BROWSER_POLICY_PROFILE_POLICY_CONNECTOR_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "components/browser_context_keyed_service/browser_context_keyed_service.h"

namespace chromeos {
class User;
}

namespace policy {

class CloudPolicyManager;
class ConfigurationPolicyProvider;
class PolicyService;
class SchemaRegistry;

// A BrowserContextKeyedService that creates and manages the per-Profile policy
// components.
class ProfilePolicyConnector : public BrowserContextKeyedService {
 public:
  ProfilePolicyConnector();
  virtual ~ProfilePolicyConnector();

  // If |force_immediate_load| then disk caches will be loaded synchronously.
  void Init(bool force_immediate_load,
#if defined(OS_CHROMEOS)
            const chromeos::User* user,
#endif
            SchemaRegistry* schema_registry,
            CloudPolicyManager* user_cloud_policy_manager);

  void InitForTesting(scoped_ptr<PolicyService> service);

  // BrowserContextKeyedService:
  virtual void Shutdown() OVERRIDE;

  // This is never NULL.
  PolicyService* policy_service() const { return policy_service_.get(); }

 private:
#if defined(ENABLE_CONFIGURATION_POLICY)
#if defined(OS_CHROMEOS)
  void InitializeDeviceLocalAccountPolicyProvider(
      const std::string& username,
      SchemaRegistry* schema_registry);

  // Some of the user policy configuration affects browser global state, and
  // can only come from one Profile. |is_primary_user_| is true if this
  // connector belongs to the first signed-in Profile, and in that case that
  // Profile's policy is the one that affects global policy settings in
  // local state.
  bool is_primary_user_;

  scoped_ptr<ConfigurationPolicyProvider> special_user_policy_provider_;
#endif  // defined(OS_CHROMEOS)

  scoped_ptr<ConfigurationPolicyProvider> forwarding_policy_provider_;
#endif  // defined(ENABLE_CONFIGURATION_POLICY)

  scoped_ptr<PolicyService> policy_service_;

  DISALLOW_COPY_AND_ASSIGN(ProfilePolicyConnector);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_PROFILE_POLICY_CONNECTOR_H_
