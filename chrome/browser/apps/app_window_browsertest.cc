// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/shell_window_geometry_cache.h"
#include "chrome/browser/apps/app_browsertest_util.h"
#include "chrome/browser/extensions/extension_test_message_listener.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/test_utils.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"

using apps::ShellWindowGeometryCache;

// This helper class can be used to wait for changes in the shell window
// geometry cache registry for a specific window in a specific extension.
class GeometryCacheChangeHelper : ShellWindowGeometryCache::Observer {
 public:
  GeometryCacheChangeHelper(ShellWindowGeometryCache* cache,
                            const std::string& extension_id,
                            const std::string& window_id,
                            const gfx::Rect& bounds)
      : cache_(cache),
        extension_id_(extension_id),
        window_id_(window_id),
        bounds_(bounds),
        satisfied_(false),
        waiting_(false) {
    cache_->AddObserver(this);
  }

  // This method will block until the shell window geometry cache registry will
  // provide a bound for |window_id_| that is entirely different (as in x/y/w/h)
  // from the initial |bounds_|.
  void WaitForEntirelyChanged() {
    if (satisfied_)
      return;

    waiting_ = true;
    content::RunMessageLoop();
  }

  // Implements the content::NotificationObserver interface.
  virtual void OnGeometryCacheChanged(const std::string& extension_id,
                                      const std::string& window_id,
                                      const gfx::Rect& bounds)
      OVERRIDE {
    if (extension_id != extension_id_ || window_id != window_id_)
      return;

    if (bounds_.x() != bounds.x() &&
        bounds_.y() != bounds.y() &&
        bounds_.width() != bounds.width() &&
        bounds_.height() != bounds.height()) {
      satisfied_ = true;
      cache_->RemoveObserver(this);

      if (waiting_)
        base::MessageLoopForUI::current()->Quit();
    }
  }

 private:
  ShellWindowGeometryCache* cache_;
  std::string extension_id_;
  std::string window_id_;
  gfx::Rect bounds_;
  bool satisfied_;
  bool waiting_;
};

// Helper class for tests related to the Apps Window API (chrome.app.window).
class AppWindowAPITest : public extensions::PlatformAppBrowserTest {
 public:
  bool RunAppWindowAPITest(const char* testName) {
    ExtensionTestMessageListener launched_listener("Launched", true);
    LoadAndLaunchPlatformApp("window_api");
    if (!launched_listener.WaitUntilSatisfied()) {
      message_ = "Did not get the 'Launched' message.";
      return false;
    }

    ResultCatcher catcher;
    launched_listener.Reply(testName);

    if (!catcher.GetNextResult()) {
      message_ = catcher.message();
      return false;
    }

    return true;
  }
};

// These tests are flaky after https://codereview.chromium.org/57433010/.
// See http://crbug.com/319613.

IN_PROC_BROWSER_TEST_F(AppWindowAPITest, DISABLED_TestCreate) {
  ASSERT_TRUE(RunAppWindowAPITest("testCreate")) << message_;
}

IN_PROC_BROWSER_TEST_F(AppWindowAPITest, TestSingleton) {
  ASSERT_TRUE(RunAppWindowAPITest("testSingleton")) << message_;
}

IN_PROC_BROWSER_TEST_F(AppWindowAPITest, DISABLED_TestBounds) {
  ASSERT_TRUE(RunAppWindowAPITest("testBounds")) << message_;
}

IN_PROC_BROWSER_TEST_F(AppWindowAPITest, TestCloseEvent) {
  ASSERT_TRUE(RunAppWindowAPITest("testCloseEvent")) << message_;
}

IN_PROC_BROWSER_TEST_F(AppWindowAPITest, DISABLED_TestMaximize) {
  ASSERT_TRUE(RunAppWindowAPITest("testMaximize")) << message_;
}

IN_PROC_BROWSER_TEST_F(AppWindowAPITest, DISABLED_TestRestore) {
  ASSERT_TRUE(RunAppWindowAPITest("testRestore")) << message_;
}

IN_PROC_BROWSER_TEST_F(AppWindowAPITest, DISABLED_TestRestoreAfterClose) {
  ASSERT_TRUE(RunAppWindowAPITest("testRestoreAfterClose")) << message_;
}

IN_PROC_BROWSER_TEST_F(AppWindowAPITest, DISABLED_TestSizeConstraints) {
  ASSERT_TRUE(RunAppWindowAPITest("testSizeConstraints")) << message_;
}

// Flaky failures on mac_rel and WinXP, see http://crbug.com/324915.
IN_PROC_BROWSER_TEST_F(AppWindowAPITest,
                       DISABLED_TestRestoreGeometryCacheChange) {
  // This test is similar to the other AppWindowAPI tests except that at some
  // point the app will send a 'ListenGeometryChange' message at which point the
  // test will check if the geometry cache entry for the test window has
  // changed. When the change happens, the test will let the app know so it can
  // continue running.
  ExtensionTestMessageListener launched_listener("Launched", true);

  content::WindowedNotificationObserver app_loaded_observer(
      content::NOTIFICATION_LOAD_COMPLETED_MAIN_FRAME,
      content::NotificationService::AllSources());

  const extensions::Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("platform_apps").AppendASCII("window_api"));
  EXPECT_TRUE(extension);

  OpenApplication(AppLaunchParams(browser()->profile(),
                                  extension,
                                  extensions::LAUNCH_CONTAINER_NONE,
                                  NEW_WINDOW));

  ExtensionTestMessageListener geometry_listener("ListenGeometryChange", true);

  ASSERT_TRUE(launched_listener.WaitUntilSatisfied());
  launched_listener.Reply("testRestoreAfterGeometryCacheChange");

  ASSERT_TRUE(geometry_listener.WaitUntilSatisfied());

  GeometryCacheChangeHelper geo_change_helper_1(
      ShellWindowGeometryCache::Get(browser()->profile()), extension->id(),
      // The next line has information that has to stay in sync with the app.
      "test-ra", gfx::Rect(200, 200, 200, 200));

  GeometryCacheChangeHelper geo_change_helper_2(
      ShellWindowGeometryCache::Get(browser()->profile()), extension->id(),
      // The next line has information that has to stay in sync with the app.
      "test-rb", gfx::Rect(200, 200, 200, 200));

  // These calls will block until the shell window geometry cache will change.
  geo_change_helper_1.WaitForEntirelyChanged();
  geo_change_helper_2.WaitForEntirelyChanged();

  ResultCatcher catcher;
  geometry_listener.Reply("");
  ASSERT_TRUE(catcher.GetNextResult());
}

