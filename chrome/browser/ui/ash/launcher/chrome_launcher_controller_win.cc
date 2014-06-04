// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"

#include "apps/shell_window.h"
#include "apps/shell_window_registry.h"
#include "base/path_service.h"
#include "base/strings/string16.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/common/chrome_switches.h"
#include "extensions/common/constants.h"
#include "ui/aura/remote_root_window_host_win.h"

bool ChromeLauncherController::LaunchedInNativeDesktop(
    const std::string& app_id) {
  // If an app has any existing windows on the native desktop, funnel the
  // launch request through the viewer process to desktop Chrome. This allows
  // Ash to relinquish foreground window status and trigger a switch to
  // desktop mode.
  apps::ShellWindow* any_existing_window =
      apps::ShellWindowRegistry::Get(profile())->
          GetCurrentShellWindowForApp(app_id);
  if (!any_existing_window ||
      chrome::GetHostDesktopTypeForNativeWindow(
          any_existing_window->GetNativeWindow())
      != chrome::HOST_DESKTOP_TYPE_NATIVE) {
    return false;
  }
  base::FilePath exe_path;
  if (!PathService::Get(base::FILE_EXE, &exe_path)) {
    NOTREACHED();
    return false;
  }

  // Construct parameters for ShellExecuteEx that mimic a desktop shortcut
  // for the app in the current Profile.
  std::string spec = base::StringPrintf("\"--%s=%s\" \"--%s=%s\"",
      switches::kProfileDirectory,
      profile_->GetPath().BaseName().AsUTF8Unsafe().c_str(),
      switches::kAppId,
      app_id.c_str());
  aura::RemoteRootWindowHostWin::Instance()->HandleOpenURLOnDesktop(
      exe_path, UTF8ToUTF16(spec));
  return true;
}
