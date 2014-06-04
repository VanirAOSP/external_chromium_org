// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/accelerator_commands.h"

#include "apps/shell_window.h"
#include "apps/ui/native_app_window.h"
#include "ash/ash_switches.h"
#include "ash/shell.h"
#include "ash/wm/window_state.h"
#include "base/command_line.h"
#include "chrome/browser/apps/app_browsertest_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/test_switches.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

using testing::Combine;
using testing::Values;
using testing::WithParamInterface;

namespace {

// WidgetDelegateView which allows the widget to be maximized.
class MaximizableWidgetDelegate : public views::WidgetDelegateView {
 public:
  MaximizableWidgetDelegate() {
  }
  virtual ~MaximizableWidgetDelegate() {
  }

  virtual bool CanMaximize() const OVERRIDE {
    return true;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MaximizableWidgetDelegate);
};

// Returns true if |window_state|'s window is in immersive fullscreen. Infer
// whether the window is in immersive fullscreen based on whether the shelf is
// hidden when the window is fullscreen. (This is not quite right because the
// shelf is hidden if a window is in both immersive fullscreen and tab
// fullscreen.)
bool IsInImmersiveFullscreen(ash::wm::WindowState* window_state) {
  return window_state->IsFullscreen() &&
      !window_state->hide_shelf_when_fullscreen();
}

}  // namespace

typedef InProcessBrowserTest AcceleratorCommandsBrowserTest;

// Confirm that toggling window miximized works properly
IN_PROC_BROWSER_TEST_F(AcceleratorCommandsBrowserTest, ToggleMaximized) {
#if defined(OS_WIN)
  // Run the test on Win Ash only.
  if (!CommandLine::ForCurrentProcess()->HasSwitch(switches::kAshBrowserTests))
    return;
#endif

  ASSERT_TRUE(ash::Shell::HasInstance()) << "No Instance";
  ash::wm::WindowState* window_state = ash::wm::GetActiveWindowState();
  ASSERT_TRUE(window_state);

  // When not in fullscreen, accelerators::ToggleMaximized toggles Maximized.
  EXPECT_FALSE(window_state->IsMaximized());
  ash::accelerators::ToggleMaximized();
  EXPECT_TRUE(window_state->IsMaximized());
  ash::accelerators::ToggleMaximized();
  EXPECT_FALSE(window_state->IsMaximized());

  // When in fullscreen accelerators::ToggleMaximized gets out of fullscreen.
  EXPECT_FALSE(window_state->IsFullscreen());
  Browser* browser = chrome::FindBrowserWithWindow(window_state->window());
  ASSERT_TRUE(browser);
  chrome::ToggleFullscreenMode(browser);
  EXPECT_TRUE(window_state->IsFullscreen());
  ash::accelerators::ToggleMaximized();
  EXPECT_FALSE(window_state->IsFullscreen());
  EXPECT_FALSE(window_state->IsMaximized());
  ash::accelerators::ToggleMaximized();
  EXPECT_FALSE(window_state->IsFullscreen());
  EXPECT_TRUE(window_state->IsMaximized());
}

class AcceleratorCommandsFullscreenBrowserTest
    : public WithParamInterface<std::tr1::tuple<bool, ui::WindowShowState> >,
      public InProcessBrowserTest {
 public:
  AcceleratorCommandsFullscreenBrowserTest()
#if defined(OS_CHROMEOS)
      : put_browser_in_immersive_(true),
        put_all_windows_in_immersive_(std::tr1::get<0>(GetParam())),
#else
      : put_browser_in_immersive_(false),
        put_all_windows_in_immersive_(false),
#endif
        initial_show_state_(std::tr1::get<1>(GetParam())) {
  }
  virtual ~AcceleratorCommandsFullscreenBrowserTest() {
  }

  // BrowserTestBase override:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    InProcessBrowserTest::SetUpCommandLine(command_line);
    if (put_all_windows_in_immersive_) {
      CommandLine::ForCurrentProcess()->AppendSwitch(
          ash::switches::kAshEnableImmersiveFullscreenForAllWindows);
    }
  }

  // Sets |window_state|'s show state to |initial_show_state_|.
  void SetToInitialShowState(ash::wm::WindowState* window_state) {
    if (initial_show_state_ == ui::SHOW_STATE_MAXIMIZED)
      window_state->Maximize();
    else
      window_state->Restore();
  }

  // Returns true if |window_state|'s show state is |initial_show_state_|.
  bool IsInitialShowState(const ash::wm::WindowState* window_state) const {
    return window_state->GetShowState() == initial_show_state_;
  }

  bool put_browser_in_immersive() const {
    return put_browser_in_immersive_;
  }

  bool put_all_windows_in_immersive() const {
    return put_all_windows_in_immersive_;
  }

 private:
  bool put_browser_in_immersive_;
  bool put_all_windows_in_immersive_;
  ui::WindowShowState initial_show_state_;

  DISALLOW_COPY_AND_ASSIGN(AcceleratorCommandsFullscreenBrowserTest);
};

// Test that toggling window fullscreen works properly.
IN_PROC_BROWSER_TEST_P(AcceleratorCommandsFullscreenBrowserTest,
                       ToggleFullscreen) {
#if defined(OS_WIN)
  // Run the test on Win Ash only.
  if (!CommandLine::ForCurrentProcess()->HasSwitch(switches::kAshBrowserTests))
    return;
#endif

  ASSERT_TRUE(ash::Shell::HasInstance()) << "No Instance";

  // 1) Browser windows.
  ASSERT_TRUE(browser()->is_type_tabbed());
  ash::wm::WindowState* window_state =
      ash::wm::GetWindowState(browser()->window()->GetNativeWindow());
  ASSERT_TRUE(window_state->IsActive());
  SetToInitialShowState(window_state);
  EXPECT_TRUE(IsInitialShowState(window_state));

  ash::accelerators::ToggleFullscreen();
  EXPECT_TRUE(window_state->IsFullscreen());
  EXPECT_EQ(put_browser_in_immersive(), IsInImmersiveFullscreen(window_state));

  ash::accelerators::ToggleFullscreen();
  EXPECT_TRUE(IsInitialShowState(window_state));

  // 2) ToggleFullscreen() should have no effect on windows which cannot be
  // maximized.
  window_state->window()->SetProperty(aura::client::kCanMaximizeKey, false);
  ash::accelerators::ToggleFullscreen();
  EXPECT_TRUE(IsInitialShowState(window_state));

  // 3) Hosted apps.
  Browser::CreateParams browser_create_params(Browser::TYPE_POPUP,
      browser()->profile(), chrome::HOST_DESKTOP_TYPE_ASH);
  browser_create_params.app_name = "Test";

  Browser* app_host_browser = new Browser(browser_create_params);
  ASSERT_TRUE(app_host_browser->is_app());
  AddBlankTabAndShow(app_host_browser);
  window_state =
      ash::wm::GetWindowState(app_host_browser->window()->GetNativeWindow());
  ASSERT_TRUE(window_state->IsActive());
  SetToInitialShowState(window_state);
  EXPECT_TRUE(IsInitialShowState(window_state));

  ash::accelerators::ToggleFullscreen();
  EXPECT_TRUE(window_state->IsFullscreen());
  EXPECT_EQ(put_all_windows_in_immersive(),
            IsInImmersiveFullscreen(window_state));

  ash::accelerators::ToggleFullscreen();
  EXPECT_TRUE(IsInitialShowState(window_state));

  // 4) Popup browser windows.
  browser_create_params.app_name = "";
  Browser* popup_browser = new Browser(browser_create_params);
  ASSERT_TRUE(popup_browser->is_type_popup());
  ASSERT_FALSE(popup_browser->is_app());
  AddBlankTabAndShow(popup_browser);
  window_state =
      ash::wm::GetWindowState(popup_browser->window()->GetNativeWindow());
  ASSERT_TRUE(window_state->IsActive());
  SetToInitialShowState(window_state);
  EXPECT_TRUE(IsInitialShowState(window_state));

  ash::accelerators::ToggleFullscreen();
  EXPECT_TRUE(window_state->IsFullscreen());
  EXPECT_EQ(put_all_windows_in_immersive(),
            IsInImmersiveFullscreen(window_state));

  ash::accelerators::ToggleFullscreen();
  EXPECT_TRUE(IsInitialShowState(window_state));

  // 5) Miscellaneous windows (e.g. task manager).
  views::Widget::InitParams params;
  params.delegate = new MaximizableWidgetDelegate();
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  scoped_ptr<views::Widget> widget(new views::Widget);
  widget->Init(params);
  widget->Show();

  window_state = ash::wm::GetWindowState(widget->GetNativeWindow());
  ASSERT_TRUE(window_state->IsActive());
  SetToInitialShowState(window_state);
  EXPECT_TRUE(IsInitialShowState(window_state));

  ash::accelerators::ToggleFullscreen();
  EXPECT_TRUE(window_state->IsFullscreen());
  EXPECT_EQ(put_all_windows_in_immersive(),
            IsInImmersiveFullscreen(window_state));

  // TODO(pkotwicz|oshima): Make toggling fullscreen restore the window to its
  // show state prior to entering fullscreen.
  ash::accelerators::ToggleFullscreen();
  EXPECT_FALSE(window_state->IsFullscreen());
}

#if defined(OS_CHROMEOS)
INSTANTIATE_TEST_CASE_P(InitiallyRestored,
                        AcceleratorCommandsFullscreenBrowserTest,
                        Combine(Values(false, true),
                                Values(ui::SHOW_STATE_NORMAL)));
INSTANTIATE_TEST_CASE_P(InitiallyMaximized,
                        AcceleratorCommandsFullscreenBrowserTest,
                        Combine(Values(false, true),
                                Values(ui::SHOW_STATE_MAXIMIZED)));
#else
// The kAshEnableImmersiveFullscreenForAllWindows flag should have no effect on
// Windows. Do not run the tests with and without the flag to spare some
// cycles.
INSTANTIATE_TEST_CASE_P(InitiallyRestored,
                        AcceleratorCommandsFullscreenBrowserTest,
                        Combine(Values(false),
                                Values(ui::SHOW_STATE_NORMAL)));
INSTANTIATE_TEST_CASE_P(InitiallyMaximized,
                        AcceleratorCommandsFullscreenBrowserTest,
                        Combine(Values(false),
                                Values(ui::SHOW_STATE_MAXIMIZED)));
#endif

class AcceleratorCommandsPlatformAppFullscreenBrowserTest
    : public WithParamInterface<std::tr1::tuple<bool, ui::WindowShowState> >,
      public extensions::PlatformAppBrowserTest {
 public:
  AcceleratorCommandsPlatformAppFullscreenBrowserTest()
#if defined(OS_CHROMEOS)
      : put_all_windows_in_immersive_(std::tr1::get<0>(GetParam())),
#else
      : put_all_windows_in_immersive_(false),
#endif
        initial_show_state_(std::tr1::get<1>(GetParam())) {
  }
  virtual ~AcceleratorCommandsPlatformAppFullscreenBrowserTest() {
  }

  // Sets |shell_window|'s show state to |initial_show_state_|.
  void SetToInitialShowState(apps::ShellWindow* shell_window) {
    if (initial_show_state_ == ui::SHOW_STATE_MAXIMIZED)
      shell_window->Maximize();
    else
      shell_window->Restore();
  }

  // Returns true if |shell_window|'s show state is |initial_show_state_|.
  bool IsInitialShowState(apps::ShellWindow* shell_window) const {
    if (initial_show_state_ == ui::SHOW_STATE_MAXIMIZED)
      return shell_window->GetBaseWindow()->IsMaximized();
    else
      return ui::BaseWindow::IsRestored(*shell_window->GetBaseWindow());
  }

  // content::BrowserTestBase override:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    if (put_all_windows_in_immersive_) {
      CommandLine::ForCurrentProcess()->AppendSwitch(
          ash::switches::kAshEnableImmersiveFullscreenForAllWindows);
    }
    extensions::PlatformAppBrowserTest::SetUpCommandLine(command_line);
  }

  bool put_all_windows_in_immersive() const {
    return put_all_windows_in_immersive_;
  }

 private:
  bool put_all_windows_in_immersive_;
  ui::WindowShowState initial_show_state_;

  DISALLOW_COPY_AND_ASSIGN(AcceleratorCommandsPlatformAppFullscreenBrowserTest);
};

// Test the behavior of platform apps when ToggleFullscreen() is called.
IN_PROC_BROWSER_TEST_P(AcceleratorCommandsPlatformAppFullscreenBrowserTest,
                       ToggleFullscreen) {
#if defined(OS_WIN)
  // Run the test on Win Ash only.
  if (!CommandLine::ForCurrentProcess()->HasSwitch(switches::kAshBrowserTests))
    return;
#endif

  ASSERT_TRUE(ash::Shell::HasInstance()) << "No Instance";
  const extensions::Extension* extension = LoadAndLaunchPlatformApp("minimal");

  {
    // Test that ToggleFullscreen() toggles a platform's app's fullscreen
    // state and that it additionally puts the app into immersive fullscreen
    // if put_all_windows_in_immersive() returns true.
    apps::ShellWindow::CreateParams params;
    params.frame = apps::ShellWindow::FRAME_CHROME;
    apps::ShellWindow* shell_window = CreateShellWindowFromParams(
        extension, params);
    apps::NativeAppWindow* app_window = shell_window->GetBaseWindow();
    SetToInitialShowState(shell_window);
    ASSERT_TRUE(shell_window->GetBaseWindow()->IsActive());
    EXPECT_TRUE(IsInitialShowState(shell_window));

    ash::accelerators::ToggleFullscreen();
    EXPECT_TRUE(app_window->IsFullscreen());
    ash::wm::WindowState* window_state =
        ash::wm::GetWindowState(app_window->GetNativeWindow());
    EXPECT_EQ(put_all_windows_in_immersive(),
              IsInImmersiveFullscreen(window_state));

    ash::accelerators::ToggleFullscreen();
    EXPECT_TRUE(IsInitialShowState(shell_window));

    CloseShellWindow(shell_window);
  }

  {
    // Repeat the test, but make sure that frameless platform apps are never put
    // into immersive fullscreen.
    apps::ShellWindow::CreateParams params;
    params.frame = apps::ShellWindow::FRAME_NONE;
    apps::ShellWindow* shell_window = CreateShellWindowFromParams(
        extension, params);
    apps::NativeAppWindow* app_window = shell_window->GetBaseWindow();
    ASSERT_TRUE(shell_window->GetBaseWindow()->IsActive());
    SetToInitialShowState(shell_window);
    EXPECT_TRUE(IsInitialShowState(shell_window));

    ash::accelerators::ToggleFullscreen();
    EXPECT_TRUE(app_window->IsFullscreen());
    ash::wm::WindowState* window_state =
        ash::wm::GetWindowState(app_window->GetNativeWindow());
    EXPECT_FALSE(IsInImmersiveFullscreen(window_state));

    ash::accelerators::ToggleFullscreen();
    EXPECT_TRUE(IsInitialShowState(shell_window));

    CloseShellWindow(shell_window);
  }
}

#if defined(OS_CHROMEOS)
INSTANTIATE_TEST_CASE_P(InitiallyRestored,
                        AcceleratorCommandsPlatformAppFullscreenBrowserTest,
                        Combine(Values(false, true),
                                Values(ui::SHOW_STATE_NORMAL)));
INSTANTIATE_TEST_CASE_P(InitiallyMaximized,
                        AcceleratorCommandsPlatformAppFullscreenBrowserTest,
                        Combine(Values(false, true),
                                Values(ui::SHOW_STATE_MAXIMIZED)));
#else
// The kAshEnableImmersiveFullscreenForAllWindows flag should have no effect on
// Windows. Do not run the tests with and without the flag to spare some
// cycles.
INSTANTIATE_TEST_CASE_P(InitiallyRestored,
                        AcceleratorCommandsPlatformAppFullscreenBrowserTest,
                        Combine(Values(false),
                                Values(ui::SHOW_STATE_NORMAL)));
INSTANTIATE_TEST_CASE_P(InitiallyMaximized,
                        AcceleratorCommandsPlatformAppFullscreenBrowserTest,
                        Combine(Values(false),
                                Values(ui::SHOW_STATE_MAXIMIZED)));
#endif
