// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/input_method_configuration.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chromeos/input_method/accessibility.h"
#include "chrome/browser/chromeos/input_method/browser_state_monitor.h"
#include "chrome/browser/chromeos/input_method/input_method_delegate_impl.h"
#include "chrome/browser/chromeos/input_method/input_method_manager_impl.h"
#include "chrome/browser/chromeos/input_method/input_method_persistence.h"
#include "ui/base/ime/chromeos/ibus_bridge.h"

namespace chromeos {
namespace input_method {

namespace {
void OnSessionStateChange(InputMethodManagerImpl* input_method_manager_impl,
                          InputMethodPersistence* input_method_persistence,
                          InputMethodManager::State new_state) {
  input_method_persistence->OnSessionStateChange(new_state);
  input_method_manager_impl->SetState(new_state);
}

class InputMethodConfiguration {
 public:
  InputMethodConfiguration() {}
  virtual ~InputMethodConfiguration() {}

  void Initialize(
      const scoped_refptr<base::SequencedTaskRunner>& ui_task_runner) {
    IBusBridge::Initialize();

    InputMethodManagerImpl* impl = new InputMethodManagerImpl(
        scoped_ptr<InputMethodDelegate>(new InputMethodDelegateImpl));
    impl->Init(ui_task_runner.get());
    InputMethodManager::Initialize(impl);

    DCHECK(InputMethodManager::Get());

    accessibility_.reset(new Accessibility(impl));
    input_method_persistence_.reset(new InputMethodPersistence(impl));
    browser_state_monitor_.reset(new BrowserStateMonitor(
        base::Bind(&OnSessionStateChange,
                   impl,
                   input_method_persistence_.get())));

    DVLOG(1) << "InputMethodManager initialized";
  }

  void InitializeForTesting(InputMethodManager* mock_manager) {
    InputMethodManager::Initialize(mock_manager);
    DVLOG(1) << "InputMethodManager for testing initialized";
  }

  void Shutdown() {
    accessibility_.reset();
    browser_state_monitor_.reset();
    input_method_persistence_.reset();

    InputMethodManager::Shutdown();

    IBusBridge::Shutdown();

    DVLOG(1) << "InputMethodManager shutdown";
  }

 private:
  scoped_ptr<Accessibility> accessibility_;
  scoped_ptr<BrowserStateMonitor> browser_state_monitor_;
  scoped_ptr<InputMethodPersistence> input_method_persistence_;
};

InputMethodConfiguration* g_input_method_configuration = NULL;

}  // namespace

void Initialize(
    const scoped_refptr<base::SequencedTaskRunner>& ui_task_runner,
    const scoped_refptr<base::SequencedTaskRunner>& file_task_runner) {
  if (!g_input_method_configuration)
    g_input_method_configuration = new InputMethodConfiguration();
  g_input_method_configuration->Initialize(ui_task_runner);
}

void InitializeForTesting(InputMethodManager* mock_manager) {
  if (!g_input_method_configuration)
    g_input_method_configuration = new InputMethodConfiguration();
  g_input_method_configuration->InitializeForTesting(mock_manager);
}

void Shutdown() {
  if (!g_input_method_configuration)
    return;

  g_input_method_configuration->Shutdown();
  delete g_input_method_configuration;
  g_input_method_configuration = NULL;
}

}  // namespace input_method
}  // namespace chromeos
