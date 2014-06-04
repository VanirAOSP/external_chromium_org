// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search/search_ipc_router.h"

#include <vector>

#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/tuple.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/search/search_ipc_router_policy_impl.h"
#include "chrome/browser/ui/search/search_tab_helper.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/instant_types.h"
#include "chrome/common/ntp_logging_events.h"
#include "chrome/common/omnibox_focus_state.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/mock_render_process_host.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_test_sink.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

namespace {

class MockSearchIPCRouterDelegate : public SearchIPCRouter::Delegate {
 public:
  virtual ~MockSearchIPCRouterDelegate() {}

  MOCK_METHOD1(OnInstantSupportDetermined, void(bool supports_instant));
  MOCK_METHOD1(OnSetVoiceSearchSupport, void(bool supports_voice_search));
  MOCK_METHOD1(FocusOmnibox, void(OmniboxFocusState state));
  MOCK_METHOD3(NavigateToURL, void(const GURL&, WindowOpenDisposition, bool));
  MOCK_METHOD1(OnDeleteMostVisitedItem, void(const GURL& url));
  MOCK_METHOD1(OnUndoMostVisitedDeletion, void(const GURL& url));
  MOCK_METHOD0(OnUndoAllMostVisitedDeletions, void());
  MOCK_METHOD1(OnLogEvent, void(NTPLoggingEventType event));
  MOCK_METHOD2(OnLogImpression, void(int position,
                                     const base::string16& provider));
  MOCK_METHOD1(PasteIntoOmnibox, void(const base::string16&));
  MOCK_METHOD1(OnChromeIdentityCheck, void(const base::string16& identity));
};

class MockSearchIPCRouterPolicy : public SearchIPCRouter::Policy {
 public:
  virtual ~MockSearchIPCRouterPolicy() {}

  MOCK_METHOD0(ShouldProcessSetVoiceSearchSupport, bool());
  MOCK_METHOD1(ShouldProcessFocusOmnibox, bool(bool));
  MOCK_METHOD1(ShouldProcessNavigateToURL, bool(bool));
  MOCK_METHOD0(ShouldProcessDeleteMostVisitedItem, bool());
  MOCK_METHOD0(ShouldProcessUndoMostVisitedDeletion, bool());
  MOCK_METHOD0(ShouldProcessUndoAllMostVisitedDeletions, bool());
  MOCK_METHOD0(ShouldProcessLogEvent, bool());
  MOCK_METHOD1(ShouldProcessPasteIntoOmnibox, bool(bool));
  MOCK_METHOD0(ShouldProcessChromeIdentityCheck, bool());
  MOCK_METHOD0(ShouldSendSetPromoInformation, bool());
  MOCK_METHOD0(ShouldSendSetDisplayInstantResults, bool());
  MOCK_METHOD0(ShouldSendSetSuggestionToPrefetch, bool());
  MOCK_METHOD0(ShouldSendMostVisitedItems, bool());
  MOCK_METHOD0(ShouldSendThemeBackgroundInfo, bool());
  MOCK_METHOD0(ShouldSendToggleVoiceSearch, bool());
  MOCK_METHOD0(ShouldSubmitQuery, bool());
};

}  // namespace

class SearchIPCRouterTest : public BrowserWithTestWindowTest {
 public:
  SearchIPCRouterTest() : field_trial_list_(NULL) {}

  virtual void SetUp() {
    BrowserWithTestWindowTest::SetUp();
    AddTab(browser(), GURL("chrome://blank"));
    SearchTabHelper::CreateForWebContents(web_contents());

    TemplateURLServiceFactory::GetInstance()->SetTestingFactoryAndUse(
        profile(),
        &TemplateURLServiceFactory::BuildInstanceFor);
    TemplateURLService* template_url_service =
        TemplateURLServiceFactory::GetForProfile(profile());
    ui_test_utils::WaitForTemplateURLServiceToLoad(template_url_service);

    TemplateURLData data;
    data.SetURL("http://foo.com/url?bar={searchTerms}");
    data.instant_url = "http://foo.com/instant?"
        "{google:omniboxStartMarginParameter}foo=foo#foo=foo&espv";
    data.new_tab_url = "https://foo.com/newtab?espv";
    data.alternate_urls.push_back("http://foo.com/alt#quux={searchTerms}");
    data.search_terms_replacement_key = "espv";

    TemplateURL* template_url = new TemplateURL(profile(), data);
    // Takes ownership of |template_url|.
    template_url_service->Add(template_url);
    template_url_service->SetDefaultSearchProvider(template_url);
    process()->sink().ClearMessages();
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  content::MockRenderProcessHost* process() {
    return static_cast<content::MockRenderProcessHost*>(
        web_contents()->GetRenderViewHost()->GetProcess());
  }

  SearchTabHelper* GetSearchTabHelper(
      content::WebContents* web_contents) {
    EXPECT_NE(static_cast<content::WebContents*>(NULL), web_contents);
    return SearchTabHelper::FromWebContents(web_contents);
  }

  void SetupMockDelegateAndPolicy() {
    content::WebContents* contents = web_contents();
    ASSERT_NE(static_cast<content::WebContents*>(NULL), contents);
    SearchTabHelper* search_tab_helper = GetSearchTabHelper(contents);
    ASSERT_NE(static_cast<SearchTabHelper*>(NULL), search_tab_helper);
    search_tab_helper->ipc_router().set_delegate(mock_delegate());
    search_tab_helper->ipc_router().set_policy(
        make_scoped_ptr(new MockSearchIPCRouterPolicy)
            .PassAs<SearchIPCRouter::Policy>());
  }

  bool MessageWasSent(uint32 id) {
    return process()->sink().GetFirstMessageMatching(id) != NULL;
  }

  void VerifyDisplayInstantResultsMsg(bool expected_param_value) {
    SetupMockDelegateAndPolicy();
    MockSearchIPCRouterPolicy* policy = GetSearchIPCRouterPolicy();
    EXPECT_CALL(*policy, ShouldSendSetDisplayInstantResults()).Times(1)
        .WillOnce(testing::Return(true));

    GetSearchIPCRouter().SetDisplayInstantResults();
    const IPC::Message* message = process()->sink().GetFirstMessageMatching(
        ChromeViewMsg_SearchBoxSetDisplayInstantResults::ID);
    EXPECT_NE(static_cast<const IPC::Message*>(NULL), message);
    Tuple1<bool> display_instant_results_param;
    ChromeViewMsg_SearchBoxSetDisplayInstantResults::Read(
        message, &display_instant_results_param);
    EXPECT_EQ(expected_param_value, display_instant_results_param.a);
  }

  MockSearchIPCRouterDelegate* mock_delegate() { return &delegate_; }

  MockSearchIPCRouterPolicy* GetSearchIPCRouterPolicy() {
    content::WebContents* contents = web_contents();
    EXPECT_NE(static_cast<content::WebContents*>(NULL), contents);
    SearchTabHelper* search_tab_helper = GetSearchTabHelper(contents);
    EXPECT_NE(static_cast<SearchTabHelper*>(NULL), search_tab_helper);
    return static_cast<MockSearchIPCRouterPolicy*>(
        search_tab_helper->ipc_router().policy());
  }

  SearchIPCRouter& GetSearchIPCRouter() {
    return GetSearchTabHelper(web_contents())->ipc_router();
  }

  void OnMessageReceived(const IPC::Message& message) {
    GetSearchIPCRouter().OnMessageReceived(message);
  }

  bool IsActiveTab(content::WebContents* contents) {
    return GetSearchTabHelper(contents)->ipc_router().is_active_tab_;
  }

 private:
  MockSearchIPCRouterDelegate delegate_;
  base::FieldTrialList field_trial_list_;
};

TEST_F(SearchIPCRouterTest, ProcessVoiceSearchSupportMsg) {
  NavigateAndCommitActiveTab(GURL("chrome-search://foo/bar"));
  SetupMockDelegateAndPolicy();
  MockSearchIPCRouterPolicy* policy = GetSearchIPCRouterPolicy();
  EXPECT_CALL(*mock_delegate(), OnSetVoiceSearchSupport(true)).Times(1);
  EXPECT_CALL(*(policy), ShouldProcessSetVoiceSearchSupport()).Times(1)
      .WillOnce(testing::Return(true));

  content::WebContents* contents = web_contents();
  scoped_ptr<IPC::Message> message(
      new ChromeViewHostMsg_SetVoiceSearchSupported(
          contents->GetRoutingID(),
          contents->GetController().GetVisibleEntry()->GetPageID(),
          true));
  OnMessageReceived(*message);
}

TEST_F(SearchIPCRouterTest, IgnoreVoiceSearchSupportMsg) {
  NavigateAndCommitActiveTab(GURL("chrome-search://foo/bar"));
  EXPECT_CALL(*mock_delegate(), OnSetVoiceSearchSupport(true)).Times(0);
  SetupMockDelegateAndPolicy();
  MockSearchIPCRouterPolicy* policy = GetSearchIPCRouterPolicy();
  EXPECT_CALL(*policy, ShouldProcessSetVoiceSearchSupport()).Times(1)
      .WillOnce(testing::Return(false));

  content::WebContents* contents = web_contents();
  scoped_ptr<IPC::Message> message(
      new ChromeViewHostMsg_SetVoiceSearchSupported(
          contents->GetRoutingID(),
          contents->GetController().GetVisibleEntry()->GetPageID(),
          true));
  OnMessageReceived(*message);
}

TEST_F(SearchIPCRouterTest, ProcessFocusOmniboxMsg) {
  NavigateAndCommitActiveTab(GURL(chrome::kChromeSearchLocalNtpUrl));
  SetupMockDelegateAndPolicy();
  MockSearchIPCRouterPolicy* policy = GetSearchIPCRouterPolicy();
  EXPECT_CALL(*mock_delegate(), FocusOmnibox(OMNIBOX_FOCUS_VISIBLE)).Times(1);

  content::WebContents* contents = web_contents();
  bool is_active_tab = IsActiveTab(contents);
  EXPECT_TRUE(is_active_tab);
  EXPECT_CALL(*policy, ShouldProcessFocusOmnibox(is_active_tab)).Times(1)
      .WillOnce(testing::Return(true));

  scoped_ptr<IPC::Message> message(new ChromeViewHostMsg_FocusOmnibox(
      contents->GetRoutingID(),
      contents->GetController().GetVisibleEntry()->GetPageID(),
      OMNIBOX_FOCUS_VISIBLE));
  OnMessageReceived(*message);
}

TEST_F(SearchIPCRouterTest, IgnoreFocusOmniboxMsg) {
  NavigateAndCommitActiveTab(GURL("chrome-search://foo/bar"));
  SetupMockDelegateAndPolicy();
  MockSearchIPCRouterPolicy* policy = GetSearchIPCRouterPolicy();
  EXPECT_CALL(*mock_delegate(), FocusOmnibox(OMNIBOX_FOCUS_VISIBLE)).Times(0);

  content::WebContents* contents = web_contents();
  bool is_active_tab = IsActiveTab(contents);
  EXPECT_TRUE(is_active_tab);
  EXPECT_CALL(*policy, ShouldProcessFocusOmnibox(is_active_tab)).Times(1)
      .WillOnce(testing::Return(false));

  scoped_ptr<IPC::Message> message(new ChromeViewHostMsg_FocusOmnibox(
      contents->GetRoutingID(),
      contents->GetController().GetVisibleEntry()->GetPageID(),
      OMNIBOX_FOCUS_VISIBLE));
  OnMessageReceived(*message);
}

TEST_F(SearchIPCRouterTest, HandleTabChangedEvents) {
  NavigateAndCommitActiveTab(GURL("chrome-search://foo/bar"));
  content::WebContents* contents = web_contents();
  EXPECT_EQ(0, browser()->tab_strip_model()->GetIndexOfWebContents(contents));
  EXPECT_TRUE(IsActiveTab(contents));

  // Add a new tab to deactivate the current tab.
  AddTab(browser(), GURL(content::kAboutBlankURL));
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  EXPECT_EQ(1, browser()->tab_strip_model()->GetIndexOfWebContents(contents));
  EXPECT_EQ(0, browser()->tab_strip_model()->active_index());
  EXPECT_FALSE(IsActiveTab(contents));

  // Activate the first tab.
  browser()->tab_strip_model()->ActivateTabAt(1, false);
  EXPECT_EQ(browser()->tab_strip_model()->active_index(),
            browser()->tab_strip_model()->GetIndexOfWebContents(contents));
  EXPECT_TRUE(IsActiveTab(contents));
}

TEST_F(SearchIPCRouterTest, ProcessNavigateToURLMsg) {
  NavigateAndCommitActiveTab(GURL("chrome-search://foo/bar"));
  SetupMockDelegateAndPolicy();
  GURL destination_url("www.foo.com");
  EXPECT_CALL(*mock_delegate(), NavigateToURL(destination_url, CURRENT_TAB,
                                              true)).Times(1);
  content::WebContents* contents = web_contents();
  bool is_active_tab = IsActiveTab(contents);
  EXPECT_TRUE(is_active_tab);

  MockSearchIPCRouterPolicy* policy = GetSearchIPCRouterPolicy();
  EXPECT_CALL(*policy, ShouldProcessNavigateToURL(is_active_tab)).Times(1)
      .WillOnce(testing::Return(true));

  scoped_ptr<IPC::Message> message(new ChromeViewHostMsg_SearchBoxNavigate(
      contents->GetRoutingID(),
      contents->GetController().GetVisibleEntry()->GetPageID(),
      destination_url, CURRENT_TAB, true));
  OnMessageReceived(*message);
}

TEST_F(SearchIPCRouterTest, IgnoreNavigateToURLMsg) {
  NavigateAndCommitActiveTab(GURL("chrome-search://foo/bar"));
  SetupMockDelegateAndPolicy();
  GURL destination_url("www.foo.com");
  EXPECT_CALL(*mock_delegate(), NavigateToURL(destination_url, CURRENT_TAB,
                                              true)).Times(0);
  content::WebContents* contents = web_contents();
  bool is_active_tab = IsActiveTab(contents);
  EXPECT_TRUE(is_active_tab);

  MockSearchIPCRouterPolicy* policy = GetSearchIPCRouterPolicy();
  EXPECT_CALL(*policy, ShouldProcessNavigateToURL(is_active_tab)).Times(1)
      .WillOnce(testing::Return(false));

  scoped_ptr<IPC::Message> message(new ChromeViewHostMsg_SearchBoxNavigate(
      contents->GetRoutingID(),
      contents->GetController().GetVisibleEntry()->GetPageID(),
      destination_url, CURRENT_TAB, true));
  OnMessageReceived(*message);
}

TEST_F(SearchIPCRouterTest, ProcessLogEventMsg) {
  NavigateAndCommitActiveTab(GURL(chrome::kChromeSearchLocalNtpUrl));
  SetupMockDelegateAndPolicy();
  MockSearchIPCRouterPolicy* policy = GetSearchIPCRouterPolicy();
  EXPECT_CALL(*mock_delegate(), OnLogEvent(NTP_MOUSEOVER)).Times(1);
  EXPECT_CALL(*policy, ShouldProcessLogEvent()).Times(1)
      .WillOnce(testing::Return(true));

  content::WebContents* contents = web_contents();
  scoped_ptr<IPC::Message> message(new ChromeViewHostMsg_LogEvent(
      contents->GetRoutingID(),
      contents->GetController().GetVisibleEntry()->GetPageID(),
      NTP_MOUSEOVER));
  OnMessageReceived(*message);
}

TEST_F(SearchIPCRouterTest, IgnoreLogEventMsg) {
  NavigateAndCommitActiveTab(GURL("chrome-search://foo/bar"));
  SetupMockDelegateAndPolicy();
  MockSearchIPCRouterPolicy* policy = GetSearchIPCRouterPolicy();
  EXPECT_CALL(*mock_delegate(), OnLogEvent(NTP_MOUSEOVER)).Times(0);
  EXPECT_CALL(*policy, ShouldProcessLogEvent()).Times(1)
      .WillOnce(testing::Return(false));

  content::WebContents* contents = web_contents();
  scoped_ptr<IPC::Message> message(new ChromeViewHostMsg_LogEvent(
      contents->GetRoutingID(),
      contents->GetController().GetVisibleEntry()->GetPageID(),
      NTP_MOUSEOVER));
  OnMessageReceived(*message);
}

TEST_F(SearchIPCRouterTest, ProcessLogImpressionMsg) {
  NavigateAndCommitActiveTab(GURL(chrome::kChromeSearchLocalNtpUrl));
  SetupMockDelegateAndPolicy();
  MockSearchIPCRouterPolicy* policy = GetSearchIPCRouterPolicy();
  EXPECT_CALL(*mock_delegate(),
              OnLogImpression(3, ASCIIToUTF16("Server"))).Times(1);
  EXPECT_CALL(*policy, ShouldProcessLogEvent()).Times(1)
      .WillOnce(testing::Return(true));

  content::WebContents* contents = web_contents();
  scoped_ptr<IPC::Message> message(new ChromeViewHostMsg_LogImpression(
      contents->GetRoutingID(),
      contents->GetController().GetVisibleEntry()->GetPageID(),
      3,
      ASCIIToUTF16("Server")));
  OnMessageReceived(*message);
}

TEST_F(SearchIPCRouterTest, ProcessChromeIdentityCheckMsg) {
  NavigateAndCommitActiveTab(GURL(chrome::kChromeSearchLocalNtpUrl));
  SetupMockDelegateAndPolicy();
  MockSearchIPCRouterPolicy* policy = GetSearchIPCRouterPolicy();
  const base::string16 test_identity = ASCIIToUTF16("foo@bar.com");
  EXPECT_CALL(*mock_delegate(), OnChromeIdentityCheck(test_identity)).Times(1);
  EXPECT_CALL(*policy, ShouldProcessChromeIdentityCheck()).Times(1)
      .WillOnce(testing::Return(true));

  content::WebContents* contents = web_contents();
  scoped_ptr<IPC::Message> message(new ChromeViewHostMsg_ChromeIdentityCheck(
      contents->GetRoutingID(),
      contents->GetController().GetVisibleEntry()->GetPageID(),
      test_identity));
  OnMessageReceived(*message);
}

TEST_F(SearchIPCRouterTest, IgnoreChromeIdentityCheckMsg) {
  NavigateAndCommitActiveTab(GURL("chrome-search://foo/bar"));
  SetupMockDelegateAndPolicy();
  MockSearchIPCRouterPolicy* policy = GetSearchIPCRouterPolicy();

  const base::string16 test_identity = ASCIIToUTF16("foo@bar.com");
  EXPECT_CALL(*mock_delegate(), OnChromeIdentityCheck(test_identity)).Times(0);
  EXPECT_CALL(*policy, ShouldProcessChromeIdentityCheck()).Times(1)
      .WillOnce(testing::Return(false));

  content::WebContents* contents = web_contents();
  scoped_ptr<IPC::Message> message(new ChromeViewHostMsg_ChromeIdentityCheck(
      contents->GetRoutingID(),
      contents->GetController().GetVisibleEntry()->GetPageID(),
      test_identity));
  OnMessageReceived(*message);
}

TEST_F(SearchIPCRouterTest, ProcessDeleteMostVisitedItemMsg) {
  NavigateAndCommitActiveTab(GURL("chrome-search://foo/bar"));
  SetupMockDelegateAndPolicy();
  MockSearchIPCRouterPolicy* policy = GetSearchIPCRouterPolicy();
  GURL item_url("www.foo.com");
  EXPECT_CALL(*mock_delegate(), OnDeleteMostVisitedItem(item_url)).Times(1);
  EXPECT_CALL(*policy, ShouldProcessDeleteMostVisitedItem()).Times(1)
      .WillOnce(testing::Return(true));

  content::WebContents* contents = web_contents();
  scoped_ptr<IPC::Message> message(
      new ChromeViewHostMsg_SearchBoxDeleteMostVisitedItem(
          contents->GetRoutingID(),
          contents->GetController().GetVisibleEntry()->GetPageID(),
          item_url));
  OnMessageReceived(*message);
}

TEST_F(SearchIPCRouterTest, IgnoreDeleteMostVisitedItemMsg) {
  NavigateAndCommitActiveTab(GURL("chrome-search://foo/bar"));
  SetupMockDelegateAndPolicy();
  MockSearchIPCRouterPolicy* policy = GetSearchIPCRouterPolicy();
  GURL item_url("www.foo.com");
  EXPECT_CALL(*mock_delegate(), OnDeleteMostVisitedItem(item_url)).Times(0);
  EXPECT_CALL(*policy, ShouldProcessDeleteMostVisitedItem()).Times(1)
      .WillOnce(testing::Return(false));

  content::WebContents* contents = web_contents();
  scoped_ptr<IPC::Message> message(
      new ChromeViewHostMsg_SearchBoxDeleteMostVisitedItem(
          contents->GetRoutingID(),
          contents->GetController().GetVisibleEntry()->GetPageID(),
          item_url));
  OnMessageReceived(*message);
}

TEST_F(SearchIPCRouterTest, ProcessUndoMostVisitedDeletionMsg) {
  NavigateAndCommitActiveTab(GURL("chrome-search://foo/bar"));
  SetupMockDelegateAndPolicy();
  MockSearchIPCRouterPolicy* policy = GetSearchIPCRouterPolicy();
  GURL item_url("www.foo.com");
  EXPECT_CALL(*mock_delegate(), OnUndoMostVisitedDeletion(item_url)).Times(1);
  EXPECT_CALL(*policy, ShouldProcessUndoMostVisitedDeletion()).Times(1)
      .WillOnce(testing::Return(true));

  content::WebContents* contents = web_contents();
  scoped_ptr<IPC::Message> message(
      new ChromeViewHostMsg_SearchBoxUndoMostVisitedDeletion(
          contents->GetRoutingID(),
          contents->GetController().GetVisibleEntry()->GetPageID(),
          item_url));
  OnMessageReceived(*message);
}

TEST_F(SearchIPCRouterTest, IgnoreUndoMostVisitedDeletionMsg) {
  NavigateAndCommitActiveTab(GURL("chrome-search://foo/bar"));
  SetupMockDelegateAndPolicy();
  MockSearchIPCRouterPolicy* policy = GetSearchIPCRouterPolicy();
  GURL item_url("www.foo.com");
  EXPECT_CALL(*mock_delegate(), OnUndoMostVisitedDeletion(item_url)).Times(0);
  EXPECT_CALL(*policy, ShouldProcessUndoMostVisitedDeletion()).Times(1)
      .WillOnce(testing::Return(false));

  content::WebContents* contents = web_contents();
  scoped_ptr<IPC::Message> message(
      new ChromeViewHostMsg_SearchBoxUndoMostVisitedDeletion(
          contents->GetRoutingID(),
          contents->GetController().GetVisibleEntry()->GetPageID(),
          item_url));
  OnMessageReceived(*message);
}

TEST_F(SearchIPCRouterTest, ProcessUndoAllMostVisitedDeletionsMsg) {
  NavigateAndCommitActiveTab(GURL("chrome-search://foo/bar"));
  SetupMockDelegateAndPolicy();
  MockSearchIPCRouterPolicy* policy = GetSearchIPCRouterPolicy();
  EXPECT_CALL(*mock_delegate(), OnUndoAllMostVisitedDeletions()).Times(1);
  EXPECT_CALL(*policy, ShouldProcessUndoAllMostVisitedDeletions()).Times(1)
      .WillOnce(testing::Return(true));

  content::WebContents* contents = web_contents();
  scoped_ptr<IPC::Message> message(
      new ChromeViewHostMsg_SearchBoxUndoAllMostVisitedDeletions(
          contents->GetRoutingID(),
          contents->GetController().GetVisibleEntry()->GetPageID()));
  OnMessageReceived(*message);
}

TEST_F(SearchIPCRouterTest, IgnoreUndoAllMostVisitedDeletionsMsg) {
  NavigateAndCommitActiveTab(GURL("chrome-search://foo/bar"));
  SetupMockDelegateAndPolicy();
  MockSearchIPCRouterPolicy* policy = GetSearchIPCRouterPolicy();
  EXPECT_CALL(*mock_delegate(), OnUndoAllMostVisitedDeletions()).Times(0);
  EXPECT_CALL(*policy, ShouldProcessUndoAllMostVisitedDeletions()).Times(1)
      .WillOnce(testing::Return(false));

  content::WebContents* contents = web_contents();
  scoped_ptr<IPC::Message> message(
      new ChromeViewHostMsg_SearchBoxUndoAllMostVisitedDeletions(
          contents->GetRoutingID(),
          contents->GetController().GetVisibleEntry()->GetPageID()));
  OnMessageReceived(*message);
}

TEST_F(SearchIPCRouterTest, IgnoreMessageIfThePageIsNotActive) {
  NavigateAndCommitActiveTab(GURL(chrome::kChromeSearchLocalNtpUrl));
  SetupMockDelegateAndPolicy();
  MockSearchIPCRouterPolicy* policy = GetSearchIPCRouterPolicy();

  content::WebContents* contents = web_contents();
  bool is_active_tab = IsActiveTab(contents);
  int invalid_page_id = 1000;
  GURL item_url("www.foo.com");
  EXPECT_CALL(*mock_delegate(), NavigateToURL(item_url, CURRENT_TAB,
                                              true)).Times(0);
  EXPECT_CALL(*policy, ShouldProcessNavigateToURL(is_active_tab)).Times(0);

  scoped_ptr<IPC::Message> message(new ChromeViewHostMsg_SearchBoxNavigate(
      contents->GetRoutingID(), invalid_page_id, item_url, CURRENT_TAB,
      true));
  OnMessageReceived(*message);

  EXPECT_CALL(*mock_delegate(), OnDeleteMostVisitedItem(item_url)).Times(0);
  EXPECT_CALL(*policy, ShouldProcessDeleteMostVisitedItem()).Times(0);
  message.reset(new ChromeViewHostMsg_SearchBoxDeleteMostVisitedItem(
      contents->GetRoutingID(), invalid_page_id, item_url));
  OnMessageReceived(*message);

  EXPECT_CALL(*mock_delegate(), OnUndoMostVisitedDeletion(item_url)).Times(0);
  EXPECT_CALL(*policy, ShouldProcessUndoMostVisitedDeletion()).Times(0);
  message.reset(new ChromeViewHostMsg_SearchBoxUndoMostVisitedDeletion(
      contents->GetRoutingID(), invalid_page_id, item_url));
  OnMessageReceived(*message);

  EXPECT_CALL(*mock_delegate(), OnUndoAllMostVisitedDeletions()).Times(0);
  EXPECT_CALL(*policy, ShouldProcessUndoAllMostVisitedDeletions()).Times(0);
  message.reset(new ChromeViewHostMsg_SearchBoxUndoAllMostVisitedDeletions(
      contents->GetRoutingID(), invalid_page_id));
  OnMessageReceived(*message);

  EXPECT_CALL(*mock_delegate(), FocusOmnibox(OMNIBOX_FOCUS_VISIBLE)).Times(0);
  EXPECT_CALL(*policy, ShouldProcessFocusOmnibox(is_active_tab)).Times(0);
  message.reset(new ChromeViewHostMsg_FocusOmnibox(
      contents->GetRoutingID(), invalid_page_id, OMNIBOX_FOCUS_VISIBLE));
  OnMessageReceived(*message);

  EXPECT_CALL(*mock_delegate(), OnLogEvent(NTP_MOUSEOVER)).Times(0);
  EXPECT_CALL(*policy, ShouldProcessLogEvent()).Times(0);
  message.reset(new ChromeViewHostMsg_LogEvent(contents->GetRoutingID(),
                                               invalid_page_id, NTP_MOUSEOVER));
  OnMessageReceived(*message);

  base::string16 text;
  EXPECT_CALL(*mock_delegate(), PasteIntoOmnibox(text)).Times(0);
  EXPECT_CALL(*policy, ShouldProcessPasteIntoOmnibox(is_active_tab)).Times(0);
  message.reset(new ChromeViewHostMsg_PasteAndOpenDropdown(
      contents->GetRoutingID(), invalid_page_id, text));
  OnMessageReceived(*message);
}

TEST_F(SearchIPCRouterTest, ProcessPasteAndOpenDropdownMsg) {
  NavigateAndCommitActiveTab(GURL(chrome::kChromeSearchLocalNtpUrl));
  SetupMockDelegateAndPolicy();
  MockSearchIPCRouterPolicy* policy = GetSearchIPCRouterPolicy();

  content::WebContents* contents = web_contents();
  bool is_active_tab = IsActiveTab(contents);
  EXPECT_TRUE(is_active_tab);

  base::string16 text;
  EXPECT_CALL(*mock_delegate(), PasteIntoOmnibox(text)).Times(1);
  EXPECT_CALL(*policy, ShouldProcessPasteIntoOmnibox(is_active_tab)).Times(1)
      .WillOnce(testing::Return(true));
  scoped_ptr<IPC::Message> message(new ChromeViewHostMsg_PasteAndOpenDropdown(
      contents->GetRoutingID(),
      contents->GetController().GetVisibleEntry()->GetPageID(), text));
  OnMessageReceived(*message);
}

TEST_F(SearchIPCRouterTest, IgnorePasteAndOpenDropdownMsg) {
  NavigateAndCommitActiveTab(GURL("chrome-search://foo/bar"));
  SetupMockDelegateAndPolicy();
  base::string16 text;
  EXPECT_CALL(*mock_delegate(), PasteIntoOmnibox(text)).Times(0);

  content::WebContents* contents = web_contents();
  bool is_active_tab = IsActiveTab(contents);
  EXPECT_TRUE(is_active_tab);

  MockSearchIPCRouterPolicy* policy = GetSearchIPCRouterPolicy();
  EXPECT_CALL(*policy, ShouldProcessPasteIntoOmnibox(is_active_tab)).Times(1)
      .WillOnce(testing::Return(false));

  scoped_ptr<IPC::Message> message(new ChromeViewHostMsg_PasteAndOpenDropdown(
      contents->GetRoutingID(),
      contents->GetController().GetVisibleEntry()->GetPageID(), text));
  OnMessageReceived(*message);
}

TEST_F(SearchIPCRouterTest, SendSetPromoInformationMsg) {
  NavigateAndCommitActiveTab(GURL("chrome-search://foo/bar"));
  SetupMockDelegateAndPolicy();
  MockSearchIPCRouterPolicy* policy = GetSearchIPCRouterPolicy();
  EXPECT_CALL(*policy, ShouldSendSetPromoInformation()).Times(1)
      .WillOnce(testing::Return(true));

  GetSearchIPCRouter().SetPromoInformation(true);
  EXPECT_TRUE(MessageWasSent(ChromeViewMsg_SearchBoxPromoInformation::ID));
}

TEST_F(SearchIPCRouterTest, DoNotSendSetPromoInformationMsg) {
  NavigateAndCommitActiveTab(GURL("chrome-search://foo/bar"));
  SetupMockDelegateAndPolicy();
  MockSearchIPCRouterPolicy* policy = GetSearchIPCRouterPolicy();
  EXPECT_CALL(*policy, ShouldSendSetPromoInformation()).Times(1)
      .WillOnce(testing::Return(false));

  GetSearchIPCRouter().SetPromoInformation(false);
  EXPECT_FALSE(MessageWasSent(ChromeViewMsg_SearchBoxPromoInformation::ID));
}

TEST_F(SearchIPCRouterTest,
       SendSetDisplayInstantResultsMsg_EnableInstantOnResultsPage) {
  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
      "EmbeddedSearch",
      "Group1 espv:42 query_extraction:1 prefetch_results_srp:1"));
  NavigateAndCommitActiveTab(GURL("https://foo.com/url?espv&bar=abc"));

  // Make sure ChromeViewMsg_SearchBoxSetDisplayInstantResults message param is
  // set to true if the underlying page is a results page and
  // "prefetch_results_srp" flag is enabled via field trials.
  VerifyDisplayInstantResultsMsg(true);
}

TEST_F(SearchIPCRouterTest,
       SendSetDisplayInstantResultsMsg_DisableInstantOnResultsPage) {
  // |prefetch_results_srp" flag is disabled via field trials.
  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
      "EmbeddedSearch", "Group1 espv:42 prefetch_results_srp:0"));
  NavigateAndCommitActiveTab(GURL("https://foo.com/url?espv&bar=abc"));

  // Make sure ChromeViewMsg_SearchBoxSetDisplayInstantResults message param is
  // set to false.
  VerifyDisplayInstantResultsMsg(false);
}

TEST_F(SearchIPCRouterTest,
       SendSetDisplayInstantResultsMsg_DisableInstantOutsideResultsPage) {
  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
      "EmbeddedSearch", "Group1 espv:42 prefetch_results_srp:1"));
  NavigateAndCommitActiveTab(GURL(chrome::kChromeSearchLocalNtpUrl));

  // Make sure ChromeViewMsg_SearchBoxSetDisplayInstantResults param is set to
  // false if the underlying page is not a search results page.
  VerifyDisplayInstantResultsMsg(false);
}

TEST_F(SearchIPCRouterTest,
       SendSetDisplayInstantResultsMsg_InstantSearchEnabled) {
  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
      "EmbeddedSearch",
      "Group1 espv:42 prefetch_results:1 use_cacheable_ntp:1"));
  NavigateAndCommitActiveTab(GURL(chrome::kChromeSearchLocalNtpUrl));

  // If the "prefetch_results" flag is enabled via field trials, then
  // ChromeViewMsg_SearchBoxSetDisplayInstantResults message param is set to
  // true irrespective of the underlying page.
  VerifyDisplayInstantResultsMsg(true);
}

TEST_F(SearchIPCRouterTest,
       SendSetDisplayInstantResultsMsg_InstantSearchDisabled) {
  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
      "EmbeddedSearch",
      "Group1 espv:42 use_cacheable_ntp:1 prefetch_results:0"));
  NavigateAndCommitActiveTab(GURL(chrome::kChromeSearchLocalNtpUrl));

  // Make sure ChromeViewMsg_SearchBoxSetDisplayInstantResults param is set to
  // false if the "prefetch_results" flag is disabled via field trials.
  VerifyDisplayInstantResultsMsg(false);
}

TEST_F(SearchIPCRouterTest, DoNotSendSetDisplayInstantResultsMsg) {
  NavigateAndCommitActiveTab(GURL("chrome-search://foo/bar"));
  SetupMockDelegateAndPolicy();
  MockSearchIPCRouterPolicy* policy = GetSearchIPCRouterPolicy();
  EXPECT_CALL(*policy, ShouldSendSetDisplayInstantResults()).Times(1)
      .WillOnce(testing::Return(false));

  process()->sink().ClearMessages();
  GetSearchIPCRouter().SetDisplayInstantResults();
  EXPECT_FALSE(MessageWasSent(
      ChromeViewMsg_SearchBoxSetDisplayInstantResults::ID));
}

TEST_F(SearchIPCRouterTest, SendSetSuggestionToPrefetch) {
  NavigateAndCommitActiveTab(GURL("chrome-search://foo/bar"));
  SetupMockDelegateAndPolicy();
  MockSearchIPCRouterPolicy* policy = GetSearchIPCRouterPolicy();
  EXPECT_CALL(*policy, ShouldSendSetSuggestionToPrefetch()).Times(1)
      .WillOnce(testing::Return(true));

  process()->sink().ClearMessages();
  content::WebContents* contents = web_contents();
  GetSearchTabHelper(contents)->SetSuggestionToPrefetch(InstantSuggestion());
  EXPECT_TRUE(MessageWasSent(
      ChromeViewMsg_SearchBoxSetSuggestionToPrefetch::ID));
}

TEST_F(SearchIPCRouterTest, DoNotSendSetSuggestionToPrefetch) {
  NavigateAndCommitActiveTab(GURL("chrome-search://foo/bar"));
  SetupMockDelegateAndPolicy();
  MockSearchIPCRouterPolicy* policy = GetSearchIPCRouterPolicy();
  EXPECT_CALL(*policy, ShouldSendSetSuggestionToPrefetch()).Times(1)
      .WillOnce(testing::Return(false));

  process()->sink().ClearMessages();
  content::WebContents* contents = web_contents();
  GetSearchTabHelper(contents)->SetSuggestionToPrefetch(InstantSuggestion());
  EXPECT_FALSE(MessageWasSent(
      ChromeViewMsg_SearchBoxSetSuggestionToPrefetch::ID));
}

TEST_F(SearchIPCRouterTest, SendMostVisitedItemsMsg) {
  NavigateAndCommitActiveTab(GURL(chrome::kChromeSearchLocalNtpUrl));
  SetupMockDelegateAndPolicy();
  MockSearchIPCRouterPolicy* policy = GetSearchIPCRouterPolicy();
  EXPECT_CALL(*policy, ShouldSendMostVisitedItems()).Times(1)
      .WillOnce(testing::Return(true));

  process()->sink().ClearMessages();
  GetSearchIPCRouter().SendMostVisitedItems(
      std::vector<InstantMostVisitedItem>());
  EXPECT_TRUE(MessageWasSent(
      ChromeViewMsg_SearchBoxMostVisitedItemsChanged::ID));
}

TEST_F(SearchIPCRouterTest, DoNotSendMostVisitedItemsMsg) {
  NavigateAndCommitActiveTab(GURL(chrome::kChromeSearchLocalNtpUrl));
  SetupMockDelegateAndPolicy();
  MockSearchIPCRouterPolicy* policy = GetSearchIPCRouterPolicy();
  EXPECT_CALL(*policy, ShouldSendMostVisitedItems()).Times(1)
      .WillOnce(testing::Return(false));

  process()->sink().ClearMessages();
  GetSearchIPCRouter().SendMostVisitedItems(
      std::vector<InstantMostVisitedItem>());
  EXPECT_FALSE(MessageWasSent(
      ChromeViewMsg_SearchBoxMostVisitedItemsChanged::ID));
}

TEST_F(SearchIPCRouterTest, SendThemeBackgroundInfoMsg) {
  NavigateAndCommitActiveTab(GURL(chrome::kChromeSearchLocalNtpUrl));
  SetupMockDelegateAndPolicy();
  MockSearchIPCRouterPolicy* policy = GetSearchIPCRouterPolicy();
  EXPECT_CALL(*policy, ShouldSendThemeBackgroundInfo()).Times(1)
      .WillOnce(testing::Return(true));

  process()->sink().ClearMessages();
  GetSearchIPCRouter().SendThemeBackgroundInfo(ThemeBackgroundInfo());
  EXPECT_TRUE(MessageWasSent(ChromeViewMsg_SearchBoxThemeChanged::ID));
}

TEST_F(SearchIPCRouterTest, DoNotSendThemeBackgroundInfoMsg) {
  NavigateAndCommitActiveTab(GURL(chrome::kChromeSearchLocalNtpUrl));
  SetupMockDelegateAndPolicy();
  MockSearchIPCRouterPolicy* policy = GetSearchIPCRouterPolicy();
  EXPECT_CALL(*policy, ShouldSendThemeBackgroundInfo()).Times(1)
      .WillOnce(testing::Return(false));

  process()->sink().ClearMessages();
  GetSearchIPCRouter().SendThemeBackgroundInfo(ThemeBackgroundInfo());
  EXPECT_FALSE(MessageWasSent(ChromeViewMsg_SearchBoxThemeChanged::ID));
}

TEST_F(SearchIPCRouterTest, SendSubmitMsg) {
  NavigateAndCommitActiveTab(GURL("chrome-search://foo/bar"));
  SetupMockDelegateAndPolicy();
  MockSearchIPCRouterPolicy* policy = GetSearchIPCRouterPolicy();
  EXPECT_CALL(*policy, ShouldSubmitQuery()).Times(1)
      .WillOnce(testing::Return(true));

  process()->sink().ClearMessages();
  GetSearchIPCRouter().Submit(base::string16());
  EXPECT_TRUE(MessageWasSent(ChromeViewMsg_SearchBoxSubmit::ID));
}

TEST_F(SearchIPCRouterTest, DoNotSendSubmitMsg) {
  NavigateAndCommitActiveTab(GURL(chrome::kChromeSearchLocalNtpUrl));
  SetupMockDelegateAndPolicy();
  MockSearchIPCRouterPolicy* policy = GetSearchIPCRouterPolicy();
  EXPECT_CALL(*policy, ShouldSubmitQuery()).Times(1)
      .WillOnce(testing::Return(false));

  process()->sink().ClearMessages();
  GetSearchIPCRouter().Submit(base::string16());
  EXPECT_FALSE(MessageWasSent(ChromeViewMsg_SearchBoxSubmit::ID));
}

TEST_F(SearchIPCRouterTest, SendToggleVoiceSearch) {
  NavigateAndCommitActiveTab(GURL("chrome-search://foo/bar"));
  SetupMockDelegateAndPolicy();
  MockSearchIPCRouterPolicy* policy = GetSearchIPCRouterPolicy();
  EXPECT_CALL(*policy, ShouldSendToggleVoiceSearch()).Times(1)
      .WillOnce(testing::Return(true));

  process()->sink().ClearMessages();
  GetSearchIPCRouter().ToggleVoiceSearch();
  EXPECT_TRUE(MessageWasSent(ChromeViewMsg_SearchBoxToggleVoiceSearch::ID));
}

TEST_F(SearchIPCRouterTest, DoNotSendToggleVoiceSearch) {
  NavigateAndCommitActiveTab(GURL("chrome-search://foo/bar"));
  SetupMockDelegateAndPolicy();
  MockSearchIPCRouterPolicy* policy = GetSearchIPCRouterPolicy();
  EXPECT_CALL(*policy, ShouldSendToggleVoiceSearch()).Times(1)
      .WillOnce(testing::Return(false));

  process()->sink().ClearMessages();
  GetSearchIPCRouter().ToggleVoiceSearch();
  EXPECT_FALSE(MessageWasSent(ChromeViewMsg_SearchBoxToggleVoiceSearch::ID));
}
