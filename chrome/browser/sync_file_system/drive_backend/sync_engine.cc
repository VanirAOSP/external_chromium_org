// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_backend/sync_engine.h"

#include "base/bind.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/values.h"
#include "chrome/browser/drive/drive_api_service.h"
#include "chrome/browser/drive/drive_notification_manager.h"
#include "chrome/browser/drive/drive_notification_manager_factory.h"
#include "chrome/browser/drive/drive_service_interface.h"
#include "chrome/browser/drive/drive_uploader.h"
#include "chrome/browser/drive/drive_uploader.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/profile_oauth2_token_service.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/sync_file_system/drive_backend/conflict_resolver.h"
#include "chrome/browser/sync_file_system/drive_backend/drive_backend_constants.h"
#include "chrome/browser/sync_file_system/drive_backend/list_changes_task.h"
#include "chrome/browser/sync_file_system/drive_backend/local_to_remote_syncer.h"
#include "chrome/browser/sync_file_system/drive_backend/metadata_database.h"
#include "chrome/browser/sync_file_system/drive_backend/register_app_task.h"
#include "chrome/browser/sync_file_system/drive_backend/remote_to_local_syncer.h"
#include "chrome/browser/sync_file_system/drive_backend/sync_engine_initializer.h"
#include "chrome/browser/sync_file_system/drive_backend/uninstall_app_task.h"
#include "chrome/browser/sync_file_system/file_status_observer.h"
#include "chrome/browser/sync_file_system/logger.h"
#include "chrome/browser/sync_file_system/sync_task.h"
#include "chrome/browser/sync_file_system/syncable_file_system_util.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/common/extension.h"
#include "google_apis/drive/drive_api_url_generator.h"
#include "google_apis/drive/gdata_wapi_url_generator.h"
#include "webkit/common/blob/scoped_file.h"
#include "webkit/common/fileapi/file_system_util.h"

namespace sync_file_system {
namespace drive_backend {

namespace {

void EmptyStatusCallback(SyncStatusCode status) {}

}  // namespace

scoped_ptr<SyncEngine> SyncEngine::CreateForBrowserContext(
    content::BrowserContext* context) {
  GURL base_drive_url(
      google_apis::DriveApiUrlGenerator::kBaseUrlForProduction);
  GURL base_download_url(
      google_apis::DriveApiUrlGenerator::kBaseDownloadUrlForProduction);
  GURL wapi_base_url(
      google_apis::GDataWapiUrlGenerator::kBaseUrlForProduction);

  scoped_refptr<base::SequencedWorkerPool> worker_pool(
      content::BrowserThread::GetBlockingPool());
  scoped_refptr<base::SequencedTaskRunner> drive_task_runner(
      worker_pool->GetSequencedTaskRunnerWithShutdownBehavior(
          worker_pool->GetSequenceToken(),
          base::SequencedWorkerPool::SKIP_ON_SHUTDOWN));

  ProfileOAuth2TokenService* token_service =
      ProfileOAuth2TokenServiceFactory::GetForProfile(
          Profile::FromBrowserContext(context));
  scoped_ptr<drive::DriveServiceInterface> drive_service(
      new drive::DriveAPIService(
          token_service,
          context->GetRequestContext(),
          drive_task_runner.get(),
          base_drive_url, base_download_url, wapi_base_url,
          std::string() /* custom_user_agent */));
  drive_service->Initialize(token_service->GetPrimaryAccountId());

  scoped_ptr<drive::DriveUploaderInterface> drive_uploader(
      new drive::DriveUploader(drive_service.get(), drive_task_runner.get()));

  drive::DriveNotificationManager* notification_manager =
      drive::DriveNotificationManagerFactory::GetForBrowserContext(context);
  ExtensionService* extension_service =
      extensions::ExtensionSystem::GetForBrowserContext(
          context)->extension_service();

  scoped_refptr<base::SequencedTaskRunner> task_runner(
      worker_pool->GetSequencedTaskRunnerWithShutdownBehavior(
          worker_pool->GetSequenceToken(),
          base::SequencedWorkerPool::SKIP_ON_SHUTDOWN));

  scoped_ptr<drive_backend::SyncEngine> sync_engine(
      new SyncEngine(
          GetSyncFileSystemDir(context->GetPath()),
          task_runner.get(),
          drive_service.Pass(),
          drive_uploader.Pass(),
          notification_manager,
          extension_service,
          token_service));
  sync_engine->Initialize();

  return sync_engine.Pass();
}

void SyncEngine::AppendDependsOnFactories(
    std::set<BrowserContextKeyedServiceFactory*>* factories) {
  DCHECK(factories);
  factories->insert(drive::DriveNotificationManagerFactory::GetInstance());
  factories->insert(ProfileOAuth2TokenServiceFactory::GetInstance());
  factories->insert(extensions::ExtensionSystemFactory::GetInstance());
}

SyncEngine::~SyncEngine() {
  net::NetworkChangeNotifier::RemoveNetworkChangeObserver(this);
  drive_service_->RemoveObserver(this);
  if (notification_manager_)
    notification_manager_->RemoveObserver(this);
}

void SyncEngine::Initialize() {
  DCHECK(!task_manager_);
  task_manager_.reset(new SyncTaskManager(weak_ptr_factory_.GetWeakPtr()));
  task_manager_->Initialize(SYNC_STATUS_OK);

  PostInitializeTask();

  if (notification_manager_)
    notification_manager_->AddObserver(this);
  drive_service_->AddObserver(this);
  net::NetworkChangeNotifier::AddNetworkChangeObserver(this);

  net::NetworkChangeNotifier::ConnectionType type =
      net::NetworkChangeNotifier::GetConnectionType();
  network_available_ =
      type != net::NetworkChangeNotifier::CONNECTION_NONE;
}

void SyncEngine::AddServiceObserver(SyncServiceObserver* observer) {
  service_observers_.AddObserver(observer);
}

void SyncEngine::AddFileStatusObserver(FileStatusObserver* observer) {
  file_status_observers_.AddObserver(observer);
}

void SyncEngine::RegisterOrigin(
    const GURL& origin,
    const SyncStatusCallback& callback) {
  if (!metadata_database_ && drive_service_->HasRefreshToken())
    PostInitializeTask();

  task_manager_->ScheduleSyncTaskAtPriority(
      scoped_ptr<SyncTask>(new RegisterAppTask(this, origin.host())),
      SyncTaskManager::PRIORITY_HIGH,
      callback);
}

void SyncEngine::EnableOrigin(
    const GURL& origin,
    const SyncStatusCallback& callback) {
  task_manager_->ScheduleTaskAtPriority(
      base::Bind(&SyncEngine::DoEnableApp,
                 weak_ptr_factory_.GetWeakPtr(),
                 origin.host()),
      SyncTaskManager::PRIORITY_HIGH,
      callback);
}

void SyncEngine::DisableOrigin(
    const GURL& origin,
    const SyncStatusCallback& callback) {
  task_manager_->ScheduleTaskAtPriority(
      base::Bind(&SyncEngine::DoDisableApp,
                 weak_ptr_factory_.GetWeakPtr(),
                 origin.host()),
      SyncTaskManager::PRIORITY_HIGH,
      callback);
}

void SyncEngine::UninstallOrigin(
    const GURL& origin,
    UninstallFlag flag,
    const SyncStatusCallback& callback) {
  task_manager_->ScheduleSyncTaskAtPriority(
      scoped_ptr<SyncTask>(new UninstallAppTask(this, origin.host(), flag)),
      SyncTaskManager::PRIORITY_HIGH,
      callback);
}

void SyncEngine::ProcessRemoteChange(
    const SyncFileCallback& callback) {
  RemoteToLocalSyncer* syncer = new RemoteToLocalSyncer(this);
  task_manager_->ScheduleSyncTask(
      scoped_ptr<SyncTask>(syncer),
      base::Bind(&SyncEngine::DidProcessRemoteChange,
                 weak_ptr_factory_.GetWeakPtr(),
                 syncer, callback));
}

void SyncEngine::SetRemoteChangeProcessor(
    RemoteChangeProcessor* processor) {
  remote_change_processor_ = processor;
}

LocalChangeProcessor* SyncEngine::GetLocalChangeProcessor() {
  return this;
}

bool SyncEngine::IsConflicting(const fileapi::FileSystemURL& url) {
  // TODO(tzik): Implement this before we support manual conflict resolution.
  return false;
}

RemoteServiceState SyncEngine::GetCurrentState() const {
  return service_state_;
}

void SyncEngine::GetOriginStatusMap(OriginStatusMap* status_map) {
  DCHECK(status_map);
  if (!extension_service_ || !metadata_database_)
    return;

  std::vector<std::string> app_ids;
  metadata_database_->GetRegisteredAppIDs(&app_ids);

  for (std::vector<std::string>::const_iterator itr = app_ids.begin();
       itr != app_ids.end(); ++itr) {
    const std::string& app_id = *itr;
    GURL origin =
        extensions::Extension::GetBaseURLFromExtensionId(app_id);
    (*status_map)[origin] =
        metadata_database_->IsAppEnabled(app_id) ? "Enabled" : "Disabled";
  }
}

scoped_ptr<base::ListValue> SyncEngine::DumpFiles(const GURL& origin) {
  if (!metadata_database_)
    return scoped_ptr<base::ListValue>();
  return metadata_database_->DumpFiles(origin.host());
}

scoped_ptr<base::ListValue> SyncEngine::DumpDatabase() {
  if (!metadata_database_)
    return scoped_ptr<base::ListValue>();
  return metadata_database_->DumpDatabase();
}

void SyncEngine::SetSyncEnabled(bool enabled) {
  if (sync_enabled_ == enabled)
    return;

  RemoteServiceState old_state = GetCurrentState();
  sync_enabled_ = enabled;
  if (old_state == GetCurrentState())
    return;

  const char* status_message = enabled ? "Sync is enabled" : "Sync is disabled";
  FOR_EACH_OBSERVER(
      Observer, service_observers_,
      OnRemoteServiceStateUpdated(GetCurrentState(), status_message));
}

SyncStatusCode SyncEngine::SetConflictResolutionPolicy(
    ConflictResolutionPolicy policy) {
  conflict_resolution_policy_ = policy;
  return SYNC_STATUS_OK;
}

ConflictResolutionPolicy
SyncEngine::GetConflictResolutionPolicy() const {
  return conflict_resolution_policy_;
}

void SyncEngine::GetRemoteVersions(
    const fileapi::FileSystemURL& url,
    const RemoteVersionsCallback& callback) {
  // TODO(tzik): Implement this before we support manual conflict resolution.
  callback.Run(SYNC_STATUS_FAILED, std::vector<Version>());
}

void SyncEngine::DownloadRemoteVersion(
    const fileapi::FileSystemURL& url,
    const std::string& version_id,
    const DownloadVersionCallback& callback) {
  // TODO(tzik): Implement this before we support manual conflict resolution.
  callback.Run(SYNC_STATUS_FAILED, webkit_blob::ScopedFile());
}

void SyncEngine::ApplyLocalChange(
    const FileChange& local_change,
    const base::FilePath& local_path,
    const SyncFileMetadata& local_metadata,
    const fileapi::FileSystemURL& url,
    const SyncStatusCallback& callback) {
  util::Log(logging::LOG_VERBOSE, FROM_HERE,
            "[Local->Remote] ApplyLocalChange: %s on %s",
            local_change.DebugString().c_str(),
            url.DebugString().c_str());

  LocalToRemoteSyncer* syncer = new LocalToRemoteSyncer(
      this, local_metadata, local_change, local_path, url);
  task_manager_->ScheduleSyncTask(
      scoped_ptr<SyncTask>(syncer),
      base::Bind(&SyncEngine::DidApplyLocalChange,
                 weak_ptr_factory_.GetWeakPtr(),
                 syncer, callback));
}

void SyncEngine::MaybeScheduleNextTask() {
  if (GetCurrentState() == REMOTE_SERVICE_DISABLED)
    return;

  // TODO(tzik): Notify observer of OnRemoteChangeQueueUpdated.
  // TODO(tzik): Add an interface to get the number of dirty trackers to
  // MetadataDatabase.

  MaybeStartFetchChanges();
}

void SyncEngine::NotifyLastOperationStatus(
    SyncStatusCode sync_status,
    bool used_network) {
  UpdateServiceStateFromSyncStatusCode(sync_status, used_network);
  if (metadata_database_) {
    FOR_EACH_OBSERVER(
        Observer,
        service_observers_,
        OnRemoteChangeQueueUpdated(
            metadata_database_->GetDirtyTrackerCount()));
  }
}

void SyncEngine::OnNotificationReceived() {
  if (service_state_ == REMOTE_SERVICE_TEMPORARY_UNAVAILABLE)
    UpdateServiceState(REMOTE_SERVICE_OK, "Got push notification for Drive.");

  should_check_remote_change_ = true;
  MaybeScheduleNextTask();
}

void SyncEngine::OnPushNotificationEnabled(bool enabled) {}

void SyncEngine::OnReadyToSendRequests() {
  if (service_state_ == REMOTE_SERVICE_OK)
    return;
  UpdateServiceState(REMOTE_SERVICE_OK, "Authenticated");

  if (!metadata_database_ && auth_token_service_) {
    drive_service_->Initialize(auth_token_service_->GetPrimaryAccountId());
    PostInitializeTask();
    return;
  }

  should_check_remote_change_ = true;
  MaybeScheduleNextTask();
}

void SyncEngine::OnRefreshTokenInvalid() {
  UpdateServiceState(
      REMOTE_SERVICE_AUTHENTICATION_REQUIRED,
      "Found invalid refresh token.");
}

void SyncEngine::OnNetworkChanged(
    net::NetworkChangeNotifier::ConnectionType type) {
  bool new_network_availability =
      type != net::NetworkChangeNotifier::CONNECTION_NONE;

  if (network_available_ && !new_network_availability) {
    UpdateServiceState(REMOTE_SERVICE_TEMPORARY_UNAVAILABLE, "Disconnected");
  } else if (!network_available_ && new_network_availability) {
    UpdateServiceState(REMOTE_SERVICE_OK, "Connected");
    should_check_remote_change_ = true;
    MaybeStartFetchChanges();
  }
  network_available_ = new_network_availability;
}

drive::DriveServiceInterface* SyncEngine::GetDriveService() {
  return drive_service_.get();
}

drive::DriveUploaderInterface* SyncEngine::GetDriveUploader() {
  return drive_uploader_.get();
}

MetadataDatabase* SyncEngine::GetMetadataDatabase() {
  return metadata_database_.get();
}

RemoteChangeProcessor* SyncEngine::GetRemoteChangeProcessor() {
  return remote_change_processor_;
}

base::SequencedTaskRunner* SyncEngine::GetBlockingTaskRunner() {
  return task_runner_.get();
}

SyncEngine::SyncEngine(
    const base::FilePath& base_dir,
    base::SequencedTaskRunner* task_runner,
    scoped_ptr<drive::DriveServiceInterface> drive_service,
    scoped_ptr<drive::DriveUploaderInterface> drive_uploader,
    drive::DriveNotificationManager* notification_manager,
    ExtensionServiceInterface* extension_service,
    ProfileOAuth2TokenService* auth_token_service)
    : base_dir_(base_dir),
      task_runner_(task_runner),
      drive_service_(drive_service.Pass()),
      drive_uploader_(drive_uploader.Pass()),
      notification_manager_(notification_manager),
      extension_service_(extension_service),
      auth_token_service_(auth_token_service),
      remote_change_processor_(NULL),
      service_state_(REMOTE_SERVICE_TEMPORARY_UNAVAILABLE),
      should_check_conflict_(true),
      should_check_remote_change_(true),
      sync_enabled_(false),
      conflict_resolution_policy_(CONFLICT_RESOLUTION_POLICY_LAST_WRITE_WIN),
      network_available_(false),
      weak_ptr_factory_(this) {
}

void SyncEngine::DoDisableApp(const std::string& app_id,
                              const SyncStatusCallback& callback) {
  metadata_database_->DisableApp(app_id, callback);
}

void SyncEngine::DoEnableApp(const std::string& app_id,
                             const SyncStatusCallback& callback) {
  metadata_database_->EnableApp(app_id, callback);
}

void SyncEngine::PostInitializeTask() {
  DCHECK(!metadata_database_);

  // This initializer task may not run if metadata_database_ is already
  // initialized when it runs.
  SyncEngineInitializer* initializer =
      new SyncEngineInitializer(this,
                                task_runner_.get(),
                                drive_service_.get(),
                                base_dir_.Append(kDatabaseName));
  task_manager_->ScheduleSyncTaskAtPriority(
      scoped_ptr<SyncTask>(initializer),
      SyncTaskManager::PRIORITY_HIGH,
      base::Bind(&SyncEngine::DidInitialize, weak_ptr_factory_.GetWeakPtr(),
                 initializer));
}

void SyncEngine::DidInitialize(SyncEngineInitializer* initializer,
                               SyncStatusCode status) {
  if (status != SYNC_STATUS_OK) {
    if (drive_service_->HasRefreshToken()) {
      UpdateServiceState(REMOTE_SERVICE_TEMPORARY_UNAVAILABLE,
                         "Could not initialize remote service");
    } else {
      UpdateServiceState(REMOTE_SERVICE_AUTHENTICATION_REQUIRED,
                         "Authentication required.");
    }
    return;
  }

  scoped_ptr<MetadataDatabase> metadata_database =
      initializer->PassMetadataDatabase();
  if (metadata_database)
    metadata_database_ = metadata_database.Pass();

  DCHECK(metadata_database_);
  UpdateRegisteredApps();
}

void SyncEngine::DidProcessRemoteChange(RemoteToLocalSyncer* syncer,
                                        const SyncFileCallback& callback,
                                        SyncStatusCode status) {
  if (syncer->is_sync_root_deletion()) {
    MetadataDatabase::ClearDatabase(metadata_database_.Pass());
    PostInitializeTask();
    callback.Run(status, syncer->url());
    return;
  }

  if (status == SYNC_STATUS_OK) {
    if (syncer->sync_action() != SYNC_ACTION_NONE &&
        syncer->url().is_valid()) {
      FOR_EACH_OBSERVER(FileStatusObserver,
                        file_status_observers_,
                        OnFileStatusChanged(syncer->url(),
                                            SYNC_FILE_STATUS_SYNCED,
                                            syncer->sync_action(),
                                            SYNC_DIRECTION_REMOTE_TO_LOCAL));
    }

    if (syncer->sync_action() == SYNC_ACTION_DELETED &&
        syncer->url().is_valid() &&
        fileapi::VirtualPath::IsRootPath(syncer->url().path())) {
      RegisterOrigin(syncer->url().origin(), base::Bind(&EmptyStatusCallback));
    }
    should_check_conflict_ = true;
  }
  callback.Run(status, syncer->url());
}

void SyncEngine::DidApplyLocalChange(LocalToRemoteSyncer* syncer,
                                     const SyncStatusCallback& callback,
                                     SyncStatusCode status) {
  util::Log(logging::LOG_VERBOSE, FROM_HERE,
            "[Local->Remote] ApplyLocalChange finished --> %s",
            SyncStatusCodeToString(status));

  if ((status == SYNC_STATUS_OK || status == SYNC_STATUS_RETRY) &&
      syncer->url().is_valid() &&
      syncer->sync_action() != SYNC_ACTION_NONE) {
    fileapi::FileSystemURL updated_url = syncer->url();
    if (!syncer->target_path().empty()) {
      updated_url = CreateSyncableFileSystemURL(syncer->url().origin(),
                                                syncer->target_path());
    }
    FOR_EACH_OBSERVER(FileStatusObserver,
                      file_status_observers_,
                      OnFileStatusChanged(updated_url,
                                          SYNC_FILE_STATUS_SYNCED,
                                          syncer->sync_action(),
                                          SYNC_DIRECTION_LOCAL_TO_REMOTE));
  }

  if (status == SYNC_STATUS_UNKNOWN_ORIGIN && syncer->url().is_valid()) {
    RegisterOrigin(syncer->url().origin(),
                   base::Bind(&EmptyStatusCallback));
  }

  if (status != SYNC_STATUS_OK &&
      status != SYNC_STATUS_NO_CHANGE_TO_SYNC) {
    callback.Run(status);
    return;
  }

  if (status == SYNC_STATUS_NO_CHANGE_TO_SYNC)
    metadata_database_->PromoteLowerPriorityTrackersToNormal();

  if (status == SYNC_STATUS_OK)
    should_check_conflict_ = true;

  callback.Run(status);
}

void SyncEngine::MaybeStartFetchChanges() {
  if (GetCurrentState() == REMOTE_SERVICE_DISABLED)
    return;

  if (!metadata_database_)
    return;

  base::TimeTicks now = base::TimeTicks::Now();
  if (!should_check_remote_change_ && now < time_to_check_changes_) {
    if (!metadata_database_->HasDirtyTracker() && should_check_conflict_) {
      task_manager_->ScheduleSyncTaskIfIdle(
          scoped_ptr<SyncTask>(new ConflictResolver(this)),
          base::Bind(&SyncEngine::DidResolveConflict,
                     weak_ptr_factory_.GetWeakPtr()));
    }
    return;
  }

  if (task_manager_->ScheduleSyncTaskIfIdle(
          scoped_ptr<SyncTask>(new ListChangesTask(this)),
          base::Bind(&SyncEngine::DidFetchChanges,
                     weak_ptr_factory_.GetWeakPtr()))) {
    should_check_remote_change_ = false;
    time_to_check_changes_ =
        now + base::TimeDelta::FromSeconds(kListChangesRetryDelaySeconds);
  }
}

void SyncEngine::DidResolveConflict(SyncStatusCode status) {
  if (status == SYNC_STATUS_NO_CONFLICT)
    should_check_conflict_ = false;
}

void SyncEngine::DidFetchChanges(SyncStatusCode status) {
  if (status == SYNC_STATUS_OK)
    should_check_conflict_ = true;
}

void SyncEngine::UpdateServiceStateFromSyncStatusCode(
      SyncStatusCode status,
      bool used_network) {
  switch (status) {
    case SYNC_STATUS_OK:
      if (used_network)
        UpdateServiceState(REMOTE_SERVICE_OK, std::string());
      break;

    // Authentication error.
    case SYNC_STATUS_AUTHENTICATION_FAILED:
      UpdateServiceState(REMOTE_SERVICE_AUTHENTICATION_REQUIRED,
                         "Authentication required");
      break;

    // OAuth token error.
    case SYNC_STATUS_ACCESS_FORBIDDEN:
      UpdateServiceState(REMOTE_SERVICE_AUTHENTICATION_REQUIRED,
                         "Access forbidden");
      break;

    // Errors which could make the service temporarily unavailable.
    case SYNC_STATUS_SERVICE_TEMPORARILY_UNAVAILABLE:
    case SYNC_STATUS_NETWORK_ERROR:
    case SYNC_STATUS_ABORT:
    case SYNC_STATUS_FAILED:
      if (drive_service_->HasRefreshToken()) {
        UpdateServiceState(REMOTE_SERVICE_TEMPORARY_UNAVAILABLE,
                           "Network or temporary service error.");
      } else {
        UpdateServiceState(REMOTE_SERVICE_AUTHENTICATION_REQUIRED,
                           "Authentication required");
      }
      break;

    // Errors which would require manual user intervention to resolve.
    case SYNC_DATABASE_ERROR_CORRUPTION:
    case SYNC_DATABASE_ERROR_IO_ERROR:
    case SYNC_DATABASE_ERROR_FAILED:
      UpdateServiceState(REMOTE_SERVICE_DISABLED,
                         "Unrecoverable database error");
      break;

    default:
      // Other errors don't affect service state
      break;
  }
}

void SyncEngine::UpdateServiceState(RemoteServiceState state,
                                    const std::string& description) {
  RemoteServiceState old_state = GetCurrentState();
  service_state_ = state;

  if (old_state == GetCurrentState())
    return;

  util::Log(logging::LOG_INFO, FROM_HERE,
            "Service state changed: %d->%d: %s",
            old_state, GetCurrentState(), description.c_str());
  FOR_EACH_OBSERVER(
      Observer, service_observers_,
      OnRemoteServiceStateUpdated(GetCurrentState(), description));
}

void SyncEngine::UpdateRegisteredApps() {
  if (!extension_service_)
    return;

  DCHECK(metadata_database_);
  std::vector<std::string> app_ids;
  metadata_database_->GetRegisteredAppIDs(&app_ids);

  // Update the status of every origin using status from ExtensionService.
  for (std::vector<std::string>::const_iterator itr = app_ids.begin();
       itr != app_ids.end(); ++itr) {
    const std::string& app_id = *itr;
    GURL origin =
        extensions::Extension::GetBaseURLFromExtensionId(app_id);
    if (!extension_service_->GetInstalledExtension(app_id)) {
      // Extension has been uninstalled.
      // (At this stage we can't know if it was unpacked extension or not,
      // so just purge the remote folder.)
      UninstallOrigin(origin,
                      RemoteFileSyncService::UNINSTALL_AND_PURGE_REMOTE,
                      base::Bind(&EmptyStatusCallback));
      continue;
    }
    FileTracker tracker;
    if (!metadata_database_->FindAppRootTracker(app_id, &tracker)) {
      // App will register itself on first run.
      continue;
    }
    bool is_app_enabled = extension_service_->IsExtensionEnabled(app_id);
    bool is_app_root_tracker_enabled =
        tracker.tracker_kind() == TRACKER_KIND_APP_ROOT;
    if (is_app_enabled && !is_app_root_tracker_enabled)
      EnableOrigin(origin, base::Bind(&EmptyStatusCallback));
    else if (!is_app_enabled && is_app_root_tracker_enabled)
      DisableOrigin(origin, base::Bind(&EmptyStatusCallback));
  }
}

}  // namespace drive_backend
}  // namespace sync_file_system
