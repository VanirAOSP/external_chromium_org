// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/page_actions_custom_bindings.h"

#include <string>

#include "base/bind.h"
#include "chrome/common/extensions/api/extension_action/action_info.h"
#include "chrome/renderer/extensions/dispatcher.h"
#include "extensions/common/extension.h"
#include "grit/renderer_resources.h"
#include "v8/include/v8.h"

namespace extensions {

PageActionsCustomBindings::PageActionsCustomBindings(
    Dispatcher* dispatcher, ChromeV8Context* context)
    : ChromeV8Extension(dispatcher, context) {
  RouteFunction("GetCurrentPageActions",
      base::Bind(&PageActionsCustomBindings::GetCurrentPageActions,
                 base::Unretained(this)));
}

void PageActionsCustomBindings::GetCurrentPageActions(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  std::string extension_id = *v8::String::Utf8Value(args[0]->ToString());
  CHECK(!extension_id.empty());
  const Extension* extension =
      dispatcher_->extensions()->GetByID(extension_id);
  CHECK(extension);

  v8::Local<v8::Array> page_action_vector = v8::Array::New(args.GetIsolate());
  if (ActionInfo::GetPageActionInfo(extension)) {
    std::string id = ActionInfo::GetPageActionInfo(extension)->id;
    page_action_vector->Set(v8::Integer::New(0),
                            v8::String::NewFromUtf8(args.GetIsolate(),
                                                    id.c_str(),
                                                    v8::String::kNormalString,
                                                    id.size()));
  }

  args.GetReturnValue().Set(page_action_vector);
}

}  // namespace extensions
