// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_backend/register_app_task.h"

#include "base/bind.h"
#include "base/location.h"
#include "chrome/browser/drive/drive_api_util.h"
#include "chrome/browser/drive/drive_service_interface.h"
#include "chrome/browser/sync_file_system/drive_backend/drive_backend_constants.h"
#include "chrome/browser/sync_file_system/drive_backend/folder_creator.h"
#include "chrome/browser/sync_file_system/drive_backend/metadata_database.h"
#include "chrome/browser/sync_file_system/drive_backend/metadata_database.pb.h"
#include "chrome/browser/sync_file_system/drive_backend/sync_engine_context.h"
#include "chrome/browser/sync_file_system/drive_backend/tracker_set.h"
#include "chrome/browser/sync_file_system/syncable_file_system_util.h"
#include "google_apis/drive/drive_api_parser.h"
#include "google_apis/drive/gdata_wapi_parser.h"

namespace sync_file_system {
namespace drive_backend {

namespace {

bool CompareOnCTime(const FileTracker& left,
                    const FileTracker& right) {
  return left.synced_details().creation_time() <
      right.synced_details().creation_time();
}

}  // namespace

RegisterAppTask::RegisterAppTask(SyncEngineContext* sync_context,
                                 const std::string& app_id)
    : sync_context_(sync_context),
      create_folder_retry_count_(0),
      app_id_(app_id),
      weak_ptr_factory_(this) {
}

RegisterAppTask::~RegisterAppTask() {
}

void RegisterAppTask::Run(const SyncStatusCallback& callback) {
  if (create_folder_retry_count_++ >= kMaxRetry) {
    callback.Run(SYNC_STATUS_FAILED);
    return;
  }

  if (!metadata_database() || !drive_service()) {
    callback.Run(SYNC_STATUS_FAILED);
    return;
  }

  int64 sync_root = metadata_database()->GetSyncRootTrackerID();
  TrackerSet trackers;
  if (!metadata_database()->FindTrackersByParentAndTitle(
          sync_root, app_id_, &trackers)) {
    CreateAppRootFolder(callback);
    return;
  }

  FileTracker candidate;
  if (!FilterCandidates(trackers, &candidate)) {
    CreateAppRootFolder(callback);
    return;
  }

  if (candidate.active()) {
    RunSoon(FROM_HERE, base::Bind(callback, SYNC_STATUS_OK));
    return;
  }

  RegisterAppIntoDatabase(candidate, callback);
}

void RegisterAppTask::CreateAppRootFolder(const SyncStatusCallback& callback) {
  int64 sync_root_tracker_id = metadata_database()->GetSyncRootTrackerID();
  FileTracker sync_root_tracker;
  bool should_success = metadata_database()->FindTrackerByTrackerID(
      sync_root_tracker_id,
      &sync_root_tracker);
  DCHECK(should_success);

  DCHECK(!folder_creator_);
  folder_creator_.reset(new FolderCreator(
      drive_service(), metadata_database(),
      sync_root_tracker.file_id(), app_id_));
  folder_creator_->Run(base::Bind(
      &RegisterAppTask::DidCreateAppRootFolder,
      weak_ptr_factory_.GetWeakPtr(), callback));
}

void RegisterAppTask::DidCreateAppRootFolder(
    const SyncStatusCallback& callback,
    const std::string& folder_id,
    SyncStatusCode status) {
  scoped_ptr<FolderCreator> deleter = folder_creator_.Pass();
  if (status != SYNC_STATUS_OK) {
    callback.Run(status);
    return;
  }

  Run(callback);
}

bool RegisterAppTask::FilterCandidates(const TrackerSet& trackers,
                                       FileTracker* candidate) {
  DCHECK(candidate);
  if (trackers.has_active()) {
    *candidate = *trackers.active_tracker();
    return true;
  }

  FileTracker* oldest_tracker = NULL;
  for (TrackerSet::const_iterator itr = trackers.begin();
       itr != trackers.end(); ++itr) {
    FileTracker* tracker = *itr;
    FileMetadata file;
    DCHECK(!tracker->active());
    DCHECK(tracker->has_synced_details());

    if (!metadata_database()->FindFileByFileID(tracker->file_id(), &file)) {
      NOTREACHED();
      // The parent folder is sync-root, whose contents are fetched in
      // initialization sequence.
      // So at this point, direct children of sync-root should have
      // FileMetadata.
      continue;
    }

    if (file.details().file_kind() != FILE_KIND_FOLDER)
      continue;

    if (file.details().missing())
      continue;

    if (oldest_tracker && CompareOnCTime(*oldest_tracker, *tracker))
      continue;

    oldest_tracker = tracker;
  }

  if (!oldest_tracker)
    return false;

  *candidate = *oldest_tracker;
  return true;
}

void RegisterAppTask::RegisterAppIntoDatabase(
    const FileTracker& tracker,
    const SyncStatusCallback& callback) {
  metadata_database()->RegisterApp(
      app_id_, tracker.file_id(), callback);
}

MetadataDatabase* RegisterAppTask::metadata_database() {
  return sync_context_->GetMetadataDatabase();
}

drive::DriveServiceInterface* RegisterAppTask::drive_service() {
  return sync_context_->GetDriveService();
}

}  // namespace drive_backend
}  // namespace sync_file_system
