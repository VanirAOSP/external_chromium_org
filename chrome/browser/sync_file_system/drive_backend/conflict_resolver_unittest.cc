// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_backend/conflict_resolver.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "chrome/browser/drive/drive_uploader.h"
#include "chrome/browser/drive/fake_drive_service.h"
#include "chrome/browser/sync_file_system/drive_backend/drive_backend_constants.h"
#include "chrome/browser/sync_file_system/drive_backend/drive_backend_test_util.h"
#include "chrome/browser/sync_file_system/drive_backend/list_changes_task.h"
#include "chrome/browser/sync_file_system/drive_backend/metadata_database.h"
#include "chrome/browser/sync_file_system/drive_backend/remote_to_local_syncer.h"
#include "chrome/browser/sync_file_system/drive_backend/sync_engine_context.h"
#include "chrome/browser/sync_file_system/drive_backend/sync_engine_initializer.h"
#include "chrome/browser/sync_file_system/drive_backend_v1/fake_drive_service_helper.h"
#include "chrome/browser/sync_file_system/drive_backend_v1/fake_drive_uploader.h"
#include "chrome/browser/sync_file_system/fake_remote_change_processor.h"
#include "chrome/browser/sync_file_system/sync_file_system_test_util.h"
#include "chrome/browser/sync_file_system/syncable_file_system_util.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "google_apis/drive/gdata_errorcode.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sync_file_system {
namespace drive_backend {

class ConflictResolverTest : public testing::Test,
                             public SyncEngineContext {
 public:
  ConflictResolverTest()
      : thread_bundle_(content::TestBrowserThreadBundle::IO_MAINLOOP) {}
  virtual ~ConflictResolverTest() {}

  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(database_dir_.CreateUniqueTempDir());

    fake_drive_service_.reset(new FakeDriveServiceWrapper);
    ASSERT_TRUE(fake_drive_service_->LoadAccountMetadataForWapi(
        "sync_file_system/account_metadata.json"));
    ASSERT_TRUE(fake_drive_service_->LoadResourceListForWapi(
        "gdata/empty_feed.json"));

    drive_uploader_.reset(new FakeDriveUploader(fake_drive_service_.get()));
    fake_drive_helper_.reset(new FakeDriveServiceHelper(
        fake_drive_service_.get(), drive_uploader_.get()));
    fake_remote_change_processor_.reset(new FakeRemoteChangeProcessor);

    RegisterSyncableFileSystem();
  }

  virtual void TearDown() OVERRIDE {
    RevokeSyncableFileSystem();

    fake_remote_change_processor_.reset();
    metadata_database_.reset();
    fake_drive_helper_.reset();
    drive_uploader_.reset();
    fake_drive_service_.reset();
    base::RunLoop().RunUntilIdle();
  }

  void InitializeMetadataDatabase() {
    SyncEngineInitializer initializer(this,
                                      base::MessageLoopProxy::current(),
                                      fake_drive_service_.get(),
                                      database_dir_.path());
    SyncStatusCode status = SYNC_STATUS_UNKNOWN;
    initializer.Run(CreateResultReceiver(&status));
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(SYNC_STATUS_OK, status);
    metadata_database_ = initializer.PassMetadataDatabase();
  }

  void RegisterApp(const std::string& app_id,
                   const std::string& app_root_folder_id) {
    SyncStatusCode status = SYNC_STATUS_FAILED;
    metadata_database_->RegisterApp(app_id, app_root_folder_id,
                                    CreateResultReceiver(&status));
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(SYNC_STATUS_OK, status);
  }

  virtual drive::DriveServiceInterface* GetDriveService() OVERRIDE {
    return fake_drive_service_.get();
  }

  virtual drive::DriveUploaderInterface* GetDriveUploader() OVERRIDE {
    return drive_uploader_.get();
  }

  virtual MetadataDatabase* GetMetadataDatabase() OVERRIDE {
    return metadata_database_.get();
  }

  virtual RemoteChangeProcessor* GetRemoteChangeProcessor() OVERRIDE {
    return fake_remote_change_processor_.get();
  }

  virtual base::SequencedTaskRunner* GetBlockingTaskRunner() OVERRIDE {
    return base::MessageLoopProxy::current().get();
  }

 protected:
  std::string CreateSyncRoot() {
    std::string sync_root_folder_id;
    EXPECT_EQ(google_apis::HTTP_CREATED,
              fake_drive_helper_->AddOrphanedFolder(
                  kSyncRootFolderTitle, &sync_root_folder_id));
    return sync_root_folder_id;
  }

  std::string CreateRemoteFolder(const std::string& parent_folder_id,
                                 const std::string& title) {
    std::string folder_id;
    EXPECT_EQ(google_apis::HTTP_CREATED,
              fake_drive_helper_->AddFolder(
                  parent_folder_id, title, &folder_id));
    return folder_id;
  }

  std::string CreateRemoteFile(const std::string& parent_folder_id,
                               const std::string& title,
                               const std::string& content) {
    std::string file_id;
    EXPECT_EQ(google_apis::HTTP_SUCCESS,
              fake_drive_helper_->AddFile(
                  parent_folder_id, title, content, &file_id));
    return file_id;
  }

  google_apis::GDataErrorCode AddFileToFolder(
      const std::string& parent_folder_id,
      const std::string& file_id) {
    google_apis::GDataErrorCode error = google_apis::GDATA_OTHER_ERROR;
    fake_drive_service_->AddResourceToDirectory(
        parent_folder_id, file_id,
        CreateResultReceiver(&error));
    base::RunLoop().RunUntilIdle();
    return error;
  }

  int CountParents(const std::string& file_id) {
    scoped_ptr<google_apis::ResourceEntry> entry;
    EXPECT_EQ(google_apis::HTTP_SUCCESS,
              fake_drive_helper_->GetResourceEntry(file_id, &entry));
    int count = 0;
    const ScopedVector<google_apis::Link>& links = entry->links();
    for (ScopedVector<google_apis::Link>::const_iterator itr = links.begin();
        itr != links.end(); ++itr) {
      if ((*itr)->type() == google_apis::Link::LINK_PARENT)
        ++count;
    }
    return count;
  }

  SyncStatusCode RunSyncer() {
    SyncStatusCode status = SYNC_STATUS_UNKNOWN;
    scoped_ptr<RemoteToLocalSyncer> syncer(new RemoteToLocalSyncer(this));
    syncer->Run(CreateResultReceiver(&status));
    base::RunLoop().RunUntilIdle();
    return status;
  }

  void RunSyncerUntilIdle() {
    SyncStatusCode status = SYNC_STATUS_UNKNOWN;
    while (status != SYNC_STATUS_NO_CHANGE_TO_SYNC)
      status = RunSyncer();
  }

  SyncStatusCode RunConflictResolver() {
    SyncStatusCode status = SYNC_STATUS_UNKNOWN;
    ConflictResolver resolver(this);
    resolver.Run(CreateResultReceiver(&status));
    base::RunLoop().RunUntilIdle();
    return status;
  }

  SyncStatusCode ListChanges() {
    ListChangesTask list_changes(this);
    SyncStatusCode status = SYNC_STATUS_UNKNOWN;
    list_changes.Run(CreateResultReceiver(&status));
    base::RunLoop().RunUntilIdle();
    return status;
  }

  ScopedVector<google_apis::ResourceEntry>
  GetResourceEntriesForParentAndTitle(const std::string& parent_folder_id,
                                      const std::string& title) {
    ScopedVector<google_apis::ResourceEntry> entries;
    EXPECT_EQ(google_apis::HTTP_SUCCESS,
              fake_drive_helper_->SearchByTitle(
                  parent_folder_id, title, &entries));
    return entries.Pass();
  }

  void VerifyConflictResolution(const std::string& parent_folder_id,
                                const std::string& title,
                                const std::string& primary_file_id,
                                google_apis::DriveEntryKind kind) {
    ScopedVector<google_apis::ResourceEntry> entries;
    EXPECT_EQ(google_apis::HTTP_SUCCESS,
              fake_drive_helper_->SearchByTitle(
                  parent_folder_id, title, &entries));
    ASSERT_EQ(1u, entries.size());
    EXPECT_EQ(primary_file_id, entries[0]->resource_id());
    EXPECT_EQ(kind, entries[0]->kind());
  }

 private:
  content::TestBrowserThreadBundle thread_bundle_;
  base::ScopedTempDir database_dir_;

  scoped_ptr<FakeDriveServiceWrapper> fake_drive_service_;
  scoped_ptr<FakeDriveUploader> drive_uploader_;
  scoped_ptr<FakeDriveServiceHelper> fake_drive_helper_;
  scoped_ptr<MetadataDatabase> metadata_database_;
  scoped_ptr<FakeRemoteChangeProcessor> fake_remote_change_processor_;

  DISALLOW_COPY_AND_ASSIGN(ConflictResolverTest);
};

TEST_F(ConflictResolverTest, NoFileToBeResolved) {
  const GURL kOrigin("chrome-extension://example");
  const std::string sync_root = CreateSyncRoot();
  const std::string app_root = CreateRemoteFolder(sync_root, kOrigin.host());
  InitializeMetadataDatabase();
  RegisterApp(kOrigin.host(), app_root);
  RunSyncerUntilIdle();

  EXPECT_EQ(SYNC_STATUS_NO_CONFLICT, RunConflictResolver());
}

TEST_F(ConflictResolverTest, ResolveConflict_Files) {
  const GURL kOrigin("chrome-extension://example");
  const std::string sync_root = CreateSyncRoot();
  const std::string app_root = CreateRemoteFolder(sync_root, kOrigin.host());
  InitializeMetadataDatabase();
  RegisterApp(kOrigin.host(), app_root);
  RunSyncerUntilIdle();

  const std::string kTitle = "foo";
  const std::string primary = CreateRemoteFile(app_root, kTitle, "data1");
  CreateRemoteFile(app_root, kTitle, "data2");
  CreateRemoteFile(app_root, kTitle, "data3");
  CreateRemoteFile(app_root, kTitle, "data4");
  EXPECT_EQ(SYNC_STATUS_OK, ListChanges());
  RunSyncerUntilIdle();

  ScopedVector<google_apis::ResourceEntry> entries =
      GetResourceEntriesForParentAndTitle(app_root, kTitle);
  ASSERT_EQ(4u, entries.size());

  // Only primary file should survive.
  EXPECT_EQ(SYNC_STATUS_OK, RunConflictResolver());
  VerifyConflictResolution(app_root, kTitle, primary,
                           google_apis::ENTRY_KIND_FILE);
}

TEST_F(ConflictResolverTest, ResolveConflict_Folders) {
  const GURL kOrigin("chrome-extension://example");
  const std::string sync_root = CreateSyncRoot();
  const std::string app_root = CreateRemoteFolder(sync_root, kOrigin.host());
  InitializeMetadataDatabase();
  RegisterApp(kOrigin.host(), app_root);
  RunSyncerUntilIdle();

  const std::string kTitle = "foo";
  const std::string primary = CreateRemoteFolder(app_root, kTitle);
  CreateRemoteFolder(app_root, kTitle);
  CreateRemoteFolder(app_root, kTitle);
  CreateRemoteFolder(app_root, kTitle);
  EXPECT_EQ(SYNC_STATUS_OK, ListChanges());
  RunSyncerUntilIdle();

  ScopedVector<google_apis::ResourceEntry> entries =
      GetResourceEntriesForParentAndTitle(app_root, kTitle);
  ASSERT_EQ(4u, entries.size());

  // Only primary file should survive.
  EXPECT_EQ(SYNC_STATUS_OK, RunConflictResolver());
  VerifyConflictResolution(app_root, kTitle, primary,
                           google_apis::ENTRY_KIND_FOLDER);
}

TEST_F(ConflictResolverTest, ResolveConflict_FilesAndFolders) {
  const GURL kOrigin("chrome-extension://example");
  const std::string sync_root = CreateSyncRoot();
  const std::string app_root = CreateRemoteFolder(sync_root, kOrigin.host());
  InitializeMetadataDatabase();
  RegisterApp(kOrigin.host(), app_root);
  RunSyncerUntilIdle();

  const std::string kTitle = "foo";
  CreateRemoteFile(app_root, kTitle, "data");
  const std::string primary = CreateRemoteFolder(app_root, kTitle);
  CreateRemoteFile(app_root, kTitle, "data2");
  CreateRemoteFolder(app_root, kTitle);
  EXPECT_EQ(SYNC_STATUS_OK, ListChanges());
  RunSyncerUntilIdle();

  ScopedVector<google_apis::ResourceEntry> entries =
      GetResourceEntriesForParentAndTitle(app_root, kTitle);
  ASSERT_EQ(4u, entries.size());

  // Only primary file should survive.
  EXPECT_EQ(SYNC_STATUS_OK, RunConflictResolver());
  VerifyConflictResolution(app_root, kTitle, primary,
                           google_apis::ENTRY_KIND_FOLDER);
}

TEST_F(ConflictResolverTest, ResolveMultiParents_File) {
  const GURL kOrigin("chrome-extension://example");
  const std::string sync_root = CreateSyncRoot();
  const std::string app_root = CreateRemoteFolder(sync_root, kOrigin.host());
  InitializeMetadataDatabase();
  RegisterApp(kOrigin.host(), app_root);
  RunSyncerUntilIdle();

  const std::string primary = CreateRemoteFolder(app_root, "primary");
  const std::string file = CreateRemoteFile(primary, "file", "data");
  ASSERT_EQ(google_apis::HTTP_SUCCESS,
            AddFileToFolder(CreateRemoteFolder(app_root, "nonprimary1"), file));
  ASSERT_EQ(google_apis::HTTP_SUCCESS,
            AddFileToFolder(CreateRemoteFolder(app_root, "nonprimary2"), file));
  ASSERT_EQ(google_apis::HTTP_SUCCESS,
            AddFileToFolder(CreateRemoteFolder(app_root, "nonprimary3"), file));

  EXPECT_EQ(SYNC_STATUS_OK, ListChanges());
  RunSyncerUntilIdle();

  EXPECT_EQ(4, CountParents(file));

  EXPECT_EQ(SYNC_STATUS_OK, RunConflictResolver());

  EXPECT_EQ(1, CountParents(file));
}

TEST_F(ConflictResolverTest, ResolveMultiParents_Folder) {
  const GURL kOrigin("chrome-extension://example");
  const std::string sync_root = CreateSyncRoot();
  const std::string app_root = CreateRemoteFolder(sync_root, kOrigin.host());
  InitializeMetadataDatabase();
  RegisterApp(kOrigin.host(), app_root);
  RunSyncerUntilIdle();

  const std::string primary = CreateRemoteFolder(app_root, "primary");
  const std::string file = CreateRemoteFolder(primary, "folder");
  ASSERT_EQ(google_apis::HTTP_SUCCESS,
            AddFileToFolder(CreateRemoteFolder(app_root, "nonprimary1"), file));
  ASSERT_EQ(google_apis::HTTP_SUCCESS,
            AddFileToFolder(CreateRemoteFolder(app_root, "nonprimary2"), file));
  ASSERT_EQ(google_apis::HTTP_SUCCESS,
            AddFileToFolder(CreateRemoteFolder(app_root, "nonprimary3"), file));

  EXPECT_EQ(SYNC_STATUS_OK, ListChanges());
  RunSyncerUntilIdle();

  EXPECT_EQ(4, CountParents(file));

  EXPECT_EQ(SYNC_STATUS_OK, RunConflictResolver());

  EXPECT_EQ(1, CountParents(file));
}

}  // namespace drive_backend
}  // namespace sync_file_system
