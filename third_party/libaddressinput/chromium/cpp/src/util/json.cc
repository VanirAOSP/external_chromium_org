// Copyright (C) 2013 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "json.h"

#include <libaddressinput/util/basictypes.h>
#include <libaddressinput/util/scoped_ptr.h>

#include <cassert>
#include <cstddef>
#include <string>

#include <rapidjson/document.h>
#include <rapidjson/reader.h>

namespace i18n {
namespace addressinput {

namespace {

class Rapidjson : public Json {
 public:
  Rapidjson() : document_(new rapidjson::Document), valid_(false) {}

  virtual ~Rapidjson() {}

  virtual bool ParseObject(const std::string& json) {
    document_->Parse<rapidjson::kParseValidateEncodingFlag>(json.c_str());
    valid_ = !document_->HasParseError() && document_->IsObject();
    return valid_;
  }

  virtual bool HasStringValueForKey(const std::string& key) const {
    assert(valid_);
    const rapidjson::Value::Member* member = document_->FindMember(key.c_str());
    return member != NULL && member->value.IsString();
  }

  virtual std::string GetStringValueForKey(const std::string& key) const {
    assert(valid_);
    const rapidjson::Value::Member* member = document_->FindMember(key.c_str());
    assert(member != NULL);
    assert(member->value.IsString());
    return std::string(member->value.GetString(),
                       member->value.GetStringLength());
  }

 protected:
  // JSON document.
  scoped_ptr<rapidjson::Document> document_;

  // True if the parsed string is a valid JSON object.
  bool valid_;

  DISALLOW_COPY_AND_ASSIGN(Rapidjson);
};

}  // namespace

Json::~Json() {}

// static
Json* Json::Build() {
  return new Rapidjson;
}

Json::Json() {}

}  // namespace addressinput
}  // namespace i18n
