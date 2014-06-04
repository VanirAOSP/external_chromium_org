// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_CLOUD_POLICY_HEADER_SERVICE_FACTORY_H_
#define CHROME_BROWSER_POLICY_CLOUD_POLICY_HEADER_SERVICE_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/browser_context_keyed_service/browser_context_keyed_service_factory.h"

namespace policy {

class PolicyHeaderService;

// Factory for PolicyHeaderService objects. PolicyHeaderService is not actually
// a BrowserContextKeyedService, so this class wraps PolicyHeaderService in
// a BrowserContextKeyedService internally.
class PolicyHeaderServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  // Returns the instance of PolicyHeaderService for the passed |context|, or
  // NULL if there is none (for instance, for incognito windows).
  static PolicyHeaderService* GetForBrowserContext(
      content::BrowserContext* context);

  // Returns an instance of the PolicyHeaderServiceFactory singleton.
  static PolicyHeaderServiceFactory* GetInstance();

 protected:
  // BrowserContextKeyedServiceFactory implementation.
  virtual BrowserContextKeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const OVERRIDE;

 private:
  friend struct DefaultSingletonTraits<PolicyHeaderServiceFactory>;

  PolicyHeaderServiceFactory();
  virtual ~PolicyHeaderServiceFactory();

  DISALLOW_COPY_AND_ASSIGN(PolicyHeaderServiceFactory);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_CLOUD_POLICY_HEADER_SERVICE_FACTORY_H_
