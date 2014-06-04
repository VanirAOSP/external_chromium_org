// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/infobars/translate_infobar.h"

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_helper.h"
#include "chrome/browser/translate/translate_infobar_delegate.h"
#include "grit/generated_resources.h"
#include "jni/TranslateInfoBarDelegate_jni.h"
#include "ui/base/l10n/l10n_util.h"


// TranslateInfoBarDelegate ---------------------------------------------------

// static
scoped_ptr<InfoBar> TranslateInfoBarDelegate::CreateInfoBar(
    scoped_ptr<TranslateInfoBarDelegate> delegate) {
  return scoped_ptr<InfoBar>(new TranslateInfoBar(delegate.Pass()));
}


// TranslateInfoBar -----------------------------------------------------------

TranslateInfoBar::TranslateInfoBar(
    scoped_ptr<TranslateInfoBarDelegate> delegate)
    : InfoBarAndroid(delegate.PassAs<InfoBarDelegate>()),
      java_translate_delegate_() {
}

TranslateInfoBar::~TranslateInfoBar() {
}

ScopedJavaLocalRef<jobject> TranslateInfoBar::CreateRenderInfoBar(JNIEnv* env) {
  java_translate_delegate_.Reset(Java_TranslateInfoBarDelegate_create(env));
  TranslateInfoBarDelegate* delegate = GetDelegate();
  std::vector<base::string16> languages;
  languages.reserve(delegate->num_languages());
  for (size_t i = 0; i < delegate->num_languages(); ++i)
    languages.push_back(delegate->language_name_at(i));

  base::android::ScopedJavaLocalRef<jobjectArray> java_languages =
      base::android::ToJavaArrayOfStrings(env, languages);
  return Java_TranslateInfoBarDelegate_showTranslateInfoBar(
      env, java_translate_delegate_.obj(), reinterpret_cast<intptr_t>(this),
      delegate->infobar_type(), delegate->original_language_index(),
      delegate->target_language_index(), delegate->ShouldAlwaysTranslate(),
      ShouldDisplayNeverTranslateInfoBarOnCancel(), java_languages.obj());
}

void TranslateInfoBar::ProcessButton(int action,
                                     const std::string& action_value) {
  if (!owner())
    return;  // We're closing; don't call anything, it might access the owner.

  TranslateInfoBarDelegate* delegate = GetDelegate();
  if (action == InfoBarAndroid::ACTION_TRANSLATE) {
    delegate->Translate();
    return;
  }

  if (action == InfoBarAndroid::ACTION_CANCEL)
    delegate->TranslationDeclined();
  else if (action == InfoBarAndroid::ACTION_TRANSLATE_SHOW_ORIGINAL)
    delegate->RevertTranslation();
  else
    DCHECK_EQ(InfoBarAndroid::ACTION_NONE, action);

  RemoveSelf();
}

void TranslateInfoBar::PassJavaInfoBar(InfoBarAndroid* source) {
  TranslateInfoBarDelegate* delegate = GetDelegate();
  DCHECK_NE(TranslateInfoBarDelegate::BEFORE_TRANSLATE,
            delegate->infobar_type());

  // Ask the former bar to transfer ownership to us.
  DCHECK(source != NULL);
  static_cast<TranslateInfoBar*>(source)->TransferOwnership(
      this, delegate->infobar_type());
}

void TranslateInfoBar::ApplyTranslateOptions(JNIEnv* env,
                                             jobject obj,
                                             int source_language_index,
                                             int target_language_index,
                                             bool always_translate,
                                             bool never_translate_language,
                                             bool never_translate_site) {
  TranslateInfoBarDelegate* delegate = GetDelegate();
  delegate->UpdateOriginalLanguageIndex(source_language_index);
  delegate->UpdateTargetLanguageIndex(target_language_index);

  if (delegate->ShouldAlwaysTranslate() != always_translate)
    delegate->ToggleAlwaysTranslate();

  if (never_translate_language && delegate->IsTranslatableLanguageByPrefs())
    delegate->ToggleTranslatableLanguageByPrefs();

  if (never_translate_site && !delegate->IsSiteBlacklisted())
    delegate->ToggleSiteBlacklist();
}

void TranslateInfoBar::TransferOwnership(
    TranslateInfoBar* destination,
    TranslateInfoBarDelegate::Type new_type) {
  JNIEnv* env = base::android::AttachCurrentThread();
  if (Java_TranslateInfoBarDelegate_changeTranslateInfoBarTypeAndPointer(
      env, java_translate_delegate_.obj(),
      reinterpret_cast<intptr_t>(destination), new_type)) {
    ReassignJavaInfoBar(destination);
    destination->SetJavaDelegate(java_translate_delegate_.Release());
  }
}

void TranslateInfoBar::SetJavaDelegate(jobject delegate) {
  JNIEnv* env = base::android::AttachCurrentThread();
  java_translate_delegate_.Reset(env, delegate);
}

bool TranslateInfoBar::ShouldDisplayNeverTranslateInfoBarOnCancel() {
  TranslateInfoBarDelegate* delegate = GetDelegate();
  return
      (delegate->infobar_type() ==
          TranslateInfoBarDelegate::BEFORE_TRANSLATE) &&
      delegate->ShouldShowNeverTranslateShortcut();
}

TranslateInfoBarDelegate* TranslateInfoBar::GetDelegate() {
  return delegate()->AsTranslateInfoBarDelegate();
}


// Native JNI methods ---------------------------------------------------------

bool RegisterTranslateInfoBarDelegate(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
