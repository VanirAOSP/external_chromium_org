// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/simple_message_box.h"

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/views/constrained_window_views.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/controls/message_box_view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"

#if defined(USE_AURA)
#include "ui/aura/client/dispatcher_client.h"
#include "ui/aura/env.h"
#include "ui/aura/root_window.h"
#if defined(OS_WIN)
#include "chrome/browser/ui/views/simple_message_box_win.h"
#endif
#endif

namespace chrome {

namespace {

// Multiple SimpleMessageBoxViews can show up at the same time. Each of these
// start a nested message-loop. However, these SimpleMessageBoxViews can be
// deleted in any order. This creates problems if a box in an inner-loop gets
// destroyed before a box in an outer-loop. So to avoid this, ref-counting is
// used so that the SimpleMessageBoxViews gets deleted at the right time.
class SimpleMessageBoxViews : public views::DialogDelegate,
                              public base::MessageLoop::Dispatcher,
                              public base::RefCounted<SimpleMessageBoxViews> {
 public:
  SimpleMessageBoxViews(const base::string16& title,
                        const base::string16& message,
                        MessageBoxType type,
                        const base::string16& yes_text,
                        const base::string16& no_text);

  MessageBoxResult result() const { return result_; }

  // Overridden from views::DialogDelegate:
  virtual int GetDialogButtons() const OVERRIDE;
  virtual base::string16 GetDialogButtonLabel(
      ui::DialogButton button) const OVERRIDE;
  virtual bool Cancel() OVERRIDE;
  virtual bool Accept() OVERRIDE;

  // Overridden from views::WidgetDelegate:
  virtual base::string16 GetWindowTitle() const OVERRIDE;
  virtual void DeleteDelegate() OVERRIDE;
  virtual ui::ModalType GetModalType() const OVERRIDE;
  virtual views::View* GetContentsView() OVERRIDE;
  virtual views::Widget* GetWidget() OVERRIDE;
  virtual const views::Widget* GetWidget() const OVERRIDE;

  // Overridden from MessageLoop::Dispatcher:
  virtual bool Dispatch(const base::NativeEvent& event) OVERRIDE;

 private:
  friend class base::RefCounted<SimpleMessageBoxViews>;
  virtual ~SimpleMessageBoxViews();

  const base::string16 window_title_;
  const MessageBoxType type_;
  base::string16 yes_text_;
  base::string16 no_text_;
  MessageBoxResult result_;
  views::MessageBoxView* message_box_view_;

  // Set to false as soon as the user clicks a dialog button; this tells the
  // dispatcher we're done.
  bool should_show_dialog_;

  DISALLOW_COPY_AND_ASSIGN(SimpleMessageBoxViews);
};

////////////////////////////////////////////////////////////////////////////////
// SimpleMessageBoxViews, public:

SimpleMessageBoxViews::SimpleMessageBoxViews(const base::string16& title,
                                             const base::string16& message,
                                             MessageBoxType type,
                                             const base::string16& yes_text,
                                             const base::string16& no_text)
    : window_title_(title),
      type_(type),
      yes_text_(yes_text),
      no_text_(no_text),
      result_(MESSAGE_BOX_RESULT_NO),
      message_box_view_(new views::MessageBoxView(
          views::MessageBoxView::InitParams(message))),
      should_show_dialog_(true) {
  AddRef();

  if (yes_text_.empty()) {
    if (type_ == MESSAGE_BOX_TYPE_QUESTION)
      yes_text_ =
          l10n_util::GetStringUTF16(IDS_CONFIRM_MESSAGEBOX_YES_BUTTON_LABEL);
    else if (type_ == MESSAGE_BOX_TYPE_OK_CANCEL)
      yes_text_ = l10n_util::GetStringUTF16(IDS_OK);
    else
      yes_text_ = l10n_util::GetStringUTF16(IDS_OK);
  }

  if (no_text_.empty()) {
    if (type_ == MESSAGE_BOX_TYPE_QUESTION)
      no_text_ =
          l10n_util::GetStringUTF16(IDS_CONFIRM_MESSAGEBOX_NO_BUTTON_LABEL);
    else if (type_ == MESSAGE_BOX_TYPE_OK_CANCEL)
      no_text_ = l10n_util::GetStringUTF16(IDS_CANCEL);
  }
}

int SimpleMessageBoxViews::GetDialogButtons() const {
  if (type_ == MESSAGE_BOX_TYPE_QUESTION ||
      type_ == MESSAGE_BOX_TYPE_OK_CANCEL) {
    return ui::DIALOG_BUTTON_OK | ui::DIALOG_BUTTON_CANCEL;
  }

  return ui::DIALOG_BUTTON_OK;
}

base::string16 SimpleMessageBoxViews::GetDialogButtonLabel(
    ui::DialogButton button) const {
  if (button == ui::DIALOG_BUTTON_CANCEL)
    return no_text_;
  return yes_text_;
}

bool SimpleMessageBoxViews::Cancel() {
  should_show_dialog_= false;
  result_ = MESSAGE_BOX_RESULT_NO;
  return true;
}

bool SimpleMessageBoxViews::Accept() {
  should_show_dialog_ = false;
  result_ = MESSAGE_BOX_RESULT_YES;
  return true;
}

base::string16 SimpleMessageBoxViews::GetWindowTitle() const {
  return window_title_;
}

void SimpleMessageBoxViews::DeleteDelegate() {
  Release();
}

ui::ModalType SimpleMessageBoxViews::GetModalType() const {
  return ui::MODAL_TYPE_WINDOW;
}

views::View* SimpleMessageBoxViews::GetContentsView() {
  return message_box_view_;
}

views::Widget* SimpleMessageBoxViews::GetWidget() {
  return message_box_view_->GetWidget();
}

const views::Widget* SimpleMessageBoxViews::GetWidget() const {
  return message_box_view_->GetWidget();
}

bool SimpleMessageBoxViews::Dispatch(const base::NativeEvent& event) {
#if defined(OS_WIN)
  TranslateMessage(&event);
  DispatchMessage(&event);
#elif defined(USE_AURA)
  aura::Env::GetInstance()->GetDispatcher()->Dispatch(event);
#endif
  return should_show_dialog_;
}

////////////////////////////////////////////////////////////////////////////////
// SimpleMessageBoxViews, private:

SimpleMessageBoxViews::~SimpleMessageBoxViews() {
}

MessageBoxResult ShowMessageBoxImpl(gfx::NativeWindow parent,
                                    const base::string16& title,
                                    const base::string16& message,
                                    MessageBoxType type,
                                    const base::string16& yes_text,
                                    const base::string16& no_text) {

#if defined(USE_AURA) && defined(OS_WIN)
  // If we're very early, we can't show a GPU-based dialog, so fallback to
  // plain Windows MessageBox.
  if (!ui::ContextFactory::GetInstance())
    return NativeShowMessageBox(NULL, title, message, type);
#endif

  scoped_refptr<SimpleMessageBoxViews> dialog(
      new SimpleMessageBoxViews(title, message, type, yes_text, no_text));
  CreateBrowserModalDialogViews(dialog.get(), parent)->Show();

#if defined(USE_AURA)
  aura::Window* anchor = parent;
  aura::client::DispatcherClient* client = anchor ?
      aura::client::GetDispatcherClient(anchor->GetRootWindow()) : NULL;
  if (!client) {
    // Use the widget's window itself so that the message loop
    // exists when the dialog is closed by some other means than
    // |Cancel| or |Accept|.
    anchor = dialog->GetWidget()->GetNativeWindow();
    client = aura::client::GetDispatcherClient(anchor->GetRootWindow());
  }
  client->RunWithDispatcher(dialog.get(), anchor, true);
#else
  {
    base::MessageLoop::ScopedNestableTaskAllower allow(
        base::MessageLoopForUI::current());
    base::RunLoop run_loop(dialog);
    run_loop.Run();
  }
#endif
  return dialog->result();
}

}  // namespace

MessageBoxResult ShowMessageBox(gfx::NativeWindow parent,
                                const base::string16& title,
                                const base::string16& message,
                                MessageBoxType type) {
  return ShowMessageBoxImpl(
      parent, title, message, type, base::string16(), base::string16());
}

#if defined(USE_AURA)
MessageBoxResult ShowMessageBoxWithButtonText(gfx::NativeWindow parent,
                                              const base::string16& title,
                                              const base::string16& message,
                                              const base::string16& yes_text,
                                              const base::string16& no_text) {
  return ShowMessageBoxImpl(
      parent, title, message, MESSAGE_BOX_TYPE_QUESTION, yes_text, no_text);
}
#endif

}  // namespace chrome
