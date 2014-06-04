// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_CONFIGURATION_POLICY_PREF_STORE_TEST_H_
#define CHROME_BROWSER_POLICY_CONFIGURATION_POLICY_PREF_STORE_TEST_H_

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "components/policy/core/browser/configuration_policy_handler_list.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_service_impl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

class PolicyMap;
class PolicyService;
class ConfigurationPolicyPrefStore;

class ConfigurationPolicyPrefStoreTest : public testing::Test {
 protected:
  ConfigurationPolicyPrefStoreTest();
  virtual ~ConfigurationPolicyPrefStoreTest();
  virtual void TearDown() OVERRIDE;
  void UpdateProviderPolicy(const PolicyMap& policy);

  PolicyServiceImpl::Providers providers_;
  ConfigurationPolicyHandlerList handler_list_;
  MockConfigurationPolicyProvider provider_;
  scoped_ptr<PolicyService> policy_service_;
  scoped_refptr<ConfigurationPolicyPrefStore> store_;
  base::MessageLoop loop_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ConfigurationPolicyPrefStoreTest);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_CONFIGURATION_POLICY_PREF_STORE_TEST_H_
