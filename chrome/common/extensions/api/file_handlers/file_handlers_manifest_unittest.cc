// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/api/file_handlers/file_handlers_parser.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/manifest_tests/extension_manifest_test.h"
//#include "extensions/common/error_utils.h"
#include "extensions/common/manifest_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace errors = extensions::manifest_errors;

using extensions::Extension;
using extensions::FileHandlers;
using extensions::FileHandlerInfo;

class FileHandlersManifestTest : public ExtensionManifestTest {
};

TEST_F(FileHandlersManifestTest, InvalidFileHandlers) {
  Testcase testcases[] = {
    Testcase("file_handlers_invalid_handlers.json",
             errors::kInvalidFileHandlers),
    Testcase("file_handlers_invalid_type.json",
             errors::kInvalidFileHandlerType),
    Testcase("file_handlers_invalid_extension.json",
             errors::kInvalidFileHandlerExtension),
    Testcase("file_handlers_invalid_no_type_or_extension.json",
             errors::kInvalidFileHandlerNoTypeOrExtension),
    Testcase("file_handlers_invalid_title.json",
             errors::kInvalidFileHandlerTitle),
    Testcase("file_handlers_invalid_type_element.json",
             errors::kInvalidFileHandlerTypeElement),
    Testcase("file_handlers_invalid_extension_element.json",
             errors::kInvalidFileHandlerExtensionElement),
    Testcase("file_handlers_invalid_too_many.json",
             errors::kInvalidFileHandlersTooManyTypesAndExtensions),
  };
  RunTestcases(testcases, arraysize(testcases), EXPECT_TYPE_ERROR);
}

TEST_F(FileHandlersManifestTest, ValidFileHandlers) {
  scoped_refptr<const Extension> extension =
      LoadAndExpectSuccess("file_handlers_valid.json");

  ASSERT_TRUE(extension.get());
  const std::vector<FileHandlerInfo>* handlers =
      FileHandlers::GetFileHandlers(extension.get());
  ASSERT_TRUE(handlers != NULL);
  ASSERT_EQ(2U, handlers->size());

  FileHandlerInfo handler = handlers->at(0);
  EXPECT_EQ("image", handler.id);
  EXPECT_EQ("Image editor", handler.title);
  EXPECT_EQ(1U, handler.types.size());
  EXPECT_EQ(1U, handler.types.count("image/*"));
  EXPECT_EQ(2U, handler.extensions.size());
  EXPECT_EQ(1U, handler.extensions.count(".png"));
  EXPECT_EQ(1U, handler.extensions.count(".gif"));

  handler = handlers->at(1);
  EXPECT_EQ("text", handler.id);
  EXPECT_EQ("Text editor", handler.title);
  EXPECT_EQ(1U, handler.types.size());
  EXPECT_EQ(1U, handler.types.count("text/*"));
  EXPECT_EQ(0U, handler.extensions.size());
}

TEST_F(FileHandlersManifestTest, NotPlatformApp) {
  // This should load successfully but have the file handlers ignored.
  scoped_refptr<const Extension> extension =
      LoadAndExpectSuccess("file_handlers_invalid_not_app.json");

  ASSERT_TRUE(extension.get());
  const std::vector<FileHandlerInfo>* handlers =
      FileHandlers::GetFileHandlers(extension.get());
  ASSERT_TRUE(handlers == NULL);
}
