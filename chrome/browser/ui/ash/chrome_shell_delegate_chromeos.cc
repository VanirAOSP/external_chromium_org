// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/chrome_shell_delegate.h"

#include "ash/accessibility_delegate.h"
#include "ash/media_delegate.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/window_util.h"
#include "base/command_line.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/accessibility/accessibility_events.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/browser/chromeos/accessibility/magnification_manager.h"
#include "chrome/browser/chromeos/background/ash_user_wallpaper_delegate.h"
#include "chrome/browser/chromeos/display/display_preferences.h"
#include "chrome/browser/chromeos/extensions/media_player_api.h"
#include "chrome/browser/chromeos/extensions/media_player_event_router.h"
#include "chrome/browser/chromeos/system/ash_system_tray_delegate.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/speech/tts_controller.h"
#include "chrome/browser/ui/ash/caps_lock_delegate_chromeos.h"
#include "chrome/browser/ui/ash/chrome_new_window_delegate_chromeos.h"
#include "chrome/browser/ui/ash/session_state_delegate_chromeos.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/pref_names.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power_manager_client.h"
#include "chromeos/ime/input_method_manager.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/user_metrics.h"
#include "grit/generated_resources.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

// This function is used for restoring focus after the user session is started.
// It's needed because some windows can be opened in background while login UI
// is still active because we currently restore browser windows before login UI
// is deleted.
void RestoreFocus() {
  ash::MruWindowTracker::WindowList mru_list =
      ash::Shell::GetInstance()->mru_window_tracker()->BuildMruWindowList();
  if (!mru_list.empty())
    mru_list.front()->Focus();
}

class AccessibilityDelegateImpl : public ash::AccessibilityDelegate {
 public:
  AccessibilityDelegateImpl() {}
  virtual ~AccessibilityDelegateImpl() {}

  virtual void ToggleHighContrast() OVERRIDE {
    DCHECK(chromeos::AccessibilityManager::Get());
    chromeos::AccessibilityManager::Get()->EnableHighContrast(
        !chromeos::AccessibilityManager::Get()->IsHighContrastEnabled());
  }

  virtual bool IsSpokenFeedbackEnabled() const OVERRIDE {
    DCHECK(chromeos::AccessibilityManager::Get());
    return chromeos::AccessibilityManager::Get()->IsSpokenFeedbackEnabled();
  }

  virtual void ToggleSpokenFeedback(
      ash::AccessibilityNotificationVisibility notify) OVERRIDE {
    DCHECK(chromeos::AccessibilityManager::Get());
    chromeos::AccessibilityManager::Get()->ToggleSpokenFeedback(notify);
  }

  virtual bool IsHighContrastEnabled() const OVERRIDE {
    DCHECK(chromeos::AccessibilityManager::Get());
    return chromeos::AccessibilityManager::Get()->IsHighContrastEnabled();
  }

  virtual void SetMagnifierEnabled(bool enabled) OVERRIDE {
    DCHECK(chromeos::MagnificationManager::Get());
    return chromeos::MagnificationManager::Get()->SetMagnifierEnabled(enabled);
  }

  virtual void SetMagnifierType(ash::MagnifierType type) OVERRIDE {
    DCHECK(chromeos::MagnificationManager::Get());
    return chromeos::MagnificationManager::Get()->SetMagnifierType(type);
  }

  virtual bool IsMagnifierEnabled() const OVERRIDE {
    DCHECK(chromeos::MagnificationManager::Get());
    return chromeos::MagnificationManager::Get()->IsMagnifierEnabled();
  }

  virtual ash::MagnifierType GetMagnifierType() const OVERRIDE {
    DCHECK(chromeos::MagnificationManager::Get());
    return chromeos::MagnificationManager::Get()->GetMagnifierType();
  }

  virtual void SetLargeCursorEnabled(bool enabled) OVERRIDE {
    DCHECK(chromeos::AccessibilityManager::Get());
    return chromeos::AccessibilityManager::Get()->EnableLargeCursor(enabled);
  }

  virtual bool IsLargeCursorEnabled() const OVERRIDE {
    DCHECK(chromeos::AccessibilityManager::Get());
    return chromeos::AccessibilityManager::Get()->IsLargeCursorEnabled();
  }

  virtual void SetAutoclickEnabled(bool enabled) OVERRIDE {
    DCHECK(chromeos::AccessibilityManager::Get());
    return chromeos::AccessibilityManager::Get()->EnableAutoclick(enabled);
  }

  virtual bool IsAutoclickEnabled() const OVERRIDE {
    DCHECK(chromeos::AccessibilityManager::Get());
    return chromeos::AccessibilityManager::Get()->IsAutoclickEnabled();
  }

  virtual bool ShouldShowAccessibilityMenu() const OVERRIDE {
    DCHECK(chromeos::AccessibilityManager::Get());
    return chromeos::AccessibilityManager::Get()->
        ShouldShowAccessibilityMenu();
  }

  virtual void SilenceSpokenFeedback() const OVERRIDE {
    TtsController::GetInstance()->Stop();
  }

  virtual void SaveScreenMagnifierScale(double scale) OVERRIDE {
    if (chromeos::MagnificationManager::Get())
      chromeos::MagnificationManager::Get()->SaveScreenMagnifierScale(scale);
  }

  virtual double GetSavedScreenMagnifierScale() OVERRIDE {
    if (chromeos::MagnificationManager::Get()) {
      return chromeos::MagnificationManager::Get()->
          GetSavedScreenMagnifierScale();
    }
    return std::numeric_limits<double>::min();
  }

  virtual void TriggerAccessibilityAlert(
      ash::AccessibilityAlert alert) OVERRIDE {
    Profile* profile = ProfileManager::GetActiveUserProfileOrOffTheRecord();
    if (profile) {
      switch (alert) {
        case ash::A11Y_ALERT_WINDOW_NEEDED: {
          AccessibilityAlertInfo event(
              profile, l10n_util::GetStringUTF8(IDS_A11Y_ALERT_WINDOW_NEEDED));
          SendControlAccessibilityNotification(
              ui::AccessibilityTypes::EVENT_ALERT, &event);
          break;
        }
        case ash::A11Y_ALERT_NONE:
          break;
      }
    }
  }

  virtual ash::AccessibilityAlert GetLastAccessibilityAlert() OVERRIDE {
    return ash::A11Y_ALERT_NONE;
  }

  virtual base::TimeDelta PlayShutdownSound() const OVERRIDE {
    return chromeos::AccessibilityManager::Get()->PlayShutdownSound();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(AccessibilityDelegateImpl);
};

class MediaDelegateImpl : public ash::MediaDelegate {
 public:
  MediaDelegateImpl() {}
  virtual ~MediaDelegateImpl() {}

  virtual void HandleMediaNextTrack() OVERRIDE {
    extensions::MediaPlayerAPI::Get(
        ProfileManager::GetActiveUserProfileOrOffTheRecord())->
            media_player_event_router()->NotifyNextTrack();
  }

  virtual void HandleMediaPlayPause() OVERRIDE {
    extensions::MediaPlayerAPI::Get(
        ProfileManager::GetActiveUserProfileOrOffTheRecord())->
            media_player_event_router()->NotifyTogglePlayState();
  }

  virtual void HandleMediaPrevTrack() OVERRIDE {
    extensions::MediaPlayerAPI::Get(
        ProfileManager::GetActiveUserProfileOrOffTheRecord())->
            media_player_event_router()->NotifyPrevTrack();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MediaDelegateImpl);
};

}  // anonymous namespace

bool ChromeShellDelegate::IsFirstRunAfterBoot() const {
  return CommandLine::ForCurrentProcess()->HasSwitch(
      chromeos::switches::kFirstExecAfterBoot);
}

void ChromeShellDelegate::PreInit() {
  chromeos::LoadDisplayPreferences(IsFirstRunAfterBoot());
}

void ChromeShellDelegate::Shutdown() {
  content::RecordAction(content::UserMetricsAction("Shutdown"));
  chromeos::DBusThreadManager::Get()->GetPowerManagerClient()->
      RequestShutdown();
}

ash::CapsLockDelegate* ChromeShellDelegate::CreateCapsLockDelegate() {
  chromeos::input_method::XKeyboard* xkeyboard =
      chromeos::input_method::InputMethodManager::Get()->GetXKeyboard();
  return new CapsLockDelegate(xkeyboard);
}

ash::SessionStateDelegate* ChromeShellDelegate::CreateSessionStateDelegate() {
  return new SessionStateDelegateChromeos;
}

ash::AccessibilityDelegate* ChromeShellDelegate::CreateAccessibilityDelegate() {
  return new AccessibilityDelegateImpl;
}

ash::NewWindowDelegate* ChromeShellDelegate::CreateNewWindowDelegate() {
  return new ChromeNewWindowDelegateChromeos;
}

ash::MediaDelegate* ChromeShellDelegate::CreateMediaDelegate() {
  return new MediaDelegateImpl;
}

ash::SystemTrayDelegate* ChromeShellDelegate::CreateSystemTrayDelegate() {
  return chromeos::CreateSystemTrayDelegate();
}

ash::UserWallpaperDelegate* ChromeShellDelegate::CreateUserWallpaperDelegate() {
  return chromeos::CreateUserWallpaperDelegate();
}

void ChromeShellDelegate::Observe(int type,
                                  const content::NotificationSource& source,
                                  const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_LOGIN_USER_PROFILE_PREPARED:
      ash::Shell::GetInstance()->OnLoginUserProfilePrepared();
      break;
    case chrome::NOTIFICATION_SESSION_STARTED:
      RestoreFocus();
      ash::Shell::GetInstance()->ShowLauncher();
      break;
    default:
      NOTREACHED() << "Unexpected notification " << type;
  }
}

void ChromeShellDelegate::PlatformInit() {
  registrar_.Add(this,
                 chrome::NOTIFICATION_LOGIN_USER_PROFILE_PREPARED,
                 content::NotificationService::AllSources());
  registrar_.Add(this,
                 chrome::NOTIFICATION_SESSION_STARTED,
                 content::NotificationService::AllSources());
}
