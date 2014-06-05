// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_WEBRTC_BROWSERTEST_BASE_H_
#define CHROME_BROWSER_MEDIA_WEBRTC_BROWSERTEST_BASE_H_

#include <string>

#include "chrome/test/base/in_process_browser_test.h"

class InfoBar;

namespace content {
class WebContents;
}

// Base class for WebRTC browser tests with useful primitives for interacting
// getUserMedia. We use inheritance here because it makes the test code look
// as clean as it can be.
class WebRtcTestBase : public InProcessBrowserTest {
 protected:
  // Typical constraints.
  static const char kAudioVideoCallConstraints[];
  static const char kAudioOnlyCallConstraints[];
  static const char kVideoOnlyCallConstraints[];

  static const char kFailedWithPermissionDeniedError[];

  WebRtcTestBase();
  virtual ~WebRtcTestBase();

  // These all require that the loaded page fulfills the public interface in
  // chrome/test/data/webrtc/message_handling.js.
  void GetUserMediaAndAccept(content::WebContents* tab_contents) const;
  void GetUserMediaWithSpecificConstraintsAndAccept(
      content::WebContents* tab_contents,
      const std::string& constraints) const;
  void GetUserMediaAndDeny(content::WebContents* tab_contents);
  void GetUserMediaWithSpecificConstraintsAndDeny(
      content::WebContents* tab_contents,
      const std::string& constraints) const;
  void GetUserMediaAndDismiss(content::WebContents* tab_contents) const;
  void GetUserMedia(content::WebContents* tab_contents,
                    const std::string& constraints) const;

  // Convenience method which opens the page at url, calls GetUserMediaAndAccept
  // and returns the new tab.
  content::WebContents* OpenPageAndGetUserMediaInNewTab(const GURL& url) const;

  // Opens the page at |url| where getUserMedia has been invoked through other
  // means and accepts the user media request.
  content::WebContents* OpenPageAndAcceptUserMedia(const GURL& url) const;

  void ConnectToPeerConnectionServer(const std::string& peer_name,
                                     content::WebContents* tab_contents) const;
  std::string ExecuteJavascript(const std::string& javascript,
                                content::WebContents* tab_contents) const;

  // Call this to enable monitoring of javascript errors for this test method.
  // This will only work if the tests are run sequentially by the test runner
  // (i.e. with --test-launcher-developer-mode or --test-launcher-jobs=1).
  void DetectErrorsInJavaScript();

 private:
  void CloseInfoBarInTab(content::WebContents* tab_contents,
                         InfoBar* infobar) const;
  InfoBar* GetUserMediaAndWaitForInfoBar(content::WebContents* tab_contents,
                                         const std::string& constraints) const;

  bool detect_errors_in_javascript_;

  DISALLOW_COPY_AND_ASSIGN(WebRtcTestBase);
};

#endif  // CHROME_BROWSER_MEDIA_WEBRTC_BROWSERTEST_BASE_H_
