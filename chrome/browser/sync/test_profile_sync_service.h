// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_PROFILE_SYNC_SERVICE_H_
#define CHROME_BROWSER_SYNC_TEST_PROFILE_SYNC_SERVICE_H_

#include <string>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/signin/profile_oauth2_token_service.h"
#include "chrome/browser/sync/glue/data_type_manager_impl.h"
#include "chrome/browser/sync/glue/sync_backend_host_impl.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/sync_prefs.h"
#include "sync/test/engine/test_id_factory.h"
#include "testing/gmock/include/gmock/gmock.h"

class Profile;
class ProfileOAuth2TokenService;
class ProfileSyncComponentsFactory;
class ProfileSyncComponentsFactoryMock;

ACTION(ReturnNewDataTypeManager) {
  return new browser_sync::DataTypeManagerImpl(arg0,
                                               arg1,
                                               arg2,
                                               arg3,
                                               arg4,
                                               arg5);
}

namespace browser_sync {

class SyncBackendHostForProfileSyncTest : public SyncBackendHostImpl {
 public:
  SyncBackendHostForProfileSyncTest(
      Profile* profile,
      const base::WeakPtr<SyncPrefs>& sync_prefs,
      base::Closure& callback);
  virtual ~SyncBackendHostForProfileSyncTest();

  virtual void RequestConfigureSyncer(
      syncer::ConfigureReason reason,
      syncer::ModelTypeSet to_download,
      syncer::ModelTypeSet to_purge,
      syncer::ModelTypeSet to_journal,
      syncer::ModelTypeSet to_unapply,
      syncer::ModelTypeSet to_ignore,
      const syncer::ModelSafeRoutingInfo& routing_info,
      const base::Callback<void(syncer::ModelTypeSet,
                                syncer::ModelTypeSet)>& ready_task,
      const base::Closure& retry_callback) OVERRIDE;

 protected:
  virtual void InitCore(scoped_ptr<DoInitializeOptions> options) OVERRIDE;

 private:
  // Invoked at the start of HandleSyncManagerInitializationOnFrontendLoop.
  // Allows extra initialization work to be performed before the backend comes
  // up.
  base::Closure& callback_;
};

}  // namespace browser_sync

class TestProfileSyncService : public ProfileSyncService {
 public:
  // TODO(tim): Add ability to inject TokenService alongside SigninManager.
  // TODO(rogerta): what does above comment mean?
  TestProfileSyncService(
      ProfileSyncComponentsFactory* factory,
      Profile* profile,
      SigninManagerBase* signin,
      ProfileOAuth2TokenService* oauth2_token_service,
      ProfileSyncService::StartBehavior behavior);

  virtual ~TestProfileSyncService();

  virtual void OnConfigureDone(
      const browser_sync::DataTypeManager::ConfigureResult& result) OVERRIDE;

  // We implement our own version to avoid some DCHECKs.
  virtual syncer::UserShare* GetUserShare() const OVERRIDE;

  static BrowserContextKeyedService* BuildAutoStartAsyncInit(
      content::BrowserContext* profile);

  ProfileSyncComponentsFactoryMock* components_factory_mock();

  // |callback| can be used to populate nodes before the OnBackendInitialized
  // callback fires.
  void set_backend_init_callback(const base::Closure& callback) {
    callback_ = callback;
  }

  syncer::TestIdFactory* id_factory();

 protected:
  virtual void CreateBackend() OVERRIDE;

  // Return NULL handle to use in backend initialization to avoid receiving
  // js messages on UI loop when it's being destroyed, which are not deleted
  // and cause memory leak in test.
  virtual syncer::WeakHandle<syncer::JsEventHandler> GetJsEventHandler()
      OVERRIDE;

 private:
  syncer::TestIdFactory id_factory_;

  base::Closure callback_;
};

#endif  // CHROME_BROWSER_SYNC_TEST_PROFILE_SYNC_SERVICE_H_
