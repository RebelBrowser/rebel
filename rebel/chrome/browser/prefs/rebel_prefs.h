// Copyright 2023 Viasat Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

class PrefRegistrySimple;

namespace rebel {

void RegisterLocalState(PrefRegistrySimple* registry);
void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

}  // namespace rebel
