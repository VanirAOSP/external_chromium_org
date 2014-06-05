// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/chrome_shell_delegate.h"

#include "apps/shell_window.h"
#include "apps/shell_window_registry.h"
#include "ash/host/root_window_host_factory.h"
#include "ash/magnifier/magnifier_constants.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "base/command_line.h"
#include "chrome/browser/app_mode/app_mode_utils.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/app_list/app_list_service.h"
#include "chrome/browser/ui/app_list/app_list_view_delegate.h"
#include "chrome/browser/ui/ash/app_list/app_list_controller_ash.h"
#include "chrome/browser/ui/ash/ash_keyboard_controller_proxy.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chrome/browser/ui/ash/launcher/launcher_context_menu.h"
#include "chrome/browser/ui/ash/user_action_handler.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/common/chrome_switches.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#endif

// static
ChromeShellDelegate* ChromeShellDelegate::instance_ = NULL;

ChromeShellDelegate::ChromeShellDelegate()
    : shelf_delegate_(NULL) {
  instance_ = this;
  PlatformInit();
}

ChromeShellDelegate::~ChromeShellDelegate() {
  if (instance_ == this)
    instance_ = NULL;
}

bool ChromeShellDelegate::IsMultiProfilesEnabled() const {
  // TODO(skuhne): There is a function named profiles::IsMultiProfilesEnabled
  // which does similar things - but it is not the same. We should investigate
  // if these two could be folded together.
  if (!CommandLine::ForCurrentProcess()->HasSwitch(switches::kMultiProfiles))
    return false;
#if defined(OS_CHROMEOS)
  // If there is a user manager, we need to see that we can at least have 2
  // simultaneous users to allow this feature.
  if (!chromeos::UserManager::IsInitialized())
    return false;
  size_t admitted_users_to_be_added =
      chromeos::UserManager::Get()->GetUsersAdmittedForMultiProfile().size();
  size_t logged_in_users =
      chromeos::UserManager::Get()->GetLoggedInUsers().size();
  if (!logged_in_users) {
    // The shelf gets created on the login screen and as such we have to create
    // all multi profile items of the the system tray menu before the user logs
    // in. For special cases like Kiosk mode and / or guest mode this isn't a
    // problem since either the browser gets restarted and / or the flag is not
    // allowed, but for an "ephermal" user (see crbug.com/312324) it is not
    // decided yet if he could add other users to his session or not.
    // TODO(skuhne): As soon as the issue above needs to be resolved, this logic
    // should change.
    logged_in_users = 1;
  }
  if (admitted_users_to_be_added + logged_in_users <= 1)
    return false;
#endif
  return true;
}

bool ChromeShellDelegate::IsIncognitoAllowed() const {
#if defined(OS_CHROMEOS)
  return chromeos::AccessibilityManager::Get()->IsIncognitoAllowed();
#endif
  return true;
}

bool ChromeShellDelegate::IsRunningInForcedAppMode() const {
  return chrome::IsRunningInForcedAppMode();
}

void ChromeShellDelegate::Exit() {
  chrome::AttemptUserExit();
}

content::BrowserContext* ChromeShellDelegate::GetActiveBrowserContext() {
#if defined(OS_CHROMEOS)
  DCHECK(chromeos::UserManager::Get()->GetLoggedInUsers().size());
#endif
  return ProfileManager::GetActiveUserProfileOrOffTheRecord();
}

app_list::AppListViewDelegate*
ChromeShellDelegate::CreateAppListViewDelegate() {
  DCHECK(ash::Shell::HasInstance());
  // Shell will own the created delegate, and the delegate will own
  // the controller.
  return new AppListViewDelegate(
      Profile::FromBrowserContext(GetActiveBrowserContext()),
      AppListService::Get(chrome::HOST_DESKTOP_TYPE_ASH)->
      GetControllerDelegate());
}

ash::ShelfDelegate* ChromeShellDelegate::CreateShelfDelegate(
    ash::ShelfModel* model) {
  DCHECK(ProfileManager::IsGetDefaultProfileAllowed());
  // TODO(oshima): This is currently broken with multiple launchers.
  // Refactor so that there is just one launcher delegate in the
  // shell.
  if (!shelf_delegate_) {
    shelf_delegate_ = ChromeLauncherController::CreateInstance(NULL, model);
    shelf_delegate_->Init();
  }
  return shelf_delegate_;
}

aura::client::UserActionClient* ChromeShellDelegate::CreateUserActionClient() {
  return new UserActionHandler;
}

ui::MenuModel* ChromeShellDelegate::CreateContextMenu(aura::Window* root) {
  DCHECK(shelf_delegate_);
  // Don't show context menu for exclusive app runtime mode.
  if (chrome::IsRunningInAppMode())
    return NULL;

  return new LauncherContextMenu(shelf_delegate_, root);
}

ash::RootWindowHostFactory* ChromeShellDelegate::CreateRootWindowHostFactory() {
  return ash::RootWindowHostFactory::Create();
}

base::string16 ChromeShellDelegate::GetProductName() const {
  return l10n_util::GetStringUTF16(IDS_PRODUCT_NAME);
}

keyboard::KeyboardControllerProxy*
    ChromeShellDelegate::CreateKeyboardControllerProxy() {
  return new AshKeyboardControllerProxy();
}
