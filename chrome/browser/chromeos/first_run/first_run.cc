// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/metrics/histogram.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/first_run/first_run_controller.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/common/chrome_switches.h"
#include "chromeos/chromeos_switches.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "extensions/common/constants.h"

namespace chromeos {
namespace first_run {

namespace {

void LaunchDialogForProfile(Profile* profile) {
  ExtensionService* service =
      extensions::ExtensionSystem::Get(profile)->extension_service();
  if (!service)
    return;

  const extensions::Extension* extension =
      service->GetExtensionById(extension_misc::kFirstRunDialogId, false);
  if (!extension)
    return;

  OpenApplication(AppLaunchParams(
      profile, extension, extensions::LAUNCH_CONTAINER_WINDOW, NEW_WINDOW));
}

// Object of this class waits for session start. Then it launches or not
// launches first-run dialog depending on flags. Than object deletes itself.
class DialogLauncher : public content::NotificationObserver {
 public:
  explicit DialogLauncher(Profile* profile)
      : profile_(profile) {
    DCHECK(profile);
    registrar_.Add(this,
                   chrome::NOTIFICATION_SESSION_STARTED,
                   content::NotificationService::AllSources());
  }

  virtual ~DialogLauncher() {}

  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE {
    DCHECK(type == chrome::NOTIFICATION_SESSION_STARTED);
    DCHECK(content::Details<const User>(details).ptr() ==
        UserManager::Get()->GetUserByProfile(profile_));
    CommandLine* command_line = CommandLine::ForCurrentProcess();
    bool launched_in_test = command_line->HasSwitch(::switches::kTestType);
    bool launched_in_telemetry =
        command_line->HasSwitch(switches::kOobeSkipPostLogin);
    bool first_run_disabled =
        command_line->HasSwitch(switches::kDisableFirstRunUI);
    bool is_user_new = chromeos::UserManager::Get()->IsCurrentUserNew();
    bool first_run_forced = command_line->HasSwitch(switches::kForceFirstRunUI);
    if (!launched_in_telemetry && !first_run_disabled &&
        ((is_user_new && !launched_in_test) || first_run_forced)) {
      LaunchDialogForProfile(profile_);
    }
    delete this;
  }

 private:
  Profile* profile_;
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(DialogLauncher);
};

}  // namespace

void MaybeLaunchDialogAfterSessionStart() {
  UserManager* user_manager = UserManager::Get();
  new DialogLauncher(
      user_manager->GetProfileByUser(user_manager->GetActiveUser()));
}

void LaunchTutorial() {
  UMA_HISTOGRAM_BOOLEAN("CrosFirstRun.TutorialLaunched", true);
  FirstRunController::Start();
}

}  // namespace first_run
}  // namespace chromeos
