// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_command_controller.h"

#include "base/command_line.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/command_updater.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window_state.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/test_browser_window.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "ui/events/keycodes/keyboard_codes.h"

typedef BrowserWithTestWindowTest BrowserCommandControllerTest;

TEST_F(BrowserCommandControllerTest, IsReservedCommandOrKey) {
#if defined(OS_CHROMEOS)
  // F1-3 keys are reserved Chrome accelerators on Chrome OS.
  EXPECT_TRUE(browser()->command_controller()->IsReservedCommandOrKey(
      IDC_BACK, content::NativeWebKeyboardEvent(
          ui::ET_KEY_PRESSED, false, ui::VKEY_BROWSER_BACK, 0, 0)));
  EXPECT_TRUE(browser()->command_controller()->IsReservedCommandOrKey(
      IDC_FORWARD, content::NativeWebKeyboardEvent(
          ui::ET_KEY_PRESSED, false, ui::VKEY_BROWSER_FORWARD, 0, 0)));
  EXPECT_TRUE(browser()->command_controller()->IsReservedCommandOrKey(
      IDC_RELOAD, content::NativeWebKeyboardEvent(
          ui::ET_KEY_PRESSED, false, ui::VKEY_BROWSER_REFRESH, 0, 0)));

  // When there are modifier keys pressed, don't reserve.
  EXPECT_FALSE(browser()->command_controller()->IsReservedCommandOrKey(
      IDC_RELOAD_IGNORING_CACHE, content::NativeWebKeyboardEvent(
          ui::ET_KEY_PRESSED, false, ui::VKEY_F3, ui::EF_SHIFT_DOWN, 0)));
  EXPECT_FALSE(browser()->command_controller()->IsReservedCommandOrKey(
      IDC_RELOAD_IGNORING_CACHE, content::NativeWebKeyboardEvent(
          ui::ET_KEY_PRESSED, false, ui::VKEY_F3, ui::EF_CONTROL_DOWN, 0)));
  EXPECT_FALSE(browser()->command_controller()->IsReservedCommandOrKey(
      IDC_FULLSCREEN, content::NativeWebKeyboardEvent(
          ui::ET_KEY_PRESSED, false, ui::VKEY_F4, ui::EF_SHIFT_DOWN, 0)));

  // F4-10 keys are not reserved since they are Ash accelerators.
  EXPECT_FALSE(browser()->command_controller()->IsReservedCommandOrKey(
      -1, content::NativeWebKeyboardEvent(ui::ET_KEY_PRESSED, false,
                                          ui::VKEY_F4, 0, 0)));
  EXPECT_FALSE(browser()->command_controller()->IsReservedCommandOrKey(
      -1, content::NativeWebKeyboardEvent(ui::ET_KEY_PRESSED, false,
                                          ui::VKEY_F5, 0, 0)));
  EXPECT_FALSE(browser()->command_controller()->IsReservedCommandOrKey(
      -1, content::NativeWebKeyboardEvent(ui::ET_KEY_PRESSED, false,
                                          ui::VKEY_F6, 0, 0)));
  EXPECT_FALSE(browser()->command_controller()->IsReservedCommandOrKey(
      -1, content::NativeWebKeyboardEvent(ui::ET_KEY_PRESSED, false,
                                          ui::VKEY_F7, 0, 0)));
  EXPECT_FALSE(browser()->command_controller()->IsReservedCommandOrKey(
      -1, content::NativeWebKeyboardEvent(ui::ET_KEY_PRESSED, false,
                                          ui::VKEY_F8, 0, 0)));
  EXPECT_FALSE(browser()->command_controller()->IsReservedCommandOrKey(
      -1, content::NativeWebKeyboardEvent(ui::ET_KEY_PRESSED, false,
                                          ui::VKEY_F9, 0, 0)));
  EXPECT_FALSE(browser()->command_controller()->IsReservedCommandOrKey(
      -1, content::NativeWebKeyboardEvent(ui::ET_KEY_PRESSED, false,
                                          ui::VKEY_F10, 0, 0)));

  // Shift+Control+Alt+F3 is also an Ash accelerator. Don't reserve it.
  EXPECT_FALSE(browser()->command_controller()->IsReservedCommandOrKey(
      -1, content::NativeWebKeyboardEvent(
          ui::ET_KEY_PRESSED, false,
          ui::VKEY_F3,
          ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN, 0)));
#endif  // OS_CHROMEOS

#if defined(USE_AURA)
  // Ctrl+n, Ctrl+w are reserved while Ctrl+f is not.

  // The content::NativeWebKeyboardEvent constructor is available only when
  // USE_AURA is #defined.
  EXPECT_TRUE(browser()->command_controller()->IsReservedCommandOrKey(
      IDC_NEW_WINDOW, content::NativeWebKeyboardEvent(
          ui::ET_KEY_PRESSED, false, ui::VKEY_N, ui::EF_CONTROL_DOWN, 0)));
  EXPECT_TRUE(browser()->command_controller()->IsReservedCommandOrKey(
      IDC_CLOSE_TAB, content::NativeWebKeyboardEvent(
          ui::ET_KEY_PRESSED, false, ui::VKEY_W, ui::EF_CONTROL_DOWN, 0)));
  EXPECT_FALSE(browser()->command_controller()->IsReservedCommandOrKey(
      IDC_FIND, content::NativeWebKeyboardEvent(
          ui::ET_KEY_PRESSED, false, ui::VKEY_F, ui::EF_CONTROL_DOWN, 0)));
#endif  // USE_AURA
}

TEST_F(BrowserCommandControllerTest, IsReservedCommandOrKeyIsApp) {
  browser()->app_name_ = "app";
  ASSERT_TRUE(browser()->is_app());

  // When is_app(), no keys are reserved.
#if defined(OS_CHROMEOS)
  EXPECT_FALSE(browser()->command_controller()->IsReservedCommandOrKey(
      IDC_BACK, content::NativeWebKeyboardEvent(ui::ET_KEY_PRESSED, false,
                                                ui::VKEY_F1, 0, 0)));
  EXPECT_FALSE(browser()->command_controller()->IsReservedCommandOrKey(
      IDC_FORWARD, content::NativeWebKeyboardEvent(ui::ET_KEY_PRESSED, false,
                                                   ui::VKEY_F2, 0, 0)));
  EXPECT_FALSE(browser()->command_controller()->IsReservedCommandOrKey(
      IDC_RELOAD, content::NativeWebKeyboardEvent(ui::ET_KEY_PRESSED, false,
                                                  ui::VKEY_F3, 0, 0)));
  EXPECT_FALSE(browser()->command_controller()->IsReservedCommandOrKey(
      -1, content::NativeWebKeyboardEvent(ui::ET_KEY_PRESSED, false,
                                          ui::VKEY_F4, 0, 0)));
#endif  // OS_CHROMEOS

#if defined(USE_AURA)
  // The content::NativeWebKeyboardEvent constructor is available only when
  // USE_AURA is #defined.
  EXPECT_FALSE(browser()->command_controller()->IsReservedCommandOrKey(
      IDC_NEW_WINDOW, content::NativeWebKeyboardEvent(
          ui::ET_KEY_PRESSED, false, ui::VKEY_N, ui::EF_CONTROL_DOWN, 0)));
  EXPECT_FALSE(browser()->command_controller()->IsReservedCommandOrKey(
      IDC_CLOSE_TAB, content::NativeWebKeyboardEvent(
          ui::ET_KEY_PRESSED, false, ui::VKEY_W, ui::EF_CONTROL_DOWN, 0)));
  EXPECT_FALSE(browser()->command_controller()->IsReservedCommandOrKey(
      IDC_FIND, content::NativeWebKeyboardEvent(
          ui::ET_KEY_PRESSED, false, ui::VKEY_F, ui::EF_CONTROL_DOWN, 0)));
#endif  // USE_AURA
}

TEST_F(BrowserCommandControllerTest, AppFullScreen) {
  // Enable for tabbed browser.
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_FULLSCREEN));

  // Enabled for app windows.
  browser()->app_name_ = "app";
  ASSERT_TRUE(browser()->is_app());
  browser()->command_controller()->FullscreenStateChanged();
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_FULLSCREEN));
}

TEST_F(BrowserCommandControllerTest, OldAvatarMenuDisabledWhenOnlyOneProfile) {
  if (!profiles::IsMultipleProfilesEnabled())
    return;

  EXPECT_FALSE(profiles::IsNewProfileManagementEnabled());

  TestingProfileManager testing_profile_manager(
      TestingBrowserProcess::GetGlobal());
  ASSERT_TRUE(testing_profile_manager.SetUp());
  ProfileManager* profile_manager = testing_profile_manager.profile_manager();

  chrome::BrowserCommandController command_controller(browser(),
                                                      profile_manager);
  const CommandUpdater* command_updater = command_controller.command_updater();

  testing_profile_manager.CreateTestingProfile("p1");
  ASSERT_EQ(1U, profile_manager->GetNumberOfProfiles());
  EXPECT_FALSE(command_updater->IsCommandEnabled(IDC_SHOW_AVATAR_MENU));

  testing_profile_manager.CreateTestingProfile("p2");
  ASSERT_EQ(2U, profile_manager->GetNumberOfProfiles());
  EXPECT_TRUE(command_updater->IsCommandEnabled(IDC_SHOW_AVATAR_MENU));

  testing_profile_manager.DeleteTestingProfile("p1");
  ASSERT_EQ(1U, profile_manager->GetNumberOfProfiles());
  EXPECT_FALSE(command_updater->IsCommandEnabled(IDC_SHOW_AVATAR_MENU));

  testing_profile_manager.DeleteTestingProfile("p2");
}

TEST_F(BrowserCommandControllerTest, NewAvatarMenuEnabledWhenOnlyOneProfile) {
  if (!profiles::IsMultipleProfilesEnabled())
    return;

  // The command line is reset at the end of every test by the test suite.
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kNewProfileManagement);
  EXPECT_TRUE(profiles::IsNewProfileManagementEnabled());

  TestingProfileManager testing_profile_manager(
      TestingBrowserProcess::GetGlobal());
  ASSERT_TRUE(testing_profile_manager.SetUp());
  ProfileManager* profile_manager = testing_profile_manager.profile_manager();

  chrome::BrowserCommandController command_controller(browser(),
                                                      profile_manager);
  const CommandUpdater* command_updater = command_controller.command_updater();

  testing_profile_manager.CreateTestingProfile("p1");
  ASSERT_EQ(1U, profile_manager->GetNumberOfProfiles());
  EXPECT_TRUE(command_updater->IsCommandEnabled(IDC_SHOW_AVATAR_MENU));
  testing_profile_manager.DeleteTestingProfile("p1");
}

TEST_F(BrowserCommandControllerTest, NewAvatarMenuEnabledInGuestMode) {
  if (!profiles::IsMultipleProfilesEnabled())
    return;

  // The command line is reset at the end of every test by the test suite.
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kNewProfileManagement);
  EXPECT_TRUE(profiles::IsNewProfileManagementEnabled());

  TestingProfileManager testing_profile_manager(
      TestingBrowserProcess::GetGlobal());
  ASSERT_TRUE(testing_profile_manager.SetUp());
  ProfileManager* profile_manager = testing_profile_manager.profile_manager();

  // Set up guest a profile.
  TestingProfile::Builder guest_builder;
  guest_builder.SetIncognito();  // Guest profiles are off the record.
  guest_builder.SetGuestSession();
  guest_builder.SetPath(ProfileManager::GetGuestProfilePath());
  scoped_ptr<TestingProfile>guest_profile = guest_builder.Build();

  ASSERT_TRUE(guest_profile->IsGuestSession());

  // Create a new browser based on the guest profile.
  Browser::CreateParams profile_params(guest_profile.get(),
                                       chrome::GetActiveDesktop());
  scoped_ptr<Browser> guest_browser(
      chrome::CreateBrowserWithTestWindowForParams(&profile_params));
  chrome::BrowserCommandController command_controller(guest_browser.get(),
                                                      profile_manager);
  const CommandUpdater* command_updater = command_controller.command_updater();
  EXPECT_TRUE(command_updater->IsCommandEnabled(IDC_SHOW_AVATAR_MENU));
}

TEST_F(BrowserCommandControllerTest, AvatarMenuAlwaysDisabledInIncognitoMode) {
  if (!profiles::IsMultipleProfilesEnabled())
    return;

  TestingProfileManager testing_profile_manager(
      TestingBrowserProcess::GetGlobal());
  ASSERT_TRUE(testing_profile_manager.SetUp());

  // Set up a profile with an off the record profile.
  TestingProfile::Builder otr_builder;
  otr_builder.SetIncognito();
  scoped_ptr<TestingProfile> otr_profile(otr_builder.Build());

  TestingProfile::Builder normal_builder;
  scoped_ptr<TestingProfile> original_profile = normal_builder.Build();
  otr_profile->SetOriginalProfile(original_profile.get());
  EXPECT_EQ(otr_profile->GetOriginalProfile(), original_profile.get());

  original_profile->SetOffTheRecordProfile(otr_profile.PassAs<Profile>());

  // Create a new browser based on the off the record profile.
  Browser::CreateParams profile_params(
      original_profile->GetOffTheRecordProfile(), chrome::GetActiveDesktop());
  scoped_ptr<Browser> otr_browser(
      chrome::CreateBrowserWithTestWindowForParams(&profile_params));

  ProfileManager* profile_manager = testing_profile_manager.profile_manager();
  chrome::BrowserCommandController command_controller(otr_browser.get(),
                                                      profile_manager);
  const CommandUpdater* command_updater = command_controller.command_updater();

  // The old style avatar menu should be disabled.
  EXPECT_FALSE(profiles::IsNewProfileManagementEnabled());
  EXPECT_FALSE(command_updater->IsCommandEnabled(IDC_SHOW_AVATAR_MENU));

  // The new style avatar menu should also be disabled.
  // The command line is reset at the end of every test by the test suite.
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kNewProfileManagement);
  EXPECT_TRUE(profiles::IsNewProfileManagementEnabled());
  EXPECT_FALSE(command_updater->IsCommandEnabled(IDC_SHOW_AVATAR_MENU));
}

//////////////////////////////////////////////////////////////////////////////

// A test browser window that can toggle fullscreen state.
class FullscreenTestBrowserWindow : public TestBrowserWindow {
 public:
  FullscreenTestBrowserWindow() : fullscreen_(false) {}
  virtual ~FullscreenTestBrowserWindow() {}

  // TestBrowserWindow overrides:
  virtual bool ShouldHideUIForFullscreen() const OVERRIDE {
    return fullscreen_;
  }
  virtual bool IsFullscreen() const OVERRIDE {
    return fullscreen_;
  }
  virtual void EnterFullscreen(
      const GURL& url, FullscreenExitBubbleType type) OVERRIDE {
    fullscreen_ = true;
  }
  virtual void ExitFullscreen() OVERRIDE {
    fullscreen_ = false;
  }

 private:
  bool fullscreen_;

  DISALLOW_COPY_AND_ASSIGN(FullscreenTestBrowserWindow);
};

// Test that uses FullscreenTestBrowserWindow for its window.
class BrowserCommandControllerFullscreenTest
    : public BrowserWithTestWindowTest {
 public:
  BrowserCommandControllerFullscreenTest() {}
  virtual ~BrowserCommandControllerFullscreenTest() {}

  // BrowserWithTestWindowTest overrides:
  virtual BrowserWindow* CreateBrowserWindow() OVERRIDE {
    return new FullscreenTestBrowserWindow;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(BrowserCommandControllerFullscreenTest);
};

TEST_F(BrowserCommandControllerFullscreenTest,
       UpdateCommandsForFullscreenMode) {
  // Defaults for a tabbed browser.
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_OPEN_CURRENT_URL));
  EXPECT_FALSE(chrome::IsCommandEnabled(browser(), IDC_SHOW_AS_TAB));
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_FOCUS_TOOLBAR));
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_FOCUS_LOCATION));
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_FOCUS_SEARCH));
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_FOCUS_MENU_BAR));
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_FOCUS_NEXT_PANE));
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_FOCUS_PREVIOUS_PANE));
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_FOCUS_BOOKMARKS));
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_DEVELOPER_MENU));
#if defined(GOOGLE_CHROME_BUILD)
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_FEEDBACK));
#endif
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_OPTIONS));
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_IMPORT_SETTINGS));
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_EDIT_SEARCH_ENGINES));
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_VIEW_PASSWORDS));
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_ABOUT));
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_SHOW_APP_MENU));
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_FULLSCREEN));

  // Simulate going fullscreen.
  chrome::ToggleFullscreenMode(browser());
  ASSERT_TRUE(browser()->window()->IsFullscreen());
  browser()->command_controller()->FullscreenStateChanged();

  // Most commands are disabled in fullscreen.
  EXPECT_FALSE(chrome::IsCommandEnabled(browser(), IDC_OPEN_CURRENT_URL));
  EXPECT_FALSE(chrome::IsCommandEnabled(browser(), IDC_SHOW_AS_TAB));
  EXPECT_FALSE(chrome::IsCommandEnabled(browser(), IDC_FOCUS_TOOLBAR));
  EXPECT_FALSE(chrome::IsCommandEnabled(browser(), IDC_FOCUS_LOCATION));
  EXPECT_FALSE(chrome::IsCommandEnabled(browser(), IDC_FOCUS_SEARCH));
  EXPECT_FALSE(chrome::IsCommandEnabled(browser(), IDC_FOCUS_MENU_BAR));
  EXPECT_FALSE(chrome::IsCommandEnabled(browser(), IDC_FOCUS_NEXT_PANE));
  EXPECT_FALSE(chrome::IsCommandEnabled(browser(), IDC_FOCUS_PREVIOUS_PANE));
  EXPECT_FALSE(chrome::IsCommandEnabled(browser(), IDC_FOCUS_BOOKMARKS));
  EXPECT_FALSE(chrome::IsCommandEnabled(browser(), IDC_DEVELOPER_MENU));
#if defined(GOOGLE_CHROME_BUILD)
  EXPECT_FALSE(chrome::IsCommandEnabled(browser(), IDC_FEEDBACK));
#endif
  EXPECT_FALSE(chrome::IsCommandEnabled(browser(), IDC_OPTIONS));
  EXPECT_FALSE(chrome::IsCommandEnabled(browser(), IDC_IMPORT_SETTINGS));
  EXPECT_FALSE(chrome::IsCommandEnabled(browser(), IDC_EDIT_SEARCH_ENGINES));
  EXPECT_FALSE(chrome::IsCommandEnabled(browser(), IDC_VIEW_PASSWORDS));
  EXPECT_FALSE(chrome::IsCommandEnabled(browser(), IDC_ABOUT));
  EXPECT_FALSE(chrome::IsCommandEnabled(browser(), IDC_SHOW_APP_MENU));
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_FULLSCREEN));

  // Exit fullscreen.
  chrome::ToggleFullscreenMode(browser());
  ASSERT_FALSE(browser()->window()->IsFullscreen());
  browser()->command_controller()->FullscreenStateChanged();
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_OPEN_CURRENT_URL));
  EXPECT_FALSE(chrome::IsCommandEnabled(browser(), IDC_SHOW_AS_TAB));
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_FOCUS_TOOLBAR));
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_FOCUS_LOCATION));
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_FOCUS_SEARCH));
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_FOCUS_MENU_BAR));
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_FOCUS_NEXT_PANE));
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_FOCUS_PREVIOUS_PANE));
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_FOCUS_BOOKMARKS));
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_DEVELOPER_MENU));
#if defined(GOOGLE_CHROME_BUILD)
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_FEEDBACK));
#endif
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_OPTIONS));
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_IMPORT_SETTINGS));
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_EDIT_SEARCH_ENGINES));
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_VIEW_PASSWORDS));
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_ABOUT));
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_SHOW_APP_MENU));
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_FULLSCREEN));
}

TEST_F(BrowserCommandControllerTest,
    IncognitoModeOnSigninAllowedPrefChange) {
  TestingProfileManager testing_profile_manager(
      TestingBrowserProcess::GetGlobal());
  ASSERT_TRUE(testing_profile_manager.SetUp());

  // Set up a profile with an off the record profile.
  TestingProfile::Builder builder;
  builder.SetIncognito();
  scoped_ptr<TestingProfile> profile2(builder.Build());
  TestingProfile::Builder builder2;
  scoped_ptr<TestingProfile> profile1 = builder2.Build();
  profile2->SetOriginalProfile(profile1.get());
  EXPECT_EQ(profile2->GetOriginalProfile(), profile1.get());
  profile1->SetOffTheRecordProfile(profile2.PassAs<Profile>());

  // Create a new browser based on the off the record profile.
  Browser::CreateParams profile_params(profile1->GetOffTheRecordProfile(),
                                       chrome::GetActiveDesktop());
  scoped_ptr<Browser> browser2(
      chrome::CreateBrowserWithTestWindowForParams(&profile_params));

  ProfileManager* profile_manager = testing_profile_manager.profile_manager();
  chrome::BrowserCommandController command_controller(browser2.get(),
                                                      profile_manager);
  const CommandUpdater* command_updater = command_controller.command_updater();

  // Check that the SYNC_SETUP command is updated on preference change.
  EXPECT_TRUE(command_updater->IsCommandEnabled(IDC_SHOW_SYNC_SETUP));
  profile1->GetPrefs()->SetBoolean(prefs::kSigninAllowed, false);
  EXPECT_FALSE(command_updater->IsCommandEnabled(IDC_SHOW_SYNC_SETUP));
}

TEST_F(BrowserCommandControllerTest,
    OnSigninAllowedPrefChange) {
  TestingProfileManager testing_profile_manager(
      TestingBrowserProcess::GetGlobal());
  ASSERT_TRUE(testing_profile_manager.SetUp());
  ProfileManager* profile_manager = testing_profile_manager.profile_manager();
  chrome::BrowserCommandController command_controller(browser(),
                                                      profile_manager);
  const CommandUpdater* command_updater = command_controller.command_updater();

  // Check that the SYNC_SETUP command is updated on preference change.
  EXPECT_TRUE(command_updater->IsCommandEnabled(IDC_SHOW_SYNC_SETUP));
  profile()->GetPrefs()->SetBoolean(prefs::kSigninAllowed, false);
  EXPECT_FALSE(command_updater->IsCommandEnabled(IDC_SHOW_SYNC_SETUP));
}
