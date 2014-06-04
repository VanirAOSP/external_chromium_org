// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/cancelable_callback.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/prefs/pref_service.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_timeouts.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/devtools/browser_list_tabcontents_provider.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/unpacked_installer.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_modal_dialogs/javascript_app_modal_dialog.h"
#include "chrome/browser/ui/app_modal_dialogs/native_app_modal_dialog.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_iterator.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/test_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/devtools_client_host.h"
#include "content/public/browser/devtools_http_handler.h"
#include "content/public/browser/devtools_manager.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/worker_service.h"
#include "content/public/browser/worker_service_observer.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/common/switches.h"
#include "net/socket/tcp_listen_socket.h"
#include "net/test/spawned_test_server/spawned_test_server.h"

using content::BrowserThread;
using content::DevToolsManager;
using content::DevToolsAgentHost;
using content::NavigationController;
using content::RenderViewHost;
using content::WebContents;
using content::WorkerService;
using content::WorkerServiceObserver;

namespace {

const char kDebuggerTestPage[] = "files/devtools/debugger_test_page.html";
const char kPauseWhenLoadingDevTools[] =
    "files/devtools/pause_when_loading_devtools.html";
const char kPauseWhenScriptIsRunning[] =
    "files/devtools/pause_when_script_is_running.html";
const char kPageWithContentScript[] =
    "files/devtools/page_with_content_script.html";
const char kNavigateBackTestPage[] =
    "files/devtools/navigate_back.html";
const char kChunkedTestPage[] = "chunked";
const char kSlowTestPage[] =
    "chunked?waitBeforeHeaders=100&waitBetweenChunks=100&chunksNumber=2";
const char kSharedWorkerTestPage[] =
    "files/workers/workers_ui_shared_worker.html";
const char kReloadSharedWorkerTestPage[] =
    "files/workers/debug_shared_worker_initialization.html";

void RunTestFunction(DevToolsWindow* window, const char* test_name) {
  std::string result;

  // At first check that JavaScript part of the front-end is loaded by
  // checking that global variable uiTests exists(it's created after all js
  // files have been loaded) and has runTest method.
  ASSERT_TRUE(
      content::ExecuteScriptAndExtractString(
          window->GetRenderViewHost(),
          "window.domAutomationController.send("
          "    '' + (window.uiTests && (typeof uiTests.runTest)));",
          &result));

  if (result == "function") {
    ASSERT_TRUE(
        content::ExecuteScriptAndExtractString(
            window->GetRenderViewHost(),
            base::StringPrintf("uiTests.runTest('%s')", test_name),
            &result));
    EXPECT_EQ("[OK]", result);
  } else {
    FAIL() << "DevTools front-end is broken.";
  }
}

class DevToolsSanityTest : public InProcessBrowserTest {
 public:
  DevToolsSanityTest()
      : window_(NULL),
        inspected_rvh_(NULL) {}

 protected:
  void RunTest(const std::string& test_name, const std::string& test_page) {
    OpenDevToolsWindow(test_page);
    RunTestFunction(window_, test_name.c_str());
    CloseDevToolsWindow();
  }

  void LoadTestPage(const std::string& test_page) {
    content::WindowedNotificationObserver load_observer(
        content::NOTIFICATION_LOAD_STOP,
        content::NotificationService::AllSources());
    GURL url = test_server()->GetURL(test_page);
    ui_test_utils::NavigateToURL(browser(), url);
    load_observer.Wait();
  }

  void OpenDevToolsWindow(const std::string& test_page) {
    ASSERT_TRUE(test_server()->Start());
    LoadTestPage(test_page);

    content::WindowedNotificationObserver observer(
        content::NOTIFICATION_LOAD_STOP,
        content::NotificationService::AllSources());
    inspected_rvh_ = GetInspectedTab()->GetRenderViewHost();
    window_ = DevToolsWindow::OpenDevToolsWindow(inspected_rvh_);
    observer.Wait();
  }

  WebContents* GetInspectedTab() {
    return browser()->tab_strip_model()->GetWebContentsAt(0);
  }

  void ToggleDevToolsWindow() {
    content::WindowedNotificationObserver close_observer(
        content::NOTIFICATION_WEB_CONTENTS_DESTROYED,
        content::Source<content::WebContents>(window_->web_contents()));
    DevToolsWindow::ToggleDevToolsWindow(inspected_rvh_, false,
        DevToolsToggleAction::Toggle());
    close_observer.Wait();
  }

  void CloseDevToolsWindow() {
    DevToolsManager* devtools_manager = DevToolsManager::GetInstance();
    content::WindowedNotificationObserver close_observer(
        content::NOTIFICATION_WEB_CONTENTS_DESTROYED,
        content::Source<content::WebContents>(window_->web_contents()));
    devtools_manager->CloseAllClientHosts();
    close_observer.Wait();
  }

  DevToolsWindow* window_;
  RenderViewHost* inspected_rvh_;
};

// Used to block until a dev tools window gets beforeunload event.
class DevToolsWindowBeforeUnloadObserver
    : public content::WebContentsObserver {
 public:
  explicit DevToolsWindowBeforeUnloadObserver(DevToolsWindow*);
  void Wait();
 private:
  // Invoked when the beforeunload handler fires.
  virtual void BeforeUnloadFired(const base::TimeTicks& proceed_time) OVERRIDE;

  bool m_fired;
  scoped_refptr<content::MessageLoopRunner> message_loop_runner_;
  DISALLOW_COPY_AND_ASSIGN(DevToolsWindowBeforeUnloadObserver);
};

DevToolsWindowBeforeUnloadObserver::DevToolsWindowBeforeUnloadObserver(
    DevToolsWindow* devtools_window)
    : WebContentsObserver(devtools_window->web_contents()),
      m_fired(false) {
}

void DevToolsWindowBeforeUnloadObserver::Wait() {
  if (m_fired)
    return;
  message_loop_runner_ = new content::MessageLoopRunner;
  message_loop_runner_->Run();
}

void DevToolsWindowBeforeUnloadObserver::BeforeUnloadFired(
    const base::TimeTicks& proceed_time) {
  m_fired = true;
  if (message_loop_runner_.get())
    message_loop_runner_->Quit();
}

class DevToolsBeforeUnloadTest: public DevToolsSanityTest {
 public:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    command_line->AppendSwitch(
        switches::kDisableHangMonitor);
  }

  void CloseInspectedTab() {
    browser()->tab_strip_model()->CloseWebContentsAt(0,
        TabStripModel::CLOSE_NONE);
  }

  void CloseDockedDevTools() {
    DevToolsWindow::ToggleDevToolsWindow(inspected_rvh_, false,
        DevToolsToggleAction::Toggle());
  }

  void CloseUndockedDevTools() {
    chrome::CloseWindow(window_->browser());
  }

  void CloseInspectedBrowser() {
    chrome::CloseWindow(browser());
  }
 protected:
  void InjectBeforeUnloadListener(content::WebContents* web_contents) {
    ASSERT_TRUE(content::ExecuteScript(web_contents->GetRenderViewHost(),
        "window.addEventListener('beforeunload',"
        "function(event) { event.returnValue = 'Foo'; });"));
  }

  void RunBeforeUnloadSanityTest(DevToolsDockSide dock_side,
                             base::Callback<void(void)> close_method,
                             bool wait_for_browser_close = true) {
    OpenDevToolsWindow(kDebuggerTestPage);
    window_->SetDockSideForTest(dock_side);
    content::WindowedNotificationObserver devtools_close_observer(
        content::NOTIFICATION_WEB_CONTENTS_DESTROYED,
        content::Source<content::WebContents>(window_->web_contents()));
    InjectBeforeUnloadListener(window_->web_contents());
    {
      DevToolsWindowBeforeUnloadObserver before_unload_observer(window_);
      close_method.Run();
      CancelModalDialog();
      before_unload_observer.Wait();
    }
    {
      content::WindowedNotificationObserver close_observer(
          chrome::NOTIFICATION_BROWSER_CLOSED,
          content::Source<Browser>(browser()));
      close_method.Run();
      AcceptModalDialog();
      if (wait_for_browser_close)
        close_observer.Wait();
    }
    devtools_close_observer.Wait();
  }

  DevToolsWindow* OpenDevToolWindowOnWebContents(
      content::WebContents* contents) {
    content::WindowedNotificationObserver observer(
        content::NOTIFICATION_LOAD_STOP,
        content::NotificationService::AllSources());
    DevToolsWindow* window = DevToolsWindow::OpenDevToolsWindow(
        contents->GetRenderViewHost());
    observer.Wait();
    return window;
  }

  void OpenDevToolsPopupWindow(DevToolsWindow* devtools_window) {
    content::WindowedNotificationObserver observer(
        content::NOTIFICATION_LOAD_STOP,
        content::NotificationService::AllSources());
    ASSERT_TRUE(content::ExecuteScript(
        devtools_window->web_contents()->GetRenderViewHost(),
        "window.open(\"\", \"\", \"location=0\");"));
    observer.Wait();
  }

  void CloseDevToolsPopupWindow(DevToolsWindow* devtools_window) {
    Browser* popup_browser = NULL;
    for (chrome::BrowserIterator it; !it.done(); it.Next()) {
      if (it->is_devtools()) {
        content::WebContents* contents =
            it->tab_strip_model()->GetWebContentsAt(0);
        if (devtools_window->web_contents() != contents) {
          popup_browser = *it;
          break;
        }
      }
    }
    ASSERT_FALSE(popup_browser == NULL);
    content::WindowedNotificationObserver close_observer(
        chrome::NOTIFICATION_BROWSER_CLOSED,
        content::Source<Browser>(popup_browser));
    chrome::CloseWindow(popup_browser);
    close_observer.Wait();
  }

  void AcceptModalDialog() {
    NativeAppModalDialog* native_dialog = GetDialog();
    native_dialog->AcceptAppModalDialog();
  }

  void CancelModalDialog() {
    NativeAppModalDialog* native_dialog = GetDialog();
    native_dialog->CancelAppModalDialog();
  }

  NativeAppModalDialog* GetDialog() {
    AppModalDialog* dialog = ui_test_utils::WaitForAppModalDialog();
    EXPECT_TRUE(dialog->IsJavaScriptModalDialog());
    JavaScriptAppModalDialog* js_dialog =
        static_cast<JavaScriptAppModalDialog*>(dialog);
    NativeAppModalDialog* native_dialog = js_dialog->native_dialog();
    EXPECT_TRUE(native_dialog);
    return native_dialog;
  }
};

class DevToolsUnresponsiveBeforeUnloadTest: public DevToolsBeforeUnloadTest {
 public:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {}
};

void TimeoutCallback(const std::string& timeout_message) {
  FAIL() << timeout_message;
  base::MessageLoop::current()->Quit();
}

// Base class for DevTools tests that test devtools functionality for
// extensions and content scripts.
class DevToolsExtensionTest : public DevToolsSanityTest,
                              public content::NotificationObserver {
 public:
  DevToolsExtensionTest() : DevToolsSanityTest() {
    PathService::Get(chrome::DIR_TEST_DATA, &test_extensions_dir_);
    test_extensions_dir_ = test_extensions_dir_.AppendASCII("devtools");
    test_extensions_dir_ = test_extensions_dir_.AppendASCII("extensions");
  }

 protected:
  // Load an extension from test\data\devtools\extensions\<extension_name>
  void LoadExtension(const char* extension_name) {
    base::FilePath path = test_extensions_dir_.AppendASCII(extension_name);
    ASSERT_TRUE(LoadExtensionFromPath(path)) << "Failed to load extension.";
  }

 private:
  bool LoadExtensionFromPath(const base::FilePath& path) {
    ExtensionService* service = extensions::ExtensionSystem::Get(
        browser()->profile())->extension_service();
    size_t num_before = service->extensions()->size();
    {
      content::NotificationRegistrar registrar;
      registrar.Add(this, chrome::NOTIFICATION_EXTENSION_LOADED,
                    content::NotificationService::AllSources());
      base::CancelableClosure timeout(
          base::Bind(&TimeoutCallback, "Extension load timed out."));
      base::MessageLoop::current()->PostDelayedTask(
          FROM_HERE, timeout.callback(), TestTimeouts::action_timeout());
      extensions::UnpackedInstaller::Create(service)->Load(path);
      content::RunMessageLoop();
      timeout.Cancel();
    }
    size_t num_after = service->extensions()->size();
    if (num_after != (num_before + 1))
      return false;

    return WaitForExtensionViewsToLoad();
  }

  bool WaitForExtensionViewsToLoad() {
    // Wait for all the extension render views that exist to finish loading.
    // NOTE: This assumes that the extension views list is not changing while
    // this method is running.

    content::NotificationRegistrar registrar;
    registrar.Add(this, chrome::NOTIFICATION_EXTENSION_HOST_DID_STOP_LOADING,
                  content::NotificationService::AllSources());
    base::CancelableClosure timeout(
        base::Bind(&TimeoutCallback, "Extension host load timed out."));
    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE, timeout.callback(), TestTimeouts::action_timeout());

    extensions::ProcessManager* manager =
        extensions::ExtensionSystem::Get(browser()->profile())->
            process_manager();
    extensions::ProcessManager::ViewSet all_views = manager->GetAllViews();
    for (extensions::ProcessManager::ViewSet::const_iterator iter =
             all_views.begin();
         iter != all_views.end();) {
      if (!(*iter)->IsLoading())
        ++iter;
      else
        content::RunMessageLoop();
    }

    timeout.Cancel();
    return true;
  }

  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE {
    switch (type) {
      case chrome::NOTIFICATION_EXTENSION_LOADED:
      case chrome::NOTIFICATION_EXTENSION_HOST_DID_STOP_LOADING:
        base::MessageLoopForUI::current()->Quit();
        break;
      default:
        NOTREACHED();
        break;
    }
  }

  base::FilePath test_extensions_dir_;
};

class DevToolsExperimentalExtensionTest : public DevToolsExtensionTest {
 public:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    command_line->AppendSwitch(
        extensions::switches::kEnableExperimentalExtensionApis);
  }
};

class WorkerDevToolsSanityTest : public InProcessBrowserTest {
 public:
  WorkerDevToolsSanityTest() : window_(NULL) {}

 protected:
  class WorkerData : public base::RefCountedThreadSafe<WorkerData> {
   public:
    WorkerData() : worker_process_id(0), worker_route_id(0) {}
    int worker_process_id;
    int worker_route_id;

   private:
    friend class base::RefCountedThreadSafe<WorkerData>;
    ~WorkerData() {}
  };

  class WorkerCreationObserver : public WorkerServiceObserver {
   public:
    explicit WorkerCreationObserver(WorkerData* worker_data)
        : worker_data_(worker_data) {
    }

   private:
    virtual ~WorkerCreationObserver() {}

    virtual void WorkerCreated (
        const GURL& url,
        const base::string16& name,
        int process_id,
        int route_id) OVERRIDE {
      worker_data_->worker_process_id = process_id;
      worker_data_->worker_route_id = route_id;
      WorkerService::GetInstance()->RemoveObserver(this);
      BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
          base::MessageLoop::QuitClosure());
      delete this;
    }
    scoped_refptr<WorkerData> worker_data_;
  };

  class WorkerTerminationObserver : public WorkerServiceObserver {
   public:
    explicit WorkerTerminationObserver(WorkerData* worker_data)
        : worker_data_(worker_data) {
    }

   private:
    virtual ~WorkerTerminationObserver() {}

    virtual void WorkerDestroyed(int process_id, int route_id) OVERRIDE {
      ASSERT_EQ(worker_data_->worker_process_id, process_id);
      ASSERT_EQ(worker_data_->worker_route_id, route_id);
      WorkerService::GetInstance()->RemoveObserver(this);
      BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
          base::MessageLoop::QuitClosure());
      delete this;
    }
    scoped_refptr<WorkerData> worker_data_;
  };

  void RunTest(const char* test_name, const char* test_page) {
    ASSERT_TRUE(test_server()->Start());
    GURL url = test_server()->GetURL(test_page);
    ui_test_utils::NavigateToURL(browser(), url);

    scoped_refptr<WorkerData> worker_data = WaitForFirstSharedWorker();
    OpenDevToolsWindowForSharedWorker(worker_data.get());
    RunTestFunction(window_, test_name);
    CloseDevToolsWindow();
  }

  static void TerminateWorkerOnIOThread(scoped_refptr<WorkerData> worker_data) {
    if (WorkerService::GetInstance()->TerminateWorker(
            worker_data->worker_process_id, worker_data->worker_route_id)) {
      WorkerService::GetInstance()->AddObserver(
          new WorkerTerminationObserver(worker_data.get()));
      return;
    }
    FAIL() << "Failed to terminate worker.\n";
  }

  static void TerminateWorker(scoped_refptr<WorkerData> worker_data) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&TerminateWorkerOnIOThread, worker_data));
    content::RunMessageLoop();
  }

  static void WaitForFirstSharedWorkerOnIOThread(
      scoped_refptr<WorkerData> worker_data) {
    std::vector<WorkerService::WorkerInfo> worker_info =
        WorkerService::GetInstance()->GetWorkers();
    if (!worker_info.empty()) {
      worker_data->worker_process_id = worker_info[0].process_id;
      worker_data->worker_route_id = worker_info[0].route_id;
      BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
          base::MessageLoop::QuitClosure());
      return;
    }

    WorkerService::GetInstance()->AddObserver(
        new WorkerCreationObserver(worker_data.get()));
  }

  static scoped_refptr<WorkerData> WaitForFirstSharedWorker() {
    scoped_refptr<WorkerData> worker_data(new WorkerData());
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&WaitForFirstSharedWorkerOnIOThread, worker_data));
    content::RunMessageLoop();
    return worker_data;
  }

  void OpenDevToolsWindowForSharedWorker(WorkerData* worker_data) {
    Profile* profile = browser()->profile();
    window_ = DevToolsWindow::CreateDevToolsWindowForWorker(profile);
    window_->Show(DevToolsToggleAction::Show());
    scoped_refptr<DevToolsAgentHost> agent_host(
        DevToolsAgentHost::GetForWorker(
            worker_data->worker_process_id,
            worker_data->worker_route_id));
    DevToolsManager::GetInstance()->RegisterDevToolsClientHostFor(
        agent_host.get(), window_->GetDevToolsClientHostForTest());
    RenderViewHost* client_rvh = window_->GetRenderViewHost();
    WebContents* client_contents = WebContents::FromRenderViewHost(client_rvh);
    if (client_contents->IsLoading()) {
      content::WindowedNotificationObserver observer(
          content::NOTIFICATION_LOAD_STOP,
          content::Source<NavigationController>(
              &client_contents->GetController()));
      observer.Wait();
    }
  }

  void CloseDevToolsWindow() {
    Browser* browser = window_->browser();
    content::WindowedNotificationObserver close_observer(
        content::NOTIFICATION_WEB_CONTENTS_DESTROYED,
        content::Source<content::WebContents>(window_->web_contents()));
    browser->tab_strip_model()->CloseAllTabs();
    close_observer.Wait();
  }

  DevToolsWindow* window_;
};

// Tests that BeforeUnload event gets called on docked devtools if
// we try to close them.
IN_PROC_BROWSER_TEST_F(DevToolsBeforeUnloadTest, TestDockedDevToolsClose) {
  RunBeforeUnloadSanityTest(DEVTOOLS_DOCK_SIDE_BOTTOM, base::Bind(
      &DevToolsBeforeUnloadTest::CloseDockedDevTools, this), false);
}

// Tests that BeforeUnload event gets called on docked devtools if
// we try to close the inspected page.
IN_PROC_BROWSER_TEST_F(DevToolsBeforeUnloadTest,
                       TestDockedDevToolsInspectedTabClose) {
  RunBeforeUnloadSanityTest(DEVTOOLS_DOCK_SIDE_BOTTOM, base::Bind(
      &DevToolsBeforeUnloadTest::CloseInspectedTab, this));
}

// Tests that BeforeUnload event gets called on docked devtools if
// we try to close the inspected browser.
IN_PROC_BROWSER_TEST_F(DevToolsBeforeUnloadTest,
                       TestDockedDevToolsInspectedBrowserClose) {
  RunBeforeUnloadSanityTest(DEVTOOLS_DOCK_SIDE_BOTTOM, base::Bind(
      &DevToolsBeforeUnloadTest::CloseInspectedBrowser, this));
}

// Tests that BeforeUnload event gets called on undocked devtools if
// we try to close them.
IN_PROC_BROWSER_TEST_F(DevToolsBeforeUnloadTest, TestUndockedDevToolsClose) {
  RunBeforeUnloadSanityTest(DEVTOOLS_DOCK_SIDE_UNDOCKED, base::Bind(
      &DevToolsBeforeUnloadTest::CloseUndockedDevTools, this), false);
}

// Tests that BeforeUnload event gets called on undocked devtools if
// we try to close the inspected page.
IN_PROC_BROWSER_TEST_F(DevToolsBeforeUnloadTest,
                       TestUndockedDevToolsInspectedTabClose) {
  RunBeforeUnloadSanityTest(DEVTOOLS_DOCK_SIDE_UNDOCKED, base::Bind(
      &DevToolsBeforeUnloadTest::CloseInspectedTab, this));
}

// Tests that BeforeUnload event gets called on undocked devtools if
// we try to close the inspected browser.
IN_PROC_BROWSER_TEST_F(DevToolsBeforeUnloadTest,
                       TestUndockedDevToolsInspectedBrowserClose) {
  RunBeforeUnloadSanityTest(DEVTOOLS_DOCK_SIDE_UNDOCKED, base::Bind(
      &DevToolsBeforeUnloadTest::CloseInspectedBrowser, this));
}

// Tests that BeforeUnload event gets called on undocked devtools if
// we try to exit application.
IN_PROC_BROWSER_TEST_F(DevToolsBeforeUnloadTest,
                       TestUndockedDevToolsApplicationClose) {
  RunBeforeUnloadSanityTest(DEVTOOLS_DOCK_SIDE_UNDOCKED, base::Bind(
      &chrome::CloseAllBrowsers));
}

// Tests that inspected tab gets closed if devtools renderer
// becomes unresponsive during beforeunload event interception.
// @see http://crbug.com/322380
IN_PROC_BROWSER_TEST_F(DevToolsUnresponsiveBeforeUnloadTest,
                       TestUndockedDevToolsUnresponsive) {
  ASSERT_TRUE(test_server()->Start());
  LoadTestPage(kDebuggerTestPage);
  DevToolsWindow* devtools_window = OpenDevToolWindowOnWebContents(
      GetInspectedTab());
  devtools_window->SetDockSideForTest(DEVTOOLS_DOCK_SIDE_UNDOCKED);
  content::WindowedNotificationObserver devtools_close_observer(
      content::NOTIFICATION_WEB_CONTENTS_DESTROYED,
      content::Source<content::WebContents>(
          devtools_window->web_contents()));

  ASSERT_TRUE(content::ExecuteScript(
      devtools_window->web_contents()->GetRenderViewHost(),
      "window.addEventListener('beforeunload',"
      "function(event) { while (true); });"));
  CloseInspectedTab();
  devtools_close_observer.Wait();
}

// Tests that closing worker inspector window does not cause browser crash
// @see http://crbug.com/323031
IN_PROC_BROWSER_TEST_F(DevToolsBeforeUnloadTest,
                       TestWorkerWindowClosing) {
  ASSERT_TRUE(test_server()->Start());
  LoadTestPage(kDebuggerTestPage);
  DevToolsWindow* devtools_window = OpenDevToolWindowOnWebContents(
      GetInspectedTab());
  devtools_window->SetDockSideForTest(DEVTOOLS_DOCK_SIDE_UNDOCKED);
  content::WindowedNotificationObserver devtools_close_observer(
      content::NOTIFICATION_WEB_CONTENTS_DESTROYED,
      content::Source<content::WebContents>(
          devtools_window->web_contents()));

  OpenDevToolsPopupWindow(devtools_window);
  CloseDevToolsPopupWindow(devtools_window);
}

// Tests that BeforeUnload event gets called on devtools that are opened
// on another devtools.
IN_PROC_BROWSER_TEST_F(DevToolsBeforeUnloadTest,
                       TestDevToolsOnDevTools) {
  ASSERT_TRUE(test_server()->Start());
  LoadTestPage(kDebuggerTestPage);

  std::vector<DevToolsWindow*> windows;
  std::vector<content::WindowedNotificationObserver*> close_observers;
  content::WebContents* inspected_web_contents = GetInspectedTab();
  for (int i = 0; i < 3; ++i) {
    DevToolsWindow* devtools_window = OpenDevToolWindowOnWebContents(
      inspected_web_contents);
    windows.push_back(devtools_window);
    content::WindowedNotificationObserver* close_observer =
        new content::WindowedNotificationObserver(
                content::NOTIFICATION_WEB_CONTENTS_DESTROYED,
                content::Source<content::WebContents>(
                    devtools_window->web_contents()));
    close_observers.push_back(close_observer);
    inspected_web_contents = devtools_window->web_contents();
  }

  InjectBeforeUnloadListener(windows[0]->web_contents());
  InjectBeforeUnloadListener(windows[2]->web_contents());
  // Try to close second devtools.
  {
    content::WindowedNotificationObserver cancel_browser(
        chrome::NOTIFICATION_BROWSER_CLOSE_CANCELLED,
        content::NotificationService::AllSources());
    chrome::CloseWindow(windows[1]->browser());
    CancelModalDialog();
    cancel_browser.Wait();
  }
  // Try to close browser window.
  {
    content::WindowedNotificationObserver cancel_browser(
        chrome::NOTIFICATION_BROWSER_CLOSE_CANCELLED,
        content::NotificationService::AllSources());
    chrome::CloseWindow(browser());
    AcceptModalDialog();
    CancelModalDialog();
    cancel_browser.Wait();
  }
  // Try to exit application.
  {
    content::WindowedNotificationObserver close_observer(
        chrome::NOTIFICATION_BROWSER_CLOSED,
        content::Source<Browser>(browser()));
    chrome::CloseAllBrowsers();
    AcceptModalDialog();
    AcceptModalDialog();
    close_observer.Wait();
  }
  for (size_t i = 0; i < close_observers.size(); ++i) {
    close_observers[i]->Wait();
    delete close_observers[i];
  }
}

// Tests scripts panel showing.
// TODO(pfeldman): figure out flake.
IN_PROC_BROWSER_TEST_F(DevToolsSanityTest, DISABLED_TestShowScriptsTab) {
  RunTest("testShowScriptsTab", kDebuggerTestPage);
}

// Tests that scripts tab is populated with inspected scripts even if it
// hadn't been shown by the moment inspected paged refreshed.
// @see http://crbug.com/26312
IN_PROC_BROWSER_TEST_F(
    DevToolsSanityTest,
    TestScriptsTabIsPopulatedOnInspectedPageRefresh) {
  // Clear inspector settings to ensure that Elements will be
  // current panel when DevTools window is open.
  content::BrowserContext* browser_context =
      GetInspectedTab()->GetBrowserContext();
  Profile::FromBrowserContext(browser_context)->GetPrefs()->
      ClearPref(prefs::kWebKitInspectorSettings);

  RunTest("testScriptsTabIsPopulatedOnInspectedPageRefresh",
          kDebuggerTestPage);
}

// Tests that chrome.devtools extension is correctly exposed.
IN_PROC_BROWSER_TEST_F(DevToolsExtensionTest,
                       TestDevToolsExtensionAPI) {
  LoadExtension("devtools_extension");
  RunTest("waitForTestResultsInConsole", std::string());
}

// Disabled on Windows due to flakiness. http://crbug.com/183649
#if defined(OS_WIN)
#define MAYBE_TestDevToolsExtensionMessaging DISABLED_TestDevToolsExtensionMessaging
#else
#define MAYBE_TestDevToolsExtensionMessaging TestDevToolsExtensionMessaging
#endif

// Tests that chrome.devtools extension can communicate with background page
// using extension messaging.
IN_PROC_BROWSER_TEST_F(DevToolsExtensionTest,
                       MAYBE_TestDevToolsExtensionMessaging) {
  LoadExtension("devtools_messaging");
  RunTest("waitForTestResultsInConsole", std::string());
}

// Tests that chrome.experimental.devtools extension is correctly exposed
// when the extension has experimental permission.
IN_PROC_BROWSER_TEST_F(DevToolsExperimentalExtensionTest,
                       TestDevToolsExperimentalExtensionAPI) {
  LoadExtension("devtools_experimental");
  RunTest("waitForTestResultsInConsole", std::string());
}

// Tests that a content script is in the scripts list.
// History of flakiness: http://crbug.com/114104, http://crbug.com/315288.
IN_PROC_BROWSER_TEST_F(DevToolsExtensionTest,
                       DISABLED_TestContentScriptIsPresent) {
  LoadExtension("simple_content_script");
  RunTest("testContentScriptIsPresent", kPageWithContentScript);
}

// Fails quite consistently on Win XP: crbug.com/317725.
#if defined(OS_WIN)
#define MAYBE_TestNoScriptDuplicatesOnPanelSwitch \
  DISABLED_TestNoScriptDuplicatesOnPanelSwitch
#else
#define MAYBE_TestNoScriptDuplicatesOnPanelSwitch \
  TestNoScriptDuplicatesOnPanelSwitch
#endif

// Tests that scripts are not duplicated after Scripts Panel switch.
IN_PROC_BROWSER_TEST_F(DevToolsSanityTest,
                       MAYBE_TestNoScriptDuplicatesOnPanelSwitch) {
  RunTest("testNoScriptDuplicatesOnPanelSwitch", kDebuggerTestPage);
}

#if defined(OS_WIN) || defined(OS_LINUX) || defined(OS_CHROMEOS)
#define MAYBE_TestPauseWhenLoadingDevTools DISABLED_TestPauseWhenLoadingDevTools
#else
#define MAYBE_TestPauseWhenLoadingDevTools TestPauseWhenLoadingDevTools
#endif
// Tests that debugger works correctly if pause event occurs when DevTools
// frontend is being loaded.
// Disabled because of flakiness on multiple platforms: crbug.com/329036
IN_PROC_BROWSER_TEST_F(DevToolsSanityTest,
                       MAYBE_TestPauseWhenLoadingDevTools) {
  RunTest("testPauseWhenLoadingDevTools", kPauseWhenLoadingDevTools);
}

// Tests that pressing 'Pause' will pause script execution if the script
// is already running.
#if defined(OS_WIN)
// Timing out on windows tryservers: http://crbug.com/219515
#define MAYBE_TestPauseWhenScriptIsRunning DISABLED_TestPauseWhenScriptIsRunning
#elif defined(OS_LINUX) && !defined(OS_CHROMEOS) && defined(ARCH_CPU_ARM_FAMILY)
// Timing out on linux ARM bot: https://crbug/238453
#define MAYBE_TestPauseWhenScriptIsRunning DISABLED_TestPauseWhenScriptIsRunning
#else
#define MAYBE_TestPauseWhenScriptIsRunning TestPauseWhenScriptIsRunning
#endif
IN_PROC_BROWSER_TEST_F(DevToolsSanityTest,
                       MAYBE_TestPauseWhenScriptIsRunning) {
  RunTest("testPauseWhenScriptIsRunning", kPauseWhenScriptIsRunning);
}

// Tests network timing.
IN_PROC_BROWSER_TEST_F(DevToolsSanityTest, TestNetworkTiming) {
  RunTest("testNetworkTiming", kSlowTestPage);
}

// Tests network size.
IN_PROC_BROWSER_TEST_F(DevToolsSanityTest, TestNetworkSize) {
  RunTest("testNetworkSize", kChunkedTestPage);
}

// Tests raw headers text.
IN_PROC_BROWSER_TEST_F(DevToolsSanityTest, TestNetworkSyncSize) {
  RunTest("testNetworkSyncSize", kChunkedTestPage);
}

// Tests raw headers text.
IN_PROC_BROWSER_TEST_F(DevToolsSanityTest, TestNetworkRawHeadersText) {
  RunTest("testNetworkRawHeadersText", kChunkedTestPage);
}

// Tests that console messages are not duplicated on navigation back.
IN_PROC_BROWSER_TEST_F(DevToolsSanityTest, TestConsoleOnNavigateBack) {
  RunTest("testConsoleOnNavigateBack", kNavigateBackTestPage);
}


// Tests that external navigation from inspector page is always handled by
// DevToolsWindow and results in inspected page navigation.
IN_PROC_BROWSER_TEST_F(DevToolsSanityTest, TestDevToolsExternalNavigation) {
  OpenDevToolsWindow(kDebuggerTestPage);
  GURL url = test_server()->GetURL(kNavigateBackTestPage);
  content::WindowedNotificationObserver observer(
      content::NOTIFICATION_LOAD_STOP,
      content::NotificationService::AllSources());
  ASSERT_TRUE(content::ExecuteScript(
      window_->web_contents(),
      std::string("window.location = \"") + url.spec() + "\""));
  observer.Wait();

  ASSERT_TRUE(window_->web_contents()->GetURL().
                  SchemeIs(chrome::kChromeDevToolsScheme));
  ASSERT_EQ(GetInspectedTab()->GetURL(), url);
  CloseDevToolsWindow();
}

#if defined(OS_WIN)
// Flakily times out: http://crbug.com/163411
#define MAYBE_TestReattachAfterCrash DISABLED_TestReattachAfterCrash
#else
#define MAYBE_TestReattachAfterCrash TestReattachAfterCrash
#endif
// Tests that inspector will reattach to inspected page when it is reloaded
// after a crash. See http://crbug.com/101952
IN_PROC_BROWSER_TEST_F(DevToolsSanityTest, MAYBE_TestReattachAfterCrash) {
  OpenDevToolsWindow(kDebuggerTestPage);

  content::CrashTab(GetInspectedTab());
  content::WindowedNotificationObserver observer(
      content::NOTIFICATION_LOAD_STOP,
      content::Source<NavigationController>(
          &browser()->tab_strip_model()->GetActiveWebContents()->
              GetController()));
  chrome::Reload(browser(), CURRENT_TAB);
  observer.Wait();

  RunTestFunction(window_, "testReattachAfterCrash");
  CloseDevToolsWindow();
}

IN_PROC_BROWSER_TEST_F(DevToolsSanityTest, TestPageWithNoJavaScript) {
  OpenDevToolsWindow("about:blank");
  std::string result;
  ASSERT_TRUE(
      content::ExecuteScriptAndExtractString(
          window_->GetRenderViewHost(),
          "window.domAutomationController.send("
          "    '' + (window.uiTests && (typeof uiTests.runTest)));",
          &result));
  ASSERT_EQ("function", result) << "DevTools front-end is broken.";
  CloseDevToolsWindow();
}

#if defined(OS_MACOSX)
#define MAYBE_InspectSharedWorker DISABLED_InspectSharedWorker
#else
#define MAYBE_InspectSharedWorker InspectSharedWorker
#endif
// Flakily fails with 25s timeout: http://crbug.com/89845
IN_PROC_BROWSER_TEST_F(WorkerDevToolsSanityTest, MAYBE_InspectSharedWorker) {
#if defined(OS_WIN) && defined(USE_ASH)
  // Disable this test in Metro+Ash for now (http://crbug.com/262796).
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kAshBrowserTests))
    return;
#endif

  RunTest("testSharedWorker", kSharedWorkerTestPage);
}

// http://crbug.com/100538
#if defined(OS_MACOSX) || defined(OS_WIN)
#define MAYBE_PauseInSharedWorkerInitialization DISABLED_PauseInSharedWorkerInitialization
#else
#define MAYBE_PauseInSharedWorkerInitialization PauseInSharedWorkerInitialization
#endif

// http://crbug.com/106114 is masking
// MAYBE_PauseInSharedWorkerInitialization into
// DISABLED_PauseInSharedWorkerInitialization
IN_PROC_BROWSER_TEST_F(WorkerDevToolsSanityTest,
                       MAYBE_PauseInSharedWorkerInitialization) {
  ASSERT_TRUE(test_server()->Start());
  GURL url = test_server()->GetURL(kReloadSharedWorkerTestPage);
  ui_test_utils::NavigateToURL(browser(), url);

  scoped_refptr<WorkerData> worker_data = WaitForFirstSharedWorker();
  OpenDevToolsWindowForSharedWorker(worker_data.get());

  TerminateWorker(worker_data);

  // Reload page to restart the worker.
  ui_test_utils::NavigateToURL(browser(), url);

  // Wait until worker script is paused on the debugger statement.
  RunTestFunction(window_, "testPauseInSharedWorkerInitialization");
  CloseDevToolsWindow();
}

class DevToolsAgentHostTest : public InProcessBrowserTest {};

// Tests DevToolsAgentHost retention by its target.
IN_PROC_BROWSER_TEST_F(DevToolsAgentHostTest, TestAgentHostReleased) {
  ui_test_utils::NavigateToURL(browser(), GURL("about:blank"));
  RenderViewHost* rvh = browser()->tab_strip_model()->GetWebContentsAt(0)->
      GetRenderViewHost();
  DevToolsAgentHost* agent_raw = DevToolsAgentHost::GetOrCreateFor(rvh).get();
  const std::string agent_id = agent_raw->GetId();
  ASSERT_EQ(agent_raw, DevToolsAgentHost::GetForId(agent_id)) <<
      "DevToolsAgentHost cannot be found by id";
  browser()->tab_strip_model()->
      CloseWebContentsAt(0, TabStripModel::CLOSE_NONE);
  ASSERT_FALSE(DevToolsAgentHost::GetForId(agent_id).get())
      << "DevToolsAgentHost is not released when the tab is closed";
}

class RemoteDebuggingTest: public ExtensionApiTest {
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    ExtensionApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(switches::kRemoteDebuggingPort, "9222");

    // Override the extension root path.
    PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir_);
    test_data_dir_ = test_data_dir_.AppendASCII("devtools");
  }
};

IN_PROC_BROWSER_TEST_F(RemoteDebuggingTest, RemoteDebugger) {
#if defined(OS_WIN) && defined(USE_ASH)
  // Disable this test in Metro+Ash for now (http://crbug.com/262796).
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kAshBrowserTests))
    return;
#endif

  ASSERT_TRUE(RunExtensionTest("target_list")) << message_;
}

}  // namespace
