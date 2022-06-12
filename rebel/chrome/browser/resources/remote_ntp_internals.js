// Copyright 2022 Viasat Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// <if expr="is_ios">
import 'chrome://resources/js/ios/web_ui.js';
// </if>

import 'chrome://resources/js/jstemplate_compiled.js';
import './strings.m.js';

import {sendWithPromise} from 'chrome://resources/js/cr.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {$} from 'chrome://resources/js/util.m.js';

let cachedIcons = {};

const Direction = Object.freeze({
  down: 1,
  up: 2,
});

function deleteCachedIcon(event, origin) {
  event.preventDefault();
  chrome.send('deleteCachedIcon', [origin]);
}

window.cachedIconsAvailable = (icons) => {
  cachedIcons = icons;
  renderSortedCachedIcons('origin', Direction.up);
};

window.iconImageAvailable = (icon) => {
  $('img-' + icon.origin).src = icon.icon;
};

function renderSortedCachedIcons(key, direction) {
  $('remote-ntp-icons').querySelectorAll('th').forEach((header) => {
    header.classList.remove('sorted-down');
    header.classList.remove('sorted-up');
  });

  let multiplier = 0;

  if (direction === Direction.down) {
    $(key).classList.add('sorted-down');
    multiplier = -1;
  } else {
    $(key).classList.add('sorted-up');
    multiplier = 1;
  }

  const icons = transformCachedIconsForRendering();

  icons.sort((a, b) => {
    if (a[key] > b[key]) {
      return multiplier * 1;
    }

    return multiplier * -1;
  });

  renderCachedIcons(icons);
}

function transformCachedIconsForRendering() {
  let icons = [];

  const iconTypeAndSize = (icon) => {
    let size = '';
    if (icon.icon_size !== -1) {
      size = ` (${icon.icon_size}x${icon.icon_size})`
    }

    switch (icon.icon_type) {
      case 1:
        return 'Favicon' + size;
      case 2:
        return 'Fluid' + size;
      case 3:
        return 'Touch' + size;
      default:
        return 'Unknown' + size;
    }
  };

  const iconElement = (icon) => {
    const img = document.createElement('img');
    img.id = 'img-' + icon.host_origin;

    const div = document.createElement('div');
    div.classList.add('icon');
    div.appendChild(img);

    return div;
  };

  cachedIcons.forEach((icon) => {
    icons.push({
      origin: icon.host_origin,
      icon: icon.icon_url,
      path: icon.icon_file,
      type: iconTypeAndSize(icon),
      image: iconElement(icon),
    });
  });

  return icons;
}

function renderCachedIcons(icons) {
  let table = $('remote-ntp-icons');

  const oldTableBody = $('remote-ntp-icons-body');
  let newTableBody = document.createElement('tbody');
  newTableBody.id = oldTableBody.id;

  icons.forEach((icon) => renderRow(newTableBody, icon));
  table.replaceChild(newTableBody, oldTableBody);
}

function renderRow(table, icon) {
  let row = document.createElement('tr');

  let origin = document.createElement('a');
  origin.href = icon.origin;
  origin.innerText = icon.origin;

  let path = document.createElement('a');
  path.href = icon.icon;
  path.innerText = icon.path;

  renderColumn(row);
  renderColumn(row, origin);
  renderColumn(row, path);
  renderColumn(row, icon.type);
  renderColumn(row, icon.image);

  const button = document.createElement('button');
  button.onclick = (e) => deleteCachedIcon(e, icon.origin);
  button.innerText = loadTimeData.getString('remoteNtpInternalsIconTableDelete');
  button.type = 'button';
  renderColumn(row, button);

  table.appendChild(row);
}

function renderColumn(row, node) {
  let column = document.createElement('td');

  if (typeof node === 'string') {
    column.appendChild(document.createTextNode(node));
  } else if (typeof node === 'number') {
    column.appendChild(document.createTextNode(node.toString()));
  } else if (typeof node !== 'undefined') {
    column.appendChild(node);
  }

  row.appendChild(column);
}

document.addEventListener('DOMContentLoaded', function() {
  $('remote-ntp-icons').querySelectorAll('th').forEach((header) => {
    if (header.id.length === 0) {
      return;
    }

    header.addEventListener('click', () => {
      if (header.classList.contains('sorted-down')) {
        renderSortedCachedIcons(header.id, Direction.up);
      } else {
        renderSortedCachedIcons(header.id, Direction.down);
      }
    });
  });

  chrome.send('requestCachedIcons');
});
