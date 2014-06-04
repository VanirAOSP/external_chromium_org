// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_DEVTOOLS_TARGETS_UI_H_
#define CHROME_BROWSER_DEVTOOLS_DEVTOOLS_TARGETS_UI_H_

#include <map>
#include <string>

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"

namespace base {
class ListValue;
class DictionaryValue;
}

class DevToolsTargetImpl;
class Profile;

class DevToolsTargetsUIHandler {
 public:
  typedef base::Callback<void(const std::string&,
                              scoped_ptr<base::ListValue>)> Callback;

  DevToolsTargetsUIHandler(const std::string& source_id, Callback callback);
  virtual ~DevToolsTargetsUIHandler();

  std::string source_id() const { return source_id_; }

  static scoped_ptr<DevToolsTargetsUIHandler> CreateForRenderers(
      Callback callback);

  static scoped_ptr<DevToolsTargetsUIHandler> CreateForWorkers(
      Callback callback);

  void Inspect(const std::string& target_id, Profile* profile);
  void Activate(const std::string& target_id);
  void Close(const std::string& target_id);
  void Reload(const std::string& target_id);

 protected:
  base::DictionaryValue* Serialize(const DevToolsTargetImpl& target);
  void SendSerializedTargets(scoped_ptr<base::ListValue> list);

  typedef std::map<std::string, DevToolsTargetImpl*> TargetMap;
  TargetMap targets_;

 private:
  const std::string source_id_;
  Callback callback_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsTargetsUIHandler);
};

class DevToolsRemoteTargetsUIHandler: public DevToolsTargetsUIHandler {
 public:
  DevToolsRemoteTargetsUIHandler(const std::string& source_id,
                                Callback callback);

  static scoped_ptr<DevToolsRemoteTargetsUIHandler> CreateForAdb(
      Callback callback, Profile* profile);

  virtual void Open(const std::string& browser_id, const std::string& url) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(DevToolsRemoteTargetsUIHandler);
};

#endif  // CHROME_BROWSER_DEVTOOLS_DEVTOOLS_TARGETS_UI_H_
