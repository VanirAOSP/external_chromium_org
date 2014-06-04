// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_manager.h"

#include <set>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/debug/trace_event.h"
#include "base/deferred_sequenced_task_runner.h"
#include "base/file_util.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/scoped_user_pref_update.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/bookmark_model_loaded_observer.h"
#include "chrome/browser/profiles/profile_destroyer.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/profiles/startup_task_runner_service.h"
#include "chrome/browser/profiles/startup_task_runner_service_factory.h"
#include "chrome/browser/signin/account_reconcilor_factory.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_iterator.h"
#include "chrome/browser/ui/sync/sync_promo_ui.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths_internal.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/logging_chrome.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/user_metrics.h"
#include "grit/generated_resources.h"
#include "net/http/http_transaction_factory.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_job.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(ENABLE_MANAGED_USERS)
#include "chrome/browser/managed_mode/managed_user_service.h"
#include "chrome/browser/managed_mode/managed_user_service_factory.h"
#endif

#if !defined(OS_IOS)
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/sessions/session_service_factory.h"
#include "chrome/browser/ui/browser_list.h"
#endif  // !defined (OS_IOS)

#if defined(OS_WIN)
#include "base/win/metro.h"
#include "chrome/installer/util/browser_distribution.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/browser_process_platform_part_chromeos.h"
#include "chrome/browser/chromeos/login/user.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/dbus/cryptohome_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#endif

using content::BrowserThread;
using content::UserMetricsAction;

namespace {

// Profiles that should be deleted on shutdown.
std::vector<base::FilePath>& ProfilesToDelete() {
  CR_DEFINE_STATIC_LOCAL(std::vector<base::FilePath>, profiles_to_delete, ());
  return profiles_to_delete;
}

int64 ComputeFilesSize(const base::FilePath& directory,
                       const base::FilePath::StringType& pattern) {
  int64 running_size = 0;
  base::FileEnumerator iter(directory, false, base::FileEnumerator::FILES,
                            pattern);
  while (!iter.Next().empty())
    running_size += iter.GetInfo().GetSize();
  return running_size;
}

// Simple task to log the size of the current profile.
void ProfileSizeTask(const base::FilePath& path, int extension_count) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  const int64 kBytesInOneMB = 1024 * 1024;

  int64 size = ComputeFilesSize(path, FILE_PATH_LITERAL("*"));
  int size_MB = static_cast<int>(size / kBytesInOneMB);
  UMA_HISTOGRAM_COUNTS_10000("Profile.TotalSize", size_MB);

  size = ComputeFilesSize(path, FILE_PATH_LITERAL("History"));
  size_MB = static_cast<int>(size / kBytesInOneMB);
  UMA_HISTOGRAM_COUNTS_10000("Profile.HistorySize", size_MB);

  size = ComputeFilesSize(path, FILE_PATH_LITERAL("History*"));
  size_MB = static_cast<int>(size / kBytesInOneMB);
  UMA_HISTOGRAM_COUNTS_10000("Profile.TotalHistorySize", size_MB);

  size = ComputeFilesSize(path, FILE_PATH_LITERAL("Cookies"));
  size_MB = static_cast<int>(size / kBytesInOneMB);
  UMA_HISTOGRAM_COUNTS_10000("Profile.CookiesSize", size_MB);

  size = ComputeFilesSize(path, FILE_PATH_LITERAL("Bookmarks"));
  size_MB = static_cast<int>(size / kBytesInOneMB);
  UMA_HISTOGRAM_COUNTS_10000("Profile.BookmarksSize", size_MB);

  size = ComputeFilesSize(path, FILE_PATH_LITERAL("Favicons"));
  size_MB = static_cast<int>(size / kBytesInOneMB);
  UMA_HISTOGRAM_COUNTS_10000("Profile.FaviconsSize", size_MB);

  size = ComputeFilesSize(path, FILE_PATH_LITERAL("Top Sites"));
  size_MB = static_cast<int>(size / kBytesInOneMB);
  UMA_HISTOGRAM_COUNTS_10000("Profile.TopSitesSize", size_MB);

  size = ComputeFilesSize(path, FILE_PATH_LITERAL("Visited Links"));
  size_MB = static_cast<int>(size / kBytesInOneMB);
  UMA_HISTOGRAM_COUNTS_10000("Profile.VisitedLinksSize", size_MB);

  size = ComputeFilesSize(path, FILE_PATH_LITERAL("Web Data"));
  size_MB = static_cast<int>(size / kBytesInOneMB);
  UMA_HISTOGRAM_COUNTS_10000("Profile.WebDataSize", size_MB);

  size = ComputeFilesSize(path, FILE_PATH_LITERAL("Extension*"));
  size_MB = static_cast<int>(size / kBytesInOneMB);
  UMA_HISTOGRAM_COUNTS_10000("Profile.ExtensionSize", size_MB);

  size = ComputeFilesSize(path, FILE_PATH_LITERAL("Policy"));
  size_MB = static_cast<int>(size / kBytesInOneMB);
  UMA_HISTOGRAM_COUNTS_10000("Profile.PolicySize", size_MB);

  // Count number of extensions in this profile, if we know.
  if (extension_count != -1)
    UMA_HISTOGRAM_COUNTS_10000("Profile.AppCount", extension_count);
}

void QueueProfileDirectoryForDeletion(const base::FilePath& path) {
  ProfilesToDelete().push_back(path);
}

bool IsProfileMarkedForDeletion(const base::FilePath& profile_path) {
  return std::find(ProfilesToDelete().begin(), ProfilesToDelete().end(),
      profile_path) != ProfilesToDelete().end();
}

// Physically remove deleted profile directories from disk.
void NukeProfileFromDisk(const base::FilePath& profile_path) {
  // Delete both the profile directory and its corresponding cache.
  base::FilePath cache_path;
  chrome::GetUserCacheDirectory(profile_path, &cache_path);
  base::DeleteFile(profile_path, true);
  base::DeleteFile(cache_path, true);
}

#if defined(OS_CHROMEOS)
void CheckCryptohomeIsMounted(chromeos::DBusMethodCallStatus call_status,
                              bool is_mounted) {
  if (call_status != chromeos::DBUS_METHOD_CALL_SUCCESS) {
    LOG(ERROR) << "IsMounted call failed.";
    return;
  }
  if (!is_mounted)
    LOG(ERROR) << "Cryptohome is not mounted.";
}

#endif

} // namespace

#if defined(ENABLE_SESSION_SERVICE)
// static
void ProfileManager::ShutdownSessionServices() {
  ProfileManager* pm = g_browser_process->profile_manager();
  if (!pm)  // Is NULL when running unit tests.
    return;
  std::vector<Profile*> profiles(pm->GetLoadedProfiles());
  for (size_t i = 0; i < profiles.size(); ++i)
    SessionServiceFactory::ShutdownForProfile(profiles[i]);
}
#endif

// static
void ProfileManager::NukeDeletedProfilesFromDisk() {
  for (std::vector<base::FilePath>::iterator it =
          ProfilesToDelete().begin();
       it != ProfilesToDelete().end();
       ++it) {
    NukeProfileFromDisk(*it);
  }
  ProfilesToDelete().clear();
}

namespace {

bool s_allow_get_default_profile = false;

}  // namespace

// static
void ProfileManager::AllowGetDefaultProfile() {
  s_allow_get_default_profile = true;
}

// static
bool ProfileManager::IsGetDefaultProfileAllowed() {
  return s_allow_get_default_profile;
}

// static
// TODO(skuhne): Remove this method once all clients are migrated.
Profile* ProfileManager::GetDefaultProfile() {
  CHECK(s_allow_get_default_profile)
      << "GetDefaultProfile() called before allowed.";
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  return profile_manager->GetDefaultProfile(profile_manager->user_data_dir_);
}

// static
// TODO(skuhne): Remove this method once all clients are migrated.
Profile* ProfileManager::GetDefaultProfileOrOffTheRecord() {
  return GetDefaultProfile();
}

// static
Profile* ProfileManager::GetLastUsedProfile() {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  return profile_manager->GetLastUsedProfile(profile_manager->user_data_dir_);
}

// static
Profile* ProfileManager::GetLastUsedProfileAllowedByPolicy() {
  Profile* profile = GetLastUsedProfile();
  if (IncognitoModePrefs::GetAvailability(profile->GetPrefs()) ==
      IncognitoModePrefs::FORCED) {
    return profile->GetOffTheRecordProfile();
  }
  return profile;
}

// static
std::vector<Profile*> ProfileManager::GetLastOpenedProfiles() {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  return profile_manager->GetLastOpenedProfiles(
      profile_manager->user_data_dir_);
}

ProfileManager::ProfileManager(const base::FilePath& user_data_dir)
    : user_data_dir_(user_data_dir),
      logged_in_(false),

#if !defined(OS_ANDROID) && !defined(OS_IOS)
      browser_list_observer_(this),
#endif
      closing_all_browsers_(false) {
#if defined(OS_CHROMEOS)
  registrar_.Add(
      this,
      chrome::NOTIFICATION_LOGIN_USER_CHANGED,
      content::NotificationService::AllSources());
#endif
  registrar_.Add(
      this,
      chrome::NOTIFICATION_BROWSER_OPENED,
      content::NotificationService::AllSources());
  registrar_.Add(
      this,
      chrome::NOTIFICATION_BROWSER_CLOSED,
      content::NotificationService::AllSources());
  registrar_.Add(
      this,
      chrome::NOTIFICATION_CLOSE_ALL_BROWSERS_REQUEST,
      content::NotificationService::AllSources());
  registrar_.Add(
      this,
      chrome::NOTIFICATION_BROWSER_CLOSE_CANCELLED,
      content::NotificationService::AllSources());
  registrar_.Add(
      this,
      chrome::NOTIFICATION_PROFILE_CACHED_INFO_CHANGED,
      content::NotificationService::AllSources());

  if (ProfileShortcutManager::IsFeatureEnabled() && !user_data_dir_.empty())
    profile_shortcut_manager_.reset(ProfileShortcutManager::Create(
                                    this));
}

ProfileManager::~ProfileManager() {
}

base::FilePath ProfileManager::GetInitialProfileDir() {
  base::FilePath relative_profile_dir;
#if defined(OS_CHROMEOS)
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  if (logged_in_) {
    base::FilePath profile_dir;
    // If the user has logged in, pick up the new profile.
    if (command_line.HasSwitch(chromeos::switches::kLoginProfile)) {
      // TODO(nkostylev): Remove this code completely once we eliminate
      // legacy --login-profile=user switch and enable multi-profiles on CrOS
      // by default. http://crbug.com/294628
      profile_dir = chromeos::ProfileHelper::
          GetProfileDirByLegacyLoginProfileSwitch();
    } else if (!command_line.HasSwitch(switches::kMultiProfiles)) {
      // We should never be logged in with no profile dir unless
      // multi-profiles are enabled.
      // In that case profile dir will be defined by user_id hash.
      NOTREACHED();
      return base::FilePath("");
    }
    // In case of multi-profiles ignore --login-profile switch.
    // TODO(nkostylev): Some cases like Guest mode will have empty username_hash
    // so default kLoginProfile dir will be used.
    std::string user_id_hash = g_browser_process->platform_part()->
        profile_helper()->active_user_id_hash();
    if (command_line.HasSwitch(switches::kMultiProfiles) &&
        !user_id_hash.empty()) {
      profile_dir = g_browser_process->platform_part()->
          profile_helper()->GetActiveUserProfileDir();
    }
    relative_profile_dir = relative_profile_dir.Append(profile_dir);
    return relative_profile_dir;
  }
#endif
  // TODO(mirandac): should not automatically be default profile.
  relative_profile_dir =
      relative_profile_dir.AppendASCII(chrome::kInitialProfile);
  return relative_profile_dir;
}

Profile* ProfileManager::GetLastUsedProfile(
    const base::FilePath& user_data_dir) {
#if defined(OS_CHROMEOS)
  // Use default login profile if user has not logged in yet.
  if (!logged_in_) {
    return GetDefaultProfile(user_data_dir);
  } else {
    // CrOS multi-profiles implementation is different so GetLastUsedProfile
    // has custom implementation too.
    base::FilePath profile_dir;
    const CommandLine& command_line = *CommandLine::ForCurrentProcess();
    if (command_line.HasSwitch(switches::kMultiProfiles)) {
      // In case of multi-profiles we ignore "last used profile" preference
      // since it may refer to profile that has been in use in previous session.
      // That profile dir may not be mounted in this session so instead return
      // active profile from current session.
      profile_dir = g_browser_process->platform_part()->
          profile_helper()->GetActiveUserProfileDir();
    } else {
      // For legacy (not multi-profiles) implementation always default to
      // --login-profile value.
      profile_dir =
          chromeos::ProfileHelper::GetProfileDirByLegacyLoginProfileSwitch();
    }

    base::FilePath profile_path(user_data_dir);
    Profile* profile = GetProfile(profile_path.Append(profile_dir));
    return (profile && profile->IsGuestSession()) ?
        profile->GetOffTheRecordProfile() : profile;
  }
#endif

  return GetProfile(GetLastUsedProfileDir(user_data_dir));
}

base::FilePath ProfileManager::GetLastUsedProfileDir(
    const base::FilePath& user_data_dir) {
  base::FilePath last_used_profile_dir(user_data_dir);
  PrefService* local_state = g_browser_process->local_state();
  DCHECK(local_state);

  if (local_state->HasPrefPath(prefs::kProfileLastUsed)) {
    return last_used_profile_dir.AppendASCII(
        local_state->GetString(prefs::kProfileLastUsed));
  }

  return last_used_profile_dir.AppendASCII(chrome::kInitialProfile);
}

std::vector<Profile*> ProfileManager::GetLastOpenedProfiles(
    const base::FilePath& user_data_dir) {
  PrefService* local_state = g_browser_process->local_state();
  DCHECK(local_state);

  std::vector<Profile*> to_return;
  if (local_state->HasPrefPath(prefs::kProfilesLastActive) &&
      local_state->GetList(prefs::kProfilesLastActive)) {
    // Make a copy because the list might change in the calls to GetProfile.
    scoped_ptr<base::ListValue> profile_list(
        local_state->GetList(prefs::kProfilesLastActive)->DeepCopy());
    base::ListValue::const_iterator it;
    std::string profile;
    for (it = profile_list->begin(); it != profile_list->end(); ++it) {
      if (!(*it)->GetAsString(&profile) || profile.empty()) {
        LOG(WARNING) << "Invalid entry in " << prefs::kProfilesLastActive;
        continue;
      }
      to_return.push_back(GetProfile(user_data_dir.AppendASCII(profile)));
    }
  }
  return to_return;
}

Profile* ProfileManager::GetPrimaryUserProfile() {
#if defined(OS_CHROMEOS)
  // TODO(skuhne): Remove once GetDefaultProfile is removed.
  CHECK(s_allow_get_default_profile)
      << "GetPrimaryUserProfile() called before allowed.";
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  if (!profile_manager->IsLoggedIn() || !chromeos::UserManager::IsInitialized())
    return GetDefaultProfile();
  chromeos::UserManager* manager = chromeos::UserManager::Get();
  // Note: The user manager will take care of guest profiles.
  return manager->GetProfileByUser(manager->GetPrimaryUser());
#else
  return GetDefaultProfile();
#endif
}

Profile* ProfileManager::GetActiveUserProfile() {
#if defined(OS_CHROMEOS)
  // TODO(skuhne): Remove once GetDefaultProfile is removed.
  CHECK(s_allow_get_default_profile)
      << "GetActiveUserProfile() called before allowed.";
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  if (!profile_manager->IsLoggedIn() || !chromeos::UserManager::IsInitialized())
    return GetDefaultProfile();
  chromeos::UserManager* manager = chromeos::UserManager::Get();
  // Note: The user manager will take care of guest profiles.
  return manager->GetProfileByUser(manager->GetActiveUser());
#else
  return GetDefaultProfile();
#endif
}

// TODO(skuhne): Remove this method once all clients are migrated.
Profile* ProfileManager::GetPrimaryUserProfileOrOffTheRecord() {
  return GetPrimaryUserProfile();
}

// TODO(skuhne): Remove this method once all clients are migrated.
Profile* ProfileManager::GetActiveUserProfileOrOffTheRecord() {
  return GetActiveUserProfile();
}

Profile* ProfileManager::GetDefaultProfile(
    const base::FilePath& user_data_dir) {
#if defined(OS_CHROMEOS)
  base::FilePath default_profile_dir(user_data_dir);
  if (logged_in_) {
    default_profile_dir = default_profile_dir.Append(GetInitialProfileDir());
  } else {
    default_profile_dir = profiles::GetDefaultProfileDir(user_data_dir);
  }
#else
  base::FilePath default_profile_dir(user_data_dir);
  default_profile_dir = default_profile_dir.Append(GetInitialProfileDir());
#endif
#if defined(OS_CHROMEOS)
  if (!logged_in_) {
    Profile* profile = GetProfile(default_profile_dir);
    // For cros, return the OTR profile so we never accidentally keep
    // user data in an unencrypted profile. But doing this makes
    // many of the browser and ui tests fail. We do return the OTR profile
    // if the login-profile switch is passed so that we can test this.
    if (ShouldGoOffTheRecord(profile))
      return profile->GetOffTheRecordProfile();
    DCHECK(!chromeos::UserManager::Get()->IsLoggedInAsGuest());
    return profile;
  }

  ProfileInfo* profile_info = GetProfileInfoByPath(default_profile_dir);
  // Fallback to default off-the-record profile, if user profile has not fully
  // loaded yet.
  if (profile_info && !profile_info->created)
    default_profile_dir = profiles::GetDefaultProfileDir(user_data_dir);

  Profile* profile = GetProfile(default_profile_dir);
  // Some unit tests didn't initialize the UserManager.
  if (chromeos::UserManager::IsInitialized() &&
      chromeos::UserManager::Get()->IsLoggedInAsGuest())
    return profile->GetOffTheRecordProfile();
  return profile;
#else
  return GetProfile(default_profile_dir);
#endif
}

bool ProfileManager::IsValidProfile(Profile* profile) {
  for (ProfilesInfoMap::iterator iter = profiles_info_.begin();
       iter != profiles_info_.end(); ++iter) {
    if (iter->second->created) {
      Profile* candidate = iter->second->profile.get();
      if (candidate == profile ||
          (candidate->HasOffTheRecordProfile() &&
           candidate->GetOffTheRecordProfile() == profile)) {
        return true;
      }
    }
  }
  return false;
}

std::vector<Profile*> ProfileManager::GetLoadedProfiles() const {
  std::vector<Profile*> profiles;
  for (ProfilesInfoMap::const_iterator iter = profiles_info_.begin();
       iter != profiles_info_.end(); ++iter) {
    if (iter->second->created)
      profiles.push_back(iter->second->profile.get());
  }
  return profiles;
}

Profile* ProfileManager::GetProfile(const base::FilePath& profile_dir) {
  TRACE_EVENT0("browser", "ProfileManager::GetProfile")
  // If the profile is already loaded (e.g., chrome.exe launched twice), just
  // return it.
  Profile* profile = GetProfileByPath(profile_dir);
  if (NULL != profile)
    return profile;

  profile = CreateProfileHelper(profile_dir);
  DCHECK(profile);
  if (profile) {
    bool result = AddProfile(profile);
    DCHECK(result);
  }
  return profile;
}

void ProfileManager::CreateProfileAsync(
    const base::FilePath& profile_path,
    const CreateCallback& callback,
    const base::string16& name,
    const base::string16& icon_url,
    const std::string& managed_user_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Make sure that this profile is not pending deletion.
  if (IsProfileMarkedForDeletion(profile_path)) {
    if (!callback.is_null())
      callback.Run(NULL, Profile::CREATE_STATUS_LOCAL_FAIL);
    return;
  }

  // Create the profile if needed and collect its ProfileInfo.
  ProfilesInfoMap::iterator iter = profiles_info_.find(profile_path);
  ProfileInfo* info = NULL;

  if (iter != profiles_info_.end()) {
    info = iter->second.get();
  } else {
    // Initiate asynchronous creation process.
    info = RegisterProfile(CreateProfileAsyncHelper(profile_path, this), false);
    ProfileInfoCache& cache = GetProfileInfoCache();
    // Get the icon index from the user's icon url
    size_t icon_index;
    std::string icon_url_std = UTF16ToASCII(icon_url);
    if (cache.IsDefaultAvatarIconUrl(icon_url_std, &icon_index)) {
      // add profile to cache with user selected name and avatar
      cache.AddProfileToCache(profile_path, name, base::string16(), icon_index,
                              managed_user_id);
    }

    if (!managed_user_id.empty()) {
      content::RecordAction(
          UserMetricsAction("ManagedMode_LocallyManagedUserCreated"));
    }

    ProfileMetrics::UpdateReportedProfilesStatistics(this);
  }

  // Call or enqueue the callback.
  if (!callback.is_null()) {
    if (iter != profiles_info_.end() && info->created) {
      Profile* profile = info->profile.get();
      // If this was the guest profile, apply settings.
      if (profile->GetPath() == ProfileManager::GetGuestProfilePath())
        SetGuestProfilePrefs(profile);
      // Profile has already been created. Run callback immediately.
      callback.Run(profile, Profile::CREATE_STATUS_INITIALIZED);
    } else {
      // Profile is either already in the process of being created, or new.
      // Add callback to the list.
      info->callbacks.push_back(callback);
    }
  }
}

bool ProfileManager::AddProfile(Profile* profile) {
  DCHECK(profile);

  // Make sure that we're not loading a profile with the same ID as a profile
  // that's already loaded.
  if (GetProfileByPath(profile->GetPath())) {
    NOTREACHED() << "Attempted to add profile with the same path (" <<
                    profile->GetPath().value() <<
                    ") as an already-loaded profile.";
    return false;
  }

  RegisterProfile(profile, true);
  InitProfileUserPrefs(profile);
  DoFinalInit(profile, ShouldGoOffTheRecord(profile));
  return true;
}

ProfileManager::ProfileInfo* ProfileManager::RegisterProfile(
    Profile* profile,
    bool created) {
  ProfileInfo* info = new ProfileInfo(profile, created);
  profiles_info_.insert(
      std::make_pair(profile->GetPath(), linked_ptr<ProfileInfo>(info)));
  return info;
}

ProfileManager::ProfileInfo* ProfileManager::GetProfileInfoByPath(
    const base::FilePath& path) const {
  ProfilesInfoMap::const_iterator iter = profiles_info_.find(path);
  return (iter == profiles_info_.end()) ? NULL : iter->second.get();
}

Profile* ProfileManager::GetProfileByPath(const base::FilePath& path) const {
  ProfileInfo* profile_info = GetProfileInfoByPath(path);
  return profile_info ? profile_info->profile.get() : NULL;
}

void ProfileManager::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
#if defined(OS_CHROMEOS)
  if (type == chrome::NOTIFICATION_LOGIN_USER_CHANGED) {
    logged_in_ = true;

    const CommandLine& command_line = *CommandLine::ForCurrentProcess();
    if (!command_line.HasSwitch(switches::kTestType)) {
      // If we don't have a mounted profile directory we're in trouble.
      // TODO(davemoore) Once we have better api this check should ensure that
      // our profile directory is the one that's mounted, and that it's mounted
      // as the current user.
      chromeos::DBusThreadManager::Get()->GetCryptohomeClient()->IsMounted(
          base::Bind(&CheckCryptohomeIsMounted));

      // Confirm that we hadn't loaded the new profile previously.
      base::FilePath default_profile_dir = user_data_dir_.Append(
          GetInitialProfileDir());
      CHECK(!GetProfileByPath(default_profile_dir))
          << "The default profile was loaded before we mounted the cryptohome.";
    }
    return;
  }
#endif
  bool save_active_profiles = false;
  switch (type) {
    case chrome::NOTIFICATION_CLOSE_ALL_BROWSERS_REQUEST: {
      // Ignore any browsers closing from now on.
      closing_all_browsers_ = true;
      save_active_profiles = true;
      break;
    }
    case chrome::NOTIFICATION_BROWSER_CLOSE_CANCELLED: {
      // This will cancel the shutdown process, so the active profiles are
      // tracked again. Also, as the active profiles may have changed (i.e. if
      // some windows were closed) we save the current list of active profiles
      // again.
      closing_all_browsers_ = false;
      save_active_profiles = true;
      break;
    }
    case chrome::NOTIFICATION_BROWSER_OPENED: {
      Browser* browser = content::Source<Browser>(source).ptr();
      DCHECK(browser);
      Profile* profile = browser->profile();
      DCHECK(profile);
      bool is_ephemeral =
          profile->GetPrefs()->GetBoolean(prefs::kForceEphemeralProfiles);
      if (!profile->IsOffTheRecord() && !is_ephemeral &&
          ++browser_counts_[profile] == 1) {
        active_profiles_.push_back(profile);
        save_active_profiles = true;
      }
      // If browsers are opening, we can't be closing all the browsers. This
      // can happen if the application was exited, but background mode or
      // packaged apps prevented the process from shutting down, and then
      // a new browser window was opened.
      closing_all_browsers_ = false;
      break;
    }
    case chrome::NOTIFICATION_BROWSER_CLOSED: {
      Browser* browser = content::Source<Browser>(source).ptr();
      DCHECK(browser);
      Profile* profile = browser->profile();
      DCHECK(profile);
      if (!profile->IsOffTheRecord() && --browser_counts_[profile] == 0) {
        active_profiles_.erase(std::find(active_profiles_.begin(),
                                         active_profiles_.end(), profile));
        save_active_profiles = !closing_all_browsers_;
      }
      break;
    }
    case chrome::NOTIFICATION_PROFILE_CACHED_INFO_CHANGED: {
      save_active_profiles = !closing_all_browsers_;
      break;
    }
    default: {
      NOTREACHED();
      break;
    }
  }

  if (save_active_profiles) {
    PrefService* local_state = g_browser_process->local_state();
    DCHECK(local_state);
    ListPrefUpdate update(local_state, prefs::kProfilesLastActive);
    ListValue* profile_list = update.Get();

    profile_list->Clear();

    // crbug.com/120112 -> several non-incognito profiles might have the same
    // GetPath().BaseName(). In that case, we cannot restore both
    // profiles. Include each base name only once in the last active profile
    // list.
    std::set<std::string> profile_paths;
    std::vector<Profile*>::const_iterator it;
    for (it = active_profiles_.begin(); it != active_profiles_.end(); ++it) {
      std::string profile_path = (*it)->GetPath().BaseName().MaybeAsASCII();
      // Some profiles might become ephemeral after they are created.
      if (!(*it)->GetPrefs()->GetBoolean(prefs::kForceEphemeralProfiles) &&
          profile_paths.find(profile_path) == profile_paths.end()) {
        profile_paths.insert(profile_path);
        profile_list->Append(new StringValue(profile_path));
      }
    }
  }
}

#if !defined(OS_ANDROID) && !defined(OS_IOS)
ProfileManager::BrowserListObserver::BrowserListObserver(
    ProfileManager* manager)
    : profile_manager_(manager) {
  BrowserList::AddObserver(this);
}

ProfileManager::BrowserListObserver::~BrowserListObserver() {
  BrowserList::RemoveObserver(this);
}

void ProfileManager::BrowserListObserver::OnBrowserAdded(
    Browser* browser) {}

void ProfileManager::BrowserListObserver::OnBrowserRemoved(
    Browser* browser) {
  Profile* profile = browser->profile();
  for (chrome::BrowserIterator it; !it.done(); it.Next()) {
    if (it->profile()->GetOriginalProfile() == profile->GetOriginalProfile())
      // Not the last window for this profile.
      return;
  }

  // If the last browser of a profile that is scheduled for deletion is closed
  // do that now.
  base::FilePath path = profile->GetPath();
  if (profile->GetPrefs()->GetBoolean(prefs::kForceEphemeralProfiles) &&
      !IsProfileMarkedForDeletion(path)) {
    g_browser_process->profile_manager()->ScheduleProfileForDeletion(
        path, ProfileManager::CreateCallback());
  }
}

void ProfileManager::BrowserListObserver::OnBrowserSetLastActive(
    Browser* browser) {
  // If all browsers are being closed (e.g. the user is in the process of
  // shutting down), this event will be fired after each browser is
  // closed. This does not represent a user intention to change the active
  // browser so is not handled here.
  if (profile_manager_->closing_all_browsers_)
    return;

  Profile* last_active = browser->profile();

  // Don't remember ephemeral profiles as last because they are not going to
  // persist after restart.
  if (last_active->GetPrefs()->GetBoolean(prefs::kForceEphemeralProfiles))
    return;

  PrefService* local_state = g_browser_process->local_state();
  DCHECK(local_state);
  // Only keep track of profiles that we are managing; tests may create others.
  if (profile_manager_->profiles_info_.find(
      last_active->GetPath()) != profile_manager_->profiles_info_.end()) {
    local_state->SetString(prefs::kProfileLastUsed,
                           last_active->GetPath().BaseName().MaybeAsASCII());
  }
}
#endif  // !defined(OS_ANDROID) && !defined(OS_IOS)

void ProfileManager::DoFinalInit(Profile* profile, bool go_off_the_record) {
  DoFinalInitForServices(profile, go_off_the_record);
  AddProfileToCache(profile);
  DoFinalInitLogging(profile);

  ProfileMetrics::LogNumberOfProfiles(this);
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_PROFILE_ADDED,
      content::Source<Profile>(profile),
      content::NotificationService::NoDetails());
}

void ProfileManager::DoFinalInitForServices(Profile* profile,
                                            bool go_off_the_record) {
#if defined(ENABLE_EXTENSIONS)
  extensions::ExtensionSystem::Get(profile)->InitForRegularProfile(
      !go_off_the_record);
  // During tests, when |profile| is an instance of TestingProfile,
  // ExtensionSystem might not create an ExtensionService.
  if (extensions::ExtensionSystem::Get(profile)->extension_service()) {
    profile->GetHostContentSettingsMap()->RegisterExtensionService(
        extensions::ExtensionSystem::Get(profile)->extension_service());
  }
#endif
#if defined(ENABLE_MANAGED_USERS)
  // Initialization needs to happen after extension system initialization (for
  // extension::ManagementPolicy) and InitProfileUserPrefs (for setting the
  // initializing the managed flag if necessary).
  ManagedUserServiceFactory::GetForProfile(profile)->Init();
#endif
  // Start the deferred task runners once the profile is loaded.
  StartupTaskRunnerServiceFactory::GetForProfile(profile)->
      StartDeferredTaskRunners();

  if (profiles::IsNewProfileManagementEnabled())
    AccountReconcilorFactory::GetForProfile(profile);
}

void ProfileManager::DoFinalInitLogging(Profile* profile) {
  // Count number of extensions in this profile.
  int extension_count = -1;
#if defined(ENABLE_EXTENSIONS)
  ExtensionService* extension_service = profile->GetExtensionService();
  if (extension_service)
    extension_count = extension_service->GetAppIds().size();
#endif

  // Log the profile size after a reasonable startup delay.
  BrowserThread::PostDelayedTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&ProfileSizeTask, profile->GetPath(), extension_count),
      base::TimeDelta::FromSeconds(112));
}

Profile* ProfileManager::CreateProfileHelper(const base::FilePath& path) {
  return Profile::CreateProfile(path, NULL, Profile::CREATE_MODE_SYNCHRONOUS);
}

Profile* ProfileManager::CreateProfileAsyncHelper(const base::FilePath& path,
                                                  Delegate* delegate) {
  return Profile::CreateProfile(path,
                                delegate,
                                Profile::CREATE_MODE_ASYNCHRONOUS);
}

void ProfileManager::OnProfileCreated(Profile* profile,
                                      bool success,
                                      bool is_new_profile) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  ProfilesInfoMap::iterator iter = profiles_info_.find(profile->GetPath());
  DCHECK(iter != profiles_info_.end());
  ProfileInfo* info = iter->second.get();

  std::vector<CreateCallback> callbacks;
  info->callbacks.swap(callbacks);

  // Invoke CREATED callback for normal profiles.
  bool go_off_the_record = ShouldGoOffTheRecord(profile);
  if (success && !go_off_the_record)
    RunCallbacks(callbacks, profile, Profile::CREATE_STATUS_CREATED);

  // Perform initialization.
  if (success) {
    DoFinalInit(profile, go_off_the_record);
    if (go_off_the_record)
      profile = profile->GetOffTheRecordProfile();
    info->created = true;
  } else {
    profile = NULL;
    profiles_info_.erase(iter);
  }

  if (profile) {
    // If this was the guest profile, finish setting its incognito status.
    if (profile->GetPath() == ProfileManager::GetGuestProfilePath())
      SetGuestProfilePrefs(profile);

    // Invoke CREATED callback for incognito profiles.
    if (go_off_the_record)
      RunCallbacks(callbacks, profile, Profile::CREATE_STATUS_CREATED);
  }

  // Invoke INITIALIZED or FAIL for all profiles.
  RunCallbacks(callbacks, profile,
               profile ? Profile::CREATE_STATUS_INITIALIZED :
                         Profile::CREATE_STATUS_LOCAL_FAIL);
}

base::FilePath ProfileManager::GenerateNextProfileDirectoryPath() {
  PrefService* local_state = g_browser_process->local_state();
  DCHECK(local_state);

  DCHECK(profiles::IsMultipleProfilesEnabled());

  // Create the next profile in the next available directory slot.
  int next_directory = local_state->GetInteger(prefs::kProfilesNumCreated);
  std::string profile_name = chrome::kMultiProfileDirPrefix;
  profile_name.append(base::IntToString(next_directory));
  base::FilePath new_path = user_data_dir_;
#if defined(OS_WIN)
  new_path = new_path.Append(ASCIIToUTF16(profile_name));
#else
  new_path = new_path.Append(profile_name);
#endif
  local_state->SetInteger(prefs::kProfilesNumCreated, ++next_directory);
  return new_path;
}

// static
base::FilePath ProfileManager::CreateMultiProfileAsync(
    const base::string16& name,
    const base::string16& icon_url,
    const CreateCallback& callback,
    const std::string& managed_user_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  ProfileManager* profile_manager = g_browser_process->profile_manager();

  base::FilePath new_path = profile_manager->GenerateNextProfileDirectoryPath();

  profile_manager->CreateProfileAsync(new_path,
                                      callback,
                                      name,
                                      icon_url,
                                      managed_user_id);
  return new_path;
}

// static
base::FilePath ProfileManager::GetGuestProfilePath() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  ProfileManager* profile_manager = g_browser_process->profile_manager();

  base::FilePath guest_path = profile_manager->user_data_dir();
  return guest_path.Append(chrome::kGuestProfileDir);
}

size_t ProfileManager::GetNumberOfProfiles() {
  return GetProfileInfoCache().GetNumberOfProfiles();
}

bool ProfileManager::CompareProfilePathAndName(
    const ProfileManager::ProfilePathAndName& pair1,
    const ProfileManager::ProfilePathAndName& pair2) {
  int name_compare = pair1.second.compare(pair2.second);
  if (name_compare < 0) {
    return true;
  } else if (name_compare > 0) {
    return false;
  } else {
    return pair1.first < pair2.first;
  }
}

ProfileInfoCache& ProfileManager::GetProfileInfoCache() {
  if (!profile_info_cache_) {
    profile_info_cache_.reset(new ProfileInfoCache(
        g_browser_process->local_state(), user_data_dir_));
  }
  return *profile_info_cache_.get();
}

ProfileShortcutManager* ProfileManager::profile_shortcut_manager() {
  return profile_shortcut_manager_.get();
}

void ProfileManager::AddProfileToCache(Profile* profile) {
  if (profile->IsGuestSession())
    return;
  ProfileInfoCache& cache = GetProfileInfoCache();
  if (profile->GetPath().DirName() != cache.GetUserDataDir())
    return;

  if (cache.GetIndexOfProfileWithPath(profile->GetPath()) != std::string::npos)
    return;

  base::string16 username = UTF8ToUTF16(profile->GetPrefs()->GetString(
      prefs::kGoogleServicesUsername));

  // Profile name and avatar are set by InitProfileUserPrefs and stored in the
  // profile. Use those values to setup the cache entry.
  base::string16 profile_name = UTF8ToUTF16(profile->GetPrefs()->GetString(
      prefs::kProfileName));

  size_t icon_index = profile->GetPrefs()->GetInteger(
      prefs::kProfileAvatarIndex);

  std::string managed_user_id =
      profile->GetPrefs()->GetString(prefs::kManagedUserId);

  cache.AddProfileToCache(profile->GetPath(),
                          profile_name,
                          username,
                          icon_index,
                          managed_user_id);

  if (profile->GetPrefs()->GetBoolean(prefs::kForceEphemeralProfiles)) {
    cache.SetProfileIsEphemeralAtIndex(
        cache.GetIndexOfProfileWithPath(profile->GetPath()), true);
  }
}

void ProfileManager::InitProfileUserPrefs(Profile* profile) {
  ProfileInfoCache& cache = GetProfileInfoCache();

  if (profile->GetPath().DirName() != cache.GetUserDataDir())
    return;

  size_t avatar_index;
  std::string profile_name;
  std::string managed_user_id;
  if (profile->IsGuestSession()) {
    profile_name = l10n_util::GetStringUTF8(IDS_PROFILES_GUEST_PROFILE_NAME);
    avatar_index = 0;
  } else {
    size_t profile_cache_index =
        cache.GetIndexOfProfileWithPath(profile->GetPath());
    // If the cache has an entry for this profile, use the cache data.
    if (profile_cache_index != std::string::npos) {
      avatar_index =
          cache.GetAvatarIconIndexOfProfileAtIndex(profile_cache_index);
      profile_name =
          UTF16ToUTF8(cache.GetNameOfProfileAtIndex(profile_cache_index));
      managed_user_id =
          cache.GetManagedUserIdOfProfileAtIndex(profile_cache_index);
    } else if (profile->GetPath() ==
               profiles::GetDefaultProfileDir(cache.GetUserDataDir())) {
      avatar_index = 0;
      profile_name = l10n_util::GetStringUTF8(IDS_DEFAULT_PROFILE_NAME);
    } else {
      avatar_index = cache.ChooseAvatarIconIndexForNewProfile();
      profile_name = UTF16ToUTF8(cache.ChooseNameForNewProfile(avatar_index));
    }
  }

  if (!profile->GetPrefs()->HasPrefPath(prefs::kProfileAvatarIndex))
    profile->GetPrefs()->SetInteger(prefs::kProfileAvatarIndex, avatar_index);

  if (!profile->GetPrefs()->HasPrefPath(prefs::kProfileName))
    profile->GetPrefs()->SetString(prefs::kProfileName, profile_name);

  CommandLine* command_line = CommandLine::ForCurrentProcess();
  bool force_managed_user_id =
      command_line->HasSwitch(switches::kManagedUserId);
  if (force_managed_user_id) {
    managed_user_id =
        command_line->GetSwitchValueASCII(switches::kManagedUserId);
  }
  if (force_managed_user_id ||
      !profile->GetPrefs()->HasPrefPath(prefs::kManagedUserId)) {
    profile->GetPrefs()->SetString(prefs::kManagedUserId, managed_user_id);
  }
}

void ProfileManager::SetGuestProfilePrefs(Profile* profile) {
  IncognitoModePrefs::SetAvailability(profile->GetPrefs(),
                                      IncognitoModePrefs::FORCED);
  profile->GetPrefs()->SetBoolean(prefs::kShowBookmarkBar, false);
}

bool ProfileManager::ShouldGoOffTheRecord(Profile* profile) {
  bool go_off_the_record = false;
#if defined(OS_CHROMEOS)
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(chromeos::switches::kGuestSession) ||
      (profile->GetPath().BaseName().value() == chrome::kInitialProfile &&
       (!command_line.HasSwitch(switches::kTestType) ||
       command_line.HasSwitch(chromeos::switches::kLoginProfile)))) {
    go_off_the_record = true;
  }
#endif
  return go_off_the_record;
}

void ProfileManager::ScheduleProfileForDeletion(
    const base::FilePath& profile_dir,
    const CreateCallback& callback) {
  DCHECK(profiles::IsMultipleProfilesEnabled());
  PrefService* local_state = g_browser_process->local_state();
  ProfileInfoCache& cache = GetProfileInfoCache();

  if (profile_dir.BaseName().MaybeAsASCII() ==
      local_state->GetString(prefs::kProfileLastUsed)) {
    // Update the last used profile pref before closing browser windows. This
    // way the correct last used profile is set for any notification observers.
    base::FilePath last_non_managed_profile_path;
    for (size_t i = 0; i < cache.GetNumberOfProfiles(); ++i) {
      base::FilePath cur_path = cache.GetPathOfProfileAtIndex(i);
      // Make sure that this profile is not pending deletion.
      if (cur_path != profile_dir && !cache.ProfileIsManagedAtIndex(i) &&
          !IsProfileMarkedForDeletion(cur_path)) {
        last_non_managed_profile_path = cur_path;
        break;
      }
    }

    // If we're deleting the last (non-managed) profile, then create a new
    // profile in its place.
    const std::string last_non_managed_profile =
        last_non_managed_profile_path.BaseName().MaybeAsASCII();
    if (last_non_managed_profile.empty()) {
      base::FilePath new_path = GenerateNextProfileDirectoryPath();
      // Make sure the last used profile path is pointing at it. This way the
      // correct last used profile is set for any notification observers.
      local_state->SetString(prefs::kProfileLastUsed,
                             new_path.BaseName().MaybeAsASCII());
      CreateProfileAsync(new_path,
                         callback,
                         base::string16(),
                         base::string16(),
                         std::string());
    } else {
      // On the Mac, the browser process is not killed when all browser windows
      // are closed, so just in case we are deleting the active profile, and no
      // other profile has been loaded, we must pre-load a next one.
#if defined(OS_MACOSX)
      CreateProfileAsync(last_non_managed_profile_path,
                         base::Bind(&ProfileManager::OnNewActiveProfileLoaded,
                                    base::Unretained(this),
                                    profile_dir,
                                    last_non_managed_profile_path,
                                    callback),
                         base::string16(),
                         base::string16(),
                         std::string());
      return;
#else
      // For OS_MACOSX the pref is updated in the callback to make sure that
      // it isn't used before the profile is actually loaded.
      local_state->SetString(prefs::kProfileLastUsed, last_non_managed_profile);
#endif
    }
  }
  FinishDeletingProfile(profile_dir);
}

// static
void ProfileManager::CleanUpStaleProfiles(
    const std::vector<base::FilePath>& profile_paths) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  for (std::vector<base::FilePath>::const_iterator it = profile_paths.begin();
       it != profile_paths.end(); ++it) {
    NukeProfileFromDisk(*it);
  }
}

void ProfileManager::OnNewActiveProfileLoaded(
    const base::FilePath& profile_to_delete_path,
    const base::FilePath& last_non_managed_profile_path,
    const CreateCallback& original_callback,
    Profile* loaded_profile,
    Profile::CreateStatus status) {
  DCHECK(status != Profile::CREATE_STATUS_LOCAL_FAIL &&
         status != Profile::CREATE_STATUS_REMOTE_FAIL);

  // Only run the code if the profile initialization has finished completely.
  if (status == Profile::CREATE_STATUS_INITIALIZED) {
    if (IsProfileMarkedForDeletion(last_non_managed_profile_path)) {
      // If the profile we tried to load as the next active profile has been
      // deleted, then retry deleting this profile to redo the logic to load
      // the next available profile.
      ScheduleProfileForDeletion(profile_to_delete_path, original_callback);
    } else {
      // Update the local state as promised in the ScheduleProfileForDeletion.
      g_browser_process->local_state()->SetString(
          prefs::kProfileLastUsed,
          last_non_managed_profile_path.BaseName().MaybeAsASCII());
      FinishDeletingProfile(profile_to_delete_path);
    }
  }
}

void ProfileManager::FinishDeletingProfile(const base::FilePath& profile_dir) {
  ProfileInfoCache& cache = GetProfileInfoCache();
  // TODO(sail): Due to bug 88586 we don't delete the profile instance. Once we
  // start deleting the profile instance we need to close background apps too.
  Profile* profile = GetProfileByPath(profile_dir);

  if (profile) {
    BrowserList::CloseAllBrowsersWithProfile(profile);

    // Disable sync for doomed profile.
    if (ProfileSyncServiceFactory::GetInstance()->HasProfileSyncService(
        profile)) {
      ProfileSyncServiceFactory::GetInstance()->GetForProfile(
          profile)->DisableForUser();
    }
  }

  QueueProfileDirectoryForDeletion(profile_dir);
  cache.DeleteProfileFromCache(profile_dir);
  ProfileMetrics::UpdateReportedProfilesStatistics(this);
}

void ProfileManager::AutoloadProfiles() {
  // If running in the background is disabled for the browser, do not autoload
  // any profiles.
  PrefService* local_state = g_browser_process->local_state();
  if (!local_state->HasPrefPath(prefs::kBackgroundModeEnabled) ||
      !local_state->GetBoolean(prefs::kBackgroundModeEnabled)) {
    return;
  }

  ProfileInfoCache& cache = GetProfileInfoCache();
  size_t number_of_profiles = cache.GetNumberOfProfiles();
  for (size_t p = 0; p < number_of_profiles; ++p) {
    if (cache.GetBackgroundStatusOfProfileAtIndex(p)) {
      // If status is true, that profile is running background apps. By calling
      // GetProfile, we automatically cause the profile to be loaded which will
      // register it with the BackgroundModeManager.
      GetProfile(cache.GetPathOfProfileAtIndex(p));
    }
  }
}

ProfileManagerWithoutInit::ProfileManagerWithoutInit(
    const base::FilePath& user_data_dir) : ProfileManager(user_data_dir) {
}

void ProfileManager::RegisterTestingProfile(Profile* profile,
                                            bool add_to_cache,
                                            bool start_deferred_task_runners) {
  RegisterProfile(profile, true);
  if (add_to_cache) {
    InitProfileUserPrefs(profile);
    AddProfileToCache(profile);
  }
  if (start_deferred_task_runners) {
    StartupTaskRunnerServiceFactory::GetForProfile(profile)->
        StartDeferredTaskRunners();
  }
}

void ProfileManager::RunCallbacks(const std::vector<CreateCallback>& callbacks,
                                  Profile* profile,
                                  Profile::CreateStatus status) {
  for (size_t i = 0; i < callbacks.size(); ++i)
    callbacks[i].Run(profile, status);
}

ProfileManager::ProfileInfo::ProfileInfo(
    Profile* profile,
    bool created)
    : profile(profile),
      created(created) {
}

ProfileManager::ProfileInfo::~ProfileInfo() {
  ProfileDestroyer::DestroyProfileWhenAppropriate(profile.release());
}
