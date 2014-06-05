// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/manifest_tests/extension_manifest_test.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/kiosk_mode_info.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

class ExtensionManifestKioskModeTest : public ExtensionManifestTest {
};

TEST_F(ExtensionManifestKioskModeTest, InvalidKioskEnabled) {
  LoadAndExpectError("kiosk_enabled_invalid.json",
                     manifest_errors::kInvalidKioskEnabled);
}

TEST_F(ExtensionManifestKioskModeTest, KioskEnabledHostedApp) {
  scoped_refptr<Extension> extension(
      LoadAndExpectSuccess("kiosk_enabled_hosted_app.json"));
  EXPECT_FALSE(KioskModeInfo::IsKioskEnabled(extension.get()));
}

TEST_F(ExtensionManifestKioskModeTest, KioskEnabledPackagedApp) {
  scoped_refptr<Extension> extension(
      LoadAndExpectSuccess("kiosk_enabled_packaged_app.json"));
  EXPECT_FALSE(KioskModeInfo::IsKioskEnabled(extension.get()));
}

TEST_F(ExtensionManifestKioskModeTest, KioskEnabledExtension) {
  scoped_refptr<Extension> extension(
      LoadAndExpectSuccess("kiosk_enabled_extension.json"));
  EXPECT_FALSE(KioskModeInfo::IsKioskEnabled(extension.get()));
}

TEST_F(ExtensionManifestKioskModeTest, KioskEnabledPlatformApp) {
  scoped_refptr<Extension> extension(
      LoadAndExpectSuccess("kiosk_enabled_platform_app.json"));
  EXPECT_TRUE(KioskModeInfo::IsKioskEnabled(extension.get()));
}

TEST_F(ExtensionManifestKioskModeTest, KioskDisabledPlatformApp) {
  scoped_refptr<Extension> extension(
      LoadAndExpectSuccess("kiosk_disabled_platform_app.json"));
  EXPECT_FALSE(KioskModeInfo::IsKioskEnabled(extension.get()));
}

TEST_F(ExtensionManifestKioskModeTest, KioskDefaultPlatformApp) {
  scoped_refptr<Extension> extension(
      LoadAndExpectSuccess("kiosk_default_platform_app.json"));
  EXPECT_FALSE(KioskModeInfo::IsKioskEnabled(extension.get()));
  EXPECT_FALSE(KioskModeInfo::IsKioskOnly(extension.get()));
}

TEST_F(ExtensionManifestKioskModeTest, KioskEnabledDefaultRequired) {
  scoped_refptr<Extension> extension(
      LoadAndExpectSuccess("kiosk_enabled_platform_app.json"));
  EXPECT_TRUE(KioskModeInfo::IsKioskEnabled(extension.get()));
  EXPECT_FALSE(KioskModeInfo::IsKioskOnly(extension.get()));
}

TEST_F(ExtensionManifestKioskModeTest, KioskOnlyPlatformApp) {
  scoped_refptr<Extension> extension(
      LoadAndExpectSuccess("kiosk_only_platform_app.json"));
  EXPECT_TRUE(KioskModeInfo::IsKioskOnly(extension.get()));
}

TEST_F(ExtensionManifestKioskModeTest, KioskOnlyInvalid) {
  LoadAndExpectError("kiosk_only_invalid.json",
                     manifest_errors::kInvalidKioskOnly);
}

TEST_F(ExtensionManifestKioskModeTest, KioskOnlyButNotEnabled) {
  LoadAndExpectError("kiosk_only_not_enabled.json",
                     manifest_errors::kInvalidKioskOnlyButNotEnabled);
}

TEST_F(ExtensionManifestKioskModeTest, KioskOnlyHostedApp) {
  scoped_refptr<Extension> extension(
      LoadAndExpectSuccess("kiosk_only_hosted_app.json"));
  EXPECT_FALSE(KioskModeInfo::IsKioskOnly(extension.get()));
}

TEST_F(ExtensionManifestKioskModeTest, KioskOnlyPackagedApp) {
  scoped_refptr<Extension> extension(
      LoadAndExpectSuccess("kiosk_only_packaged_app.json"));
  EXPECT_FALSE(KioskModeInfo::IsKioskOnly(extension.get()));
}

TEST_F(ExtensionManifestKioskModeTest, KioskOnlyExtension) {
  scoped_refptr<Extension> extension(
      LoadAndExpectSuccess("kiosk_only_extension.json"));
  EXPECT_FALSE(KioskModeInfo::IsKioskOnly(extension.get()));
}

}  // namespace extensions
