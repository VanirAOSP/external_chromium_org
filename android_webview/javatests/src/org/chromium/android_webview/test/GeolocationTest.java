// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.test.suitebuilder.annotation.MediumTest;
import android.webkit.GeolocationPermissions;

import org.chromium.android_webview.AwContents;
import org.chromium.base.test.util.Feature;
import org.chromium.content.browser.LocationProviderFactory;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.MockLocationProvider;

/**
 * Test suite for Geolocation in AwContents. Smoke tests for
 * basic functionality, and tests to ensure the AwContents.onPause
 * and onResume APIs affect Geolocation as expected.
 */
public class GeolocationTest extends AwTestBase {

    private static final long TEST_TIMEOUT_MS = 5000L;
    private static final int CHECK_INTERVAL_MS = 100;

    private TestAwContentsClient mContentsClient;
    private AwContents mAwContents;
    private MockLocationProvider mMockLocationProvider;

    private static final String RAW_HTML =
            "<!DOCTYPE html>\n" +
            "<html>\n" +
            "  <head>\n" +
            "    <title>Geolocation</title>\n" +
            "    <script>\n" +
            "      var positionCount = 0;\n" +
            "      function gotPos(position) {\n" +
            "        positionCount++;\n" +
            "      }\n" +
            "      function initiate_getCurrentPosition() {\n" +
            "        navigator.geolocation.getCurrentPosition(\n" +
            "            gotPos, function() { }, { });\n" +
            "      }\n" +
            "      function initiate_watchPosition() {\n" +
            "        navigator.geolocation.watchPosition(\n" +
            "            gotPos, function() { }, { });\n" +
            "      }\n" +
            "    </script>\n" +
            "  </head>\n" +
            "  <body>\n" +
            "  </body>\n" +
            "</html>";

    @Override
    public void setUp() throws Exception {
        super.setUp();
        mContentsClient = new TestAwContentsClient() {
                @Override
                public void onGeolocationPermissionsShowPrompt(String origin,
                        GeolocationPermissions.Callback callback) {
                    callback.invoke(origin, true, true);
                }
        };
        mAwContents = createAwTestContainerViewOnMainSync(mContentsClient).getAwContents();
        enableJavaScriptOnUiThread(mAwContents);
        setupGeolocation();
    }

    @Override
    public void tearDown() throws Exception {
        mMockLocationProvider.stopUpdates();
        GeolocationPermissions.getInstance().clearAll();
        super.tearDown();
    }

    private void setupGeolocation() {
        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                mAwContents.getSettings().setGeolocationEnabled(true);
            }
        });
        mMockLocationProvider = new MockLocationProvider();
        LocationProviderFactory.setLocationProviderImpl(mMockLocationProvider);
    }

    private int getPositionCountFromJS() {
        int result = -1;
        try {
            result = Integer.parseInt(executeJavaScriptAndWaitForResult(
                    mAwContents, mContentsClient, "positionCount"));
        } catch (Exception e) {
            fail("Unable to get positionCount");
        }
        return result;
    }

    private void ensureGeolocationRunning(final boolean running) throws Exception {
        assertTrue(CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return mMockLocationProvider.isRunning() == running;
            }
        }, TEST_TIMEOUT_MS, CHECK_INTERVAL_MS));
    }


    /**
     * Ensure that a call to navigator.getCurrentPosition works in WebView.
     */
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testGetPosition() throws Throwable {
        loadDataSync(mAwContents, mContentsClient.getOnPageFinishedHelper(),
                RAW_HTML, "text/html", false);

        mAwContents.evaluateJavaScript("initiate_getCurrentPosition();", null);

        assertTrue(CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
               return getPositionCountFromJS() == 1;
            }
        }, TEST_TIMEOUT_MS, CHECK_INTERVAL_MS));

        mAwContents.evaluateJavaScript("initiate_getCurrentPosition();", null);
        assertTrue(CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return getPositionCountFromJS() == 2;
            }
        }, TEST_TIMEOUT_MS, CHECK_INTERVAL_MS));
    }

    /**
     * Ensure that a call to navigator.watchPosition works in WebView.
     */
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testWatchPosition() throws Throwable {
        loadDataSync(mAwContents, mContentsClient.getOnPageFinishedHelper(),
                RAW_HTML, "text/html", false);

        mAwContents.evaluateJavaScript("initiate_watchPosition();", null);

        assertTrue(CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return getPositionCountFromJS() > 1;
            }
        }, TEST_TIMEOUT_MS, CHECK_INTERVAL_MS));
    }

    @MediumTest
    @Feature({"AndroidWebView"})
    public void testPauseGeolocationOnPause() throws Throwable {
        // Start a watch going.
        loadDataSync(mAwContents, mContentsClient.getOnPageFinishedHelper(),
                RAW_HTML, "text/html", false);

        mAwContents.evaluateJavaScript("initiate_watchPosition();", null);

        assertTrue(CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return getPositionCountFromJS() > 1;
            }
        }, TEST_TIMEOUT_MS, CHECK_INTERVAL_MS));

        ensureGeolocationRunning(true);

        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                mAwContents.onPause();
            }
        });

        ensureGeolocationRunning(false);

        try {
            executeJavaScriptAndWaitForResult(mAwContents, mContentsClient, "positionCount = 0");
        } catch (Exception e) {
            fail("Unable to clear positionCount");
        }
        assertEquals(0, getPositionCountFromJS());

        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                mAwContents.onResume();
            }
        });

        ensureGeolocationRunning(true);

        assertTrue(CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return getPositionCountFromJS() > 1;
            }
        }, TEST_TIMEOUT_MS, CHECK_INTERVAL_MS));
    }

    @MediumTest
    @Feature({"AndroidWebView"})
    public void testPauseAwContentsBeforeNavigating() throws Throwable {
        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                mAwContents.onPause();
            }
        });

        // Start a watch going.
        loadDataSync(mAwContents, mContentsClient.getOnPageFinishedHelper(),
                RAW_HTML, "text/html", false);

        mAwContents.evaluateJavaScript("initiate_watchPosition();", null);

        assertEquals(0, getPositionCountFromJS());

        ensureGeolocationRunning(false);

        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                mAwContents.onResume();
            }
        });

        ensureGeolocationRunning(true);

        assertTrue(CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return getPositionCountFromJS() > 1;
            }
        }, TEST_TIMEOUT_MS, CHECK_INTERVAL_MS));

    }

    @MediumTest
    @Feature({"AndroidWebView"})
    public void testResumeWhenNotStarted() throws Throwable {
        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                mAwContents.onPause();
            }
        });

        loadDataSync(mAwContents, mContentsClient.getOnPageFinishedHelper(),
                RAW_HTML, "text/html", false);

        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                mAwContents.onResume();
            }
        });

        ensureGeolocationRunning(false);
    }

}
