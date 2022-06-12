// Copyright 2022 Viasat Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;
import org.rebel.mojom.WiFiStatus;

import org.chromium.build.BuildConfig;
import org.chromium.chrome.browser.app.RebelActivity;
import org.chromium.content_public.browser.WebContents;
import org.chromium.mojo.bindings.SerializationException;
import org.chromium.ui.base.PageTransition;
import org.chromium.url.GURL;

import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.List;

/**
 * Provides functionality when the user interacts with the RemoteNTP.
 */
@JNINamespace("rebel")
public class RemoteNtpBridge {
    private final RebelActivity mActivity;
    private final WebContents mWebContents;
    private final boolean mDarkModeEnabled;
    private long mNativeRemoteNtpBridge;

    public RemoteNtpBridge(
            RebelActivity activity, WebContents webContents, boolean darkModeEnabled) {
        mActivity = activity;
        mWebContents = webContents;
        mDarkModeEnabled = darkModeEnabled;

        mNativeRemoteNtpBridge =
                RemoteNtpBridgeJni.get().init(RemoteNtpBridge.this, mWebContents, mDarkModeEnabled);
    }

    /**
     * Check if the given URL is the RemoteNTP URL.
     */
    public static boolean IsRemoteNtpUrl(String url) {
        if (IsRemoteNtpEnabled()) {
            return RemoteNtpBridgeJni.get().isRemoteNtpUrl(url);
        }

        return false;
    }

    /**
     * Gets the current RemoteNTP URL.
     */
    public static GURL GetRemoteNtpUrl() {
        return RemoteNtpBridgeJni.get().getRemoteNtpUrl();
    }

    /**
     * Check if the RemoteNTP URL has been retrieved from native.
     */
    public static boolean IsRemoteNtpEnabled() {
        return RemoteNtpBridgeJni.get().isRemoteNtpEnabled();
    }

    /**
     * Destroys this instance so no further calls can be executed.
     */
    public void destroy() {
        RemoteNtpBridgeJni.get().destroy(mNativeRemoteNtpBridge, RemoteNtpBridge.this);
        mNativeRemoteNtpBridge = 0;
    }

    /**
     * @return The web contents associated with this bridge.
     */
    public WebContents getWebContents() {
        return mWebContents;
    }

    /**
     * Callback invoked by native when the RemoteNTP has requested a chrome://
     * URL be loaded.
     */
    @CalledByNative
    private void loadInternalUrl(String url) {
        mActivity.loadInternalUrl(url);
    }

    /**
     * Callback invoked by native when the RemoteNTP has requested an
     * autocomplete match URL be loaded.
     */
    @CalledByNative
    private void loadAutocompleteMatchUrl(String url, @PageTransition int transition) {
        mActivity.loadUrl(url, transition | PageTransition.FROM_ADDRESS_BAR);
    }

    /**
     * Callback invoked by native when the RemoteNTP has requested information
     * about the device's network status.
     */
    @CalledByNative
    private void updateWiFiStatus() {
        mActivity.updateWiFiStatus();
    }

    public void setWiFiStatus(List<WiFiStatus> wifiStatus) {
        List<ByteBuffer> wifiStatusBuffer = new ArrayList<ByteBuffer>();

        for (WiFiStatus status : wifiStatus) {
            try {
                wifiStatusBuffer.add(status.serialize());
            } catch (SerializationException | UnsupportedOperationException e) {
                if (BuildConfig.ENABLE_ASSERTS) {
                    throw new RuntimeException(e);
                }
            }
        }

        RemoteNtpBridgeJni.get().onWiFiStatusChanged(mNativeRemoteNtpBridge, RemoteNtpBridge.this,
                wifiStatusBuffer.toArray(new ByteBuffer[wifiStatusBuffer.size()]));
    }

    @NativeMethods
    interface Natives {
        long init(RemoteNtpBridge caller, WebContents webContents, boolean darkModeEnabled);
        void destroy(long nativeRemoteNtpBridge, RemoteNtpBridge caller);

        boolean isRemoteNtpEnabled();
        boolean isRemoteNtpUrl(String url);
        GURL getRemoteNtpUrl();

        void onWiFiStatusChanged(
                long nativeRemoteNtpBridge, RemoteNtpBridge caller, ByteBuffer[] wifiStatus);
    }
}
