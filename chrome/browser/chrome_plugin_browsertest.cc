// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/prefs/pref_service.h"
#include "base/process/kill.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/plugins/plugin_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/browser_child_process_host_iterator.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_paths.h"
#include "content/public/common/process_type.h"
#include "content/public/common/webplugininfo.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "net/base/net_util.h"

#if defined(OS_WIN)
#include "content/public/browser/web_contents_view.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#endif

using content::BrowserThread;

namespace {

class CallbackBarrier : public base::RefCountedThreadSafe<CallbackBarrier> {
 public:
  explicit CallbackBarrier(const base::Closure& target_callback)
      : target_callback_(target_callback),
        outstanding_callbacks_(0),
        did_enable_(true) {
  }

  base::Callback<void(bool)> CreateCallback() {
    outstanding_callbacks_++;
    return base::Bind(&CallbackBarrier::MayRunTargetCallback, this);
  }

 private:
  friend class base::RefCountedThreadSafe<CallbackBarrier>;

  ~CallbackBarrier() {
    EXPECT_TRUE(target_callback_.is_null());
  }

  void MayRunTargetCallback(bool did_enable) {
    EXPECT_GT(outstanding_callbacks_, 0);
    did_enable_ = did_enable_ && did_enable;
    if (--outstanding_callbacks_ == 0) {
      EXPECT_TRUE(did_enable_);
      target_callback_.Run();
      target_callback_.Reset();
    }
  }

  base::Closure target_callback_;
  int outstanding_callbacks_;
  bool did_enable_;
};

}  // namespace

class ChromePluginTest : public InProcessBrowserTest {
 protected:
  ChromePluginTest() {}

  static GURL GetURL(const char* filename) {
    base::FilePath path;
    PathService::Get(content::DIR_TEST_DATA, &path);
    path = path.AppendASCII("plugin").AppendASCII(filename);
    CHECK(base::PathExists(path));
    return net::FilePathToFileURL(path);
  }

  static void LoadAndWait(Browser* window, const GURL& url, bool pass) {
    content::WebContents* web_contents =
        window->tab_strip_model()->GetActiveWebContents();
    base::string16 expected_title(ASCIIToUTF16(pass ? "OK" : "plugin_not_found"));
    content::TitleWatcher title_watcher(web_contents, expected_title);
    title_watcher.AlsoWaitForTitle(ASCIIToUTF16("FAIL"));
    title_watcher.AlsoWaitForTitle(ASCIIToUTF16(
        pass ? "plugin_not_found" : "OK"));
    ui_test_utils::NavigateToURL(window, url);
    ASSERT_EQ(expected_title, title_watcher.WaitAndGetTitle());
  }

  static void CrashFlash() {
    scoped_refptr<content::MessageLoopRunner> runner =
        new content::MessageLoopRunner;
    BrowserThread::PostTask(
        BrowserThread::IO,
        FROM_HERE,
        base::Bind(&CrashFlashInternal, runner->QuitClosure()));
    runner->Run();
  }

  static void GetFlashPath(std::vector<base::FilePath>* paths) {
    paths->clear();
    std::vector<content::WebPluginInfo> plugins = GetPlugins();
    for (std::vector<content::WebPluginInfo>::const_iterator it =
             plugins.begin(); it != plugins.end(); ++it) {
      if (it->name == ASCIIToUTF16(content::kFlashPluginName))
        paths->push_back(it->path);
    }
  }

  static std::vector<content::WebPluginInfo> GetPlugins() {
    std::vector<content::WebPluginInfo> plugins;
    scoped_refptr<content::MessageLoopRunner> runner =
        new content::MessageLoopRunner;
    content::PluginService::GetInstance()->GetPlugins(
        base::Bind(&GetPluginsInfoCallback, &plugins, runner->QuitClosure()));
    runner->Run();
    return plugins;
  }

  static void EnableFlash(bool enable, Profile* profile) {
    std::vector<base::FilePath> paths;
    GetFlashPath(&paths);
    ASSERT_FALSE(paths.empty());

    PluginPrefs* plugin_prefs = PluginPrefs::GetForProfile(profile).get();
    scoped_refptr<content::MessageLoopRunner> runner =
        new content::MessageLoopRunner;
    scoped_refptr<CallbackBarrier> callback_barrier(
        new CallbackBarrier(runner->QuitClosure()));
    for (std::vector<base::FilePath>::iterator iter = paths.begin();
         iter != paths.end(); ++iter) {
      plugin_prefs->EnablePlugin(enable, *iter,
                                 callback_barrier->CreateCallback());
    }
    runner->Run();
  }

  static void EnsureFlashProcessCount(int expected) {
    int actual = 0;
    scoped_refptr<content::MessageLoopRunner> runner =
        new content::MessageLoopRunner;
    BrowserThread::PostTask(
        BrowserThread::IO,
        FROM_HERE,
        base::Bind(&CountPluginProcesses, &actual, runner->QuitClosure()));
    runner->Run();
    ASSERT_EQ(expected, actual);
  }

 private:
  static void CrashFlashInternal(const base::Closure& quit_task) {
    bool found = false;
    for (content::BrowserChildProcessHostIterator iter; !iter.Done(); ++iter) {
      if (iter.GetData().process_type != content::PROCESS_TYPE_PLUGIN &&
          iter.GetData().process_type != content::PROCESS_TYPE_PPAPI_PLUGIN) {
        continue;
      }
      base::KillProcess(iter.GetData().handle, 0, true);
      found = true;
    }
    ASSERT_TRUE(found) << "Didn't find Flash process!";
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, quit_task);
  }

  static void GetPluginsInfoCallback(
      std::vector<content::WebPluginInfo>* rv,
      const base::Closure& quit_task,
      const std::vector<content::WebPluginInfo>& plugins) {
    *rv = plugins;
    quit_task.Run();
  }

  static void CountPluginProcesses(int* count, const base::Closure& quit_task) {
    for (content::BrowserChildProcessHostIterator iter; !iter.Done(); ++iter) {
      if (iter.GetData().process_type == content::PROCESS_TYPE_PLUGIN ||
          iter.GetData().process_type == content::PROCESS_TYPE_PPAPI_PLUGIN) {
        (*count)++;
      }
    }
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, quit_task);
  }
};

// Tests a bunch of basic scenarios with Flash.
// This test fails under ASan on Mac, see http://crbug.com/147004.
// It fails elsewhere, too.  See http://crbug.com/152071.
IN_PROC_BROWSER_TEST_F(ChromePluginTest, DISABLED_Flash) {
  // Official builds always have bundled Flash.
#if !defined(OFFICIAL_BUILD)
  std::vector<base::FilePath> flash_paths;
  GetFlashPath(&flash_paths);
  if (flash_paths.empty()) {
    LOG(INFO) << "Test not running because couldn't find Flash.";
    return;
  }
#endif

  GURL url = GetURL("flash.html");
  EnsureFlashProcessCount(0);

  // Try a single tab.
  ASSERT_NO_FATAL_FAILURE(LoadAndWait(browser(), url, true));
  EnsureFlashProcessCount(1);
  Profile* profile = browser()->profile();
  // Try another tab.
  ASSERT_NO_FATAL_FAILURE(LoadAndWait(CreateBrowser(profile), url, true));
  // Try an incognito window.
  ASSERT_NO_FATAL_FAILURE(LoadAndWait(CreateIncognitoBrowser(), url, true));
  EnsureFlashProcessCount(1);

  // Now kill Flash process and verify it reloads.
  CrashFlash();
  EnsureFlashProcessCount(0);

  ASSERT_NO_FATAL_FAILURE(LoadAndWait(browser(), url, true));
  EnsureFlashProcessCount(1);

  // Now try disabling it.
  EnableFlash(false, profile);
  CrashFlash();

  ASSERT_NO_FATAL_FAILURE(LoadAndWait(browser(), url, false));
  EnsureFlashProcessCount(0);

  // Now enable it again.
  EnableFlash(true, profile);
  ASSERT_NO_FATAL_FAILURE(LoadAndWait(browser(), url, true));
  EnsureFlashProcessCount(1);
}

// Verify that the official builds have the known set of plugins.
IN_PROC_BROWSER_TEST_F(ChromePluginTest, InstalledPlugins) {
#if !defined(OFFICIAL_BUILD)
  return;
#endif
  const char* expected[] = {
    "Chrome PDF Viewer",
    "Shockwave Flash",
    "Native Client",
    "Chrome Remote Desktop Viewer",
#if defined(OS_CHROMEOS)
    "Google Talk Plugin",
    "Google Talk Plugin Video Accelerator",
    "Netflix",
#endif
  };

  std::vector<content::WebPluginInfo> plugins = GetPlugins();
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(expected); ++i) {
    size_t j = 0;
    for (; j < plugins.size(); ++j) {
      if (plugins[j].name == ASCIIToUTF16(expected[i]))
        break;
    }
    ASSERT_TRUE(j != plugins.size()) << "Didn't find " << expected[i];
  }
}

#if defined(OS_WIN)

namespace {

BOOL CALLBACK EnumerateChildren(HWND hwnd, LPARAM l_param) {
  HWND* child = reinterpret_cast<HWND*>(l_param);
  *child = hwnd;
  // The first child window is the plugin, then its children. So stop
  // enumerating after the first callback.
  return FALSE;
}

}

// Test that if a background tab loads an NPAPI plugin, they are displayed after
// switching to that page.  http://crbug.com/335900
IN_PROC_BROWSER_TEST_F(ChromePluginTest, WindowedNPAPIPluginHidden) {
  browser()->profile()->GetPrefs()->SetBoolean(prefs::kPluginsAlwaysAuthorize,
                                               true);

  // First load the page in the background and wait for the NPAPI plugin's
  // window to be created.
  GURL url = ui_test_utils::GetTestUrl(
      base::FilePath(),
      base::FilePath().AppendASCII("windowed_npapi_plugin.html"));

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, NEW_BACKGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);

  // We create a third window just to trigger the second one to update its
  // constrained window list. Normally this would be triggered by the status bar
  // animation closing after the user middle clicked a link.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("about:blank"), NEW_BACKGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB);

  base::string16 expected_title(base::ASCIIToUTF16("created"));
  content::WebContents* tab =
      browser()->tab_strip_model()->GetWebContentsAt(1);
  if (tab->GetTitle() != expected_title) {
    content::TitleWatcher title_watcher(tab, expected_title);
    EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());
  }

  // Now activate the tab and verify that the plugin painted.
  browser()->tab_strip_model()->ActivateTabAt(1, true);

  base::string16 expected_title2(base::ASCIIToUTF16("shown"));
  content::TitleWatcher title_watcher2(tab, expected_title2);
  EXPECT_EQ(expected_title2, title_watcher2.WaitAndGetTitle());

  HWND child = NULL;
  HWND hwnd = tab->GetView()->GetNativeView()->GetDispatcher()->host()->
      GetAcceleratedWidget();
  EnumChildWindows(hwnd, EnumerateChildren,reinterpret_cast<LPARAM>(&child));

  RECT region;
  int result = GetWindowRgnBox(child, &region);
  ASSERT_NE(result, NULLREGION);
}

#endif
