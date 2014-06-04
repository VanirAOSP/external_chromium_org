// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util;

import android.view.ContextMenu;

import org.chromium.chrome.browser.EmptyTabObserver;
import org.chromium.chrome.browser.TabBase;
import org.chromium.content.browser.ContentViewClient;
import org.chromium.content.browser.test.util.CallbackHelper;
import org.chromium.content.browser.test.util.TestCallbackHelperContainer;
import org.chromium.content.browser.test.util.TestContentViewClient;
import org.chromium.content.browser.test.util.TestContentViewClientWrapper;
import org.chromium.content.browser.test.util.TestWebContentsObserver;

import java.lang.ref.WeakReference;

/**
 * A utility class that contains methods generic to all Tabs tests.
 */
public class TabBaseTabUtils {
    private static TestContentViewClient createTestContentViewClientForTab(TabBase tab) {
        ContentViewClient client = tab.getContentView().getContentViewClient();
        if (client instanceof TestContentViewClient) return (TestContentViewClient) client;

        TestContentViewClient testClient = new TestContentViewClientWrapper(client);
        tab.getContentView().setContentViewClient(testClient);
        return testClient;
    }

    public static class TestCallbackHelperContainerForTab extends TestCallbackHelperContainer {
        private final OnCloseTabHelper mOnCloseTabHelper;
        private final OnContextMenuShownHelper mOnContextMenuShownHelper;

        public TestCallbackHelperContainerForTab(TabBase tab) {
            super(createTestContentViewClientForTab(tab),
                    new TestWebContentsObserver(tab.getContentView().getContentViewCore()));
            mOnCloseTabHelper = new OnCloseTabHelper();
            mOnContextMenuShownHelper = new OnContextMenuShownHelper();
            tab.addObserver(new EmptyTabObserver() {
                @Override
                public void onDestroyed(TabBase tab) {
                    mOnCloseTabHelper.notifyCalled();
                }

                @Override
                public void onContextMenuShown(TabBase tab, ContextMenu menu) {
                    mOnContextMenuShownHelper.notifyCalled(menu);
                }
            });
        }

        public static class OnCloseTabHelper extends CallbackHelper {
        }

        public static class OnContextMenuShownHelper extends CallbackHelper {
            private WeakReference<ContextMenu> mContextMenu;

            public void notifyCalled(ContextMenu menu) {
                mContextMenu = new WeakReference<ContextMenu>(menu);
                notifyCalled();
            }

            public ContextMenu getContextMenu() {
                assert getCallCount() > 0;
                return mContextMenu.get();
            }
        }

        public OnCloseTabHelper getOnCloseTabHelper() {
            return mOnCloseTabHelper;
        }

        public OnContextMenuShownHelper getOnContextMenuShownHelper() {
            return mOnContextMenuShownHelper;
        }
    }

    /**
     * Creates, binds and returns a TestCallbackHelperContainer for a given Tab.
     */
    public static TestCallbackHelperContainerForTab getTestCallbackHelperContainer(TabBase tab) {
        return tab == null ? null : new TestCallbackHelperContainerForTab(tab);
    }
}
