// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/test/component_updater_service_unittest.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/component_updater/test/test_installer.h"
#include "chrome/common/chrome_paths.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/resource_controller.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/browser/resource_throttle.h"
#include "libxml/globals.h"
#include "net/base/upload_bytes_element_reader.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_test_util.h"
#include "url/gurl.h"

using content::BrowserThread;

using ::testing::_;
using ::testing::InSequence;
using ::testing::Mock;

namespace component_updater {

#define POST_INTERCEPT_SCHEME    "http"
#define POST_INTERCEPT_HOSTNAME  "localhost2"
#define POST_INTERCEPT_PATH      "/update2"

MockComponentObserver::MockComponentObserver() {
}

MockComponentObserver::~MockComponentObserver() {
}

TestConfigurator::TestConfigurator()
    : initial_time_(0),
      times_(1),
      recheck_time_(0),
      ondemand_time_(0),
      cus_(NULL),
      context_(new net::TestURLRequestContextGetter(
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO))) {
}

TestConfigurator::~TestConfigurator() {
}

int TestConfigurator::InitialDelay() { return initial_time_; }

int TestConfigurator::NextCheckDelay() {
  // This is called when a new full cycle of checking for updates is going
  // to happen. In test we normally only test one cycle so it is a good
  // time to break from the test messageloop Run() method so the test can
  // finish.
  if (--times_ <= 0) {
    quit_closure_.Run();
    return 0;
  }
  return 1;
}

int TestConfigurator::StepDelay() {
  return 0;
}

int TestConfigurator::StepDelayMedium() {
  return NextCheckDelay();
}

int TestConfigurator::MinimumReCheckWait() {
  return recheck_time_;
}

int TestConfigurator::OnDemandDelay() {
  return ondemand_time_;
}

GURL TestConfigurator::UpdateUrl() {
  return GURL(POST_INTERCEPT_SCHEME "://"
              POST_INTERCEPT_HOSTNAME POST_INTERCEPT_PATH);
}

GURL TestConfigurator::PingUrl() {
  return UpdateUrl();
}

std::string TestConfigurator::ExtraRequestParams() { return "extra=\"foo\""; }

size_t TestConfigurator::UrlSizeLimit() { return 256; }

net::URLRequestContextGetter* TestConfigurator::RequestContext() {
  return context_.get();
}

// Don't use the utility process to run code out-of-process.
bool TestConfigurator::InProcess() { return true; }

ComponentPatcher* TestConfigurator::CreateComponentPatcher() {
  return new MockComponentPatcher();
}

bool TestConfigurator::DeltasEnabled() const {
  return true;
}

bool TestConfigurator::UseBackgroundDownloader() const {
  return false;
}

// Set how many update checks are called, the default value is just once.
void TestConfigurator::SetLoopCount(int times) { times_ = times; }

void TestConfigurator::SetRecheckTime(int seconds) {
  recheck_time_ = seconds;
}

void TestConfigurator::SetOnDemandTime(int seconds) {
  ondemand_time_ = seconds;
}

void TestConfigurator::SetComponentUpdateService(ComponentUpdateService* cus) {
  cus_ = cus;
}

void TestConfigurator::SetQuitClosure(const base::Closure& quit_closure) {
  quit_closure_ = quit_closure;
}

void TestConfigurator::SetInitialDelay(int seconds) {
  initial_time_ = seconds;
}

InterceptorFactory::InterceptorFactory()
    : URLRequestPostInterceptorFactory(POST_INTERCEPT_SCHEME,
                                       POST_INTERCEPT_HOSTNAME) {}

InterceptorFactory::~InterceptorFactory() {}

URLRequestPostInterceptor* InterceptorFactory::CreateInterceptor() {
  return URLRequestPostInterceptorFactory::CreateInterceptor(
    base::FilePath::FromUTF8Unsafe(POST_INTERCEPT_PATH));
}

class PartialMatch : public URLRequestPostInterceptor::RequestMatcher {
 public:
  explicit PartialMatch(const std::string& expected) : expected_(expected) {}
  virtual bool Match(const std::string& actual) const OVERRIDE {
    return actual.find(expected_) != std::string::npos;
  }

 private:
  const std::string expected_;

  DISALLOW_COPY_AND_ASSIGN(PartialMatch);
};

ComponentUpdaterTest::ComponentUpdaterTest()
    : test_config_(NULL),
      thread_bundle_(content::TestBrowserThreadBundle::IO_MAINLOOP) {
  // The component updater instance under test.
  test_config_ = new TestConfigurator;
  component_updater_.reset(ComponentUpdateServiceFactory(test_config_));
  test_config_->SetComponentUpdateService(component_updater_.get());

  // The test directory is chrome/test/data/components.
  PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir_);
  test_data_dir_ = test_data_dir_.AppendASCII("components");

  net::URLFetcher::SetEnableInterceptionForTests(true);
}

ComponentUpdaterTest::~ComponentUpdaterTest() {
  net::URLFetcher::SetEnableInterceptionForTests(false);
}

void ComponentUpdaterTest::SetUp() {
  get_interceptor_.reset(new GetInterceptor);
  interceptor_factory_.reset(new InterceptorFactory);
  post_interceptor_ = interceptor_factory_->CreateInterceptor();
  EXPECT_TRUE(post_interceptor_);
}

void ComponentUpdaterTest::TearDown() {
  interceptor_factory_.reset();
  get_interceptor_.reset();
  xmlCleanupGlobals();
}

ComponentUpdateService* ComponentUpdaterTest::component_updater() {
  return component_updater_.get();
}

  // Makes the full path to a component updater test file.
const base::FilePath ComponentUpdaterTest::test_file(const char* file) {
  return test_data_dir_.AppendASCII(file);
}

TestConfigurator* ComponentUpdaterTest::test_configurator() {
  return test_config_;
}

ComponentUpdateService::Status ComponentUpdaterTest::RegisterComponent(
    CrxComponent* com,
    TestComponents component,
    const Version& version,
    TestInstaller* installer) {
  if (component == kTestComponent_abag) {
    com->name = "test_abag";
    com->pk_hash.assign(abag_hash, abag_hash + arraysize(abag_hash));
  } else if (component == kTestComponent_jebg) {
    com->name = "test_jebg";
    com->pk_hash.assign(jebg_hash, jebg_hash + arraysize(jebg_hash));
  } else {
    com->name = "test_ihfo";
    com->pk_hash.assign(ihfo_hash, ihfo_hash + arraysize(ihfo_hash));
  }
  com->version = version;
  com->installer = installer;
  return component_updater_->RegisterComponent(*com);
}

void ComponentUpdaterTest::RunThreads() {
  base::RunLoop runloop;
  test_configurator()->SetQuitClosure(runloop.QuitClosure());
  runloop.Run();

  // Since some tests need to drain currently enqueued tasks such as network
  // intercepts on the IO thread, run the threads until they are
  // idle. The component updater service won't loop again until the loop count
  // is set and the service is started.
  RunThreadsUntilIdle();
}

void ComponentUpdaterTest::RunThreadsUntilIdle() {
  base::RunLoop().RunUntilIdle();
}

ComponentUpdateService::Status OnDemandTester::OnDemand(
    ComponentUpdateService* cus, const std::string& component_id) {
  return cus->OnDemandUpdate(component_id);
}

// Verify that our test fixture work and the component updater can
// be created and destroyed with no side effects.
TEST_F(ComponentUpdaterTest, VerifyFixture) {
  EXPECT_TRUE(component_updater() != NULL);
}

// Verify that the component updater can be caught in a quick
// start-shutdown situation. Failure of this test will be a crash.
TEST_F(ComponentUpdaterTest, StartStop) {
  component_updater()->Start();
  RunThreadsUntilIdle();
  component_updater()->Stop();
}

// Verify that when the server has no updates, we go back to sleep and
// the COMPONENT_UPDATER_STARTED and COMPONENT_UPDATER_SLEEPING notifications
// are generated. No pings are sent.
TEST_F(ComponentUpdaterTest, CheckCrxSleep) {
  MockComponentObserver observer;

  EXPECT_CALL(observer,
              OnEvent(ComponentObserver::COMPONENT_UPDATER_STARTED, 0))
              .Times(1);
  EXPECT_CALL(observer,
              OnEvent(ComponentObserver::COMPONENT_UPDATER_SLEEPING, 0))
              .Times(2);
  EXPECT_CALL(observer,
              OnEvent(ComponentObserver::COMPONENT_NOT_UPDATED, 0))
              .Times(2);

  EXPECT_TRUE(post_interceptor_->ExpectRequest(new PartialMatch(
      "updatecheck"), test_file("updatecheck_reply_1.xml")));
  EXPECT_TRUE(post_interceptor_->ExpectRequest(new PartialMatch(
      "updatecheck"), test_file("updatecheck_reply_1.xml")));

  TestInstaller installer;
  CrxComponent com;
  com.observer = &observer;
  EXPECT_EQ(ComponentUpdateService::kOk,
            RegisterComponent(&com,
                              kTestComponent_abag,
                              Version("1.1"),
                              &installer));

  // We loop twice, but there are no updates so we expect two sleep messages.
  test_configurator()->SetLoopCount(2);
  component_updater()->Start();
  RunThreads();

  EXPECT_EQ(0, static_cast<TestInstaller*>(com.installer)->error());
  EXPECT_EQ(0, static_cast<TestInstaller*>(com.installer)->install_count());

  // Expect to see the two update check requests and no other requests,
  // including pings.
  EXPECT_EQ(2, post_interceptor_->GetHitCount())
      << post_interceptor_->GetRequestsAsString();
  EXPECT_EQ(2, post_interceptor_->GetCount())
      << post_interceptor_->GetRequestsAsString();
  EXPECT_NE(string::npos, post_interceptor_->GetRequests()[0].find(
      "<app appid=\"abagagagagagagagagagagagagagagag\" version=\"1.1\">"
      "<updatecheck /></app>"))
      << post_interceptor_->GetRequestsAsString();
  EXPECT_NE(string::npos, post_interceptor_->GetRequests()[1].find(
      "<app appid=\"abagagagagagagagagagagagagagagag\" version=\"1.1\">"
      "<updatecheck /></app>"))
      << post_interceptor_->GetRequestsAsString();

  component_updater()->Stop();

  // Loop twice again but this case we simulate a server error by returning
  // an empty file. Expect the behavior of the service to be the same as before.
  EXPECT_CALL(observer,
              OnEvent(ComponentObserver::COMPONENT_UPDATER_STARTED, 0))
              .Times(1);
  EXPECT_CALL(observer,
              OnEvent(ComponentObserver::COMPONENT_UPDATER_SLEEPING, 0))
              .Times(2);
  EXPECT_CALL(observer,
              OnEvent(ComponentObserver::COMPONENT_NOT_UPDATED, 0))
              .Times(2);

  post_interceptor_->Reset();
  EXPECT_TRUE(post_interceptor_->ExpectRequest(new PartialMatch(
      "updatecheck"), test_file("updatecheck_reply_empty")));
  EXPECT_TRUE(post_interceptor_->ExpectRequest(new PartialMatch(
      "updatecheck"), test_file("updatecheck_reply_empty")));

  test_configurator()->SetLoopCount(2);
  component_updater()->Start();
  RunThreads();

  EXPECT_EQ(0, static_cast<TestInstaller*>(com.installer)->error());
  EXPECT_EQ(0, static_cast<TestInstaller*>(com.installer)->install_count());

  EXPECT_EQ(2, post_interceptor_->GetHitCount())
      << post_interceptor_->GetRequestsAsString();
  EXPECT_EQ(2, post_interceptor_->GetCount())
      << post_interceptor_->GetRequestsAsString();
  EXPECT_NE(string::npos, post_interceptor_->GetRequests()[0].find(
      "<app appid=\"abagagagagagagagagagagagagagagag\" version=\"1.1\">"
      "<updatecheck /></app>"))
      << post_interceptor_->GetRequestsAsString();
  EXPECT_NE(string::npos, post_interceptor_->GetRequests()[1].find(
      "<app appid=\"abagagagagagagagagagagagagagagag\" version=\"1.1\">"
      "<updatecheck /></app>"))
      << post_interceptor_->GetRequestsAsString();

  component_updater()->Stop();
}

// Verify that we can check for updates and install one component. Besides
// the notifications above COMPONENT_UPDATE_FOUND and COMPONENT_UPDATE_READY
// should have been fired. We do two loops so the second time around there
// should be nothing left to do.
// We also check that the following network requests are issued:
// 1- update check
// 2- download crx
// 3- ping
// 4- second update check.
TEST_F(ComponentUpdaterTest, InstallCrx) {
  MockComponentObserver observer1;
  {
    InSequence seq;
    EXPECT_CALL(observer1,
                OnEvent(ComponentObserver::COMPONENT_UPDATER_STARTED, 0))
                .Times(1);
    EXPECT_CALL(observer1,
                OnEvent(ComponentObserver::COMPONENT_UPDATE_FOUND, 0))
                .Times(1);
    EXPECT_CALL(observer1,
                OnEvent(ComponentObserver::COMPONENT_UPDATE_READY, 0))
                .Times(1);
    EXPECT_CALL(observer1,
                OnEvent(ComponentObserver::COMPONENT_UPDATED, 0))
                .Times(1);
    EXPECT_CALL(observer1,
                OnEvent(ComponentObserver::COMPONENT_UPDATER_SLEEPING, 0))
                .Times(1);
    EXPECT_CALL(observer1,
                OnEvent(ComponentObserver::COMPONENT_NOT_UPDATED, 0))
                .Times(1);
    EXPECT_CALL(observer1,
                OnEvent(ComponentObserver::COMPONENT_UPDATER_SLEEPING, 0))
                .Times(1);
  }

  MockComponentObserver observer2;
  {
    InSequence seq;
    EXPECT_CALL(observer2,
                OnEvent(ComponentObserver::COMPONENT_UPDATER_STARTED, 0))
                .Times(1);
    EXPECT_CALL(observer2,
                OnEvent(ComponentObserver::COMPONENT_NOT_UPDATED, 0))
                .Times(1);
    EXPECT_CALL(observer2,
                OnEvent(ComponentObserver::COMPONENT_UPDATER_SLEEPING, 0))
                .Times(1);
    EXPECT_CALL(observer2,
                OnEvent(ComponentObserver::COMPONENT_NOT_UPDATED, 0))
                .Times(1);
    EXPECT_CALL(observer2,
                OnEvent(ComponentObserver::COMPONENT_UPDATER_SLEEPING, 0))
                .Times(1);
  }

  EXPECT_TRUE(post_interceptor_->ExpectRequest(new PartialMatch(
      "updatecheck"), test_file("updatecheck_reply_1.xml")));
  EXPECT_TRUE(post_interceptor_->ExpectRequest(new PartialMatch("event")));
  EXPECT_TRUE(post_interceptor_->ExpectRequest(new PartialMatch(
      "updatecheck"), test_file("updatecheck_reply_1.xml")));

  get_interceptor_->SetResponse(
      GURL(expected_crx_url),
      test_file("jebgalgnebhfojomionfpkfelancnnkf.crx"));

  TestInstaller installer1;
  CrxComponent com1;
  com1.observer = &observer1;
  RegisterComponent(&com1, kTestComponent_jebg, Version("0.9"), &installer1);
  TestInstaller installer2;
  CrxComponent com2;
  com2.observer = &observer2;
  RegisterComponent(&com2, kTestComponent_abag, Version("2.2"), &installer2);

  test_configurator()->SetLoopCount(2);
  component_updater()->Start();
  RunThreads();

  EXPECT_EQ(0, static_cast<TestInstaller*>(com1.installer)->error());
  EXPECT_EQ(1, static_cast<TestInstaller*>(com1.installer)->install_count());
  EXPECT_EQ(0, static_cast<TestInstaller*>(com2.installer)->error());
  EXPECT_EQ(0, static_cast<TestInstaller*>(com2.installer)->install_count());

  // Expect three request in total: two update checks and one ping.
  EXPECT_EQ(3, post_interceptor_->GetHitCount())
      << post_interceptor_->GetRequestsAsString();
  EXPECT_EQ(3, post_interceptor_->GetCount())
      << post_interceptor_->GetRequestsAsString();

  // Expect one component download.
  EXPECT_EQ(1, get_interceptor_->GetHitCount());

  EXPECT_NE(string::npos, post_interceptor_->GetRequests()[0].find(
      "<app appid=\"jebgalgnebhfojomionfpkfelancnnkf\" version=\"0.9\">"
      "<updatecheck /></app>"))
      << post_interceptor_->GetRequestsAsString();
  EXPECT_NE(string::npos, post_interceptor_->GetRequests()[0].find(
      "<app appid=\"abagagagagagagagagagagagagagagag\" version=\"2.2\">"
      "<updatecheck /></app>"))
      << post_interceptor_->GetRequestsAsString();

  EXPECT_NE(string::npos, post_interceptor_->GetRequests()[1].find(
      "<app appid=\"jebgalgnebhfojomionfpkfelancnnkf\" "
      "version=\"0.9\" nextversion=\"1.0\">"
      "<event eventtype=\"3\" eventresult=\"1\"/>"))
      << post_interceptor_->GetRequestsAsString();

  EXPECT_NE(string::npos, post_interceptor_->GetRequests()[2].find(
      "<app appid=\"jebgalgnebhfojomionfpkfelancnnkf\" version=\"1.0\">"
      "<updatecheck /></app>"));
  EXPECT_NE(string::npos, post_interceptor_->GetRequests()[2].find(
      "<app appid=\"abagagagagagagagagagagagagagagag\" version=\"2.2\">"
      "<updatecheck /></app>"))
      << post_interceptor_->GetRequestsAsString();

  // Test the protocol version is correct and the extra request attributes
  // are included in the request.
  EXPECT_NE(string::npos, post_interceptor_->GetRequests()[0].find(
      "request protocol=\"3.0\" extra=\"foo\""))
      << post_interceptor_->GetRequestsAsString();

  // Tokenize the request string to look for specific attributes, which
  // are important for backward compatibility with the version v2 of the update
  // protocol. In this case, inspect the <request>, which is the first element
  // after the xml declaration of the update request body.
  // Expect to find the |os|, |arch|, |prodchannel|, and |prodversion|
  // attributes:
  // <?xml version="1.0" encoding="UTF-8"?>
  // <request... os=... arch=... prodchannel=... prodversion=...>
  // ...
  // </request>
  const std::string update_request(post_interceptor_->GetRequests()[0]);
  std::vector<base::StringPiece> elements;
  Tokenize(update_request, "<>", &elements);
  EXPECT_NE(string::npos, elements[1].find(" os="));
  EXPECT_NE(string::npos, elements[1].find(" arch="));
  EXPECT_NE(string::npos, elements[1].find(" prodchannel="));
  EXPECT_NE(string::npos, elements[1].find(" prodversion="));

  component_updater()->Stop();
}

// This test checks that the "prodversionmin" value is handled correctly. In
// particular there should not be an install because the minimum product
// version is much higher than of chrome.
TEST_F(ComponentUpdaterTest, ProdVersionCheck) {
  EXPECT_TRUE(post_interceptor_->ExpectRequest(new PartialMatch(
      "updatecheck"), test_file("updatecheck_reply_2.xml")));

  get_interceptor_->SetResponse(
      GURL(expected_crx_url),
      test_file("jebgalgnebhfojomionfpkfelancnnkf.crx"));

  TestInstaller installer;
  CrxComponent com;
  RegisterComponent(&com, kTestComponent_jebg, Version("0.9"), &installer);

  test_configurator()->SetLoopCount(1);
  component_updater()->Start();
  RunThreads();

  // Expect one update check and no ping.
  EXPECT_EQ(1, post_interceptor_->GetHitCount())
      << post_interceptor_->GetRequestsAsString();
  EXPECT_EQ(1, post_interceptor_->GetCount())
      << post_interceptor_->GetRequestsAsString();

  // Expect no download to occur.
  EXPECT_EQ(0, get_interceptor_->GetHitCount());

  EXPECT_EQ(0, static_cast<TestInstaller*>(com.installer)->error());
  EXPECT_EQ(0, static_cast<TestInstaller*>(com.installer)->install_count());

  component_updater()->Stop();
}

// Test that a update check due to an on demand call can cause installs.
// Here is the timeline:
//  - First loop: we return a reply that indicates no update, so
//    nothing happens.
//  - We make an on demand call.
//  - This triggers a second loop, which has a reply that triggers an install.
TEST_F(ComponentUpdaterTest, OnDemandUpdate) {
  MockComponentObserver observer1;
  {
    InSequence seq;
    EXPECT_CALL(observer1,
                OnEvent(ComponentObserver::COMPONENT_UPDATER_STARTED, 0))
                .Times(1);
    EXPECT_CALL(observer1,
                OnEvent(ComponentObserver::COMPONENT_NOT_UPDATED, 0))
                .Times(1);
    EXPECT_CALL(observer1,
                OnEvent(ComponentObserver::COMPONENT_UPDATER_SLEEPING, 0))
                .Times(1);
    EXPECT_CALL(observer1,
                OnEvent(ComponentObserver::COMPONENT_UPDATER_STARTED, 0))
                .Times(1);
    EXPECT_CALL(observer1,
                OnEvent(ComponentObserver::COMPONENT_NOT_UPDATED, 0))
                .Times(1);
    EXPECT_CALL(observer1,
                OnEvent(ComponentObserver::COMPONENT_UPDATER_SLEEPING, 0))
                .Times(1);
  }

  MockComponentObserver observer2;
  {
    InSequence seq;
    EXPECT_CALL(observer2,
                OnEvent(ComponentObserver::COMPONENT_UPDATER_STARTED, 0))
                .Times(1);
    EXPECT_CALL(observer2,
                OnEvent(ComponentObserver::COMPONENT_NOT_UPDATED, 0))
                .Times(1);
    EXPECT_CALL(observer2,
                OnEvent(ComponentObserver::COMPONENT_UPDATER_SLEEPING, 0))
                .Times(1);
    EXPECT_CALL(observer2,
                OnEvent(ComponentObserver::COMPONENT_UPDATER_STARTED, 0))
                .Times(1);
    EXPECT_CALL(observer2,
                OnEvent(ComponentObserver::COMPONENT_UPDATE_FOUND, 0))
                .Times(1);
    EXPECT_CALL(observer2,
                OnEvent(ComponentObserver::COMPONENT_UPDATE_READY, 0))
                .Times(1);
    EXPECT_CALL(observer2,
                OnEvent(ComponentObserver::COMPONENT_UPDATED, 0))
                .Times(1);
    EXPECT_CALL(observer2,
                OnEvent(ComponentObserver::COMPONENT_UPDATER_SLEEPING, 0))
                .Times(1);
  }

  EXPECT_TRUE(post_interceptor_->ExpectRequest(new PartialMatch(
      "updatecheck"), test_file("updatecheck_reply_empty")));

  get_interceptor_->SetResponse(
      GURL(expected_crx_url),
      test_file("jebgalgnebhfojomionfpkfelancnnkf.crx"));

  TestInstaller installer1;
  CrxComponent com1;
  com1.observer = &observer1;
  RegisterComponent(&com1, kTestComponent_abag, Version("2.2"), &installer1);
  TestInstaller installer2;
  CrxComponent com2;
  com2.observer = &observer2;
  RegisterComponent(&com2, kTestComponent_jebg, Version("0.9"), &installer2);

  // No update normally.
  test_configurator()->SetLoopCount(1);
  component_updater()->Start();
  RunThreads();
  component_updater()->Stop();

  EXPECT_EQ(1, post_interceptor_->GetHitCount())
      << post_interceptor_->GetRequestsAsString();
  EXPECT_EQ(1, post_interceptor_->GetCount())
      << post_interceptor_->GetRequestsAsString();

  EXPECT_EQ(0, get_interceptor_->GetHitCount());

  // Update after an on-demand check is issued.
  post_interceptor_->Reset();
  EXPECT_TRUE(post_interceptor_->ExpectRequest(new PartialMatch(
      "updatecheck"), test_file("updatecheck_reply_1.xml")));
  EXPECT_TRUE(post_interceptor_->ExpectRequest(new PartialMatch("event")));

  EXPECT_EQ(ComponentUpdateService::kOk,
            OnDemandTester::OnDemand(component_updater(),
                                     GetCrxComponentID(com2)));
  test_configurator()->SetLoopCount(1);
  component_updater()->Start();
  RunThreads();

  EXPECT_EQ(0, static_cast<TestInstaller*>(com1.installer)->error());
  EXPECT_EQ(0, static_cast<TestInstaller*>(com1.installer)->install_count());
  EXPECT_EQ(0, static_cast<TestInstaller*>(com2.installer)->error());
  EXPECT_EQ(1, static_cast<TestInstaller*>(com2.installer)->install_count());

  EXPECT_EQ(2, post_interceptor_->GetHitCount())
      << post_interceptor_->GetRequestsAsString();
  EXPECT_EQ(2, post_interceptor_->GetCount())
      << post_interceptor_->GetRequestsAsString();

  EXPECT_EQ(1, get_interceptor_->GetHitCount());

  // Expect the update check to contain an "ondemand" request for the
  // second component (com2) and a normal request for the other component.
  EXPECT_NE(string::npos, post_interceptor_->GetRequests()[0].find(
      "<app appid=\"abagagagagagagagagagagagagagagag\" "
      "version=\"2.2\"><updatecheck /></app>"))
      << post_interceptor_->GetRequestsAsString();
  EXPECT_NE(string::npos, post_interceptor_->GetRequests()[0].find(
      "<app appid=\"jebgalgnebhfojomionfpkfelancnnkf\" "
      "version=\"0.9\" installsource=\"ondemand\"><updatecheck /></app>"))
      << post_interceptor_->GetRequestsAsString();
  EXPECT_NE(string::npos, post_interceptor_->GetRequests()[1].find(
      "<app appid=\"jebgalgnebhfojomionfpkfelancnnkf\" "
      "version=\"0.9\" nextversion=\"1.0\">"
      "<event eventtype=\"3\" eventresult=\"1\"/>"))
      << post_interceptor_->GetRequestsAsString();

  // Also check what happens if previous check too soon.
  test_configurator()->SetOnDemandTime(60 * 60);
  EXPECT_EQ(ComponentUpdateService::kError,
            OnDemandTester::OnDemand(component_updater(),
                                     GetCrxComponentID(com2)));
  // Okay, now reset to 0 for the other tests.
  test_configurator()->SetOnDemandTime(0);
  component_updater()->Stop();

  // Test a few error cases. NOTE: We don't have callbacks for
  // when the updates failed yet.
  EXPECT_TRUE(Mock::VerifyAndClearExpectations(&observer1));
  {
    InSequence seq;
    EXPECT_CALL(observer1,
                OnEvent(ComponentObserver::COMPONENT_UPDATER_STARTED, 0))
                .Times(1);
    EXPECT_CALL(observer1,
                OnEvent(ComponentObserver::COMPONENT_NOT_UPDATED, 0))
                .Times(1);
    EXPECT_CALL(observer1,
                OnEvent(ComponentObserver::COMPONENT_UPDATER_SLEEPING, 0))
                .Times(1);
  }
  EXPECT_TRUE(Mock::VerifyAndClearExpectations(&observer2));
  {
    InSequence seq;
    EXPECT_CALL(observer2,
                OnEvent(ComponentObserver::COMPONENT_UPDATER_STARTED, 0))
                .Times(1);
    EXPECT_CALL(observer2,
                OnEvent(ComponentObserver::COMPONENT_NOT_UPDATED, 0))
                .Times(1);
    EXPECT_CALL(observer2,
                OnEvent(ComponentObserver::COMPONENT_UPDATER_SLEEPING, 0))
                .Times(1);
  }

  // No update: error from no server response
  post_interceptor_->Reset();
  EXPECT_TRUE(post_interceptor_->ExpectRequest(new PartialMatch(
      "updatecheck"), test_file("updatecheck_reply_empty")));

  test_configurator()->SetLoopCount(1);
  component_updater()->Start();
  EXPECT_EQ(ComponentUpdateService::kOk,
            OnDemandTester::OnDemand(component_updater(),
                                     GetCrxComponentID(com2)));
  RunThreads();
  component_updater()->Stop();

  EXPECT_EQ(1, post_interceptor_->GetHitCount())
      << post_interceptor_->GetRequestsAsString();
  EXPECT_EQ(1, post_interceptor_->GetCount())
      << post_interceptor_->GetRequestsAsString();

  // No update: already updated to 1.0 so nothing new
  EXPECT_TRUE(Mock::VerifyAndClearExpectations(&observer1));
  {
    InSequence seq;
    EXPECT_CALL(observer1,
                OnEvent(ComponentObserver::COMPONENT_UPDATER_STARTED, 0))
                .Times(1);
    EXPECT_CALL(observer1,
                OnEvent(ComponentObserver::COMPONENT_NOT_UPDATED, 0))
                .Times(1);
    EXPECT_CALL(observer1,
                OnEvent(ComponentObserver::COMPONENT_UPDATER_SLEEPING, 0))
                .Times(1);
  }
  EXPECT_TRUE(Mock::VerifyAndClearExpectations(&observer2));
  {
    InSequence seq;
    EXPECT_CALL(observer2,
                OnEvent(ComponentObserver::COMPONENT_UPDATER_STARTED, 0))
                .Times(1);
    EXPECT_CALL(observer2,
                OnEvent(ComponentObserver::COMPONENT_NOT_UPDATED, 0))
                .Times(1);
    EXPECT_CALL(observer2,
                OnEvent(ComponentObserver::COMPONENT_UPDATER_SLEEPING, 0))
                .Times(1);
  }

  post_interceptor_->Reset();
  EXPECT_TRUE(post_interceptor_->ExpectRequest(new PartialMatch(
      "updatecheck"), test_file("updatecheck_reply_1.xml")));

  test_configurator()->SetLoopCount(1);
  component_updater()->Start();
  EXPECT_EQ(ComponentUpdateService::kOk,
            OnDemandTester::OnDemand(component_updater(),
                                     GetCrxComponentID(com2)));
  RunThreads();

  EXPECT_EQ(1, post_interceptor_->GetHitCount())
      << post_interceptor_->GetRequestsAsString();
  EXPECT_EQ(1, post_interceptor_->GetCount())
      << post_interceptor_->GetRequestsAsString();

  component_updater()->Stop();
}

// Verify that a previously registered component can get re-registered
// with a different version.
TEST_F(ComponentUpdaterTest, CheckReRegistration) {
  MockComponentObserver observer1;
  {
    InSequence seq;
    EXPECT_CALL(observer1,
                OnEvent(ComponentObserver::COMPONENT_UPDATER_STARTED, 0))
                .Times(1);
    EXPECT_CALL(observer1,
                OnEvent(ComponentObserver::COMPONENT_UPDATE_FOUND, 0))
                .Times(1);
    EXPECT_CALL(observer1,
                OnEvent(ComponentObserver::COMPONENT_UPDATE_READY, 0))
                .Times(1);
    EXPECT_CALL(observer1,
                OnEvent(ComponentObserver::COMPONENT_UPDATED, 0))
                .Times(1);
    EXPECT_CALL(observer1,
                OnEvent(ComponentObserver::COMPONENT_UPDATER_SLEEPING, 0))
                .Times(1);
    EXPECT_CALL(observer1,
                OnEvent(ComponentObserver::COMPONENT_NOT_UPDATED, 0))
                .Times(1);
    EXPECT_CALL(observer1,
                OnEvent(ComponentObserver::COMPONENT_UPDATER_SLEEPING, 0))
                .Times(1);
  }

  MockComponentObserver observer2;
  {
    InSequence seq;
    EXPECT_CALL(observer2,
                OnEvent(ComponentObserver::COMPONENT_UPDATER_STARTED, 0))
                .Times(1);
    EXPECT_CALL(observer2,
                OnEvent(ComponentObserver::COMPONENT_NOT_UPDATED, 0))
                .Times(1);
    EXPECT_CALL(observer2,
                OnEvent(ComponentObserver::COMPONENT_UPDATER_SLEEPING, 0))
                .Times(1);
    EXPECT_CALL(observer2,
                OnEvent(ComponentObserver::COMPONENT_NOT_UPDATED, 0))
                .Times(1);
    EXPECT_CALL(observer2,
                OnEvent(ComponentObserver::COMPONENT_UPDATER_SLEEPING, 0))
                .Times(1);
  }

  EXPECT_TRUE(post_interceptor_->ExpectRequest(new PartialMatch(
      "updatecheck"), test_file("updatecheck_reply_1.xml")));
  EXPECT_TRUE(post_interceptor_->ExpectRequest(new PartialMatch("event")));
  EXPECT_TRUE(post_interceptor_->ExpectRequest(new PartialMatch(
      "updatecheck"), test_file("updatecheck_reply_1.xml")));

  get_interceptor_->SetResponse(
      GURL(expected_crx_url),
      test_file("jebgalgnebhfojomionfpkfelancnnkf.crx"));

  TestInstaller installer1;
  CrxComponent com1;
  com1.observer = &observer1;
  RegisterComponent(&com1, kTestComponent_jebg, Version("0.9"), &installer1);
  TestInstaller installer2;
  CrxComponent com2;
  com2.observer = &observer2;
  RegisterComponent(&com2, kTestComponent_abag, Version("2.2"), &installer2);

  // Loop twice to issue two checks: (1) with original 0.9 version, update to
  // 1.0, and do the second check (2) with the updated 1.0 version.
  test_configurator()->SetLoopCount(2);
  component_updater()->Start();
  RunThreads();

  EXPECT_EQ(0, static_cast<TestInstaller*>(com1.installer)->error());
  EXPECT_EQ(1, static_cast<TestInstaller*>(com1.installer)->install_count());
  EXPECT_EQ(0, static_cast<TestInstaller*>(com2.installer)->error());
  EXPECT_EQ(0, static_cast<TestInstaller*>(com2.installer)->install_count());

  EXPECT_EQ(3, post_interceptor_->GetHitCount())
        << post_interceptor_->GetRequestsAsString();
  EXPECT_EQ(1, get_interceptor_->GetHitCount());

  EXPECT_NE(string::npos, post_interceptor_->GetRequests()[0].find(
      "<app appid=\"jebgalgnebhfojomionfpkfelancnnkf\" version=\"0.9\">"
      "<updatecheck /></app>"))
      << post_interceptor_->GetRequestsAsString();
  EXPECT_NE(string::npos, post_interceptor_->GetRequests()[1].find(
      "<app appid=\"jebgalgnebhfojomionfpkfelancnnkf\" "
      "version=\"0.9\" nextversion=\"1.0\">"
      "<event eventtype=\"3\" eventresult=\"1\"/>"))
      << post_interceptor_->GetRequestsAsString();
  EXPECT_NE(string::npos, post_interceptor_->GetRequests()[2].find(
      "<app appid=\"jebgalgnebhfojomionfpkfelancnnkf\" version=\"1.0\">"
      "<updatecheck /></app>"))
      << post_interceptor_->GetRequestsAsString();

  component_updater()->Stop();

  // Now re-register, pretending to be an even newer version (2.2)
  EXPECT_TRUE(Mock::VerifyAndClearExpectations(&observer1));
  {
    InSequence seq;
    EXPECT_CALL(observer1,
                OnEvent(ComponentObserver::COMPONENT_UPDATER_STARTED, 0))
                .Times(1);
    EXPECT_CALL(observer1,
                OnEvent(ComponentObserver::COMPONENT_NOT_UPDATED, 0))
                .Times(1);
    EXPECT_CALL(observer1,
                OnEvent(ComponentObserver::COMPONENT_UPDATER_SLEEPING, 0))
                .Times(1);
  }

  EXPECT_TRUE(Mock::VerifyAndClearExpectations(&observer2));
  {
    InSequence seq;
    EXPECT_CALL(observer2,
                OnEvent(ComponentObserver::COMPONENT_UPDATER_STARTED, 0))
                .Times(1);
    EXPECT_CALL(observer2,
                OnEvent(ComponentObserver::COMPONENT_NOT_UPDATED, 0))
                .Times(1);
    EXPECT_CALL(observer2,
                OnEvent(ComponentObserver::COMPONENT_UPDATER_SLEEPING, 0))
                .Times(1);
  }

  post_interceptor_->Reset();
  EXPECT_TRUE(post_interceptor_->ExpectRequest(new PartialMatch(
      "updatecheck"), test_file("updatecheck_reply_1.xml")));

  TestInstaller installer3;
  EXPECT_EQ(ComponentUpdateService::kReplaced,
            RegisterComponent(&com1,
                              kTestComponent_jebg,
                              Version("2.2"),
                              &installer3));

  // Loop once just to notice the check happening with the re-register version.
  test_configurator()->SetLoopCount(1);
  component_updater()->Start();
  RunThreads();

  // We created a new installer, so the counts go back to 0.
  EXPECT_EQ(0, static_cast<TestInstaller*>(com1.installer)->error());
  EXPECT_EQ(0, static_cast<TestInstaller*>(com1.installer)->install_count());
  EXPECT_EQ(0, static_cast<TestInstaller*>(com2.installer)->error());
  EXPECT_EQ(0, static_cast<TestInstaller*>(com2.installer)->install_count());

  // One update check and no additional pings are expected.
  EXPECT_EQ(1, post_interceptor_->GetHitCount())
        << post_interceptor_->GetRequestsAsString();
  EXPECT_EQ(1, post_interceptor_->GetCount())
        << post_interceptor_->GetRequestsAsString();

  EXPECT_NE(string::npos, post_interceptor_->GetRequests()[0].find(
      "<app appid=\"jebgalgnebhfojomionfpkfelancnnkf\" version=\"2.2\">"
      "<updatecheck /></app>"));

  component_updater()->Stop();
}

// Verify that we can download and install a component and a differential
// update to that component. We do three loops; the final loop should do
// nothing.
// We also check that exactly 5 non-ping network requests are issued:
// 1- update check (response: v1 available)
// 2- download crx (v1)
// 3- update check (response: v2 available)
// 4- download differential crx (v1 to v2)
// 5- update check (response: no further update available)
// There should be two pings, one for each update. The second will bear a
// diffresult=1, while the first will not.
TEST_F(ComponentUpdaterTest, DifferentialUpdate) {
  EXPECT_TRUE(post_interceptor_->ExpectRequest(new PartialMatch(
      "updatecheck"), test_file("updatecheck_diff_reply_1.xml")));
  EXPECT_TRUE(post_interceptor_->ExpectRequest(new PartialMatch("event")));
  EXPECT_TRUE(post_interceptor_->ExpectRequest(new PartialMatch(
      "updatecheck"), test_file("updatecheck_diff_reply_2.xml")));
  EXPECT_TRUE(post_interceptor_->ExpectRequest(new PartialMatch("event")));
  EXPECT_TRUE(post_interceptor_->ExpectRequest(new PartialMatch(
      "updatecheck"), test_file("updatecheck_diff_reply_3.xml")));

  get_interceptor_->SetResponse(
      GURL("http://localhost/download/ihfokbkgjpifnbbojhneepfflplebdkc_1.crx"),
      test_file("ihfokbkgjpifnbbojhneepfflplebdkc_1.crx"));
  get_interceptor_->SetResponse(
      GURL("http://localhost/download/"
           "ihfokbkgjpifnbbojhneepfflplebdkc_1to2.crx"),
      test_file("ihfokbkgjpifnbbojhneepfflplebdkc_1to2.crx"));

  VersionedTestInstaller installer;
  CrxComponent com;
  RegisterComponent(&com, kTestComponent_ihfo, Version("0.0"), &installer);

  test_configurator()->SetLoopCount(3);
  component_updater()->Start();
  RunThreads();

  EXPECT_EQ(0, static_cast<TestInstaller*>(com.installer)->error());
  EXPECT_EQ(2, static_cast<TestInstaller*>(com.installer)->install_count());

  EXPECT_EQ(5, post_interceptor_->GetHitCount())
      << post_interceptor_->GetRequestsAsString();
  EXPECT_EQ(5, post_interceptor_->GetCount())
      << post_interceptor_->GetRequestsAsString();
  EXPECT_EQ(2, get_interceptor_->GetHitCount());

  EXPECT_NE(string::npos, post_interceptor_->GetRequests()[0].find(
      "<app appid=\"ihfokbkgjpifnbbojhneepfflplebdkc\" version=\"0.0\">"
      "<updatecheck /></app>"))
      << post_interceptor_->GetRequestsAsString();
  EXPECT_NE(string::npos, post_interceptor_->GetRequests()[1].find(
      "<app appid=\"ihfokbkgjpifnbbojhneepfflplebdkc\" "
      "version=\"0.0\" nextversion=\"1.0\">"
      "<event eventtype=\"3\" eventresult=\"1\" nextfp=\"1\"/>"))
      << post_interceptor_->GetRequestsAsString();
  EXPECT_NE(string::npos, post_interceptor_->GetRequests()[2].find(
      "<app appid=\"ihfokbkgjpifnbbojhneepfflplebdkc\" version=\"1.0\">"
      "<updatecheck /><packages><package fp=\"1\"/></packages></app>"))
      << post_interceptor_->GetRequestsAsString();
  EXPECT_NE(string::npos, post_interceptor_->GetRequests()[3].find(
      "<app appid=\"ihfokbkgjpifnbbojhneepfflplebdkc\" "
      "version=\"1.0\" nextversion=\"2.0\">"
      "<event eventtype=\"3\" eventresult=\"1\" diffresult=\"1\" "
      "previousfp=\"1\" nextfp=\"22\"/>"))
      << post_interceptor_->GetRequestsAsString();
  EXPECT_NE(string::npos, post_interceptor_->GetRequests()[4].find(
      "<app appid=\"ihfokbkgjpifnbbojhneepfflplebdkc\" version=\"2.0\">"
      "<updatecheck /><packages><package fp=\"22\"/></packages></app>"))
      << post_interceptor_->GetRequestsAsString();
  component_updater()->Stop();
}

// Verify that component installation falls back to downloading and installing
// a full update if the differential update fails (in this case, because the
// installer does not know about the existing files). We do two loops; the final
// loop should do nothing.
// We also check that exactly 4 non-ping network requests are issued:
// 1- update check (loop 1)
// 2- download differential crx
// 3- download full crx
// 4- update check (loop 2 - no update available)
// There should be one ping for the first attempted update.
TEST_F(ComponentUpdaterTest, DifferentialUpdateFails) {
  EXPECT_TRUE(post_interceptor_->ExpectRequest(new PartialMatch(
      "updatecheck"), test_file("updatecheck_diff_reply_2.xml")));
  EXPECT_TRUE(post_interceptor_->ExpectRequest(new PartialMatch("event")));
  EXPECT_TRUE(post_interceptor_->ExpectRequest(new PartialMatch(
      "updatecheck"), test_file("updatecheck_diff_reply_3.xml")));

  get_interceptor_->SetResponse(
      GURL("http://localhost/download/ihfokbkgjpifnbbojhneepfflplebdkc_1.crx"),
      test_file("ihfokbkgjpifnbbojhneepfflplebdkc_1.crx"));
  get_interceptor_->SetResponse(
      GURL("http://localhost/download/"
           "ihfokbkgjpifnbbojhneepfflplebdkc_1to2.crx"),
      test_file("ihfokbkgjpifnbbojhneepfflplebdkc_1to2.crx"));
  get_interceptor_->SetResponse(
      GURL("http://localhost/download/ihfokbkgjpifnbbojhneepfflplebdkc_2.crx"),
      test_file("ihfokbkgjpifnbbojhneepfflplebdkc_2.crx"));

  TestInstaller installer;
  CrxComponent com;
  RegisterComponent(&com, kTestComponent_ihfo, Version("1.0"), &installer);

  test_configurator()->SetLoopCount(2);
  component_updater()->Start();
  RunThreads();

  // A failed differential update does not count as a failed install.
  EXPECT_EQ(0, static_cast<TestInstaller*>(com.installer)->error());
  EXPECT_EQ(1, static_cast<TestInstaller*>(com.installer)->install_count());

  EXPECT_EQ(3, post_interceptor_->GetHitCount())
      << post_interceptor_->GetRequestsAsString();
  EXPECT_EQ(3, post_interceptor_->GetCount())
      << post_interceptor_->GetRequestsAsString();
  EXPECT_EQ(2, get_interceptor_->GetHitCount());

  EXPECT_NE(string::npos, post_interceptor_->GetRequests()[0].find(
      "<app appid=\"ihfokbkgjpifnbbojhneepfflplebdkc\" version=\"1.0\">"
      "<updatecheck /></app>"))
      << post_interceptor_->GetRequestsAsString();
  EXPECT_NE(string::npos, post_interceptor_->GetRequests()[1].find(
      "<app appid=\"ihfokbkgjpifnbbojhneepfflplebdkc\" "
      "version=\"1.0\" nextversion=\"2.0\">"
      "<event eventtype=\"3\" eventresult=\"1\" diffresult=\"0\" "
      "differrorcat=\"2\" differrorcode=\"16\" nextfp=\"22\"/>"))
      << post_interceptor_->GetRequestsAsString();
  EXPECT_NE(string::npos, post_interceptor_->GetRequests()[2].find(
      "<app appid=\"ihfokbkgjpifnbbojhneepfflplebdkc\" version=\"2.0\">"
      "<updatecheck /><packages><package fp=\"22\"/></packages></app>"))
      << post_interceptor_->GetRequestsAsString();

  component_updater()->Stop();
}

// Verify that a failed installation causes an install failure ping.
TEST_F(ComponentUpdaterTest, CheckFailedInstallPing) {
  // This test installer reports installation failure.
  class : public TestInstaller {
    virtual bool Install(const base::DictionaryValue& manifest,
                         const base::FilePath& unpack_path) OVERRIDE {
      ++install_count_;
      base::DeleteFile(unpack_path, true);
      return false;
    }
  } installer;

  EXPECT_TRUE(post_interceptor_->ExpectRequest(new PartialMatch(
      "updatecheck"), test_file("updatecheck_reply_1.xml")));
  EXPECT_TRUE(post_interceptor_->ExpectRequest(new PartialMatch("event")));
  EXPECT_TRUE(post_interceptor_->ExpectRequest(new PartialMatch(
      "updatecheck"), test_file("updatecheck_reply_1.xml")));
  EXPECT_TRUE(post_interceptor_->ExpectRequest(new PartialMatch("event")));
  get_interceptor_->SetResponse(
      GURL(expected_crx_url),
      test_file("jebgalgnebhfojomionfpkfelancnnkf.crx"));

  // Start with 0.9, and attempt update to 1.0.
  // Loop twice to issue two checks: (1) with original 0.9 version
  // and (2), which should retry with 0.9.
  CrxComponent com;
  RegisterComponent(&com, kTestComponent_jebg, Version("0.9"), &installer);

  test_configurator()->SetLoopCount(2);
  component_updater()->Start();
  RunThreads();

  EXPECT_EQ(4, post_interceptor_->GetHitCount())
      << post_interceptor_->GetRequestsAsString();
  EXPECT_EQ(2, get_interceptor_->GetHitCount());

  EXPECT_NE(string::npos, post_interceptor_->GetRequests()[0].find(
      "<app appid=\"jebgalgnebhfojomionfpkfelancnnkf\" version=\"0.9\">"
      "<updatecheck /></app>"))
      << post_interceptor_->GetRequestsAsString();
  EXPECT_NE(string::npos, post_interceptor_->GetRequests()[1].find(
      "<app appid=\"jebgalgnebhfojomionfpkfelancnnkf\" "
      "version=\"0.9\" nextversion=\"1.0\">"
      "<event eventtype=\"3\" eventresult=\"0\" "
      "errorcat=\"3\" errorcode=\"9\"/>"))
      << post_interceptor_->GetRequestsAsString();
  EXPECT_NE(string::npos, post_interceptor_->GetRequests()[2].find(
      "<app appid=\"jebgalgnebhfojomionfpkfelancnnkf\" version=\"0.9\">"
      "<updatecheck /></app>"))
      << post_interceptor_->GetRequestsAsString();
  EXPECT_NE(string::npos, post_interceptor_->GetRequests()[3].find(
      "<app appid=\"jebgalgnebhfojomionfpkfelancnnkf\" "
      "version=\"0.9\" nextversion=\"1.0\">"
      "<event eventtype=\"3\" eventresult=\"0\" "
      "errorcat=\"3\" errorcode=\"9\"/>"))
      << post_interceptor_->GetRequestsAsString();

  // Loop once more, but expect no ping because a noupdate response is issued.
  // This is necessary to clear out the fire-and-forget ping from the previous
  // iteration.
  post_interceptor_->Reset();
  EXPECT_TRUE(post_interceptor_->ExpectRequest(new PartialMatch(
      "updatecheck"), test_file("updatecheck_reply_noupdate.xml")));

  test_configurator()->SetLoopCount(1);
  component_updater()->Start();
  RunThreads();

  EXPECT_EQ(0, static_cast<TestInstaller*>(com.installer)->error());
  EXPECT_EQ(2, static_cast<TestInstaller*>(com.installer)->install_count());

  EXPECT_EQ(1, post_interceptor_->GetHitCount())
      << post_interceptor_->GetRequestsAsString();
  EXPECT_EQ(1, post_interceptor_->GetCount())
      << post_interceptor_->GetRequestsAsString();

  EXPECT_NE(string::npos, post_interceptor_->GetRequests()[0].find(
      "<app appid=\"jebgalgnebhfojomionfpkfelancnnkf\" version=\"0.9\">"
      "<updatecheck /></app>"))
      << post_interceptor_->GetRequestsAsString();

  component_updater()->Stop();
}

// Verify that we successfully propagate a patcher error.
// ihfokbkgjpifnbbojhneepfflplebdkc_1to2_bad.crx contains an incorrect
// patching instruction that should fail.
TEST_F(ComponentUpdaterTest, DifferentialUpdateFailErrorcode) {
  EXPECT_TRUE(post_interceptor_->ExpectRequest(new PartialMatch(
      "updatecheck"), test_file("updatecheck_diff_reply_1.xml")));
  EXPECT_TRUE(post_interceptor_->ExpectRequest(new PartialMatch("event")));
  EXPECT_TRUE(post_interceptor_->ExpectRequest(new PartialMatch(
      "updatecheck"), test_file("updatecheck_diff_reply_2.xml")));
  EXPECT_TRUE(post_interceptor_->ExpectRequest(new PartialMatch("event")));
  EXPECT_TRUE(post_interceptor_->ExpectRequest(new PartialMatch(
      "updatecheck"), test_file("updatecheck_diff_reply_3.xml")));

  get_interceptor_->SetResponse(
      GURL("http://localhost/download/ihfokbkgjpifnbbojhneepfflplebdkc_1.crx"),
      test_file("ihfokbkgjpifnbbojhneepfflplebdkc_1.crx"));
  // This intercept returns a different file than what is specified in the
  // update check response and requested in the download. The file that is
  // actually dowloaded contains a patching error, an therefore, an error
  // is injected at the time of patching.
  get_interceptor_->SetResponse(
      GURL("http://localhost/download/"
           "ihfokbkgjpifnbbojhneepfflplebdkc_1to2.crx"),
      test_file("ihfokbkgjpifnbbojhneepfflplebdkc_1to2_bad.crx"));
  get_interceptor_->SetResponse(
      GURL("http://localhost/download/ihfokbkgjpifnbbojhneepfflplebdkc_2.crx"),
      test_file("ihfokbkgjpifnbbojhneepfflplebdkc_2.crx"));

  VersionedTestInstaller installer;
  CrxComponent com;
  RegisterComponent(&com, kTestComponent_ihfo, Version("0.0"), &installer);

  test_configurator()->SetLoopCount(3);
  component_updater()->Start();
  RunThreads();
  component_updater()->Stop();

  EXPECT_EQ(0, static_cast<TestInstaller*>(com.installer)->error());
  EXPECT_EQ(2, static_cast<TestInstaller*>(com.installer)->install_count());

  EXPECT_EQ(5, post_interceptor_->GetHitCount())
      << post_interceptor_->GetRequestsAsString();
  EXPECT_EQ(5, post_interceptor_->GetCount())
      << post_interceptor_->GetRequestsAsString();
  EXPECT_EQ(3, get_interceptor_->GetHitCount());

  EXPECT_NE(string::npos, post_interceptor_->GetRequests()[0].find(
      "<app appid=\"ihfokbkgjpifnbbojhneepfflplebdkc\" version=\"0.0\">"
      "<updatecheck /></app>"))
      << post_interceptor_->GetRequestsAsString();
  EXPECT_NE(string::npos, post_interceptor_->GetRequests()[1].find(
      "<app appid=\"ihfokbkgjpifnbbojhneepfflplebdkc\" "
      "version=\"0.0\" nextversion=\"1.0\">"
      "<event eventtype=\"3\" eventresult=\"1\" nextfp=\"1\"/>"))
      << post_interceptor_->GetRequestsAsString();
  EXPECT_NE(string::npos, post_interceptor_->GetRequests()[2].find(
      "<app appid=\"ihfokbkgjpifnbbojhneepfflplebdkc\" version=\"1.0\">"
      "<updatecheck /><packages><package fp=\"1\"/></packages></app>"))
      << post_interceptor_->GetRequestsAsString();
  EXPECT_NE(string::npos, post_interceptor_->GetRequests()[3].find(
      "<app appid=\"ihfokbkgjpifnbbojhneepfflplebdkc\" "
      "version=\"1.0\" nextversion=\"2.0\">"
      "<event eventtype=\"3\" eventresult=\"1\" "
      "diffresult=\"0\" differrorcat=\"2\" "
      "differrorcode=\"14\" diffextracode1=\"305\" "
      "previousfp=\"1\" nextfp=\"22\"/>"))
      << post_interceptor_->GetRequestsAsString();
  EXPECT_NE(string::npos, post_interceptor_->GetRequests()[4].find(
      "<app appid=\"ihfokbkgjpifnbbojhneepfflplebdkc\" version=\"2.0\">"
      "<updatecheck /><packages><package fp=\"22\"/></packages></app>"))
      << post_interceptor_->GetRequestsAsString();
}

class TestResourceController : public content::ResourceController {
 public:
  virtual void SetThrottle(content::ResourceThrottle* throttle) {}
};

content::ResourceThrottle* RequestTestResourceThrottle(
    ComponentUpdateService* cus,
    TestResourceController* controller,
    const char* crx_id) {

  net::TestURLRequestContext context;
  net::TestURLRequest url_request(
      GURL("http://foo.example.com/thing.bin"),
      net::DEFAULT_PRIORITY,
      NULL,
      &context);

  content::ResourceThrottle* rt =
      cus->GetOnDemandResourceThrottle(&url_request, crx_id);
  rt->set_controller_for_testing(controller);
  controller->SetThrottle(rt);
  return rt;
}

void RequestAndDeleteResourceThrottle(
    ComponentUpdateService* cus, const char* crx_id) {
  // By requesting a throttle and deleting it immediately we ensure that we
  // hit the case where the component updater tries to use the weak
  // pointer to a dead Resource throttle.
  class  NoCallResourceController : public TestResourceController {
   public:
    virtual ~NoCallResourceController() {}
    virtual void Cancel() OVERRIDE { CHECK(false); }
    virtual void CancelAndIgnore() OVERRIDE { CHECK(false); }
    virtual void CancelWithError(int error_code) OVERRIDE { CHECK(false); }
    virtual void Resume() OVERRIDE { CHECK(false); }
  } controller;

  delete RequestTestResourceThrottle(cus, &controller, crx_id);
}

TEST_F(ComponentUpdaterTest, ResourceThrottleDeletedNoUpdate) {
  MockComponentObserver observer;
  EXPECT_CALL(observer,
              OnEvent(ComponentObserver::COMPONENT_UPDATER_STARTED, 0))
              .Times(1);
  EXPECT_CALL(observer,
              OnEvent(ComponentObserver::COMPONENT_UPDATER_SLEEPING, 0))
              .Times(1);
  EXPECT_CALL(observer,
              OnEvent(ComponentObserver::COMPONENT_NOT_UPDATED, 0))
              .Times(1);

  EXPECT_TRUE(post_interceptor_->ExpectRequest(new PartialMatch(
      "updatecheck"), test_file("updatecheck_reply_1.xml")));

  TestInstaller installer;
  CrxComponent com;
  com.observer = &observer;
  EXPECT_EQ(ComponentUpdateService::kOk,
            RegisterComponent(&com,
                              kTestComponent_abag,
                              Version("1.1"),
                              &installer));
  // The following two calls ensure that we don't do an update check via the
  // timer, so the only update check should be the on-demand one.
  test_configurator()->SetInitialDelay(1000000);
  test_configurator()->SetRecheckTime(1000000);
  test_configurator()->SetLoopCount(1);
  component_updater()->Start();

  RunThreadsUntilIdle();

  EXPECT_EQ(0, post_interceptor_->GetHitCount());

  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&RequestAndDeleteResourceThrottle,
                 component_updater(),
                 "abagagagagagagagagagagagagagagag"));

  RunThreads();

  EXPECT_EQ(1, post_interceptor_->GetHitCount());
  EXPECT_EQ(0, static_cast<TestInstaller*>(com.installer)->error());
  EXPECT_EQ(0, static_cast<TestInstaller*>(com.installer)->install_count());

  component_updater()->Stop();
}

class  CancelResourceController: public TestResourceController {
  public:
  CancelResourceController() : throttle_(NULL), resume_called_(0) {}
  virtual ~CancelResourceController() {
    // Check that the throttle has been resumed by the time we
    // exit the test.
    CHECK(resume_called_ == 1);
    delete throttle_;
  }
  virtual void Cancel() OVERRIDE { CHECK(false); }
  virtual void CancelAndIgnore() OVERRIDE { CHECK(false); }
  virtual void CancelWithError(int error_code) OVERRIDE { CHECK(false); }
  virtual void Resume() OVERRIDE {
    BrowserThread::PostTask(
        BrowserThread::IO,
        FROM_HERE,
        base::Bind(&CancelResourceController::ResumeCalled,
                    base::Unretained(this)));
  }
  virtual void SetThrottle(content::ResourceThrottle* throttle) OVERRIDE {
    throttle_ = throttle;
    bool defer = false;
    // Initially the throttle is blocked. The CUS needs to run a
    // task on the UI thread to  decide if it should unblock.
    throttle_->WillStartRequest(&defer);
    CHECK(defer);
  }

  private:
  void ResumeCalled() { ++resume_called_; }

  content::ResourceThrottle* throttle_;
  int resume_called_;
};

TEST_F(ComponentUpdaterTest, ResourceThrottleLiveNoUpdate) {
  MockComponentObserver observer;
  EXPECT_CALL(observer,
              OnEvent(ComponentObserver::COMPONENT_UPDATER_STARTED, 0))
              .Times(1);
  EXPECT_CALL(observer,
              OnEvent(ComponentObserver::COMPONENT_UPDATER_SLEEPING, 0))
              .Times(1);
  EXPECT_CALL(observer,
              OnEvent(ComponentObserver::COMPONENT_NOT_UPDATED, 0))
              .Times(1);

  EXPECT_TRUE(post_interceptor_->ExpectRequest(new PartialMatch(
      "updatecheck"), test_file("updatecheck_reply_1.xml")));

  TestInstaller installer;
  CrxComponent com;
  com.observer = &observer;
  EXPECT_EQ(ComponentUpdateService::kOk,
            RegisterComponent(&com,
                              kTestComponent_abag,
                              Version("1.1"),
                              &installer));
  // The following two calls ensure that we don't do an update check via the
  // timer, so the only update check should be the on-demand one.
  test_configurator()->SetInitialDelay(1000000);
  test_configurator()->SetRecheckTime(1000000);
  test_configurator()->SetLoopCount(1);
  component_updater()->Start();

  RunThreadsUntilIdle();

  EXPECT_EQ(0, post_interceptor_->GetHitCount());

  CancelResourceController controller;

  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(base::IgnoreResult(&RequestTestResourceThrottle),
                 component_updater(),
                 &controller,
                 "abagagagagagagagagagagagagagagag"));

  RunThreads();

  EXPECT_EQ(1, post_interceptor_->GetHitCount());
  EXPECT_EQ(0, static_cast<TestInstaller*>(com.installer)->error());
  EXPECT_EQ(0, static_cast<TestInstaller*>(com.installer)->install_count());

  component_updater()->Stop();
}


}  // namespace component_updater

