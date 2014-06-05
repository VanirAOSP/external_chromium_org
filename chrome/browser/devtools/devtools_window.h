// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_DEVTOOLS_WINDOW_H_
#define CHROME_BROWSER_DEVTOOLS_DEVTOOLS_WINDOW_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "chrome/browser/devtools/devtools_embedder_message_dispatcher.h"
#include "chrome/browser/devtools/devtools_file_helper.h"
#include "chrome/browser/devtools/devtools_file_system_indexer.h"
#include "chrome/browser/devtools/devtools_toggle_action.h"
#include "content/public/browser/devtools_client_host.h"
#include "content/public/browser/devtools_frontend_host_delegate.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/web_contents_delegate.h"

class Browser;
class BrowserWindow;
class DevToolsControllerTest;
class Profile;

namespace base {
class Value;
}

namespace content {
class DevToolsAgentHost;
class DevToolsClientHost;
struct FileChooserParams;
class RenderViewHost;
class WebContents;
}

namespace IPC {
class Message;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

enum DevToolsDockSide {
  DEVTOOLS_DOCK_SIDE_UNDOCKED = 0,
  DEVTOOLS_DOCK_SIDE_BOTTOM,
  DEVTOOLS_DOCK_SIDE_RIGHT,
  DEVTOOLS_DOCK_SIDE_MINIMIZED
};

class DevToolsWindow : private content::NotificationObserver,
                       private content::WebContentsDelegate,
                       private content::DevToolsFrontendHostDelegate,
                       private DevToolsEmbedderMessageDispatcher::Delegate {
 public:
  typedef base::Callback<void(bool)> InfoBarCallback;

  static const char kDevToolsApp[];

  virtual ~DevToolsWindow();

  static std::string GetDevToolsWindowPlacementPrefKey();
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);
  // Return the DevToolsWindow for the given RenderViewHost if one exists,
  // otherwise NULL.
  static DevToolsWindow* GetInstanceForInspectedRenderViewHost(
      content::RenderViewHost* inspected_rvh);
  static DevToolsWindow* GetDockedInstanceForInspectedTab(
      content::WebContents* inspected_tab);
  static bool IsDevToolsWindow(content::RenderViewHost* window_rvh);
  static DevToolsWindow* OpenDevToolsWindowForWorker(
      Profile* profile,
      content::DevToolsAgentHost* worker_agent);
  static DevToolsWindow* CreateDevToolsWindowForWorker(Profile* profile);
  static DevToolsWindow* OpenDevToolsWindow(
      content::RenderViewHost* inspected_rvh);
  static DevToolsWindow* ToggleDevToolsWindow(
      Browser* browser,
      const DevToolsToggleAction& action);
  static void OpenExternalFrontend(Profile* profile,
                                   const std::string& frontend_uri,
                                   content::DevToolsAgentHost* agent_host);

  // Exposed for testing, normal clients should not use this method.
  static DevToolsWindow* ToggleDevToolsWindow(
      content::RenderViewHost* inspected_rvh,
      bool force_open,
      const DevToolsToggleAction& action);

  static void InspectElement(
      content::RenderViewHost* inspected_rvh, int x, int y);

  static int GetMinimumWidth();
  static int GetMinimumHeight();
  static int GetMinimizedHeight();

  // content::DevToolsFrontendHostDelegate:
  virtual void InspectedContentsClosing() OVERRIDE;

  content::WebContents* web_contents() { return web_contents_; }
  Browser* browser() { return browser_; }  // For tests.
  DevToolsDockSide dock_side() const { return dock_side_; }

  content::RenderViewHost* GetRenderViewHost();
  content::DevToolsClientHost* GetDevToolsClientHostForTest();

  // Returns preferred devtools window width for given |container_width|. It
  // tries to use the saved window width, or, if none exists, 1/3 of the
  // container width, then clamps to try and ensure both devtools and content
  // are at least somewhat visible.
  // Called only for the case when devtools window is docked to the side.
  int GetWidth(int container_width);

  // Returns preferred devtools window height for given |container_height|.
  // Uses the same logic as GetWidth.
  // Called only for the case when devtools window is docked to bottom.
  int GetHeight(int container_height);

  // Stores preferred devtools window width for this instance.
  void SetWidth(int width);

  // Stores preferred devtools window height for this instance.
  void SetHeight(int height);

  void Show(const DevToolsToggleAction& action);

  // BeforeUnload interception ////////////////////////////////////////////////

  // In order to preserve any edits the user may have made in devtools, the
  // beforeunload event of the inspected page is hooked - devtools gets the
  // first shot at handling beforeunload and presents a dialog to the user. If
  // the user accepts the dialog then the script is given a chance to handle
  // it. This way 2 dialogs may be displayed: one from the devtools asking the
  // user to confirm that they're ok with their devtools edits going away and
  // another from the webpage as the result of its beforeunload handler.
  // The following set of methods handle beforeunload event flow through
  // devtools window. When the |contents| with devtools opened on them are
  // getting closed, the following sequence of calls takes place:
  // 1. |DevToolsWindow::InterceptPageBeforeUnload| is called and indicates
  //    whether devtools intercept the beforeunload event.
  //    If InterceptPageBeforeUnload() returns true then the following steps
  //    will take place; otherwise only step 4 will be reached and none of the
  //    corresponding functions in steps 2 & 3 will get called.
  // 2. |DevToolsWindow::InterceptPageBeforeUnload| fires beforeunload event
  //    for devtools frontend, which will asynchronously call
  //    |WebContentsDelegate::BeforeUnloadFired| method.
  //    In case of docked devtools window, devtools are set as a delegate for
  //    its frontend, so method |DevToolsWindow::BeforeUnloadFired| will be
  //    called directly.
  //    If devtools window is undocked it's not set as the delegate so the call
  //    to BeforeUnloadFired is proxied through HandleBeforeUnload() rather
  //    than getting called directly.
  // 3a. If |DevToolsWindow::BeforeUnloadFired| is called with |proceed|=false
  //     it calls throught to the content's BeforeUnloadFired(), which from the
  //     WebContents perspective looks the same as the |content|'s own
  //     beforeunload dialog having had it's 'stay on this page' button clicked.
  // 3b. If |proceed| = true, then it fires beforeunload event on |contents|
  //     and everything proceeds as it normally would without the Devtools
  //     interception.
  // 4. If the user cancels the dialog put up by either the WebContents or
  //    devtools frontend, then |contents|'s |BeforeUnloadFired| callback is
  //    called with the proceed argument set to false, this causes
  //    |DevToolsWindow::OnPageCloseCancelled| to be called.

  // Devtools window in undocked state is not set as a delegate of
  // its frontend. Instead, an instance of browser is set as the delegate, and
  // thus beforeunload event callback from devtools frontend is not delivered
  // to the instance of devtools window, which is solely responsible for
  // managing custom beforeunload event flow.
  // This is a helper method to route callback from
  // |Browser::BeforeUnloadFired| back to |DevToolsWindow::BeforeUnloadFired|.
  // * |proceed| - true if the user clicked 'ok' in the beforeunload dialog,
  //   false otherwise.
  // * |proceed_to_fire_unload| - output parameter, whether we should continue
  //   to fire the unload event or stop things here.
  // Returns true if devtools window is in a state of intercepting beforeunload
  // event and if it will manage unload process on its own.
  static bool HandleBeforeUnload(content::WebContents* contents,
                                 bool proceed,
                                 bool* proceed_to_fire_unload);

  // Returns true if this contents beforeunload event was intercepted by
  // devtools and false otherwise. If the event was intercepted, caller should
  // not fire beforeunlaod event on |contents| itself as devtools window will
  // take care of it, otherwise caller should continue handling the event as
  // usual.
  static bool InterceptPageBeforeUnload(content::WebContents* contents);

  // Returns true if devtools browser has already fired its beforeunload event
  // as a result of beforeunload event interception.
  static bool HasFiredBeforeUnloadEventForDevToolsBrowser(Browser* browser);

  // Returns true if devtools window would like to hook beforeunload event
  // of this |contents|.
  static bool NeedsToInterceptBeforeUnload(content::WebContents* contents);

  // Notify devtools window that closing of |contents| was cancelled
  // by user.
  static void OnPageCloseCanceled(content::WebContents* contents);

  void SetDockSideForTest(DevToolsDockSide dock_side);

 private:
  friend class DevToolsControllerTest;

  DevToolsWindow(Profile* profile,
                 const GURL& frontend_url,
                 content::RenderViewHost* inspected_rvh,
                 DevToolsDockSide dock_side);

  static DevToolsWindow* Create(Profile* profile,
                                const GURL& frontend_url,
                                content::RenderViewHost* inspected_rvh,
                                DevToolsDockSide dock_side,
                                bool shared_worker_frontend,
                                bool external_frontend,
                                bool can_dock);
  static GURL GetDevToolsURL(Profile* profile,
                             const GURL& base_url,
                             DevToolsDockSide dock_side,
                             bool shared_worker_frontend,
                             bool external_frontend,
                             bool can_dock);
  static DevToolsWindow* FindDevToolsWindow(content::DevToolsAgentHost*);
  static DevToolsWindow* AsDevToolsWindow(content::RenderViewHost*);
  static DevToolsDockSide GetDockSideFromPrefs(Profile* profile);
  static std::string SideToString(DevToolsDockSide dock_side);
  static DevToolsDockSide SideFromString(const std::string& dock_side);
  static bool FindInspectedBrowserAndTabIndex(
      content::WebContents* inspected_web_contents, Browser**, int* tab);

  // content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // content::WebContentsDelegate:
  virtual content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params) OVERRIDE;
  virtual void AddNewContents(content::WebContents* source,
                              content::WebContents* new_contents,
                              WindowOpenDisposition disposition,
                              const gfx::Rect& initial_pos,
                              bool user_gesture,
                              bool* was_blocked) OVERRIDE;
  virtual void CloseContents(content::WebContents* source) OVERRIDE;
  virtual void BeforeUnloadFired(content::WebContents* tab,
                                 bool proceed,
                                 bool* proceed_to_fire_unload) OVERRIDE;
  virtual bool PreHandleKeyboardEvent(
      content::WebContents* source,
      const content::NativeWebKeyboardEvent& event,
      bool* is_keyboard_shortcut) OVERRIDE;
  virtual void HandleKeyboardEvent(
      content::WebContents* source,
      const content::NativeWebKeyboardEvent& event) OVERRIDE;
  virtual content::JavaScriptDialogManager*
      GetJavaScriptDialogManager() OVERRIDE;
  virtual content::ColorChooser* OpenColorChooser(
      content::WebContents* web_contents,
      SkColor color,
      const std::vector<content::ColorSuggestion>& suggestions) OVERRIDE;
  virtual void RunFileChooser(
      content::WebContents* web_contents,
      const content::FileChooserParams& params) OVERRIDE;
  virtual void WebContentsFocused(content::WebContents* contents) OVERRIDE;

  // content::DevToolsFrontendHostDelegate override:
  virtual void DispatchOnEmbedder(const std::string& message) OVERRIDE;

  // DevToolsEmbedderMessageDispatcher::Delegate overrides:
  virtual void ActivateWindow() OVERRIDE;
  virtual void ActivateContents(content::WebContents* contents) OVERRIDE;
  virtual void CloseWindow() OVERRIDE;
  virtual void SetWindowBounds(int x, int y, int width, int height) OVERRIDE;
  virtual void MoveWindow(int x, int y) OVERRIDE;
  virtual void SetDockSide(const std::string& side) OVERRIDE;
  virtual void OpenInNewTab(const std::string& url) OVERRIDE;
  virtual void SaveToFile(const std::string& url,
                          const std::string& content,
                          bool save_as) OVERRIDE;
  virtual void AppendToFile(const std::string& url,
                            const std::string& content) OVERRIDE;
  virtual void RequestFileSystems() OVERRIDE;
  virtual void AddFileSystem() OVERRIDE;
  virtual void RemoveFileSystem(const std::string& file_system_path) OVERRIDE;
  virtual void UpgradeDraggedFileSystemPermissions(
      const std::string& file_system_url) OVERRIDE;
  virtual void IndexPath(int request_id,
                         const std::string& file_system_path) OVERRIDE;
  virtual void StopIndexing(int request_id) OVERRIDE;
  virtual void SearchInPath(int request_id,
                            const std::string& file_system_path,
                            const std::string& query) OVERRIDE;

  // DevToolsFileHelper callbacks.
  void FileSavedAs(const std::string& url);
  void CanceledFileSaveAs(const std::string& url);
  void AppendedTo(const std::string& url);
  void FileSystemsLoaded(
      const std::vector<DevToolsFileHelper::FileSystem>& file_systems);
  void FileSystemAdded(const DevToolsFileHelper::FileSystem& file_system);
  void IndexingTotalWorkCalculated(int request_id,
                                   const std::string& file_system_path,
                                   int total_work);
  void IndexingWorked(int request_id,
                      const std::string& file_system_path,
                      int worked);
  void IndexingDone(int request_id, const std::string& file_system_path);
  void SearchCompleted(int request_id,
                       const std::string& file_system_path,
                       const std::vector<std::string>& file_paths);
  void ShowDevToolsConfirmInfoBar(const base::string16& message,
                                  const InfoBarCallback& callback);

  void CreateDevToolsBrowser();
  BrowserWindow* GetInspectedBrowserWindow();
  bool IsInspectedBrowserPopup();
  void UpdateFrontendDockSide();
  void ScheduleAction(const DevToolsToggleAction& action);
  void DoAction();
  void UpdateTheme();
  void AddDevToolsExtensionsToClient();
  void CallClientFunction(const std::string& function_name,
                          const base::Value* arg1,
                          const base::Value* arg2,
                          const base::Value* arg3);
  void UpdateBrowserToolbar();
  bool IsDocked();
  void Restore();
  content::WebContents* GetInspectedWebContents();
  void DocumentOnLoadCompletedInMainFrame();

  class InspectedWebContentsObserver;
  scoped_ptr<InspectedWebContentsObserver> inspected_contents_observer_;
  class FrontendWebContentsObserver;
  friend class FrontendWebContentsObserver;
  scoped_ptr<FrontendWebContentsObserver> frontend_contents_observer_;

  Profile* profile_;
  content::WebContents* web_contents_;
  Browser* browser_;
  DevToolsDockSide dock_side_;
  bool is_loaded_;
  DevToolsToggleAction action_on_load_;
  content::NotificationRegistrar registrar_;
  scoped_ptr<content::DevToolsClientHost> frontend_host_;
  scoped_ptr<DevToolsFileHelper> file_helper_;
  scoped_refptr<DevToolsFileSystemIndexer> file_system_indexer_;
  typedef std::map<
      int,
      scoped_refptr<DevToolsFileSystemIndexer::FileSystemIndexingJob> >
      IndexingJobsMap;
  IndexingJobsMap indexing_jobs_;
  int width_;
  int height_;
  DevToolsDockSide dock_side_before_minimized_;
  // True if we're in the process of handling a beforeunload event originating
  // from the inspected webcontents, see InterceptPageBeforeUnload for details.
  bool intercepted_page_beforeunload_;

  scoped_ptr<DevToolsEmbedderMessageDispatcher> embedder_message_dispatcher_;
  base::WeakPtrFactory<DevToolsWindow> weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(DevToolsWindow);
};

#endif  // CHROME_BROWSER_DEVTOOLS_DEVTOOLS_WINDOW_H_
