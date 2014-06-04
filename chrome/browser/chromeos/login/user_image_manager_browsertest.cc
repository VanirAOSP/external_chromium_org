// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/json/json_writer.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/path_service.h"
#include "base/prefs/pref_change_registrar.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/scoped_user_pref_update.h"
#include "base/run_loop.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/login/default_user_images.h"
#include "chrome/browser/chromeos/login/login_manager_test.h"
#include "chrome/browser/chromeos/login/mock_user_manager.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/login/user.h"
#include "chrome/browser/chromeos/login/user_image.h"
#include "chrome/browser/chromeos/login/user_image_manager.h"
#include "chrome/browser/chromeos/login/user_image_manager_impl.h"
#include "chrome/browser/chromeos/login/user_image_manager_test_util.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/policy/cloud_external_data_manager_base_test_util.h"
#include "chrome/browser/chromeos/policy/user_cloud_policy_manager_chromeos.h"
#include "chrome/browser/chromeos/policy/user_cloud_policy_manager_factory_chromeos.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_downloader.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/chromeos_paths.h"
#include "chromeos/dbus/cryptohome_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_dbus_thread_manager.h"
#include "chromeos/dbus/fake_session_manager_client.h"
#include "chromeos/dbus/session_manager_client.h"
#include "components/policy/core/common/cloud/cloud_policy_core.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/policy/core/common/cloud/policy_builder.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/test/test_utils.h"
#include "crypto/rsa_private_key.h"
#include "google_apis/gaia/oauth2_token_service.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request_status.h"
#include "policy/proto/cloud_policy.pb.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/layout.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia.h"
#include "url/gurl.h"

namespace chromeos {

namespace {

const char kTestUser1[] = "test-user@example.com";
const char kTestUser2[] = "test-user2@example.com";

policy::CloudPolicyStore* GetStoreForUser(const User* user) {
  Profile* profile = UserManager::Get()->GetProfileByUser(user);
  if (!profile) {
    ADD_FAILURE();
    return NULL;
  }
  policy::UserCloudPolicyManagerChromeOS* policy_manager =
      policy::UserCloudPolicyManagerFactoryChromeOS::GetForProfile(profile);
  if (!policy_manager) {
    ADD_FAILURE();
    return NULL;
  }
  return policy_manager->core()->store();
}

}  // namespace

class UserImageManagerTest : public LoginManagerTest,
                             public UserManager::Observer {
 protected:
  UserImageManagerTest() : LoginManagerTest(true) {
  }

  // LoginManagerTest overrides:
  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
    LoginManagerTest::SetUpInProcessBrowserTestFixture();

    ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir_));
    ASSERT_TRUE(PathService::Get(chrome::DIR_USER_DATA, &user_data_dir_));
  }

  virtual void SetUpOnMainThread() OVERRIDE {
    LoginManagerTest::SetUpOnMainThread();
    local_state_ = g_browser_process->local_state();
    UserManager::Get()->AddObserver(this);
  }

  virtual void TearDownOnMainThread() OVERRIDE {
    UserManager::Get()->RemoveObserver(this);
    LoginManagerTest::TearDownOnMainThread();
  }

  // UserManager::Observer overrides:
  virtual void LocalStateChanged(UserManager* user_manager) OVERRIDE {
    if (run_loop_)
      run_loop_->Quit();
  }

  // Logs in |username|.
  void LogIn(const std::string& username) {
    UserManager::Get()->UserLoggedIn(username, username, false);
  }

  // Stores old (pre-migration) user image info.
  void SetOldUserImageInfo(const std::string& username,
                           int image_index,
                           const base::FilePath& image_path) {
    RegisterUser(username);
    DictionaryPrefUpdate images_pref(local_state_, "UserImages");
    base::DictionaryValue* image_properties = new base::DictionaryValue();
    image_properties->Set(
        "index", base::Value::CreateIntegerValue(image_index));
    image_properties->Set(
        "path" , new base::StringValue(image_path.value()));
    images_pref->SetWithoutPathExpansion(username, image_properties);
  }

  // Verifies user image info in |images_pref| dictionary.
  void ExpectUserImageInfo(const base::DictionaryValue* images_pref,
                           const std::string& username,
                           int image_index,
                           const base::FilePath& image_path) {
    ASSERT_TRUE(images_pref);
    const base::DictionaryValue* image_properties = NULL;
    images_pref->GetDictionaryWithoutPathExpansion(username, &image_properties);
    ASSERT_TRUE(image_properties);
    int actual_image_index;
    std::string actual_image_path;
    ASSERT_TRUE(image_properties->GetInteger("index", &actual_image_index) &&
                image_properties->GetString("path", &actual_image_path));
    EXPECT_EQ(image_index, actual_image_index);
    EXPECT_EQ(image_path.value(), actual_image_path);
  }

  // Verifies that there is no image info for |username| in dictionary
  // |images_pref|.
  void ExpectNoUserImageInfo(const base::DictionaryValue* images_pref,
                             const std::string& username) {
    ASSERT_TRUE(images_pref);
    const base::DictionaryValue* image_properties = NULL;
    images_pref->GetDictionaryWithoutPathExpansion(username, &image_properties);
    ASSERT_FALSE(image_properties);
  }

  // Verifies that old user image info matches |image_index| and |image_path|
  // and that new user image info does not exist.
  void ExpectOldUserImageInfo(const std::string& username,
                              int image_index,
                              const base::FilePath& image_path) {
    ExpectUserImageInfo(local_state_->GetDictionary("UserImages"),
                        username, image_index, image_path);
    ExpectNoUserImageInfo(local_state_->GetDictionary("user_image_info"),
                          username);
  }

  // Verifies that new user image info matches |image_index| and |image_path|
  // and that old user image info does not exist.
  void ExpectNewUserImageInfo(const std::string& username,
                              int image_index,
                              const base::FilePath& image_path) {
    ExpectUserImageInfo(local_state_->GetDictionary("user_image_info"),
                        username, image_index, image_path);
    ExpectNoUserImageInfo(local_state_->GetDictionary("UserImages"),
                          username);
  }

  // Sets bitmap |resource_id| as image for |username| and saves it to disk.
  void SaveUserImagePNG(const std::string& username,
                        int resource_id) {
    base::FilePath image_path = GetUserImagePath(username, "png");
    scoped_refptr<base::RefCountedStaticMemory> image_data(
        ResourceBundle::GetSharedInstance().LoadDataResourceBytesForScale(
            resource_id, ui::SCALE_FACTOR_100P));
    int written = file_util::WriteFile(
        image_path,
        reinterpret_cast<const char*>(image_data->front()),
        image_data->size());
    EXPECT_EQ(static_cast<int>(image_data->size()), written);
    SetOldUserImageInfo(username, User::kExternalImageIndex, image_path);
  }

  // Returns the image path for user |username| with specified |extension|.
  base::FilePath GetUserImagePath(const std::string& username,
                                  const std::string& extension) {
    return user_data_dir_.Append(username).AddExtension(extension);
  }

  // Completes the download of all non-image profile data for the currently
  // logged-in user. This method must only be called after a profile data
  // download has been started.
  // |url_fetcher_factory| will capture the net::TestURLFetcher created by the
  // ProfileDownloader to download the profile image.
  void CompleteProfileMetadataDownload(
      net::TestURLFetcherFactory* url_fetcher_factory) {
    ProfileDownloader* profile_downloader =
        reinterpret_cast<UserImageManagerImpl*>(
            UserManager::Get()->GetUserImageManager())->
                profile_downloader_.get();
    ASSERT_TRUE(profile_downloader);

    static_cast<OAuth2TokenService::Consumer*>(profile_downloader)->
        OnGetTokenSuccess(NULL,
                          std::string(),
                          base::Time::Now() + base::TimeDelta::FromDays(1));

    net::TestURLFetcher* fetcher = url_fetcher_factory->GetFetcherByID(0);
    ASSERT_TRUE(fetcher);
    fetcher->SetResponseString(
        "{ \"picture\": \"http://localhost/avatar.jpg\" }");
    fetcher->set_status(net::URLRequestStatus(net::URLRequestStatus::SUCCESS,
                                              net::OK));
    fetcher->set_response_code(200);
    fetcher->delegate()->OnURLFetchComplete(fetcher);
    base::RunLoop().RunUntilIdle();
  }

  // Completes the download of the currently logged-in user's profile image.
  // This method must only be called after a profile data download including
  // the profile image has been started, the download of all non-image data has
  // been completed by calling CompleteProfileMetadataDownload() and the
  // net::TestURLFetcher created by the ProfileDownloader to download the
  // profile image has been captured by |url_fetcher_factory|.
  void CompleteProfileImageDownload(
      net::TestURLFetcherFactory* url_fetcher_factory) {
    std::string profile_image_data;
    base::FilePath test_data_dir;
    ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir));
    EXPECT_TRUE(ReadFileToString(
        test_data_dir.Append("chromeos").Append("avatar1.jpg"),
        &profile_image_data));

    base::RunLoop run_loop;
    PrefChangeRegistrar pref_change_registrar;
    pref_change_registrar.Init(local_state_);
    pref_change_registrar.Add("UserDisplayName", run_loop.QuitClosure());
    net::TestURLFetcher* fetcher = url_fetcher_factory->GetFetcherByID(0);
    ASSERT_TRUE(fetcher);
    fetcher->SetResponseString(profile_image_data);
    fetcher->set_status(net::URLRequestStatus(net::URLRequestStatus::SUCCESS,
                                              net::OK));
    fetcher->set_response_code(200);
    fetcher->delegate()->OnURLFetchComplete(fetcher);
    run_loop.Run();

    const User* user = UserManager::Get()->GetLoggedInUser();
    ASSERT_TRUE(user);
    const std::map<std::string, linked_ptr<UserImageManagerImpl::Job> >&
        job_map = reinterpret_cast<UserImageManagerImpl*>(
            UserManager::Get()->GetUserImageManager())->jobs_;
    if (job_map.find(user->email()) != job_map.end()) {
      run_loop_.reset(new base::RunLoop);
      run_loop_->Run();
    }
  }

  base::FilePath test_data_dir_;
  base::FilePath user_data_dir_;

  PrefService* local_state_;

  scoped_ptr<gfx::ImageSkia> decoded_image_;

  scoped_ptr<base::RunLoop> run_loop_;

 private:
  DISALLOW_COPY_AND_ASSIGN(UserImageManagerTest);
};

IN_PROC_BROWSER_TEST_F(UserImageManagerTest, PRE_DefaultUserImagePreserved) {
  // Setup an old default (stock) user image.
  ScopedUserManagerEnabler(new MockUserManager);
  SetOldUserImageInfo(kTestUser1, kFirstDefaultImageIndex, base::FilePath());
}

IN_PROC_BROWSER_TEST_F(UserImageManagerTest, DefaultUserImagePreserved) {
  UserManager::Get()->GetUsers();  // Load users.
  // Old info preserved.
  ExpectOldUserImageInfo(kTestUser1, kFirstDefaultImageIndex, base::FilePath());
  LogIn(kTestUser1);
  // Image info is migrated now.
  ExpectNewUserImageInfo(kTestUser1, kFirstDefaultImageIndex, base::FilePath());
}

IN_PROC_BROWSER_TEST_F(UserImageManagerTest, PRE_OtherUsersUnaffected) {
  // Setup two users with stock images.
  ScopedUserManagerEnabler(new MockUserManager);
  SetOldUserImageInfo(kTestUser1, kFirstDefaultImageIndex, base::FilePath());
  SetOldUserImageInfo(kTestUser2, kFirstDefaultImageIndex + 1,
                      base::FilePath());
}

IN_PROC_BROWSER_TEST_F(UserImageManagerTest, OtherUsersUnaffected) {
  UserManager::Get()->GetUsers();  // Load users.
  // Old info preserved.
  ExpectOldUserImageInfo(kTestUser1, kFirstDefaultImageIndex, base::FilePath());
  ExpectOldUserImageInfo(kTestUser2, kFirstDefaultImageIndex + 1,
                         base::FilePath());
  LogIn(kTestUser1);
  // Image info is migrated for the first user and unaffected for the rest.
  ExpectNewUserImageInfo(kTestUser1, kFirstDefaultImageIndex, base::FilePath());
  ExpectOldUserImageInfo(kTestUser2, kFirstDefaultImageIndex + 1,
                         base::FilePath());
}

IN_PROC_BROWSER_TEST_F(UserImageManagerTest, PRE_PRE_NonJPEGImageFromFile) {
  // Setup a user with non-JPEG image.
  ScopedUserManagerEnabler(new MockUserManager);
  SaveUserImagePNG(
      kTestUser1, kDefaultImageResourceIDs[kFirstDefaultImageIndex]);
}

IN_PROC_BROWSER_TEST_F(UserImageManagerTest, PRE_NonJPEGImageFromFile) {
  UserManager::Get()->GetUsers();  // Load users.
  // Old info preserved.
  ExpectOldUserImageInfo(kTestUser1, User::kExternalImageIndex,
                         GetUserImagePath(kTestUser1, "png"));
  const User* user = UserManager::Get()->FindUser(kTestUser1);
  EXPECT_TRUE(user->image_is_stub());

  base::RunLoop run_loop;
  PrefChangeRegistrar pref_change_registrar_;
  pref_change_registrar_.Init(local_state_);
  pref_change_registrar_.Add("UserImages", run_loop.QuitClosure());
  LogIn(kTestUser1);

  // Wait for migration.
  run_loop.Run();

  // Image info is migrated and the image is converted to JPG.
  ExpectNewUserImageInfo(kTestUser1, User::kExternalImageIndex,
                         GetUserImagePath(kTestUser1, "jpg"));
  user = UserManager::Get()->GetLoggedInUser();
  ASSERT_TRUE(user);
  EXPECT_FALSE(user->image_is_safe_format());
  // Check image dimensions.
  const gfx::ImageSkia& saved_image = GetDefaultImage(kFirstDefaultImageIndex);
  EXPECT_EQ(saved_image.width(), user->image().width());
  EXPECT_EQ(saved_image.height(), user->image().height());
}

IN_PROC_BROWSER_TEST_F(UserImageManagerTest, NonJPEGImageFromFile) {
  UserManager::Get()->GetUsers();  // Load users.
  const User* user = UserManager::Get()->FindUser(kTestUser1);
  ASSERT_TRUE(user);
  // Wait for image load.
  if (user->image_index() == User::kInvalidImageIndex) {
    content::WindowedNotificationObserver(
        chrome::NOTIFICATION_LOGIN_USER_IMAGE_CHANGED,
        content::NotificationService::AllSources()).Wait();
  }
  // Now the migrated image is used.
  EXPECT_TRUE(user->image_is_safe_format());
  // Check image dimensions. Images can't be compared since JPEG is lossy.
  const gfx::ImageSkia& saved_image = GetDefaultImage(kFirstDefaultImageIndex);
  EXPECT_EQ(saved_image.width(), user->image().width());
  EXPECT_EQ(saved_image.height(), user->image().height());
}

IN_PROC_BROWSER_TEST_F(UserImageManagerTest, PRE_SaveUserDefaultImageIndex) {
  RegisterUser(kTestUser1);
}

// Verifies that SaveUserDefaultImageIndex() correctly sets and persists the
// chosen user image.
IN_PROC_BROWSER_TEST_F(UserImageManagerTest, SaveUserDefaultImageIndex) {
  const User* user = UserManager::Get()->FindUser(kTestUser1);
  ASSERT_TRUE(user);

  const gfx::ImageSkia& default_image =
      GetDefaultImage(kFirstDefaultImageIndex);

  UserImageManager* user_image_manager =
      UserManager::Get()->GetUserImageManager();
  user_image_manager->SaveUserDefaultImageIndex(kTestUser1,
                                                kFirstDefaultImageIndex);

  EXPECT_TRUE(user->HasDefaultImage());
  EXPECT_EQ(kFirstDefaultImageIndex, user->image_index());
  EXPECT_TRUE(test::AreImagesEqual(default_image, user->image()));
  ExpectNewUserImageInfo(kTestUser1, kFirstDefaultImageIndex, base::FilePath());
}

IN_PROC_BROWSER_TEST_F(UserImageManagerTest, PRE_SaveUserImage) {
  RegisterUser(kTestUser1);
}

// Verifies that SaveUserImage() correctly sets and persists the chosen user
// image.
IN_PROC_BROWSER_TEST_F(UserImageManagerTest, SaveUserImage) {
  const User* user = UserManager::Get()->FindUser(kTestUser1);
  ASSERT_TRUE(user);

  SkBitmap custom_image_bitmap;
  custom_image_bitmap.setConfig(SkBitmap::kARGB_8888_Config, 10, 10);
  custom_image_bitmap.allocPixels();
  custom_image_bitmap.setImmutable();
  const gfx::ImageSkia custom_image =
      gfx::ImageSkia::CreateFrom1xBitmap(custom_image_bitmap);

  run_loop_.reset(new base::RunLoop);
  UserImageManager* user_image_manager =
      UserManager::Get()->GetUserImageManager();
  user_image_manager->SaveUserImage(kTestUser1,
                                    UserImage::CreateAndEncode(custom_image));
  run_loop_->Run();

  EXPECT_FALSE(user->HasDefaultImage());
  EXPECT_EQ(User::kExternalImageIndex, user->image_index());
  EXPECT_TRUE(test::AreImagesEqual(custom_image, user->image()));
  ExpectNewUserImageInfo(kTestUser1,
                         User::kExternalImageIndex,
                         GetUserImagePath(kTestUser1, "jpg"));

  const scoped_ptr<gfx::ImageSkia> saved_image =
      test::ImageLoader(GetUserImagePath(kTestUser1, "jpg")).Load();
  ASSERT_TRUE(saved_image);

  // Check image dimensions. Images can't be compared since JPEG is lossy.
  EXPECT_EQ(custom_image.width(), saved_image->width());
  EXPECT_EQ(custom_image.height(), saved_image->height());
}

IN_PROC_BROWSER_TEST_F(UserImageManagerTest, PRE_SaveUserImageFromFile) {
  RegisterUser(kTestUser1);
}

// Verifies that SaveUserImageFromFile() correctly sets and persists the chosen
// user image.
IN_PROC_BROWSER_TEST_F(UserImageManagerTest, SaveUserImageFromFile) {
  const User* user = UserManager::Get()->FindUser(kTestUser1);
  ASSERT_TRUE(user);

  const base::FilePath custom_image_path =
      test_data_dir_.Append(test::kUserAvatarImage1RelativePath);
  const scoped_ptr<gfx::ImageSkia> custom_image =
      test::ImageLoader(custom_image_path).Load();
  ASSERT_TRUE(custom_image);

  run_loop_.reset(new base::RunLoop);
  UserImageManager* user_image_manager =
      UserManager::Get()->GetUserImageManager();
  user_image_manager->SaveUserImageFromFile(kTestUser1, custom_image_path);
  run_loop_->Run();

  EXPECT_FALSE(user->HasDefaultImage());
  EXPECT_EQ(User::kExternalImageIndex, user->image_index());
  EXPECT_TRUE(test::AreImagesEqual(*custom_image, user->image()));
  ExpectNewUserImageInfo(kTestUser1,
                         User::kExternalImageIndex,
                         GetUserImagePath(kTestUser1, "jpg"));

  const scoped_ptr<gfx::ImageSkia> saved_image =
      test::ImageLoader(GetUserImagePath(kTestUser1, "jpg")).Load();
  ASSERT_TRUE(saved_image);

  // Check image dimensions. Images can't be compared since JPEG is lossy.
  EXPECT_EQ(custom_image->width(), saved_image->width());
  EXPECT_EQ(custom_image->height(), saved_image->height());
}

IN_PROC_BROWSER_TEST_F(UserImageManagerTest,
                       PRE_SaveUserImageFromProfileImage) {
  RegisterUser(kTestUser1);
  chromeos::StartupUtils::MarkOobeCompleted();
}

// Verifies that SaveUserImageFromProfileImage() correctly downloads, sets and
// persists the chosen user image.
IN_PROC_BROWSER_TEST_F(UserImageManagerTest, SaveUserImageFromProfileImage) {
  const User* user = UserManager::Get()->FindUser(kTestUser1);
  ASSERT_TRUE(user);

  UserImageManagerImpl::IgnoreProfileDataDownloadDelayForTesting();
  LoginUser(kTestUser1);

  run_loop_.reset(new base::RunLoop);
  UserImageManager* user_image_manager =
      UserManager::Get()->GetUserImageManager();
  user_image_manager->SaveUserImageFromProfileImage(kTestUser1);
  run_loop_->Run();

  net::TestURLFetcherFactory url_fetcher_factory;
  CompleteProfileMetadataDownload(&url_fetcher_factory);
  CompleteProfileImageDownload(&url_fetcher_factory);

  const gfx::ImageSkia& profile_image =
      user_image_manager->DownloadedProfileImage();

  EXPECT_FALSE(user->HasDefaultImage());
  EXPECT_EQ(User::kProfileImageIndex, user->image_index());
  EXPECT_TRUE(test::AreImagesEqual(profile_image, user->image()));
  ExpectNewUserImageInfo(kTestUser1,
                         User::kProfileImageIndex,
                         GetUserImagePath(kTestUser1, "jpg"));

  const scoped_ptr<gfx::ImageSkia> saved_image =
      test::ImageLoader(GetUserImagePath(kTestUser1, "jpg")).Load();
  ASSERT_TRUE(saved_image);

  // Check image dimensions. Images can't be compared since JPEG is lossy.
  EXPECT_EQ(profile_image.width(), saved_image->width());
  EXPECT_EQ(profile_image.height(), saved_image->height());
}

IN_PROC_BROWSER_TEST_F(UserImageManagerTest,
                       PRE_ProfileImageDownloadDoesNotClobber) {
  RegisterUser(kTestUser1);
  chromeos::StartupUtils::MarkOobeCompleted();
}

// Sets the user image to the profile image, then sets it to one of the default
// images while the profile image download is still in progress. Verifies that
// when the download completes, the profile image is ignored and does not
// clobber the default image chosen in the meantime.
IN_PROC_BROWSER_TEST_F(UserImageManagerTest,
                       ProfileImageDownloadDoesNotClobber) {
  const User* user = UserManager::Get()->FindUser(kTestUser1);
  ASSERT_TRUE(user);

  const gfx::ImageSkia& default_image =
      GetDefaultImage(kFirstDefaultImageIndex);

  UserImageManagerImpl::IgnoreProfileDataDownloadDelayForTesting();
  LoginUser(kTestUser1);

  run_loop_.reset(new base::RunLoop);
  UserImageManager* user_image_manager =
      UserManager::Get()->GetUserImageManager();
  user_image_manager->SaveUserImageFromProfileImage(kTestUser1);
  run_loop_->Run();

  net::TestURLFetcherFactory url_fetcher_factory;
  CompleteProfileMetadataDownload(&url_fetcher_factory);

  user_image_manager->SaveUserDefaultImageIndex(kTestUser1,
                                                kFirstDefaultImageIndex);

  CompleteProfileImageDownload(&url_fetcher_factory);

  EXPECT_TRUE(user->HasDefaultImage());
  EXPECT_EQ(kFirstDefaultImageIndex, user->image_index());
  EXPECT_TRUE(test::AreImagesEqual(default_image, user->image()));
  ExpectNewUserImageInfo(kTestUser1, kFirstDefaultImageIndex, base::FilePath());
}

class UserImageManagerPolicyTest : public UserImageManagerTest,
                                   public policy::CloudPolicyStore::Observer {
 protected:
  UserImageManagerPolicyTest()
      : fake_dbus_thread_manager_(new chromeos::FakeDBusThreadManager),
        fake_session_manager_client_(new chromeos::FakeSessionManagerClient) {
    fake_dbus_thread_manager_->SetFakeClients();
    fake_dbus_thread_manager_->SetSessionManagerClient(
        scoped_ptr<SessionManagerClient>(fake_session_manager_client_));
  }

  // UserImageManagerTest overrides:
  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
    DBusThreadManager::SetInstanceForTesting(fake_dbus_thread_manager_);
    UserImageManagerTest::SetUpInProcessBrowserTestFixture();
  }

  virtual void SetUpOnMainThread() OVERRIDE {
    UserImageManagerTest::SetUpOnMainThread();

    base::FilePath user_keys_dir;
    ASSERT_TRUE(PathService::Get(chromeos::DIR_USER_POLICY_KEYS,
                                 &user_keys_dir));
    const std::string sanitized_username =
        chromeos::CryptohomeClient::GetStubSanitizedUsername(kTestUser1);
    const base::FilePath user_key_file =
        user_keys_dir.AppendASCII(sanitized_username)
                     .AppendASCII("policy.pub");
    std::vector<uint8> user_key_bits;
    ASSERT_TRUE(user_policy_.GetSigningKey()->ExportPublicKey(&user_key_bits));
    ASSERT_TRUE(base::CreateDirectory(user_key_file.DirName()));
    ASSERT_EQ(file_util::WriteFile(
                  user_key_file,
                  reinterpret_cast<const char*>(user_key_bits.data()),
                  user_key_bits.size()),
              static_cast<int>(user_key_bits.size()));
    user_policy_.policy_data().set_username(kTestUser1);

    ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

    policy_image_ = test::ImageLoader(test_data_dir_.Append(
        test::kUserAvatarImage2RelativePath)).Load();
    ASSERT_TRUE(policy_image_);
  }

  // policy::CloudPolicyStore::Observer overrides:
  virtual void OnStoreLoaded(policy::CloudPolicyStore* store) OVERRIDE {
    if (run_loop_)
      run_loop_->Quit();
  }

  virtual void OnStoreError(policy::CloudPolicyStore* store) OVERRIDE {
    if (run_loop_)
      run_loop_->Quit();
  }

  std::string ConstructPolicy(const std::string& relative_path) {
    std::string image_data;
    if (!base::ReadFileToString(test_data_dir_.Append(relative_path),
                                &image_data)) {
      ADD_FAILURE();
    }
    std::string policy;
    base::JSONWriter::Write(policy::test::ConstructExternalDataReference(
        embedded_test_server()->GetURL(std::string("/") + relative_path).spec(),
        image_data).get(),
        &policy);
    return policy;
  }

  policy::UserPolicyBuilder user_policy_;
  FakeDBusThreadManager* fake_dbus_thread_manager_;
  FakeSessionManagerClient* fake_session_manager_client_;

  scoped_ptr<gfx::ImageSkia> policy_image_;

 private:
  DISALLOW_COPY_AND_ASSIGN(UserImageManagerPolicyTest);
};

IN_PROC_BROWSER_TEST_F(UserImageManagerPolicyTest, PRE_SetAndClear) {
  RegisterUser(kTestUser1);
  chromeos::StartupUtils::MarkOobeCompleted();
}

// Verifies that the user image can be set through policy. Also verifies that
// after the policy has been cleared, the user is able to choose a different
// image.
IN_PROC_BROWSER_TEST_F(UserImageManagerPolicyTest, SetAndClear) {
  const User* user = UserManager::Get()->FindUser(kTestUser1);
  ASSERT_TRUE(user);

  LoginUser(kTestUser1);
  base::RunLoop().RunUntilIdle();

  policy::CloudPolicyStore* store = GetStoreForUser(user);
  ASSERT_TRUE(store);

  // Set policy. Verify that the policy-provided user image is downloaded, set
  // and persisted.
  user_policy_.payload().mutable_useravatarimage()->set_value(
      ConstructPolicy(test::kUserAvatarImage2RelativePath));
  user_policy_.Build();
  fake_session_manager_client_->set_user_policy(kTestUser1,
                                                user_policy_.GetBlob());
  run_loop_.reset(new base::RunLoop);
  store->Load();
  run_loop_->Run();

  EXPECT_FALSE(user->HasDefaultImage());
  EXPECT_EQ(User::kExternalImageIndex, user->image_index());
  EXPECT_TRUE(test::AreImagesEqual(*policy_image_, user->image()));
  ExpectNewUserImageInfo(kTestUser1,
                         User::kExternalImageIndex,
                         GetUserImagePath(kTestUser1, "jpg"));

  scoped_ptr<gfx::ImageSkia> saved_image =
      test::ImageLoader(GetUserImagePath(kTestUser1, "jpg")).Load();
  ASSERT_TRUE(saved_image);

  // Check image dimensions. Images can't be compared since JPEG is lossy.
  EXPECT_EQ(policy_image_->width(), saved_image->width());
  EXPECT_EQ(policy_image_->height(), saved_image->height());

  // Clear policy. Verify that the policy-provided user image remains set as no
  // different user image has been chosen yet.
  user_policy_.payload().Clear();
  user_policy_.Build();
  fake_session_manager_client_->set_user_policy(kTestUser1,
                                                user_policy_.GetBlob());
  run_loop_.reset(new base::RunLoop);
  store->AddObserver(this);
  store->Load();
  run_loop_->Run();
  store->RemoveObserver(this);
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(user->HasDefaultImage());
  EXPECT_EQ(User::kExternalImageIndex, user->image_index());
  EXPECT_TRUE(test::AreImagesEqual(*policy_image_, user->image()));
  ExpectNewUserImageInfo(kTestUser1,
                         User::kExternalImageIndex,
                         GetUserImagePath(kTestUser1, "jpg"));

  saved_image = test::ImageLoader(GetUserImagePath(kTestUser1, "jpg")).Load();
  ASSERT_TRUE(saved_image);

  // Check image dimensions. Images can't be compared since JPEG is lossy.
  EXPECT_EQ(policy_image_->width(), saved_image->width());
  EXPECT_EQ(policy_image_->height(), saved_image->height());

  // Choose a different user image. Verify that the chosen user image is set and
  // persisted.
  const gfx::ImageSkia& default_image =
      GetDefaultImage(kFirstDefaultImageIndex);

  UserImageManager* user_image_manager =
      UserManager::Get()->GetUserImageManager();
  user_image_manager->SaveUserDefaultImageIndex(kTestUser1,
                                                kFirstDefaultImageIndex);

  EXPECT_TRUE(user->HasDefaultImage());
  EXPECT_EQ(kFirstDefaultImageIndex, user->image_index());
  EXPECT_TRUE(test::AreImagesEqual(default_image, user->image()));
  ExpectNewUserImageInfo(kTestUser1, kFirstDefaultImageIndex, base::FilePath());
}

IN_PROC_BROWSER_TEST_F(UserImageManagerPolicyTest, PRE_PolicyOverridesUser) {
  RegisterUser(kTestUser1);
  chromeos::StartupUtils::MarkOobeCompleted();
}

// Verifies that when the user chooses a user image and a different image is
// then set through policy, the policy takes precedence, overriding the
// previously chosen image.
IN_PROC_BROWSER_TEST_F(UserImageManagerPolicyTest, PolicyOverridesUser) {
  const User* user = UserManager::Get()->FindUser(kTestUser1);
  ASSERT_TRUE(user);

  LoginUser(kTestUser1);
  base::RunLoop().RunUntilIdle();

  policy::CloudPolicyStore* store = GetStoreForUser(user);
  ASSERT_TRUE(store);

  // Choose a user image. Verify that the chosen user image is set and
  // persisted.
  const gfx::ImageSkia& default_image =
      GetDefaultImage(kFirstDefaultImageIndex);

  UserImageManager* user_image_manager =
      UserManager::Get()->GetUserImageManager();
  user_image_manager->SaveUserDefaultImageIndex(kTestUser1,
                                                kFirstDefaultImageIndex);

  EXPECT_TRUE(user->HasDefaultImage());
  EXPECT_EQ(kFirstDefaultImageIndex, user->image_index());
  EXPECT_TRUE(test::AreImagesEqual(default_image, user->image()));
  ExpectNewUserImageInfo(kTestUser1, kFirstDefaultImageIndex, base::FilePath());

  // Set policy. Verify that the policy-provided user image is downloaded, set
  // and persisted, overriding the previously set image.
  user_policy_.payload().mutable_useravatarimage()->set_value(
      ConstructPolicy(test::kUserAvatarImage2RelativePath));
  user_policy_.Build();
  fake_session_manager_client_->set_user_policy(kTestUser1,
                                                user_policy_.GetBlob());
  run_loop_.reset(new base::RunLoop);
  store->Load();
  run_loop_->Run();

  EXPECT_FALSE(user->HasDefaultImage());
  EXPECT_EQ(User::kExternalImageIndex, user->image_index());
  EXPECT_TRUE(test::AreImagesEqual(*policy_image_, user->image()));
  ExpectNewUserImageInfo(kTestUser1,
                         User::kExternalImageIndex,
                         GetUserImagePath(kTestUser1, "jpg"));

  scoped_ptr<gfx::ImageSkia> saved_image =
      test::ImageLoader(GetUserImagePath(kTestUser1, "jpg")).Load();
  ASSERT_TRUE(saved_image);

  // Check image dimensions. Images can't be compared since JPEG is lossy.
  EXPECT_EQ(policy_image_->width(), saved_image->width());
  EXPECT_EQ(policy_image_->height(), saved_image->height());
}

IN_PROC_BROWSER_TEST_F(UserImageManagerPolicyTest,
                       PRE_UserDoesNotOverridePolicy) {
  RegisterUser(kTestUser1);
  chromeos::StartupUtils::MarkOobeCompleted();
}

// Verifies that when the user image has been set through policy and the user
// chooses a different image, the policy takes precedence, preventing the user
// from overriding the previously chosen image.
IN_PROC_BROWSER_TEST_F(UserImageManagerPolicyTest, UserDoesNotOverridePolicy) {
  const User* user = UserManager::Get()->FindUser(kTestUser1);
  ASSERT_TRUE(user);

  LoginUser(kTestUser1);
  base::RunLoop().RunUntilIdle();

  policy::CloudPolicyStore* store = GetStoreForUser(user);
  ASSERT_TRUE(store);

  // Set policy. Verify that the policy-provided user image is downloaded, set
  // and persisted.
  user_policy_.payload().mutable_useravatarimage()->set_value(
      ConstructPolicy(test::kUserAvatarImage2RelativePath));
  user_policy_.Build();
  fake_session_manager_client_->set_user_policy(kTestUser1,
                                                user_policy_.GetBlob());
  run_loop_.reset(new base::RunLoop);
  store->Load();
  run_loop_->Run();

  EXPECT_FALSE(user->HasDefaultImage());
  EXPECT_EQ(User::kExternalImageIndex, user->image_index());
  EXPECT_TRUE(test::AreImagesEqual(*policy_image_, user->image()));
  ExpectNewUserImageInfo(kTestUser1,
                         User::kExternalImageIndex,
                         GetUserImagePath(kTestUser1, "jpg"));

  scoped_ptr<gfx::ImageSkia> saved_image =
      test::ImageLoader(GetUserImagePath(kTestUser1, "jpg")).Load();
  ASSERT_TRUE(saved_image);

  // Check image dimensions. Images can't be compared since JPEG is lossy.
  EXPECT_EQ(policy_image_->width(), saved_image->width());
  EXPECT_EQ(policy_image_->height(), saved_image->height());

  // Choose a different user image. Verify that the user image does not change
  // as policy takes precedence.
  UserImageManager* user_image_manager =
      UserManager::Get()->GetUserImageManager();
  user_image_manager->SaveUserDefaultImageIndex(kTestUser1,
                                                kFirstDefaultImageIndex);

  EXPECT_FALSE(user->HasDefaultImage());
  EXPECT_EQ(User::kExternalImageIndex, user->image_index());
  EXPECT_TRUE(test::AreImagesEqual(*policy_image_, user->image()));
  ExpectNewUserImageInfo(kTestUser1,
                         User::kExternalImageIndex,
                         GetUserImagePath(kTestUser1, "jpg"));

  saved_image = test::ImageLoader(GetUserImagePath(kTestUser1, "jpg")).Load();
  ASSERT_TRUE(saved_image);

  // Check image dimensions. Images can't be compared since JPEG is lossy.
  EXPECT_EQ(policy_image_->width(), saved_image->width());
  EXPECT_EQ(policy_image_->height(), saved_image->height());
}

}  // namespace chromeos
