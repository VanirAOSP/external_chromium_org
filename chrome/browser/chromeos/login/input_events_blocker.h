// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_INPUT_EVENTS_BLOCKER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_INPUT_EVENTS_BLOCKER_H_

#include "base/compiler_specific.h"
#include "ui/events/event_handler.h"

namespace chromeos {

// A simple input events blocker that just makes device unresponsive.
// Should be used only for a (very) short time lock as it's confusing to the
// user.
class InputEventsBlocker : public ui::EventHandler {
 public:
  InputEventsBlocker();
  virtual ~InputEventsBlocker();

  virtual void OnKeyEvent(ui::KeyEvent* event) OVERRIDE;
  virtual void OnMouseEvent(ui::MouseEvent* event) OVERRIDE;
  virtual void OnTouchEvent(ui::TouchEvent* event) OVERRIDE;
  virtual void OnGestureEvent(ui::GestureEvent* event) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(InputEventsBlocker);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_INPUT_EVENTS_BLOCKER_H_
