// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/content_settings/popup_blocked_infobar_delegate.h"

#include "base/prefs/pref_service.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/infobars/infobar.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/blocked_content/popup_blocker_tab_helper.h"
#include "chrome/common/content_settings_types.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"


// static
void PopupBlockedInfoBarDelegate::Create(InfoBarService* infobar_service,
                                         int num_popups) {
  scoped_ptr<InfoBar> infobar(ConfirmInfoBarDelegate::CreateInfoBar(
      scoped_ptr<ConfirmInfoBarDelegate>(
          new PopupBlockedInfoBarDelegate(num_popups))));

  // See if there is an existing popup infobar already.
  // TODO(dfalcantara) When triggering more than one popup the infobar
  // will be shown once, then hide then be shown again.
  // This will be fixed once we have an in place replace infobar mechanism.
  for (size_t i = 0; i < infobar_service->infobar_count(); ++i) {
    InfoBar* existing_infobar = infobar_service->infobar_at(i);
    if (existing_infobar->delegate()->AsPopupBlockedInfoBarDelegate()) {
      infobar_service->ReplaceInfoBar(existing_infobar, infobar.Pass());
      return;
    }
  }

  infobar_service->AddInfoBar(infobar.Pass());
}

PopupBlockedInfoBarDelegate::~PopupBlockedInfoBarDelegate() {
}

int PopupBlockedInfoBarDelegate::GetIconID() const {
  return IDR_BLOCKED_POPUPS;
}

PopupBlockedInfoBarDelegate*
    PopupBlockedInfoBarDelegate::AsPopupBlockedInfoBarDelegate() {
  return this;
}

PopupBlockedInfoBarDelegate::PopupBlockedInfoBarDelegate(int num_popups)
    : ConfirmInfoBarDelegate(),
      num_popups_(num_popups) {
}

base::string16 PopupBlockedInfoBarDelegate::GetMessageText() const {
  return l10n_util::GetStringFUTF16Int(IDS_POPUPS_BLOCKED_INFOBAR_TEXT,
                                       num_popups_);
}

int PopupBlockedInfoBarDelegate::GetButtons() const {
  return BUTTON_OK;
}

base::string16 PopupBlockedInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  return l10n_util::GetStringUTF16(IDS_POPUPS_BLOCKED_INFOBAR_BUTTON_SHOW);
}

bool PopupBlockedInfoBarDelegate::Accept() {
  // Create exceptions.
  const GURL& url = web_contents()->GetURL();
  Profile* profile = Profile::FromBrowserContext(
      web_contents()->GetBrowserContext());
  profile->GetHostContentSettingsMap()->AddExceptionForURL(
      url, url, CONTENT_SETTINGS_TYPE_POPUPS, CONTENT_SETTING_ALLOW);

  // Launch popups.
  PopupBlockerTabHelper* popup_blocker_helper =
      PopupBlockerTabHelper::FromWebContents(web_contents());
  DCHECK(popup_blocker_helper);
  PopupBlockerTabHelper::PopupIdMap blocked_popups =
      popup_blocker_helper->GetBlockedPopupRequests();
  for (PopupBlockerTabHelper::PopupIdMap::iterator it = blocked_popups.begin();
      it != blocked_popups.end(); ++it)
    popup_blocker_helper->ShowBlockedPopup(it->first);

  return true;
}

