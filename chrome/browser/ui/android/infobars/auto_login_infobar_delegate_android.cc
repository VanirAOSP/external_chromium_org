// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/infobars/auto_login_infobar_delegate_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_helper.h"
#include "base/android/jni_string.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/infobars/infobar.h"
#include "chrome/browser/infobars/simple_alert_infobar_delegate.h"
#include "chrome/browser/ui/auto_login_infobar_delegate.h"
#include "content/public/browser/web_contents.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "jni/AutoLoginDelegate_jni.h"
#include "ui/base/l10n/l10n_util.h"

using base::android::ConvertUTF8ToJavaString;
using base::android::ScopedJavaLocalRef;


AutoLoginInfoBarDelegateAndroid::AutoLoginInfoBarDelegateAndroid(
    const Params& params,
    Profile* profile)
    : AutoLoginInfoBarDelegate(params, profile),
      params_(params) {
}

AutoLoginInfoBarDelegateAndroid::~AutoLoginInfoBarDelegateAndroid() {
}

bool AutoLoginInfoBarDelegateAndroid::AttachAccount(
    JavaObjectWeakGlobalRef weak_java_auto_login_delegate) {
  weak_java_auto_login_delegate_ = weak_java_auto_login_delegate;
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jstring> jrealm = ConvertUTF8ToJavaString(env, realm());
  ScopedJavaLocalRef<jstring> jaccount =
      ConvertUTF8ToJavaString(env, account());
  ScopedJavaLocalRef<jstring> jargs = ConvertUTF8ToJavaString(env, args());
  DCHECK(!jrealm.is_null());
  DCHECK(!jaccount.is_null());
  DCHECK(!jargs.is_null());

  ScopedJavaLocalRef<jobject> delegate =
      weak_java_auto_login_delegate_.get(env);
  DCHECK(delegate.obj());
  user_ = base::android::ConvertJavaStringToUTF8(
      Java_AutoLoginDelegate_initializeAccount(
          env, delegate.obj(), reinterpret_cast<intptr_t>(this), jrealm.obj(),
          jaccount.obj(), jargs.obj()));
  return !user_.empty();
}

base::string16 AutoLoginInfoBarDelegateAndroid::GetMessageText() const {
  return l10n_util::GetStringFUTF16(IDS_AUTOLOGIN_INFOBAR_MESSAGE,
                                    UTF8ToUTF16(user_));
}

bool AutoLoginInfoBarDelegateAndroid::Accept() {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> delegate =
      weak_java_auto_login_delegate_.get(env);
  DCHECK(delegate.obj());
  Java_AutoLoginDelegate_logIn(env, delegate.obj(),
                               reinterpret_cast<intptr_t>(this));
  // Do not close the infobar on accept, it will be closed as part
  // of the log in callback.
  return false;
}

bool AutoLoginInfoBarDelegateAndroid::Cancel() {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> delegate =
      weak_java_auto_login_delegate_.get(env);
  DCHECK(delegate.obj());
  Java_AutoLoginDelegate_cancelLogIn(env, delegate.obj(),
                                     reinterpret_cast<intptr_t>(this));
  return true;
}

void AutoLoginInfoBarDelegateAndroid::LoginSuccess(JNIEnv* env,
                                                   jobject obj,
                                                   jstring result) {
  if (!infobar()->owner())
     return;  // We're closing; don't call anything, it might access the owner.

  // TODO(miguelg): Test whether the Stop() and RemoveInfoBar() calls here are
  // necessary, or whether OpenURL() will do this for us.
  content::WebContents* contents = web_contents();
  contents->Stop();
  infobar()->RemoveSelf();
  // WARNING: |this| may be deleted at this point!  Do not access any members!
  contents->OpenURL(content::OpenURLParams(
      GURL(base::android::ConvertJavaStringToUTF8(env, result)),
      content::Referrer(), CURRENT_TAB, content::PAGE_TRANSITION_AUTO_BOOKMARK,
      false));
}

void AutoLoginInfoBarDelegateAndroid::LoginFailed(JNIEnv* env, jobject obj) {
  if (!infobar()->owner())
     return;  // We're closing; don't call anything, it might access the owner.

  // TODO(miguelg): Using SimpleAlertInfoBarDelegate::Create() animates in a new
  // infobar while we animate the current one closed.  It would be better to use
  // ReplaceInfoBar().
  SimpleAlertInfoBarDelegate::Create(
      infobar()->owner(), IDR_INFOBAR_WARNING,
      l10n_util::GetStringUTF16(IDS_AUTO_LOGIN_FAILED), false);
  infobar()->RemoveSelf();
}

void AutoLoginInfoBarDelegateAndroid::LoginDismiss(JNIEnv* env, jobject obj) {
  infobar()->RemoveSelf();
}

bool AutoLoginInfoBarDelegateAndroid::Register(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
