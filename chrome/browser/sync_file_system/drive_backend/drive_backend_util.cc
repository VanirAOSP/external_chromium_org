// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_backend/drive_backend_util.h"

#include "base/file_util.h"
#include "base/logging.h"
#include "base/memory/scoped_vector.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/drive/drive_api_util.h"
#include "chrome/browser/sync_file_system/drive_backend/drive_backend_constants.h"
#include "chrome/browser/sync_file_system/drive_backend/metadata_database.pb.h"
#include "google_apis/drive/drive_api_parser.h"
#include "google_apis/drive/gdata_wapi_parser.h"
#include "net/base/mime_util.h"
#include "third_party/leveldatabase/src/include/leveldb/write_batch.h"

namespace sync_file_system {
namespace drive_backend {

namespace {

const char kMimeTypeOctetStream[] = "application/octet-stream";

}  // namespace

void PutServiceMetadataToBatch(const ServiceMetadata& service_metadata,
                               leveldb::WriteBatch* batch) {
  std::string value;
  bool success = service_metadata.SerializeToString(&value);
  DCHECK(success);
  batch->Put(kServiceMetadataKey, value);
}

void PutFileToBatch(const FileMetadata& file, leveldb::WriteBatch* batch) {
  std::string value;
  bool success = file.SerializeToString(&value);
  DCHECK(success);
  batch->Put(kFileMetadataKeyPrefix + file.file_id(), value);
}

void PutTrackerToBatch(const FileTracker& tracker, leveldb::WriteBatch* batch) {
  std::string value;
  bool success = tracker.SerializeToString(&value);
  DCHECK(success);
  batch->Put(kFileTrackerKeyPrefix + base::Int64ToString(tracker.tracker_id()),
             value);
}

void PopulateFileDetailsByFileResource(
    const google_apis::FileResource& file_resource,
    FileDetails* details) {
  details->clear_parent_folder_ids();
  for (ScopedVector<google_apis::ParentReference>::const_iterator itr =
           file_resource.parents().begin();
       itr != file_resource.parents().end();
       ++itr) {
    details->add_parent_folder_ids((*itr)->file_id());
  }
  details->set_title(file_resource.title());

  google_apis::DriveEntryKind kind = drive::util::GetKind(file_resource);
  if (kind == google_apis::ENTRY_KIND_FILE)
    details->set_file_kind(FILE_KIND_FILE);
  else if (kind == google_apis::ENTRY_KIND_FOLDER)
    details->set_file_kind(FILE_KIND_FOLDER);
  else
    details->set_file_kind(FILE_KIND_UNSUPPORTED);

  details->set_md5(file_resource.md5_checksum());
  details->set_etag(file_resource.etag());
  details->set_creation_time(file_resource.created_date().ToInternalValue());
  details->set_modification_time(
      file_resource.modified_date().ToInternalValue());
  details->set_missing(false);
}

scoped_ptr<FileMetadata> CreateFileMetadataFromFileResource(
    int64 change_id,
    const google_apis::FileResource& resource) {
  scoped_ptr<FileMetadata> file(new FileMetadata);
  file->set_file_id(resource.file_id());

  FileDetails* details = file->mutable_details();
  details->set_change_id(change_id);

  if (resource.labels().is_trashed()) {
    details->set_missing(true);
    return file.Pass();
  }

  PopulateFileDetailsByFileResource(resource, details);
  return file.Pass();
}

scoped_ptr<FileMetadata> CreateFileMetadataFromChangeResource(
    const google_apis::ChangeResource& change) {
  scoped_ptr<FileMetadata> file(new FileMetadata);
  file->set_file_id(change.file_id());

  FileDetails* details = file->mutable_details();
  details->set_change_id(change.change_id());

  if (change.is_deleted()) {
    details->set_missing(true);
    return file.Pass();
  }

  PopulateFileDetailsByFileResource(*change.file(), details);
  return file.Pass();
}

scoped_ptr<FileMetadata> CreateDeletedFileMetadata(
    int64 change_id,
    const std::string& file_id) {
  scoped_ptr<FileMetadata> file(new FileMetadata);
  file->set_file_id(file_id);

  FileDetails* details = file->mutable_details();
  details->set_change_id(change_id);
  details->set_missing(true);
  return file.Pass();
}

webkit_blob::ScopedFile CreateTemporaryFile(
    base::TaskRunner* file_task_runner) {
  base::FilePath temp_file_path;
  if (!base::CreateTemporaryFile(&temp_file_path))
    return webkit_blob::ScopedFile();

  return webkit_blob::ScopedFile(
      temp_file_path,
      webkit_blob::ScopedFile::DELETE_ON_SCOPE_OUT,
      file_task_runner);
}

std::string FileKindToString(FileKind file_kind) {
  switch (file_kind) {
    case FILE_KIND_UNSUPPORTED:
      return "unsupported";
    case FILE_KIND_FILE:
      return "file";
    case FILE_KIND_FOLDER:
      return "folder";
  }

  NOTREACHED();
  return "unknown";
}

bool HasFileAsParent(const FileDetails& details, const std::string& file_id) {
  for (int i = 0; i < details.parent_folder_ids_size(); ++i) {
    if (details.parent_folder_ids(i) == file_id)
      return true;
  }
  return false;
}

std::string GetMimeTypeFromTitle(const base::FilePath& title) {
  base::FilePath::StringType extension = title.Extension();
  std::string mime_type;
  if (extension.empty() ||
      !net::GetWellKnownMimeTypeFromExtension(extension.substr(1), &mime_type))
    return kMimeTypeOctetStream;
  return mime_type;
}

scoped_ptr<google_apis::ResourceEntry> GetOldestCreatedFolderResource(
    ScopedVector<google_apis::ResourceEntry> candidates) {
  scoped_ptr<google_apis::ResourceEntry> oldest;
  for (size_t i = 0; i < candidates.size(); ++i) {
    google_apis::ResourceEntry* entry = candidates[i];
    if (!entry->is_folder() || entry->deleted())
      continue;

    if (!oldest || oldest->published_time() > entry->published_time()) {
      oldest.reset(entry);
      candidates[i] = NULL;
    }
  }

  return oldest.Pass();
}

}  // namespace drive_backend
}  // namespace sync_file_system
