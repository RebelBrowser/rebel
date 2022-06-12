// Copyright 2022 Viasat Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app;

import android.content.Context;
import android.net.wifi.ScanResult;
import android.net.wifi.WifiInfo;
import android.net.wifi.WifiManager;
import android.os.Build;
import android.text.TextUtils;

import org.rebel.mojom.WiFiStatus;

import org.chromium.base.ContextUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.bookmarks.BookmarkUtils;
import org.chromium.chrome.browser.download.DownloadOpenSource;
import org.chromium.chrome.browser.download.DownloadUtils;
import org.chromium.chrome.browser.init.AsyncInitializationActivity;
import org.chromium.chrome.browser.ntp.RemoteNtpBridge;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
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
import java.util.ArrayList;
import java.util.List;

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

    public void updateWiFiStatus() {
        WifiManager wifiManager =
                (WifiManager) ContextUtils.getApplicationContext().getSystemService(
                        Context.WIFI_SERVICE);

        List<WiFiStatus> wifiStatus = new ArrayList<WiFiStatus>();
        WiFiStatus connected = null;

        // https://developer.android.com/reference/android/net/wifi/WifiInfo
        WifiInfo wifiInfo = wifiManager.getConnectionInfo();

        if (wifiInfo != null) {
            connected = new WiFiStatus();
            connected.ssid = wifiInfo.getSSID();
            connected.bssid = wifiInfo.getBSSID();
            connected.connectionState = "Connected"; // onc::connection_state::kConnected
            connected.rssi = wifiInfo.getRssi();
            connected.linkSpeed = wifiInfo.getLinkSpeed();

            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
                connected.maxRxMbps = wifiInfo.getMaxSupportedRxLinkSpeedMbps();
                connected.maxTxMbps = wifiInfo.getMaxSupportedTxLinkSpeedMbps();
            }
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
                connected.rxMbps = wifiInfo.getRxLinkSpeedMbps();
                connected.txMbps = wifiInfo.getTxLinkSpeedMbps();
            }

            normalizeWiFiStatus(wifiManager, connected);
            wifiStatus.add(connected);
        }

        // https://developer.android.com/reference/android/net/wifi/ScanResult
        List<ScanResult> results = wifiManager.getScanResults();

        for (ScanResult result : results) {
            if ((connected != null) && result.SSID.equals(connected.ssid)) {
                connected.frequency = result.frequency;
                continue;
            }

            WiFiStatus status = new WiFiStatus();
            status.ssid = result.SSID;
            status.bssid = result.BSSID;
            status.connectionState = "NotConnected"; // onc::connection_state::kNotConnected
            status.rssi = result.level;
            status.frequency = result.frequency;

            normalizeWiFiStatus(wifiManager, status);
            wifiStatus.add(status);
        }

        if (mRemoteNtpBridge != null) {
            mRemoteNtpBridge.setWiFiStatus(wifiStatus);
        }
    }

    private void normalizeWiFiStatus(WifiManager wifiManager, WiFiStatus status) {
        // The SSID returned by |WifiManager.getConnectionInfo.getSSID| may be surrounded by
        // quotes, whereas the SSID from ScanResult.SSID wil not be.
        int ssidLength = status.ssid.length();
        if (ssidLength > 1) {
            if ((status.ssid.charAt(0) == '"') && (status.ssid.charAt(ssidLength - 1) == '"')) {
                status.ssid = status.ssid.substring(1, ssidLength - 1);
            }
        }

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            status.signalLevel = wifiManager.calculateSignalLevel(status.rssi);
            status.maxSignalLevel = wifiManager.getMaxSignalLevel();
        } else if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            status.signalLevel = WifiManager.calculateSignalLevel(status.rssi, 5);
        }
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
            public void didAddTab(Tab tab, @TabLaunchType int type,
                    @TabCreationState int creationState, boolean markedForSelection) {
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
