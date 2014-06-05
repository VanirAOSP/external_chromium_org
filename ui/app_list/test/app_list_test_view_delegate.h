// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_APP_LIST_TEST_APP_LIST_TEST_VIEW_DELEGATE_H_
#define UI_APP_LIST_TEST_APP_LIST_TEST_VIEW_DELEGATE_H_

#include <string>

#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "ui/app_list/app_list_view_delegate.h"
#include "ui/app_list/speech_ui_model.h"

namespace app_list {
namespace test {

class AppListTestModel;
class TestSigninDelegate;

// A concrete AppListViewDelegate for unit tests.
class AppListTestViewDelegate : public AppListViewDelegate {
 public:
  AppListTestViewDelegate();
  virtual ~AppListTestViewDelegate();

  int dismiss_count() { return dismiss_count_; }
  int open_search_result_count() { return open_search_result_count_; }
  void SetUsers(const Users& users) {
    users_ = users;
  }

  // Sets the signin status of the signin delegate, creating one if there isn't
  // one already.
  void SetSignedIn(bool signed_in);

  // AppListViewDelegate overrides:
  virtual bool ForceNativeDesktop() const OVERRIDE;
  virtual void SetProfileByPath(const base::FilePath& profile_path) OVERRIDE {}
  virtual AppListModel* GetModel() OVERRIDE;
  virtual SigninDelegate* GetSigninDelegate() OVERRIDE;
  virtual SpeechUIModel* GetSpeechUI() OVERRIDE;
  virtual void GetShortcutPathForApp(
      const std::string& app_id,
      const base::Callback<void(const base::FilePath&)>& callback) OVERRIDE;
  virtual void StartSearch() OVERRIDE {}
  virtual void StopSearch() OVERRIDE {}
  virtual void OpenSearchResult(SearchResult* result,
                                int event_flags) OVERRIDE;
  virtual void InvokeSearchResultAction(SearchResult* result,
                                        int action_index,
                                        int event_flags) OVERRIDE {}
  virtual void Dismiss() OVERRIDE;
  virtual void ViewClosing() OVERRIDE {}
  virtual gfx::ImageSkia GetWindowIcon() OVERRIDE;
  virtual void OpenSettings() OVERRIDE {}
  virtual void OpenHelp() OVERRIDE {}
  virtual void OpenFeedback() OVERRIDE {}
  virtual void ToggleSpeechRecognition() OVERRIDE {}
  virtual void ShowForProfileByPath(
      const base::FilePath& profile_path) OVERRIDE {}
  virtual content::WebContents* GetStartPageContents() OVERRIDE;
  virtual const Users& GetUsers() const OVERRIDE;
  virtual void AddObserver(AppListViewDelegateObserver* observer) OVERRIDE;
  virtual void RemoveObserver(AppListViewDelegateObserver* observer) OVERRIDE;

  // Do a bulk replacement of the items in the model.
  void ReplaceTestModel(int item_count);

  AppListTestModel* ReleaseTestModel() { return model_.release(); }
  AppListTestModel* GetTestModel() { return model_.get(); }

 private:
  int dismiss_count_;
  int open_search_result_count_;
  Users users_;
  scoped_ptr<TestSigninDelegate> test_signin_delegate_;
  scoped_ptr<AppListTestModel> model_;
  ObserverList<AppListViewDelegateObserver> observers_;
  SpeechUIModel speech_ui_;

  DISALLOW_COPY_AND_ASSIGN(AppListTestViewDelegate);
};

}  // namespace test
}  // namespace app_list

#endif  // UI_APP_LIST_TEST_APP_LIST_TEST_VIEW_DELEGATE_H_
