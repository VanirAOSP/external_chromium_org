// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.appmenu;

import android.view.GestureDetector;
import android.view.GestureDetector.SimpleOnGestureListener;
import android.view.MotionEvent;
import android.view.View;
import android.view.View.OnTouchListener;

import org.chromium.chrome.browser.UmaBridge;

/**
 * A helper class for a menu button to decide when to show the app menu and forward touch
 * events.
 *
 * Simply construct this class and pass the class instance to a menu button as TouchListener.
 * Then this class will handle everything regarding showing app menu for you.
 */
public class AppMenuButtonHelper extends SimpleOnGestureListener implements OnTouchListener {
    private final View mMenuButton;
    private final AppMenuHandler mMenuHandler;
    private final GestureDetector mAppMenuGestureDetector;
    private boolean mSeenFirstScrollEvent;
    private Runnable mOnAppMenuShownListener;

    /**
     * @param menuButton  Menu button instance that will trigger the app menu.
     * @param menuHandler MenuHandler implementation that can show and get the app menu.
     */
    public AppMenuButtonHelper(View menuButton, AppMenuHandler menuHandler) {
        mMenuButton = menuButton;
        mMenuHandler = menuHandler;
        mAppMenuGestureDetector = new GestureDetector(menuButton.getContext(), this);
        mAppMenuGestureDetector.setIsLongpressEnabled(false);
    }

    /**
     * @param onAppMenuShownListener This is called when the app menu is shown by this class.
     */
    public void setOnAppMenuShownListener(Runnable onAppMenuShownListener) {
        mOnAppMenuShownListener = onAppMenuShownListener;
    }

    /**
     * Shows the app menu if it is not already shown.
     * @param startDragging Whether dragging is started.
     * @return Whether or not if the app menu is successfully shown.
     */
    private boolean showAppMenu(boolean startDragging) {
        if (!mMenuHandler.isAppMenuShowing() &&
                mMenuHandler.showAppMenu(mMenuButton, false, startDragging)) {
            UmaBridge.usingMenu(false, startDragging);
            if (mOnAppMenuShownListener != null) {
                mOnAppMenuShownListener.run();
            }
            return true;
        }
        return false;
    }

    /**
     * @return Whether the App Menu is currently showing.
     */
    public boolean isAppMenuShowing() {
        return mMenuHandler.isAppMenuShowing();
    }

    /**
     * Handle the key press event on a menu button.
     * @return Whether the app menu was shown as a result of this action.
     */
    public boolean onEnterKeyPress() {
        return showAppMenu(false);
    }

    @Override
    public boolean onDown(MotionEvent e) {
        mSeenFirstScrollEvent = false;
        mMenuButton.setPressed(true);

        return true;
    }

    @Override
    public boolean onSingleTapUp(MotionEvent e) {
        // A single tap shows the app menu, but dragging disabled.
        return showAppMenu(false);
    }

    @Override
    public boolean onScroll(MotionEvent e1, MotionEvent e2, float distanceX, float distanceY) {
        if (mSeenFirstScrollEvent) return false;
        mSeenFirstScrollEvent = true;

        // If the scrolling direction is roughly down on the first onScroll detection,
        // we consider it as dragging start, so shows the app menu. Otherwise, we
        // don't show menu so that toolbar horizontal swiping can happen.
        return -distanceY >= Math.abs(distanceX) && showAppMenu(true);
    }

    @Override
    public boolean onTouch(View view, MotionEvent event) {
        if (event.getActionMasked() == MotionEvent.ACTION_CANCEL ||
                event.getActionMasked() == MotionEvent.ACTION_UP) {
            mMenuButton.setPressed(false);
        }

        // This will take care of showing app menu.
        boolean isTouchEventConsumed = mAppMenuGestureDetector.onTouchEvent(event);

        // If user starts to drag on this menu button, ACTION_DOWN and all the subsequent touch
        // events are received here. We need to forward this event to the app menu to handle
        // dragging correctly.
        AppMenu menu = mMenuHandler.getAppMenu();
        if (menu != null && !isTouchEventConsumed) {
            isTouchEventConsumed |= menu.handleDragging(event);
        }
        return isTouchEventConsumed;
    }
}