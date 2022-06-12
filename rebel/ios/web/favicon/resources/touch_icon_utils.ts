// Copyright 2022 Viasat Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Provides a function which collects touch icon details from the
 * current document.
 *
 * This is copied from //ios/web/favicon/resources/favicon_utils.ts to allow
 * parsing fluid icons (used by e.g. github.com). Also, WebState only allows one
 * listener per command, so duplicating the favicon message is required to set
 * up a listener for RemoteNTP.
 */

import { sendWebKitMessage } from '//ios/web/public/js_messaging/resources/utils.js'

declare interface TouchIconData {
  rel: string,
  href: string | undefined,
  sizes?: string
}

/**
 * Retrieves touch icon information.
 *
 * @return {TouchIconData[]} An array of objects containing touch icon data.
 */
function getTouchIcons(): TouchIconData[] {
  let touchIcons: TouchIconData[] = [];
  let links = document.getElementsByTagName('link');
  let linkCount = links.length;
  for (var i = 0; i < linkCount; ++i) {
    let link = links[i];
    let rel = link?.rel.toLowerCase();

    if (rel === 'shortcut icon' ||
        rel === 'icon' ||
        rel === 'fluid-icon' ||
        rel === 'apple-touch-icon' ||
        rel === 'apple-touch-icon-precomposed') {
      let touchIcon: TouchIconData =
          {rel: rel, href: link?.href};
      let size_value = link?.sizes?.value;

      if (size_value) {
        touchIcon.sizes = size_value;
      }
      touchIcons.push(touchIcon);
    }
  }
  return touchIcons;
}

function sendTouchIconUrls(): void {
  sendWebKitMessage('TouchIconUrlsHandler', getTouchIcons());
}

export {sendTouchIconUrls}
