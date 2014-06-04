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

#ifndef I18N_ADDRESSINPUT_UTIL_JSON_H_
#define I18N_ADDRESSINPUT_UTIL_JSON_H_

#include <string>

namespace i18n {
namespace addressinput {

// Parses a JSON dictionary of strings. Sample usage:
//    scoped_ptr<Json> json(Json::Build());
//    if (json->ParseObject("{'key1':'value1', 'key2':'value2'}") &&
//        json->HasStringKey("key1")) {
//      Process(json->GetStringValueForKey("key1"));
//    }
class Json {
 public:
  virtual ~Json();

  // Returns a new instanec of |Json| object. The caller owns the result.
  static Json* Build();

  // Parses the |json| string and returns true if |json| is valid and it is an
  // object.
  virtual bool ParseObject(const std::string& json) = 0;

  // Returns true if the parsed JSON contains a string value for |key|. The JSON
  // object must be parsed successfully in ParseObject() before invoking this
  // method.
  virtual bool HasStringValueForKey(const std::string& key) const = 0;

  // Returns the string value for the |key|. The |key| must be present and its
  // value must be of string type, i.e., HasStringValueForKey(key) must return
  // true before invoking this method.
  virtual std::string GetStringValueForKey(const std::string& key) const = 0;

 protected:
  Json();
};

}  // namespace addressinput
}  // namespace i18n

#endif  // I18N_ADDRESSINPUT_UTIL_JSON_H_
