// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/mode_indicator_controller.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/input_method/input_method_util.h"
#include "chrome/browser/chromeos/input_method/mode_indicator_delegate_view.h"
#include "chromeos/chromeos_switches.h"

namespace chromeos {
namespace input_method {

namespace {
ModeIndicatorObserverInterface* g_mode_indicator_observer_for_testing_ = NULL;
}  // namespace

class ModeIndicatorObserver : public ModeIndicatorObserverInterface {
 public:
  ModeIndicatorObserver()
    : active_widget_(NULL) {}

  virtual ~ModeIndicatorObserver() {
    if (active_widget_)
      active_widget_->RemoveObserver(this);
  }

  // If other active mode indicator widget is shown, close it immedicately
  // without fading animation.  Then store this widget as the active widget.
  virtual void AddModeIndicatorWidget(views::Widget* widget) OVERRIDE {
    DCHECK(widget);
    if (active_widget_)
      active_widget_->Close();
    active_widget_ = widget;
    widget->AddObserver(this);
  }

  // views::WidgetObserver override:
  virtual void OnWidgetDestroying(views::Widget* widget) OVERRIDE {
    if (widget == active_widget_)
      active_widget_ = NULL;
  }

 private:
  views::Widget* active_widget_;
};


ModeIndicatorController::ModeIndicatorController(InputMethodManager* imm)
    : imm_(imm),
      is_focused_(false),
      mi_observer_(new ModeIndicatorObserver) {
  DCHECK(imm_);
  imm_->AddObserver(this);
}

ModeIndicatorController::~ModeIndicatorController() {
  imm_->RemoveObserver(this);
}

void ModeIndicatorController::SetCursorBounds(
    const gfx::Rect& cursor_bounds) {
  cursor_bounds_ = cursor_bounds;
}

void ModeIndicatorController::FocusStateChanged(bool is_focused) {
  is_focused_ = is_focused;
}

// static
void ModeIndicatorController::SetModeIndicatorObserverForTesting(
    ModeIndicatorObserverInterface* observer) {
  g_mode_indicator_observer_for_testing_ = observer;
}

// static
ModeIndicatorObserverInterface*
ModeIndicatorController::GetModeIndicatorObserverForTesting() {
  return g_mode_indicator_observer_for_testing_;
}

void ModeIndicatorController::InputMethodChanged(InputMethodManager* manager,
                                                 bool show_message) {
  if (!show_message)
    return;
  ShowModeIndicator();
}

void ModeIndicatorController::InputMethodPropertyChanged(
    InputMethodManager* manager) {
  // Do nothing.
}

void ModeIndicatorController::ShowModeIndicator() {
  // TODO(komatsu): When this is permanently enabled by defalut,
  // delete command_line.h and chromeos_switches.h from the header
  // files.
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableIMEModeIndicator))
    return;

  // TODO(komatsu): Show the mode indicator in the right bottom of the
  // display when the launch bar is hidden and the focus is out.  To
  // implement it, we should consider to use message center or system
  // notification.  Note, launch bar can be vertical and can be placed
  // right/left side of display.
  if (!is_focused_)
    return;

  // Get the short name of the changed input method (e.g. US, JA, etc.)
  const InputMethodDescriptor descriptor = imm_->GetCurrentInputMethod();
  const base::string16 short_name =
      imm_->GetInputMethodUtil()->GetInputMethodShortName(descriptor);

  ModeIndicatorDelegateView* mi_delegate_view =
      new ModeIndicatorDelegateView(cursor_bounds_, short_name);
  views::BubbleDelegateView::CreateBubble(mi_delegate_view);

  views::Widget* mi_widget = mi_delegate_view->GetWidget();
  if (GetModeIndicatorObserverForTesting())
    GetModeIndicatorObserverForTesting()->AddModeIndicatorWidget(mi_widget);

  mi_observer_->AddModeIndicatorWidget(mi_widget);
  mi_delegate_view->ShowAndFadeOut();
}

}  // namespace input_method
}  // namespace chromeos
