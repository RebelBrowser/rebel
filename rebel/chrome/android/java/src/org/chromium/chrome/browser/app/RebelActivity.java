// Copyright 2022 Viasat Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app;

import android.text.TextUtils;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.bookmarks.BookmarkUtils;
import org.chromium.chrome.browser.download.DownloadOpenSource;
import org.chromium.chrome.browser.download.DownloadUtils;
import org.chromium.chrome.browser.init.AsyncInitializationActivity;
import org.chromium.chrome.browser.ntp.RemoteNtpBridge;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabObserver;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.PageTransition;
import org.chromium.url.GURL;

import java.net.URI;

/**
 * Rebel-specific actions for a ChromeActivity.
 */
public abstract class RebelActivity extends AsyncInitializationActivity {
    private RemoteNtpBridge mRemoteNtpBridge;

    private TabModelSelectorTabObserver mTabModelSelectorTabObserver;
    private TabModelSelectorTabModelObserver mTabModelObserver;

    protected RebelActivity() {}

    /**
     * Load a chrome:// URL, requested by the RemoteNTP bridge. Some URLs that
     * load on desktop are actually native pages on Android. Support a specific
     * set of of these URLs, but also allow navigating to WebUI pages.
     */
    public void loadInternalUrl(String url) {
        ChromeActivity activity = (ChromeActivity) this;
        URI uri = null;

        try {
            uri = new URI(url);
        } catch (Exception e) {
            return;
        }

        String host = uri.getHost();

        if (TextUtils.equals(host, "settings")) {
            activity.onOptionsItemSelected(R.id.preferences_id, null);
        } else if (TextUtils.equals(host, "history")) {
            activity.onOptionsItemSelected(R.id.open_history_menu_id, null);
        } else if (TextUtils.equals(host, "bookmarks")) {
            BookmarkUtils.showBookmarkManager(activity, false);
        } else if (TextUtils.equals(host, "downloads")) {
            DownloadUtils.showDownloadManager(
                    activity, activity.getActivityTab(), null, DownloadOpenSource.NEW_TAB_PAGE);
        } else {
            loadUrl(url, PageTransition.LINK);
        }
    }

    public void loadUrl(String url, int transition_type) {
        LoadUrlParams params = new LoadUrlParams(url);
        params.setTransitionType(transition_type);

        ChromeActivity activity = (ChromeActivity) this;
        activity.getActivityTab().loadUrl(params);
    }

    /**
     * Triggered by ChromeActivity after the native library has initialized. Set
     * up the tab observers.
     */
    @Override
    public void finishNativeInitialization() {
        ChromeActivity activity = (ChromeActivity) this;
        TabModelSelector tabModelSelector = activity.getTabModelSelector();

        mTabModelObserver = new TabModelSelectorTabModelObserver(tabModelSelector) {
            @Override
            public void didAddTab(Tab tab, @TabLaunchType int type, int creationState) {
                reinitNativeBridges(tab);
            }

            @Override
            public void didSelectTab(Tab tab, @TabSelectionType int type, int lastId) {
                reinitNativeBridges(tab);
            }

            @Override
            public void tabClosureCommitted(Tab tab) {
                destroyNativeBridges(tab);
            }
        };

        mTabModelSelectorTabObserver = new TabModelSelectorTabObserver(tabModelSelector) {
            @Override
            public void onPageLoadStarted(Tab tab, GURL url) {
                reinitNativeBridges(tab, url.getValidSpecOrEmpty());
            }

            @Override
            public void onShown(Tab tab, @TabSelectionType int type) {
                reinitNativeBridges(tab);
            }

            @Override
            public void onDestroyed(Tab tab) {
                destroyNativeBridges(tab);
            }
        };

        super.finishNativeInitialization();
    }

    /**
     * Triggered by ChromeActivity after the native library has resumed. Re-
     * intialize the native bridgs.
     */
    @Override
    public void onResumeWithNative() {
        super.onResumeWithNative();

        ChromeActivity activity = (ChromeActivity) this;
        Tab tab = activity.getActivityTab();

        if (tab != null) {
            reinitNativeBridges(tab);
        }
    }

    /**
     * Triggered by ChromeActivity on app shutdown. Destroy any open observers
     * and bridges.
     */
    @Override
    protected void onDestroy() {
        destroyNativeBridges(null);

        if (mTabModelSelectorTabObserver != null) {
            mTabModelSelectorTabObserver.destroy();
            mTabModelSelectorTabObserver = null;
        }
        if (mTabModelObserver != null) {
            mTabModelObserver.destroy();
            mTabModelObserver = null;
        }

        super.onDestroy();
    }

    private void reinitNativeBridges(Tab tab) {
        reinitNativeBridges(tab, tab.getUrl().getValidSpecOrEmpty());
    }

    private void reinitNativeBridges(Tab tab, String url) {
        destroyNativeBridges(tab);

        WebContents webContents = tab.getWebContents();
        if (webContents == null) {
            return;
        }

        if (RemoteNtpBridge.IsRemoteNtpUrl(url)) {
            boolean isInNightMode = getNightModeStateProvider().isInNightMode();
            mRemoteNtpBridge = new RemoteNtpBridge(this, webContents, isInNightMode);
        }
    }

    private void destroyNativeBridges(Tab tab) {
        if (tab == null) {
            if (mRemoteNtpBridge != null) {
                mRemoteNtpBridge.destroy();
                mRemoteNtpBridge = null;
            }
        } else {
            WebContents webContents = tab.getWebContents();
            if (webContents == null) {
                return;
            }

            if ((mRemoteNtpBridge != null) && (webContents == mRemoteNtpBridge.getWebContents())) {
                mRemoteNtpBridge.destroy();
                mRemoteNtpBridge = null;
            }
        }
    }
}
