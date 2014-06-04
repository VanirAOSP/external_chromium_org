// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/status_change_checker.h"

StatusChangeChecker::StatusChangeChecker(
    StatusChangeCallback callback,
    const std::string& callback_name)
    : callback_(callback),
      callback_name_(callback_name) {
}

StatusChangeChecker::~StatusChangeChecker() {
}

bool StatusChangeChecker::IsExitConditionSatisfied() {
  return callback_.Run();
}
