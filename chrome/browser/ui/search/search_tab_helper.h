// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SEARCH_SEARCH_TAB_HELPER_H_
#define CHROME_BROWSER_UI_SEARCH_SEARCH_TAB_HELPER_H_

#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "chrome/browser/search/instant_service_observer.h"
#include "chrome/browser/ui/search/search_ipc_router.h"
#include "chrome/browser/ui/search/search_model.h"
#include "chrome/common/instant_types.h"
#include "chrome/common/ntp_logging_events.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "ui/base/window_open_disposition.h"

namespace content {
class WebContents;
struct LoadCommittedDetails;
}

class GURL;
class InstantPageTest;
class InstantService;
class Profile;
class SearchIPCRouterTest;

// Per-tab search "helper".  Acts as the owner and controller of the tab's
// search UI model.
//
// When the page is finished loading, SearchTabHelper determines the instant
// support for the page. When a navigation entry is committed (except for
// in-page navigations), SearchTabHelper resets the instant support state to
// INSTANT_SUPPORT_UNKNOWN and cause support to be determined again.
class SearchTabHelper : public content::WebContentsObserver,
                        public content::WebContentsUserData<SearchTabHelper>,
                        public InstantServiceObserver,
                        public SearchIPCRouter::Delegate {
 public:
  virtual ~SearchTabHelper();

  SearchModel* model() {
    return &model_;
  }

  // Sets up the initial state correctly for a preloaded NTP.
  void InitForPreloadedNTP();

  // Invoked when the OmniboxEditModel changes state in some way that might
  // affect the search mode.
  void OmniboxEditModelChanged(bool user_input_in_progress, bool cancelling);

  // Invoked when the active navigation entry is updated in some way that might
  // affect the search mode. This is used by Instant when it "fixes up" the
  // virtual URL of the active entry. Regular navigations are captured through
  // the notification system and shouldn't call this method.
  void NavigationEntryUpdated();

  // Invoked to update the instant support state.
  void InstantSupportChanged(bool supports_instant);

  // Returns true if the page supports instant. If the instant support state is
  // not determined or if the page does not support instant returns false.
  bool SupportsInstant() const;

  // Sends the current SearchProvider suggestion to the Instant page if any.
  void SetSuggestionToPrefetch(const InstantSuggestion& suggestion);

  // Tells the page that the user pressed Enter in the omnibox.
  void Submit(const base::string16& text);

  // Called when the tab corresponding to |this| instance is activated.
  void OnTabActivated();

  // Called when the tab corresponding to |this| instance is deactivated.
  void OnTabDeactivated();

  // Tells the page to toggle voice search.
  void ToggleVoiceSearch();

  // Returns true if the underlying page is a search results page.
  bool IsSearchResultsPage();

 private:
  friend class content::WebContentsUserData<SearchTabHelper>;
  friend class InstantPageTest;
  friend class SearchIPCRouterPolicyTest;
  friend class SearchIPCRouterTest;
  FRIEND_TEST_ALL_PREFIXES(SearchTabHelperTest,
                           DetermineIfPageSupportsInstant_Local);
  FRIEND_TEST_ALL_PREFIXES(SearchTabHelperTest,
                           DetermineIfPageSupportsInstant_NonLocal);
  FRIEND_TEST_ALL_PREFIXES(SearchTabHelperTest,
                           PageURLDoesntBelongToInstantRenderer);
  FRIEND_TEST_ALL_PREFIXES(SearchTabHelperTest,
                           OnChromeIdentityCheckMatch);
  FRIEND_TEST_ALL_PREFIXES(SearchTabHelperTest,
                           OnChromeIdentityCheckMismatch);
  FRIEND_TEST_ALL_PREFIXES(SearchTabHelperTest,
                           OnChromeIdentityCheckSignedOutMatch);
  FRIEND_TEST_ALL_PREFIXES(SearchTabHelperTest,
                           OnChromeIdentityCheckSignedOutMismatch);
  FRIEND_TEST_ALL_PREFIXES(SearchTabHelperWindowTest,
                           OnProvisionalLoadFailRedirectNTPToLocal);
  FRIEND_TEST_ALL_PREFIXES(SearchTabHelperWindowTest,
                           OnProvisionalLoadFailDontRedirectIfAborted);
  FRIEND_TEST_ALL_PREFIXES(SearchTabHelperWindowTest,
                           OnProvisionalLoadFailDontRedirectNonNTP);
  FRIEND_TEST_ALL_PREFIXES(SearchIPCRouterTest,
                           IgnoreMessageIfThePageIsNotActive);
  FRIEND_TEST_ALL_PREFIXES(SearchIPCRouterTest,
                           DoNotSendSetDisplayInstantResultsMsg);
  FRIEND_TEST_ALL_PREFIXES(SearchIPCRouterTest, HandleTabChangedEvents);
  FRIEND_TEST_ALL_PREFIXES(InstantPageTest,
                           DetermineIfPageSupportsInstant_Local);
  FRIEND_TEST_ALL_PREFIXES(InstantPageTest,
                           DetermineIfPageSupportsInstant_NonLocal);
  FRIEND_TEST_ALL_PREFIXES(InstantPageTest,
                           PageURLDoesntBelongToInstantRenderer);
  FRIEND_TEST_ALL_PREFIXES(InstantPageTest, PageSupportsInstant);

  explicit SearchTabHelper(content::WebContents* web_contents);

  // Overridden from contents::WebContentsObserver:
  virtual void RenderViewCreated(
      content::RenderViewHost* render_view_host) OVERRIDE;
  virtual void DidStartNavigationToPendingEntry(
      const GURL& url,
      content::NavigationController::ReloadType reload_type) OVERRIDE;
  virtual void DidNavigateMainFrame(
      const content::LoadCommittedDetails& details,
      const content::FrameNavigateParams& params) OVERRIDE;
  virtual void DidFailProvisionalLoad(
      int64 frame_id,
      const base::string16& frame_unique_name,
      bool is_main_frame,
      const GURL& validated_url,
      int error_code,
      const base::string16& error_description,
      content::RenderViewHost* render_view_host) OVERRIDE;
  virtual void DidFinishLoad(
      int64 frame_id,
      const GURL& validated_url,
      bool is_main_frame,
      content::RenderViewHost* render_view_host) OVERRIDE;
  virtual void NavigationEntryCommitted(
      const content::LoadCommittedDetails& load_details) OVERRIDE;

  // Overridden from SearchIPCRouter::Delegate:
  virtual void OnInstantSupportDetermined(bool supports_instant) OVERRIDE;
  virtual void OnSetVoiceSearchSupport(bool supports_voice_search) OVERRIDE;
  virtual void FocusOmnibox(OmniboxFocusState state) OVERRIDE;
  virtual void NavigateToURL(const GURL& url,
                             WindowOpenDisposition disposition,
                             bool is_most_visited_item_url) OVERRIDE;
  virtual void OnDeleteMostVisitedItem(const GURL& url) OVERRIDE;
  virtual void OnUndoMostVisitedDeletion(const GURL& url) OVERRIDE;
  virtual void OnUndoAllMostVisitedDeletions() OVERRIDE;
  virtual void OnLogEvent(NTPLoggingEventType event) OVERRIDE;
  virtual void OnLogImpression(int position,
                               const base::string16& provider) OVERRIDE;
  virtual void PasteIntoOmnibox(const base::string16& text) OVERRIDE;
  virtual void OnChromeIdentityCheck(const base::string16& identity) OVERRIDE;

  // Overridden from InstantServiceObserver:
  virtual void ThemeInfoChanged(const ThemeBackgroundInfo& theme_info) OVERRIDE;
  virtual void MostVisitedItemsChanged(
      const std::vector<InstantMostVisitedItem>& items) OVERRIDE;

  // Removes recommended URLs if a matching URL is already open in the Browser,
  // if the Most Visited Tile Placement experiment is enabled, and the client is
  // in the experiment group.
  void MaybeRemoveMostVisitedItems(std::vector<InstantMostVisitedItem>* items);

  // Sets the mode of the model based on the current URL of web_contents().
  // Only updates the origin part of the mode if |update_origin| is true,
  // otherwise keeps the current origin. If |is_preloaded_ntp| is true, the mode
  // is set to NTP regardless of the current URL; this is used to ensure that
  // InstantController can bind InstantTab to new tab pages immediately.
  void UpdateMode(bool update_origin, bool is_preloaded_ntp);

  // Tells the renderer to determine if the page supports the Instant API, which
  // results in a call to OnInstantSupportDetermined() when the reply is
  // received.
  void DetermineIfPageSupportsInstant();

  // Used by unit tests.
  SearchIPCRouter& ipc_router() { return ipc_router_; }

  Profile* profile() const;

  // Helper function to navigate the given contents to the local fallback
  // Instant URL and trim the history correctly.
  void RedirectToLocalNTP();

  const bool is_search_enabled_;

  // Tracks the last value passed to OmniboxEditModelChanged().
  bool user_input_in_progress_;

  // Model object for UI that cares about search state.
  SearchModel model_;

  content::WebContents* web_contents_;

  SearchIPCRouter ipc_router_;

  InstantService* instant_service_;

  DISALLOW_COPY_AND_ASSIGN(SearchTabHelper);
};

#endif  // CHROME_BROWSER_UI_SEARCH_SEARCH_TAB_HELPER_H_
