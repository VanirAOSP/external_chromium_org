// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_IME_EXTENSION_IME_UTIL_H_
#define CHROMEOS_IME_EXTENSION_IME_UTIL_H_

#include <string>

#include "chromeos/chromeos_export.h"

namespace chromeos {

// Extension IME related utilities.
namespace extension_ime_util {

// Returns InputMethodID for |engine_id| in |extension_id| of extension IME.
// This function does not check |extension_id| is installed extension IME nor
// |engine_id| is really a member of |extension_id|.
std::string CHROMEOS_EXPORT GetInputMethodID(const std::string& extension_id,
                                             const std::string& engine_id);

// Returns InputMethodID for |engine_id| in |extension_id| of component
// extension IME, This function does not check |extension_id| is component one
// nor |engine_id| is really a member of |extension_id|.
std::string CHROMEOS_EXPORT GetComponentInputMethodID(
    const std::string& extension_id,
    const std::string& engine_id);

// Returns extension ID if |input_method_id| is extension IME ID or component
// extension IME ID. Otherwise returns an empty string ("").
std::string CHROMEOS_EXPORT GetExtensionIDFromInputMethodID(
    const std::string& input_method_id);

// Returns true if |input_method_id| is extension IME ID. This function does not
// check |input_method_id| is installed extension IME.
bool CHROMEOS_EXPORT IsExtensionIME(const std::string& input_method_id);

// Returns true if |input_method_id| is component extension IME ID. This
// function does not check |input_method_id| is really whitelisted one or not.
// If you want to check |input_method_id| is whitelisted component extension
// IME, please use ComponentExtensionIMEManager::IsWhitelisted instead.
bool CHROMEOS_EXPORT IsComponentExtensionIME(
    const std::string& input_method_id);

// Returns true if the |input_method| is a member of |extension_id| of extension
// IME, otherwise returns false.
bool CHROMEOS_EXPORT IsMemberOfExtension(const std::string& input_method_id,
                                         const std::string& extension_id);

}  // namespace extension_ime_util

}  // namespace chromeos

#endif  // CHROMEOS_IME_EXTENSION_IME_UTIL_H_
