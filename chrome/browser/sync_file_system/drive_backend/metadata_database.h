// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_METADATA_DATABASE_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_METADATA_DATABASE_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/sync_file_system/drive_backend/tracker_set.h"
#include "chrome/browser/sync_file_system/sync_callbacks.h"
#include "chrome/browser/sync_file_system/sync_status_code.h"

namespace base {
class FilePath;
class SequencedTaskRunner;
class SingleThreadTaskRunner;
}

namespace leveldb {
class DB;
class WriteBatch;
}

namespace google_apis {
class ChangeResource;
class FileResource;
class ResourceEntry;
}

namespace tracked_objects {
class Location;
}

namespace sync_file_system {
namespace drive_backend {

class FileDetails;
class FileMetadata;
class FileTracker;
class ServiceMetadata;
struct DatabaseContents;

// MetadataDatabase holds and maintains a LevelDB instance and its indexes,
// which holds 1)ServiceMetadata, 2)FileMetadata and 3)FileTracker.
// 1) ServiceMetadata is a singleton in the database which holds information for
//    the backend.
// 2) FileMetadata represents a remote-side file and holds latest known
//    metadata of the remote file.
// 3) FileTracker represents a synced or to-be-synced file and maintains
//    the local-side folder tree.
//
// The term "file" includes files, folders and other resources on Drive.
//
// FileTrackers form a tree structure on the database, which represents the
// FileSystem trees of SyncFileSystem.  The tree has a FileTracker named
// sync-root as its root node, and a set of FileTracker named app-root.  An
// app-root represents a remote folder for an installed Chrome App and holds all
// synced contents for the App.
//
// One FileMetadata is created for each tracked remote file, which is identified
// by FileID.
// One FileTracker is created for every different {parent tracker, FileID} pair,
// excluding non-app-root inactive parent trackers. Multiple trackers may be
// associated to one FileID when the file has multiple parents. Multiple
// trackers may have the same {parent tracker, title} pair when the associated
// remote files have the same title.
//
// Files have following state:
//   - Unknown file
//     - Has a dirty inactive tracker and empty synced_details.
//     - Is initial state of a tracker, only file_id and parent_tracker_id field
//       are known.
//   - Folder
//     - Is either one of sync-root folder, app-root folder or a regular folder.
//     - Sync-root folder holds app-root folders as its direct children, and
//       holds entire SyncFileSystem files as its descentants.  Its tracker
//       should be stored in ServiceMetadata by its tracker_id.
//     - App-root folder holds all files for an application as its descendants.
//   - File
//   - Unsupported file
//     - Represents unsupported files such as hosted documents. Must be
//       inactive.
//
// Invariants:
//   - Any tracker in the database must either:
//     - be sync-root,
//     - have an app-root as its parent tracker, or
//     - have an active tracker as its parent.
//   That is, all trackers must be reachable from sync-root via app-root folders
//   and active trackers.
//
//   - Any active tracker must either:
//     - have |needs_folder_listing| flag and dirty flag, or
//     - have all children at the stored largest change ID.
//
//   - If multiple trackers have the same parent tracker and same title, they
//     must not have same |file_id|, and at most one of them may be active.
//   - If multiple trackers have the same |file_id|, at most one of them may be
//     active.
//
class MetadataDatabase {
 public:
  typedef std::map<std::string, FileMetadata*> FileByID;
  typedef std::map<int64, FileTracker*> TrackerByID;
  typedef std::map<std::string, TrackerSet> TrackersByFileID;
  typedef std::map<std::string, TrackerSet> TrackersByTitle;
  typedef std::map<int64, TrackersByTitle> TrackersByParentAndTitle;
  typedef std::map<std::string, FileTracker*> TrackerByAppID;
  typedef std::vector<std::string> FileIDList;

  typedef base::Callback<
      void(SyncStatusCode status, scoped_ptr<MetadataDatabase> instance)>
      CreateCallback;

  // The entry point of the MetadataDatabase for production code.
  static void Create(base::SequencedTaskRunner* task_runner,
                     const base::FilePath& database_path,
                     const CreateCallback& callback);
  ~MetadataDatabase();

  static void ClearDatabase(scoped_ptr<MetadataDatabase> metadata_database);

  int64 GetLargestFetchedChangeID() const;
  int64 GetSyncRootTrackerID() const;
  bool HasSyncRoot() const;

  // Returns all file metadata for the given |app_id|.
  scoped_ptr<base::ListValue> DumpFiles(const std::string& app_id);

  // Returns all database data.
  scoped_ptr<base::ListValue> DumpDatabase();

  // TODO(tzik): Move GetLargestKnownChangeID() to private section, and hide its
  // handling in the class, instead of letting user do.
  //
  // Gets / updates the largest known change ID.
  // The largest known change ID is on-memory and not persist over restart.
  // This is supposed to use when a task fetches ChangeList in parallel to other
  // operation.  When a task starts fetching paged ChangeList one by one, it
  // should update the largest known change ID on the first round and background
  // remaining fetch job.
  // Then, when other tasks that update FileMetadata by UpdateByFileResource,
  // it should use largest known change ID as the |change_id| that prevents
  // FileMetadata from overwritten by ChangeList.
  // Also if other tasks try to update a remote resource whose change is not yet
  // retrieved the task should fail due to etag check, so we should be fine.
  int64 GetLargestKnownChangeID() const;
  void UpdateLargestKnownChangeID(int64 change_id);

  // Populates empty database with initial data.
  // Adds a file metadata and a file tracker for |sync_root_folder|, and adds
  // file metadata and file trackers for each |app_root_folders|.
  // Newly added tracker for |sync_root_folder| is active and non-dirty.
  // Newly added trackers for |app_root_folders| are inactive and non-dirty.
  // Trackers for |app_root_folders| are not yet registered as app-roots, but
  // are ready to register.
  void PopulateInitialData(
      int64 largest_change_id,
      const google_apis::FileResource& sync_root_folder,
      const ScopedVector<google_apis::FileResource>& app_root_folders,
      const SyncStatusCallback& callback);

  // Returns true if the folder associated to |app_id| is enabled.
  bool IsAppEnabled(const std::string& app_id) const;

  // Registers existing folder as the app-root for |app_id|.  The folder
  // must be an inactive folder that does not yet associated to any App.
  // This method associates the folder with |app_id| and activates it.
  void RegisterApp(const std::string& app_id,
                   const std::string& folder_id,
                   const SyncStatusCallback& callback);

  // Inactivates the folder associated to the app to disable |app_id|.
  // Does nothing if |app_id| is already disabled.
  void DisableApp(const std::string& app_id,
                  const SyncStatusCallback& callback);

  // Activates the folder associated to |app_id| to enable |app_id|.
  // Does nothing if |app_id| is already enabled.
  void EnableApp(const std::string& app_id,
                 const SyncStatusCallback& callback);

  // Unregisters the folder as the app-root for |app_id|.  If |app_id| does not
  // exist, does nothing.  The folder is left as an inactive regular folder.
  // Note that the inactivation drops all descendant files since they are no
  // longer reachable from sync-root via active folder or app-root.
  void UnregisterApp(const std::string& app_id,
                     const SyncStatusCallback& callback);

  // Finds the app-root folder for |app_id|.  Returns true if exists.
  // Copies the result to |tracker| if it is non-NULL.
  bool FindAppRootTracker(const std::string& app_id,
                          FileTracker* tracker) const;

  // Finds the file identified by |file_id|.  Returns true if the file is found.
  // Copies the metadata identified by |file_id| into |file| if exists and
  // |file| is non-NULL.
  bool FindFileByFileID(const std::string& file_id, FileMetadata* file) const;

  // Finds the tracker identified by |tracker_id|.  Returns true if the tracker
  // is found.
  // Copies the tracker identified by |tracker_id| into |tracker| if exists and
  // |tracker| is non-NULL.
  bool FindTrackerByTrackerID(int64 tracker_id, FileTracker* tracker) const;

  // Finds the trackers tracking |file_id|.  Returns true if the trackers are
  // found.
  bool FindTrackersByFileID(const std::string& file_id,
                            TrackerSet* trackers) const;

  // Finds the set of trackers whose parent's tracker ID is |parent_tracker_id|,
  // and who has |title| as its title in the synced_details.
  // Copies the tracker set to |trackers| if it is non-NULL.
  // Returns true if the trackers are found.
  bool FindTrackersByParentAndTitle(
      int64 parent_tracker_id,
      const std::string& title,
      TrackerSet* trackers) const;

  // Builds the file path for the given tracker.  Returns true on success.
  // |path| can be NULL.
  // The file path is relative to app-root and have a leading path separator.
  bool BuildPathForTracker(int64 tracker_id, base::FilePath* path) const;

  // Builds the file path for the given tracker for display purpose.
  // This may return a path ending with '<unknown>' if the given tracker does
  // not have title information (yet). This may return an empty path.
  base::FilePath BuildDisplayPathForTracker(const FileTracker& tracker) const;

  // Returns false if no registered app exists associated to |app_id|.
  // If |full_path| is active, assigns the tracker of |full_path| to |tracker|.
  // Otherwise, assigns the nearest active ancestor to |full_path| to |tracker|.
  // Also, assigns the full path of |tracker| to |path|.
  bool FindNearestActiveAncestor(const std::string& app_id,
                                 const base::FilePath& full_path,
                                 FileTracker* tracker,
                                 base::FilePath* path) const;

  // Updates database by |changes|.
  // Marks each tracker for modified file as dirty and adds new trackers if
  // needed.
  void UpdateByChangeList(int64 largest_change_id,
                          ScopedVector<google_apis::ChangeResource> changes,
                          const SyncStatusCallback& callback);

  // Updates database by |resource|.
  // Marks each tracker for modified file as dirty and adds new trackers if
  // needed.
  void UpdateByFileResource(const google_apis::FileResource& resource,
                            const SyncStatusCallback& callback);
  void UpdateByFileResourceList(
      ScopedVector<google_apis::FileResource> resources,
      const SyncStatusCallback& callback);

  void UpdateByDeletedRemoteFile(const std::string& file_id,
                                 const SyncStatusCallback& callback);

  // TODO(tzik): Drop |change_id| parameter.
  // Adds new FileTracker and FileMetadata.  The database must not have
  // |resource| beforehand.
  // The newly added tracker under |parent_tracker_id| is active and non-dirty.
  // Deactivates existing active tracker if exists that has the same title and
  // parent_tracker to the newly added tracker.
  void ReplaceActiveTrackerWithNewResource(
      int64 parent_tracker_id,
      const google_apis::FileResource& resource,
      const SyncStatusCallback& callback);

  // Adds |child_file_ids| to |folder_id| as its children.
  // This method affects the active tracker only.
  // If the tracker has no further change to sync, unmarks its dirty flag.
  void PopulateFolderByChildList(const std::string& folder_id,
                                 const FileIDList& child_file_ids,
                                 const SyncStatusCallback& callback);

  // Updates |synced_details| of the tracker with |updated_details|.
  void UpdateTracker(int64 tracker_id,
                     const FileDetails& updated_details,
                     const SyncStatusCallback& callback);

  // Returns true if a tracker of the file can be safely activated without
  // deactivating any other trackers.  In this case, tries to activate the
  // tracker, and invokes |callback| upon completion.
  // Returns false otherwise.  In false case, |callback| will not be invoked.
  bool TryNoSideEffectActivation(int64 parent_tracker_id,
                                 const std::string& file_id,
                                 const SyncStatusCallback& callback);


  // Changes the priority of the tracker to low.
  void LowerTrackerPriority(int64 tracker_id);
  void PromoteLowerPriorityTrackersToNormal();

  // Returns true if there is a normal priority dirty tracker.
  // Assigns the dirty tracker if exists and |tracker| is non-NULL.
  bool GetNormalPriorityDirtyTracker(FileTracker* tracker) const;

  // Returns true if there is a low priority dirty tracker.
  // Assigns the dirty tracker if exists and |tracker| is non-NULL.
  bool GetLowPriorityDirtyTracker(FileTracker* tracker) const;

  bool HasDirtyTracker() const {
    return !dirty_trackers_.empty() || !low_priority_dirty_trackers_.empty();
  }

  size_t GetDirtyTrackerCount() {
    return dirty_trackers_.size();
  }

  bool GetMultiParentFileTrackers(std::string* file_id,
                                  TrackerSet* trackers);
  bool GetConflictingTrackers(TrackerSet* trackers);

  // Sets |app_ids| to a list of all registered app ids.
  void GetRegisteredAppIDs(std::vector<std::string>* app_ids);

 private:
  friend class ListChangesTaskTest;
  friend class MetadataDatabaseTest;
  friend class RegisterAppTaskTest;
  friend class SyncEngineInitializerTest;

  struct DirtyTrackerComparator {
    bool operator()(const FileTracker* left,
                    const FileTracker* right) const;
  };

  typedef std::set<FileTracker*, DirtyTrackerComparator> DirtyTrackers;

  MetadataDatabase(base::SequencedTaskRunner* task_runner,
                   const base::FilePath& database_path);
  static void CreateOnTaskRunner(base::SingleThreadTaskRunner* callback_runner,
                                 base::SequencedTaskRunner* task_runner,
                                 const base::FilePath& database_path,
                                 const CreateCallback& callback);
  static SyncStatusCode CreateForTesting(
      scoped_ptr<leveldb::DB> db,
      scoped_ptr<MetadataDatabase>* metadata_database_out);
  SyncStatusCode InitializeOnTaskRunner();
  void BuildIndexes(DatabaseContents* contents);

  // Database manipulation methods.
  void RegisterTrackerAsAppRoot(const std::string& app_id,
                                int64 tracker_id,
                                leveldb::WriteBatch* batch);
  void MakeTrackerActive(int64 tracker_id, leveldb::WriteBatch* batch);
  void MakeTrackerInactive(int64 tracker_id, leveldb::WriteBatch* batch);
  void MakeAppRootDisabled(int64 tracker_id, leveldb::WriteBatch* batch);
  void MakeAppRootEnabled(int64 tracker_id, leveldb::WriteBatch* batch);

  void UnregisterTrackerAsAppRoot(const std::string& app_id,
                                  leveldb::WriteBatch* batch);
  void RemoveAllDescendantTrackers(int64 root_tracker_id,
                                   leveldb::WriteBatch* batch);

  void CreateTrackerForParentAndFileID(const FileTracker& parent_tracker,
                                       const std::string& file_id,
                                       leveldb::WriteBatch* batch);
  void CreateTrackerForParentAndFileMetadata(const FileTracker& parent_tracker,
                                             const FileMetadata& file_metadata,
                                             leveldb::WriteBatch* batch);
  void CreateTrackerInternal(const FileTracker& parent_tracker,
                             const std::string& file_id,
                             const FileDetails* details,
                             leveldb::WriteBatch* batch);

  void RemoveTracker(int64 tracker_id, leveldb::WriteBatch* batch);
  void RemoveTrackerIgnoringSameTitle(int64 tracker_id,
                                      leveldb::WriteBatch* batch);
  void RemoveTrackerInternal(int64 tracker_id,
                             leveldb::WriteBatch* batch,
                             bool ignoring_same_title);
  void MaybeAddTrackersForNewFile(const FileMetadata& file,
                                  leveldb::WriteBatch* batch);

  void MarkSingleTrackerDirty(FileTracker* tracker,
                              leveldb::WriteBatch* batch);
  void MarkTrackerSetDirty(TrackerSet* trackers,
                           leveldb::WriteBatch* batch);
  void MarkTrackersDirtyByFileID(const std::string& file_id,
                                 leveldb::WriteBatch* batch);
  void MarkTrackersDirtyByPath(int64 parent_tracker_id,
                               const std::string& title,
                               leveldb::WriteBatch* batch);

  void EraseTrackerFromFileIDIndex(FileTracker* tracker,
                                   leveldb::WriteBatch* batch);
  void EraseTrackerFromPathIndex(FileTracker* tracker);
  void EraseFileFromDatabase(const std::string& file_id,
                             leveldb::WriteBatch* batch);

  int64 GetNextTrackerID(leveldb::WriteBatch* batch);

  void RecursiveMarkTrackerAsDirty(int64 root_tracker_id,
                                   leveldb::WriteBatch* batch);
  bool CanActivateTracker(const FileTracker& tracker);
  bool ShouldKeepDirty(const FileTracker& tracker) const;

  bool HasDisabledAppRoot(const FileTracker& tracker) const;
  bool HasActiveTrackerForFileID(const std::string& file_id) const;
  bool HasActiveTrackerForPath(int64 parent_tracker,
                               const std::string& title) const;

  void UpdateByFileMetadata(const tracked_objects::Location& from_where,
                            scoped_ptr<FileMetadata> file,
                            leveldb::WriteBatch* batch);

  void WriteToDatabase(scoped_ptr<leveldb::WriteBatch> batch,
                       const SyncStatusCallback& callback);

  bool HasNewerFileMetadata(const std::string& file_id, int64 change_id);

  scoped_ptr<base::ListValue> DumpTrackers();
  scoped_ptr<base::ListValue> DumpMetadata();

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  base::FilePath database_path_;
  scoped_ptr<leveldb::DB> db_;

  scoped_ptr<ServiceMetadata> service_metadata_;
  int64 largest_known_change_id_;

  FileByID file_by_id_;  // Owned.
  TrackerByID tracker_by_id_;  // Owned.

  // Maps FileID to trackers.  The active tracker must be unique per FileID.
  // This must be updated when updating |active| field of a tracker.
  TrackersByFileID trackers_by_file_id_;  // Not owned.

  // Maps AppID to the app-root tracker.
  // This must be updated when a tracker is registered/unregistered as an
  // app-root.
  TrackerByAppID app_root_by_app_id_;  // Not owned.

  // Maps |tracker_id| to its children grouped by their |title|.
  // If the title is unknown for a tracker, treats its title as empty. Empty
  // titled file must not be active.
  // The active tracker must be unique per its parent_tracker and its title.
  // This must be updated when updating |title|, |active| or
  // |parent_tracker_id|.
  TrackersByParentAndTitle trackers_by_parent_and_title_;

  // Holds all trackers which marked as dirty.
  // This must be updated when updating |dirty| field of a tracker.
  DirtyTrackers dirty_trackers_;  // Not owned.
  DirtyTrackers low_priority_dirty_trackers_;  // Not owned.

  base::WeakPtrFactory<MetadataDatabase> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(MetadataDatabase);
};

}  // namespace drive_backend
}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_METADATA_DATABASE_H_
