// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_samples.h"
#include "base/metrics/statistics_recorder.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/managed_mode/managed_mode_url_filter.h"
#include "chrome/browser/managed_mode/managed_user_service.h"
#include "chrome/browser/managed_mode/managed_user_service_factory.h"
#include "chrome/browser/search/instant_service.h"
#include "chrome/browser/search/instant_service_factory.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/search_engines/search_terms_data.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/variations/entropy_provider.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/renderer_preferences.h"
#include "url/gurl.h"

namespace chrome {

class EmbeddedSearchFieldTrialTest : public testing::Test {
 protected:
  virtual void SetUp() {
    field_trial_list_.reset(new base::FieldTrialList(
        new metrics::SHA1EntropyProvider("42")));
    base::StatisticsRecorder::Initialize();
  }

 private:
  scoped_ptr<base::FieldTrialList> field_trial_list_;
};

TEST_F(EmbeddedSearchFieldTrialTest, GetFieldTrialInfoEmptyAndValid) {
  FieldTrialFlags flags;

  EXPECT_TRUE(GetFieldTrialInfo(&flags));
  EXPECT_EQ(0ul, flags.size());

  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial("EmbeddedSearch",
                                                     "Group77"));
  EXPECT_TRUE(GetFieldTrialInfo(&flags));
  EXPECT_EQ(0ul, flags.size());
}

TEST_F(EmbeddedSearchFieldTrialTest, GetFieldTrialInfoInvalidNumber) {
  FieldTrialFlags flags;

  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial("EmbeddedSearch",
                                                     "Group77.2"));
  EXPECT_TRUE(GetFieldTrialInfo(&flags));
  EXPECT_EQ(0ul, flags.size());
}

TEST_F(EmbeddedSearchFieldTrialTest, GetFieldTrialInfoInvalidName) {
  FieldTrialFlags flags;

  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial("EmbeddedSearch",
                                                     "Invalid77"));
  EXPECT_TRUE(GetFieldTrialInfo(&flags));
  EXPECT_EQ(0ul, flags.size());
}

TEST_F(EmbeddedSearchFieldTrialTest, GetFieldTrialInfoValidGroup) {
  FieldTrialFlags flags;

  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial("EmbeddedSearch",
                                                     "Group77"));
  EXPECT_TRUE(GetFieldTrialInfo(&flags));
  EXPECT_EQ(0ul, flags.size());
}

TEST_F(EmbeddedSearchFieldTrialTest, GetFieldTrialInfoValidFlag) {
  FieldTrialFlags flags;

  EXPECT_EQ(9999ul, GetUInt64ValueForFlagWithDefault("foo", 9999, flags));
  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial("EmbeddedSearch",
                                                     "Group77 foo:6"));
  EXPECT_TRUE(GetFieldTrialInfo(&flags));
  EXPECT_EQ(1ul, flags.size());
  EXPECT_EQ(6ul, GetUInt64ValueForFlagWithDefault("foo", 9999, flags));
}

TEST_F(EmbeddedSearchFieldTrialTest, GetFieldTrialInfoNewName) {
  FieldTrialFlags flags;

  EXPECT_EQ(9999ul, GetUInt64ValueForFlagWithDefault("foo", 9999, flags));
  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial("EmbeddedSearch",
                                                     "Group77 foo:6"));
  EXPECT_TRUE(GetFieldTrialInfo(&flags));
  EXPECT_EQ(1ul, flags.size());
  EXPECT_EQ(6ul, GetUInt64ValueForFlagWithDefault("foo", 9999, flags));
}

TEST_F(EmbeddedSearchFieldTrialTest, GetFieldTrialInfoNewNameOverridesOld) {
  FieldTrialFlags flags;

  EXPECT_EQ(9999ul, GetUInt64ValueForFlagWithDefault("foo", 9999, flags));
  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial("EmbeddedSearch",
                                                     "Group77 foo:6"));
  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial("InstantExtended",
                                                     "Group78 foo:5"));
  EXPECT_TRUE(GetFieldTrialInfo(&flags));
  EXPECT_EQ(1ul, flags.size());
  EXPECT_EQ(6ul, GetUInt64ValueForFlagWithDefault("foo", 9999, flags));
}

TEST_F(EmbeddedSearchFieldTrialTest, GetFieldTrialInfoLotsOfFlags) {
  FieldTrialFlags flags;

  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
      "EmbeddedSearch", "Group77 bar:1 baz:7 cat:dogs"));
  EXPECT_TRUE(GetFieldTrialInfo(&flags));
  EXPECT_EQ(3ul, flags.size());
  EXPECT_EQ(true, GetBoolValueForFlagWithDefault("bar", false, flags));
  EXPECT_EQ(7ul, GetUInt64ValueForFlagWithDefault("baz", 0, flags));
  EXPECT_EQ("dogs",
            GetStringValueForFlagWithDefault("cat", std::string(), flags));
  EXPECT_EQ("default",
            GetStringValueForFlagWithDefault("moose", "default", flags));
}

TEST_F(EmbeddedSearchFieldTrialTest, GetFieldTrialInfoDisabled) {
  FieldTrialFlags flags;

  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
      "EmbeddedSearch", "Group77 bar:1 baz:7 cat:dogs DISABLED"));
  EXPECT_FALSE(GetFieldTrialInfo(&flags));
  EXPECT_EQ(0ul, flags.size());
}

TEST_F(EmbeddedSearchFieldTrialTest, GetFieldTrialInfoControlFlags) {
  FieldTrialFlags flags;

  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
      "EmbeddedSearch", "Control77 bar:1 baz:7 cat:dogs"));
  EXPECT_TRUE(GetFieldTrialInfo(&flags));
  EXPECT_EQ(3ul, flags.size());
}

class SearchTest : public BrowserWithTestWindowTest {
 protected:
  virtual void SetUp() OVERRIDE {
    BrowserWithTestWindowTest::SetUp();
    field_trial_list_.reset(new base::FieldTrialList(
        new metrics::SHA1EntropyProvider("42")));
    TemplateURLServiceFactory::GetInstance()->SetTestingFactoryAndUse(
        profile(), &TemplateURLServiceFactory::BuildInstanceFor);
    TemplateURLService* template_url_service =
        TemplateURLServiceFactory::GetForProfile(profile());
    ui_test_utils::WaitForTemplateURLServiceToLoad(template_url_service);
    SetSearchProvider(true, false);
  }

  void SetSearchProvider(bool set_ntp_url, bool insecure_ntp_url) {
    TemplateURLService* template_url_service =
        TemplateURLServiceFactory::GetForProfile(profile());
    TemplateURLData data;
    data.SetURL("http://foo.com/url?bar={searchTerms}");
    data.instant_url = "http://foo.com/instant?"
        "{google:omniboxStartMarginParameter}{google:forceInstantResults}"
        "foo=foo#foo=foo&strk";
    if (set_ntp_url) {
      data.new_tab_url = (insecure_ntp_url ? "http" : "https") +
          std::string("://foo.com/newtab?strk");
    }
    data.alternate_urls.push_back("http://foo.com/alt#quux={searchTerms}");
    data.search_terms_replacement_key = "strk";

    TemplateURL* template_url = new TemplateURL(profile(), data);
    // Takes ownership of |template_url|.
    template_url_service->Add(template_url);
    template_url_service->SetDefaultSearchProvider(template_url);
  }

  // Build an Instant URL with or without a valid search terms replacement key
  // as per |has_search_term_replacement_key|. Set that URL as the instant URL
  // for the default search provider.
  void SetDefaultInstantTemplateUrl(bool has_search_term_replacement_key) {
    TemplateURLService* template_url_service =
        TemplateURLServiceFactory::GetForProfile(profile());

    static const char kInstantURLWithStrk[] =
        "http://foo.com/instant?foo=foo#foo=foo&strk";
    static const char kInstantURLNoStrk[] =
        "http://foo.com/instant?foo=foo#foo=foo";

    TemplateURLData data;
    data.SetURL("http://foo.com/url?bar={searchTerms}");
    data.instant_url = (has_search_term_replacement_key ?
        kInstantURLWithStrk : kInstantURLNoStrk);
    data.search_terms_replacement_key = "strk";

    TemplateURL* template_url = new TemplateURL(profile(), data);
    // Takes ownership of |template_url|.
    template_url_service->Add(template_url);
    template_url_service->SetDefaultSearchProvider(template_url);
  }

  bool InInstantProcess(const content::WebContents* contents) {
    InstantService* instant_service =
        InstantServiceFactory::GetForProfile(profile());
    return instant_service->IsInstantProcess(
        contents->GetRenderProcessHost()->GetID());
  }

  scoped_ptr<base::FieldTrialList> field_trial_list_;
};

struct SearchTestCase {
  const char* url;
  bool expected_result;
  const char* comment;
};

TEST_F(SearchTest, ShouldAssignURLToInstantRendererExtendedEnabled) {
  EnableQueryExtractionForTesting();

  const SearchTestCase kTestCases[] = {
    {chrome::kChromeSearchLocalNtpUrl, true,  ""},
    {"https://foo.com/instant?strk",   true,  ""},
    {"https://foo.com/instant#strk",   true,  ""},
    {"https://foo.com/instant?strk=0", true,  ""},
    {"https://foo.com/url?strk",       true,  ""},
    {"https://foo.com/alt?strk",       true,  ""},
    {"http://foo.com/instant",         false, "Non-HTTPS"},
    {"http://foo.com/instant?strk",    false, "Non-HTTPS"},
    {"http://foo.com/instant?strk=1",  false, "Non-HTTPS"},
    {"https://foo.com/instant",        false, "No search terms replacement"},
    {"https://foo.com/?strk",          false, "Non-exact path"},
  };

  for (size_t i = 0; i < arraysize(kTestCases); ++i) {
    const SearchTestCase& test = kTestCases[i];
    EXPECT_EQ(test.expected_result,
              ShouldAssignURLToInstantRenderer(GURL(test.url), profile()))
        << test.url << " " << test.comment;
  }
}

TEST_F(SearchTest, ShouldAssignURLToInstantRendererExtendedEnabledNotOnSRP) {
  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
      "EmbeddedSearch", "Group1 espv:2 suppress_on_srp:1"));

  const SearchTestCase kTestCases[] = {
    {chrome::kChromeSearchLocalNtpUrl, true,  ""},
    {"https://foo.com/instant?strk",   true,  ""},
    {"https://foo.com/instant#strk",   true,  ""},
    {"https://foo.com/instant?strk=0", true,  ""},
    {"https://foo.com/url?strk",       false, "Disabled on SRP"},
    {"https://foo.com/alt?strk",       false, "Disabled ON SRP"},
    {"http://foo.com/instant",         false, "Non-HTTPS"},
    {"http://foo.com/instant?strk",    false, "Non-HTTPS"},
    {"http://foo.com/instant?strk=1",  false, "Non-HTTPS"},
    {"https://foo.com/instant",        false, "No search terms replacement"},
    {"https://foo.com/?strk",          false, "Non-exact path"},
  };

  for (size_t i = 0; i < arraysize(kTestCases); ++i) {
    const SearchTestCase& test = kTestCases[i];
    EXPECT_EQ(test.expected_result,
              ShouldAssignURLToInstantRenderer(GURL(test.url), profile()))
        << test.url << " " << test.comment;
  }
}

TEST_F(SearchTest, ShouldUseProcessPerSiteForInstantURL) {
  EnableQueryExtractionForTesting();

  const SearchTestCase kTestCases[] = {
    {"chrome-search://local-ntp",      true,  "Local NTP"},
    {"chrome-search://online-ntp",     true,  "Online NTP"},
    {"invalid-scheme://local-ntp",     false, "Invalid Local NTP URL"},
    {"invalid-scheme://online-ntp",    false, "Invalid Online NTP URL"},
    {"chrome-search://foo.com",        false, "Search result page"},
    {"https://foo.com/instant?strk",   false,  ""},
    {"https://foo.com/instant#strk",   false,  ""},
    {"https://foo.com/instant?strk=0", false,  ""},
    {"https://foo.com/url?strk",       false,  ""},
    {"https://foo.com/alt?strk",       false,  ""},
    {"http://foo.com/instant",         false,  "Non-HTTPS"},
    {"http://foo.com/instant?strk",    false,  "Non-HTTPS"},
    {"http://foo.com/instant?strk=1",  false,  "Non-HTTPS"},
    {"https://foo.com/instant",        false,  "No search terms replacement"},
    {"https://foo.com/?strk",          false,  "Non-exact path"},
  };

  for (size_t i = 0; i < arraysize(kTestCases); ++i) {
    const SearchTestCase& test = kTestCases[i];
    EXPECT_EQ(test.expected_result,
              ShouldUseProcessPerSiteForInstantURL(GURL(test.url), profile()))
        << test.url << " " << test.comment;
  }
}

// Each test case represents a navigation to |start_url| followed by a
// navigation to |end_url|. We will check whether each navigation lands in an
// Instant process, and also whether the navigation from start to end re-uses
// the same SiteInstance (and hence the same RenderViewHost, etc.).
const struct ProcessIsolationTestCase {
  const char* description;
  const char* start_url;
  bool start_in_instant_process;
  const char* end_url;
  bool end_in_instant_process;
  bool same_site_instance;
} kProcessIsolationTestCases[] = {
  {"Local NTP -> SRP",
   "chrome-search://local-ntp",       true,
   "https://foo.com/url?strk",        true,   false },
  {"Local NTP -> Regular",
   "chrome-search://local-ntp",       true,
   "https://foo.com/other",           false,  false },
  {"Remote NTP -> SRP",
   "https://foo.com/instant?strk",    true,
   "https://foo.com/url?strk",        true,   false },
  {"Remote NTP -> Regular",
   "https://foo.com/instant?strk",    true,
   "https://foo.com/other",           false,  false },
  {"SRP -> SRP",
   "https://foo.com/url?strk",        true,
   "https://foo.com/url?strk",        true,   true  },
  {"SRP -> Regular",
   "https://foo.com/url?strk",        true,
   "https://foo.com/other",           false,  false },
  {"Regular -> SRP",
   "https://foo.com/other",           false,
   "https://foo.com/url?strk",        true,   false },
};

TEST_F(SearchTest, ProcessIsolation) {
  EnableQueryExtractionForTesting();

  for (size_t i = 0; i < arraysize(kProcessIsolationTestCases); ++i) {
    const ProcessIsolationTestCase& test = kProcessIsolationTestCases[i];
    AddTab(browser(), GURL("chrome://blank"));
    const content::WebContents* contents =
        browser()->tab_strip_model()->GetActiveWebContents();

    // Navigate to start URL.
    NavigateAndCommitActiveTab(GURL(test.start_url));
    EXPECT_EQ(test.start_in_instant_process, InInstantProcess(contents))
        << test.description;

    // Save state.
    const scoped_refptr<content::SiteInstance> start_site_instance =
        contents->GetSiteInstance();
    const content::RenderProcessHost* start_rph =
        contents->GetRenderProcessHost();
    const content::RenderViewHost* start_rvh =
        contents->GetRenderViewHost();

    // Navigate to end URL.
    NavigateAndCommitActiveTab(GURL(test.end_url));
    EXPECT_EQ(test.end_in_instant_process, InInstantProcess(contents))
        << test.description;

    EXPECT_EQ(test.same_site_instance,
              start_site_instance == contents->GetSiteInstance())
        << test.description;
    EXPECT_EQ(test.same_site_instance,
              start_rvh == contents->GetRenderViewHost())
        << test.description;
    EXPECT_EQ(test.same_site_instance,
              start_rph == contents->GetRenderProcessHost())
        << test.description;
  }
}

TEST_F(SearchTest, ProcessIsolation_RendererInitiated) {
  EnableQueryExtractionForTesting();

  for (size_t i = 0; i < arraysize(kProcessIsolationTestCases); ++i) {
    const ProcessIsolationTestCase& test = kProcessIsolationTestCases[i];
    AddTab(browser(), GURL("chrome://blank"));
    content::WebContents* contents =
        browser()->tab_strip_model()->GetActiveWebContents();

    // Navigate to start URL.
    NavigateAndCommitActiveTab(GURL(test.start_url));
    EXPECT_EQ(test.start_in_instant_process, InInstantProcess(contents))
        << test.description;

    // Save state.
    const scoped_refptr<content::SiteInstance> start_site_instance =
        contents->GetSiteInstance();
    const content::RenderProcessHost* start_rph =
        contents->GetRenderProcessHost();
    const content::RenderViewHost* start_rvh =
        contents->GetRenderViewHost();

    // Navigate to end URL via a renderer-initiated navigation.
    content::NavigationController* controller = &contents->GetController();
    content::NavigationController::LoadURLParams load_params(
        GURL(test.end_url));
    load_params.is_renderer_initiated = true;
    load_params.transition_type = content::PAGE_TRANSITION_LINK;

    controller->LoadURLWithParams(load_params);
    CommitPendingLoad(controller);
    EXPECT_EQ(test.end_in_instant_process, InInstantProcess(contents))
        << test.description;

    EXPECT_EQ(test.same_site_instance,
              start_site_instance == contents->GetSiteInstance())
        << test.description;
    EXPECT_EQ(test.same_site_instance,
              start_rvh == contents->GetRenderViewHost())
        << test.description;
    EXPECT_EQ(test.same_site_instance,
              start_rph == contents->GetRenderProcessHost())
        << test.description;
  }
}

const SearchTestCase kInstantNTPTestCases[] = {
  {"https://foo.com/instant?strk",         true,  "Valid Instant URL"},
  {"https://foo.com/instant#strk",         true,  "Valid Instant URL"},
  {"https://foo.com/url?strk",             true,  "Valid search URL"},
  {"https://foo.com/url#strk",             true,  "Valid search URL"},
  {"https://foo.com/alt?strk",             true,  "Valid alternative URL"},
  {"https://foo.com/alt#strk",             true,  "Valid alternative URL"},
  {"https://foo.com/url?strk&bar=",        true,  "No query terms"},
  {"https://foo.com/url?strk&q=abc",       true,  "No query terms key"},
  {"https://foo.com/url?strk#bar=abc",     true,  "Query terms key in ref"},
  {"https://foo.com/url?strk&bar=abc",     false, "Has query terms"},
  {"http://foo.com/instant?strk=1",        false, "Insecure URL"},
  {"https://foo.com/instant",              false, "No search term replacement"},
  {"chrome://blank/",                      false, "Chrome scheme"},
  {"chrome-search://foo",                  false, "Chrome-search scheme"},
  {chrome::kChromeSearchLocalNtpUrl,       true,  "Local new tab page"},
  {"https://bar.com/instant?strk=1",       false, "Random non-search page"},
};

TEST_F(SearchTest, InstantNTPExtendedEnabled) {
  EnableQueryExtractionForTesting();
  // TODO(samarth): update test cases to use cacheable NTP URLs and remove this.
  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
      "InstantExtended", "Group1 use_cacheable_ntp:0"));
  AddTab(browser(), GURL("chrome://blank"));
  for (size_t i = 0; i < arraysize(kInstantNTPTestCases); ++i) {
    const SearchTestCase& test = kInstantNTPTestCases[i];
    NavigateAndCommitActiveTab(GURL(test.url));
    const content::WebContents* contents =
        browser()->tab_strip_model()->GetWebContentsAt(0);
    EXPECT_EQ(test.expected_result, IsInstantNTP(contents))
        << test.url << " " << test.comment;
  }
}

TEST_F(SearchTest, InstantNTPCustomNavigationEntry) {
  EnableQueryExtractionForTesting();
  // TODO(samarth): update test cases to use cacheable NTP URLs and remove this.
  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
      "InstantExtended", "Group1 use_cacheable_ntp:0"));
  AddTab(browser(), GURL("chrome://blank"));
  for (size_t i = 0; i < arraysize(kInstantNTPTestCases); ++i) {
    const SearchTestCase& test = kInstantNTPTestCases[i];
    NavigateAndCommitActiveTab(GURL(test.url));
    content::WebContents* contents =
        browser()->tab_strip_model()->GetWebContentsAt(0);
    content::NavigationController& controller = contents->GetController();
    controller.SetTransientEntry(
        controller.CreateNavigationEntry(GURL("chrome://blank"),
                                         content::Referrer(),
                                         content::PAGE_TRANSITION_LINK,
                                         false,
                                         std::string(),
                                         contents->GetBrowserContext()));
    // The active entry is chrome://blank and not an NTP.
    EXPECT_FALSE(IsInstantNTP(contents));
    EXPECT_EQ(test.expected_result,
              NavEntryIsInstantNTP(contents,
                                   controller.GetLastCommittedEntry()))
        << test.url << " " << test.comment;
  }
}

TEST_F(SearchTest, InstantCacheableNTPNavigationEntry) {
  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
      "EmbeddedSearch", "Group1 use_cacheable_ntp:1"));

  AddTab(browser(), GURL("chrome://blank"));
  content::WebContents* contents =
        browser()->tab_strip_model()->GetWebContentsAt(0);
  content::NavigationController& controller = contents->GetController();
  // Local NTP.
  NavigateAndCommitActiveTab(GURL(chrome::kChromeSearchLocalNtpUrl));
  EXPECT_TRUE(NavEntryIsInstantNTP(contents,
                                   controller.GetLastCommittedEntry()));
  // Instant page is not cacheable NTP.
  NavigateAndCommitActiveTab(GetInstantURL(profile(), kDisableStartMargin,
                                           false));
  EXPECT_FALSE(NavEntryIsInstantNTP(contents,
                                    controller.GetLastCommittedEntry()));
  // Test Cacheable NTP
  NavigateAndCommitActiveTab(chrome::GetNewTabPageURL(profile()));
  EXPECT_TRUE(NavEntryIsInstantNTP(contents,
                                   controller.GetLastCommittedEntry()));
}

TEST_F(SearchTest, InstantCacheableNTPNavigationEntryNewProfile) {
  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
      "EmbeddedSearch", "Group1 use_cacheable_ntp:1"));
  SetSearchProvider(false, false);
  AddTab(browser(), GURL(chrome::kChromeUINewTabURL));
  content::WebContents* contents =
        browser()->tab_strip_model()->GetWebContentsAt(0);
  content::NavigationController& controller = contents->GetController();
  // Test virtual url chrome://newtab  for first NTP of a new profile
  EXPECT_TRUE(NavEntryIsInstantNTP(contents,
                                   controller.GetLastCommittedEntry()));
  // The new_tab_url gets set after the first NTP is visible.
  SetSearchProvider(true, false);
  EXPECT_TRUE(NavEntryIsInstantNTP(contents,
                                   controller.GetLastCommittedEntry()));
}

TEST_F(SearchTest, UseLocalNTPInIncognito) {
  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
      "EmbeddedSearch", "Group1 use_cacheable_ntp:1"));
  EXPECT_EQ(GURL(), chrome::GetNewTabPageURL(
      profile()->GetOffTheRecordProfile()));
}

TEST_F(SearchTest, UseLocalNTPIfNTPURLIsInsecure) {
  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
      "EmbeddedSearch", "Group1 use_cacheable_ntp:1"));
  // Set an insecure new tab page URL and verify that it's ignored.
  SetSearchProvider(true, true);
  EXPECT_EQ(GURL(chrome::kChromeSearchLocalNtpUrl),
            chrome::GetNewTabPageURL(profile()));
}

TEST_F(SearchTest, UseLocalNTPIfNTPURLIsNotSet) {
  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
      "EmbeddedSearch", "Group1 use_cacheable_ntp:1"));
  // Set an insecure new tab page URL and verify that it's ignored.
  SetSearchProvider(false, true);
  EXPECT_EQ(GURL(chrome::kChromeSearchLocalNtpUrl),
            chrome::GetNewTabPageURL(profile()));
}

TEST_F(SearchTest, UseLocalNTPIfNTPURLIsBlockedForSupervisedUser) {
  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
      "EmbeddedSearch", "Group1 use_cacheable_ntp:1"));

  // Block access to foo.com in the URL filter.
  ManagedUserService* managed_user_service =
      ManagedUserServiceFactory::GetForProfile(profile());
  ManagedModeURLFilter* url_filter =
      managed_user_service->GetURLFilterForUIThread();
  std::map<std::string, bool> hosts;
  hosts["foo.com"] = false;
  url_filter->SetManualHosts(&hosts);

  EXPECT_EQ(GURL(chrome::kChromeSearchLocalNtpUrl),
            chrome::GetNewTabPageURL(profile()));
  EXPECT_EQ(GURL(), GetInstantURL(profile(), kDisableStartMargin, false));
}

TEST_F(SearchTest, GetInstantURL) {
  // No Instant URL because "strk" is missing.
  SetDefaultInstantTemplateUrl(false);
  EXPECT_EQ(GURL(), GetInstantURL(profile(), kDisableStartMargin, false));

  // Set an Instant URL with a valid search terms replacement key.
  SetDefaultInstantTemplateUrl(true);

  // Now there should be a valid Instant URL. Note the HTTPS "upgrade".
  EXPECT_EQ(GURL("https://foo.com/instant?foo=foo#foo=foo&strk"),
            GetInstantURL(profile(), kDisableStartMargin, false));

  // Enable suggest. No difference.
  profile()->GetPrefs()->SetBoolean(prefs::kSearchSuggestEnabled, true);
  EXPECT_EQ(GURL("https://foo.com/instant?foo=foo#foo=foo&strk"),
            GetInstantURL(profile(), kDisableStartMargin, false));

  // Disable suggest. No Instant URL.
  profile()->GetPrefs()->SetBoolean(prefs::kSearchSuggestEnabled, false);
  EXPECT_EQ(GURL(), GetInstantURL(profile(), kDisableStartMargin, false));
}

TEST_F(SearchTest, StartMarginCGI) {
  // No margin.
  profile()->GetPrefs()->SetBoolean(prefs::kSearchSuggestEnabled, true);

  EXPECT_EQ(GURL("https://foo.com/instant?foo=foo#foo=foo&strk"),
            GetInstantURL(profile(), kDisableStartMargin, false));

  // With start margin.
  EXPECT_EQ(GURL("https://foo.com/instant?es_sm=10&foo=foo#foo=foo&strk"),
            GetInstantURL(profile(), 10, false));
}

TEST_F(SearchTest, InstantSearchEnabledCGI) {
  // Disable Instant Search.
  // Make sure {google:forceInstantResults} is not set in the Instant URL.
  EXPECT_EQ(GURL("https://foo.com/instant?foo=foo#foo=foo&strk"),
            GetInstantURL(profile(), kDisableStartMargin, false));

  // Enable Instant Search.
  // Make sure {google:forceInstantResults} is set in the Instant URL.
  EXPECT_EQ(GURL("https://foo.com/instant?ion=1&foo=foo#foo=foo&strk"),
            GetInstantURL(profile(), kDisableStartMargin, true));
}

TEST_F(SearchTest, CommandLineOverrides) {
  GURL local_instant_url(GetLocalInstantURL(profile()));
  EXPECT_EQ(GURL(chrome::kChromeSearchLocalNtpUrl), local_instant_url);

  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile());
  TemplateURLData data;
  data.SetURL("{google:baseURL}search?q={searchTerms}");
  data.instant_url = "{google:baseURL}webhp?strk";
  data.search_terms_replacement_key = "strk";
  TemplateURL* template_url = new TemplateURL(profile(), data);
  // Takes ownership of |template_url|.
  template_url_service->Add(template_url);
  template_url_service->SetDefaultSearchProvider(template_url);

  // By default, Instant Extended forces the instant URL to be HTTPS, so even if
  // we set a Google base URL that is HTTP, we should get an HTTPS URL.
  UIThreadSearchTermsData::SetGoogleBaseURL("http://www.foo.com/");
  GURL instant_url(GetInstantURL(profile(), kDisableStartMargin, false));
  ASSERT_TRUE(instant_url.is_valid());
  EXPECT_EQ("https://www.foo.com/webhp?strk", instant_url.spec());

  // However, if the Google base URL is specified on the command line, the
  // instant URL should just use it, even if it's HTTP.
  UIThreadSearchTermsData::SetGoogleBaseURL(std::string());
  CommandLine::ForCurrentProcess()->AppendSwitchASCII(switches::kGoogleBaseURL,
                                                      "http://www.bar.com/");
  instant_url = GetInstantURL(profile(), kDisableStartMargin, false);
  ASSERT_TRUE(instant_url.is_valid());
  EXPECT_EQ("http://www.bar.com/webhp?strk", instant_url.spec());

  // Similarly, setting a Google base URL on the command line should allow you
  // to get the Google version of the local NTP, even though search provider's
  // URL doesn't contain "google".
  local_instant_url = GetLocalInstantURL(profile());
  EXPECT_EQ(GURL(chrome::kChromeSearchLocalNtpUrl), local_instant_url);

  // If we specify extra search query params, they should be inserted into the
  // query portion of the instant URL.
  CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kExtraSearchQueryParams, "a=b");
  instant_url = GetInstantURL(profile(), kDisableStartMargin, false);
  ASSERT_TRUE(instant_url.is_valid());
  EXPECT_EQ("http://www.bar.com/webhp?a=b&strk", instant_url.spec());
}

TEST_F(SearchTest, ShouldShowInstantNTP_Default) {
  EXPECT_FALSE(ShouldShowInstantNTP());
}

TEST_F(SearchTest, ShouldShowInstantNTP_DisabledViaFinch) {
  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
      "EmbeddedSearch", "Group1 show_ntp:0"));
  EXPECT_FALSE(ShouldShowInstantNTP());
}

TEST_F(SearchTest, ShouldShowInstantNTP_DisabledByUseCacheableNTPFinchFlag) {
  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
      "EmbeddedSearch", "Group1 use_cacheable_ntp:1"));
  EXPECT_FALSE(ShouldShowInstantNTP());
}

TEST_F(SearchTest, ShouldUseCacheableNTP_Default) {
  EXPECT_TRUE(ShouldUseCacheableNTP());
}

TEST_F(SearchTest, ShouldUseCacheableNTP_EnabledViaFinch) {
  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
      "EmbeddedSearch", "Group1 use_cacheable_ntp:1"));
  EXPECT_TRUE(ShouldUseCacheableNTP());
}

TEST_F(SearchTest, ShouldPrefetchSearchResults_Default) {
  EXPECT_FALSE(ShouldPrefetchSearchResults());
}

TEST_F(SearchTest, ShouldPrefetchSearchResults_InstantExtendedAPIEnabled) {
  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
      "EmbeddedSearch",
      "Group1 espv:2 use_cacheable_ntp:1 prefetch_results:1"));
  EXPECT_TRUE(ShouldPrefetchSearchResults());
#if defined(OS_IOS) || defined(OS_ANDROID)
  EXPECT_EQ(1ul, EmbeddedSearchPageVersion());
#else
  EXPECT_EQ(2ul, EmbeddedSearchPageVersion());
#endif
}

TEST_F(SearchTest, ShouldPrefetchSearchResults_DisabledViaFinch) {
  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
      "EmbeddedSearch",
      "Group1 use_cacheable_ntp:1 espv:89 prefetch_results:0"));
  EXPECT_FALSE(ShouldPrefetchSearchResults());
  EXPECT_EQ(89ul, EmbeddedSearchPageVersion());
}

TEST_F(SearchTest, ShouldPrefetchSearchResults_EnabledViaFinch) {
  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
      "EmbeddedSearch",
      "Group1 espv:80 use_cacheable_ntp:1 prefetch_results:1"));
  EXPECT_TRUE(ShouldPrefetchSearchResults());
  EXPECT_EQ(80ul, EmbeddedSearchPageVersion());
}

TEST_F(SearchTest, IsNTPURL) {
  GURL invalid_url;
  GURL ntp_url(chrome::kChromeUINewTabURL);
  GURL local_ntp_url(GetLocalInstantURL(profile()));

  EXPECT_FALSE(chrome::IsNTPURL(invalid_url, profile()));
  // No margin.
  EnableQueryExtractionForTesting();
  profile()->GetPrefs()->SetBoolean(prefs::kSearchSuggestEnabled, true);
  GURL remote_ntp_url(GetInstantURL(profile(), kDisableStartMargin, false));
  GURL search_url_with_search_terms("https://foo.com/url?strk&bar=abc");
  GURL search_url_without_search_terms("https://foo.com/url?strk&bar");

  EXPECT_FALSE(chrome::IsNTPURL(ntp_url, profile()));
  EXPECT_TRUE(chrome::IsNTPURL(local_ntp_url, profile()));
  EXPECT_TRUE(chrome::IsNTPURL(remote_ntp_url, profile()));
  EXPECT_FALSE(chrome::IsNTPURL(search_url_with_search_terms, profile()));
  EXPECT_TRUE(chrome::IsNTPURL(search_url_without_search_terms, profile()));

  EXPECT_FALSE(chrome::IsNTPURL(ntp_url, NULL));
  EXPECT_FALSE(chrome::IsNTPURL(local_ntp_url, NULL));
  EXPECT_FALSE(chrome::IsNTPURL(remote_ntp_url, NULL));
  EXPECT_FALSE(chrome::IsNTPURL(search_url_with_search_terms, NULL));
  EXPECT_FALSE(chrome::IsNTPURL(search_url_without_search_terms, NULL));
}

TEST_F(SearchTest, GetSearchURLs) {
  std::vector<GURL> search_urls = GetSearchURLs(profile());
  EXPECT_EQ(2U, search_urls.size());
  EXPECT_EQ("http://foo.com/alt#quux=", search_urls[0].spec());
  EXPECT_EQ("http://foo.com/url?bar=", search_urls[1].spec());
}

TEST_F(SearchTest, GetSearchResultPrefetchBaseURL) {
  // "prefetch_results" flag is disabled.
  EXPECT_EQ(GURL(), GetSearchResultPrefetchBaseURL(profile()));

  // "prefetch_results" flag is enabled via field trials.
  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
      "EmbeddedSearch",
      "Group1 espv:80 use_cacheable_ntp:1 prefetch_results:1"));
  EXPECT_TRUE(ShouldPrefetchSearchResults());

  EXPECT_EQ(GURL("https://foo.com/instant?ion=1&foo=foo#foo=foo&strk"),
            GetSearchResultPrefetchBaseURL(profile()));
}

typedef SearchTest IsQueryExtractionEnabledTest;

TEST_F(IsQueryExtractionEnabledTest, NotSet) {
  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
      "EmbeddedSearch", "Group1 espv:2"));
  EXPECT_TRUE(IsInstantExtendedAPIEnabled());
  EXPECT_FALSE(IsQueryExtractionEnabled());
  EXPECT_EQ(2ul, EmbeddedSearchPageVersion());
}

TEST_F(IsQueryExtractionEnabledTest, EnabledViaFinch) {
  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
      "EmbeddedSearch", "Group1 espv:2 query_extraction:1"));
  EXPECT_TRUE(IsInstantExtendedAPIEnabled());
  EXPECT_TRUE(IsQueryExtractionEnabled());
  EXPECT_EQ(2ul, EmbeddedSearchPageVersion());
}

TEST_F(IsQueryExtractionEnabledTest, DisabledViaFinch) {
  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
      "EmbeddedSearch", "Group1 espv:2 query_extraction:0"));
  EXPECT_TRUE(IsInstantExtendedAPIEnabled());
  EXPECT_FALSE(IsQueryExtractionEnabled());
  EXPECT_EQ(2ul, EmbeddedSearchPageVersion());
}

TEST_F(IsQueryExtractionEnabledTest, EnabledViaCommandLine) {
  EnableQueryExtractionForTesting();
  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
      "EmbeddedSearch", "Group1 espv:2 query_extraction:0"));
  EXPECT_TRUE(IsInstantExtendedAPIEnabled());
  EXPECT_TRUE(IsQueryExtractionEnabled());
  EXPECT_EQ(2ul, EmbeddedSearchPageVersion());
}

typedef SearchTest ShouldHideTopVerbatimTest;

TEST_F(ShouldHideTopVerbatimTest, DoNotHideByDefault) {
  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial("EmbeddedSearch",
                                                     "Control"));
  EXPECT_FALSE(ShouldHideTopVerbatimMatch());
}

TEST_F(ShouldHideTopVerbatimTest, DoNotHideInInstantExtended) {
  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial("EmbeddedSearch",
                                                     "Group1"));
  EXPECT_FALSE(ShouldHideTopVerbatimMatch());
}

TEST_F(ShouldHideTopVerbatimTest, EnableByFlagInInstantExtended) {
  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial("EmbeddedSearch",
                                                     "Group1 hide_verbatim:1"));
  EXPECT_TRUE(ShouldHideTopVerbatimMatch());
}

TEST_F(ShouldHideTopVerbatimTest, EnableByFlagOutsideInstantExtended) {
  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
      "EmbeddedSearch", "Controll1 hide_verbatim:1"));
  EXPECT_TRUE(ShouldHideTopVerbatimMatch());
}

TEST_F(ShouldHideTopVerbatimTest, DisableByFlag) {
  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial("EmbeddedSearch",
                                                     "Group1 hide_verbatim:0"));
  EXPECT_FALSE(ShouldHideTopVerbatimMatch());
}

typedef SearchTest DisplaySearchButtonTest;

TEST_F(DisplaySearchButtonTest, NotSet) {
  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
      "EmbeddedSearch", "Group1 espv:2"));
  EXPECT_EQ(DISPLAY_SEARCH_BUTTON_NEVER, GetDisplaySearchButtonConditions());
}

TEST_F(DisplaySearchButtonTest, Never) {
  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
      "EmbeddedSearch", "Group1 espv:2 display_search_button:0"));
  EXPECT_EQ(DISPLAY_SEARCH_BUTTON_NEVER, GetDisplaySearchButtonConditions());
}

TEST_F(DisplaySearchButtonTest, CommandLineNever) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisableSearchButtonInOmnibox);
  EXPECT_EQ(DISPLAY_SEARCH_BUTTON_NEVER, GetDisplaySearchButtonConditions());

  // Command-line disable should override Finch.
  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
      "EmbeddedSearch", "Group1 espv:2 display_search_button:1"));
  EXPECT_EQ(DISPLAY_SEARCH_BUTTON_NEVER, GetDisplaySearchButtonConditions());
}

TEST_F(DisplaySearchButtonTest, ForSearchTermReplacement) {
  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
      "EmbeddedSearch", "Group1 espv:2 display_search_button:1"));
  EXPECT_EQ(DISPLAY_SEARCH_BUTTON_FOR_STR, GetDisplaySearchButtonConditions());
}

TEST_F(DisplaySearchButtonTest, CommandLineForSearchTermReplacement) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableSearchButtonInOmniboxForStr);
  EXPECT_EQ(DISPLAY_SEARCH_BUTTON_FOR_STR, GetDisplaySearchButtonConditions());
}

TEST_F(DisplaySearchButtonTest, ForSearchTermReplacementOrInputInProgress) {
  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
      "EmbeddedSearch", "Group1 espv:2 display_search_button:2"));
  EXPECT_EQ(DISPLAY_SEARCH_BUTTON_FOR_STR_OR_IIP,
            GetDisplaySearchButtonConditions());
}

TEST_F(DisplaySearchButtonTest,
       CommandLineForSearchTermReplacementOrInputInProgress) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableSearchButtonInOmniboxForStrOrIip);
  EXPECT_EQ(DISPLAY_SEARCH_BUTTON_FOR_STR_OR_IIP,
            GetDisplaySearchButtonConditions());
}

TEST_F(DisplaySearchButtonTest, Always) {
  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
      "EmbeddedSearch", "Group1 espv:2 display_search_button:3"));
  EXPECT_EQ(DISPLAY_SEARCH_BUTTON_ALWAYS, GetDisplaySearchButtonConditions());
}

TEST_F(DisplaySearchButtonTest, CommandLineAlways) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableSearchButtonInOmniboxAlways);
  EXPECT_EQ(DISPLAY_SEARCH_BUTTON_ALWAYS, GetDisplaySearchButtonConditions());
}

TEST_F(DisplaySearchButtonTest, InvalidValue) {
  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
      "EmbeddedSearch", "Group1 espv:2 display_search_button:4"));
  EXPECT_EQ(DISPLAY_SEARCH_BUTTON_NEVER, GetDisplaySearchButtonConditions());
}

typedef SearchTest OriginChipTest;

TEST_F(OriginChipTest, NotSet) {
  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
      "EmbeddedSearch", "Group1 espv:2"));
  EXPECT_FALSE(ShouldDisplayOriginChip());
}

TEST_F(OriginChipTest, NoOriginChip) {
  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
      "EmbeddedSearch", "Group1 espv:2 origin_chip:0"));
  EXPECT_FALSE(ShouldDisplayOriginChip());
}

TEST_F(OriginChipTest, CommandLineNoOriginChip) {
  CommandLine::ForCurrentProcess()->AppendSwitch(switches::kDisableOriginChip);
  EXPECT_FALSE(ShouldDisplayOriginChip());

  // Command-line disable should override Finch.
  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
      "EmbeddedSearch", "Group1 espv:2 origin_chip:1"));
  EXPECT_FALSE(ShouldDisplayOriginChip());
}

TEST_F(OriginChipTest, OriginChip) {
  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
      "EmbeddedSearch", "Group1 espv:2 origin_chip:1"));
  EXPECT_TRUE(ShouldDisplayOriginChip());
}

TEST_F(OriginChipTest, CommandLineOriginChip) {
  CommandLine::ForCurrentProcess()->AppendSwitch(switches::kEnableOriginChip);
  EXPECT_TRUE(ShouldDisplayOriginChip());
}

}  // namespace chrome
