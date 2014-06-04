// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_TEST_TEST_EVENT_TARGET_H_
#define UI_EVENTS_TEST_TEST_EVENT_TARGET_H_

#include <set>

#include "base/memory/scoped_vector.h"
#include "ui/events/event_target.h"

namespace gfx {
class Point;
}

namespace ui {
namespace test {

class TestEventTarget : public EventTarget {
 public:
  TestEventTarget();
  virtual ~TestEventTarget();

  void AddChild(scoped_ptr<TestEventTarget> child);
  scoped_ptr<TestEventTarget> RemoveChild(TestEventTarget* child);

  TestEventTarget* parent() { return parent_; }

  TestEventTarget* child_at(int index) { return children_[index]; }
  size_t child_count() const { return children_.size(); }

  void SetEventTargeter(scoped_ptr<EventTargeter> targeter);

  bool DidReceiveEvent(ui::EventType type) const;
  void ResetReceivedEvents();

 protected:
  bool Contains(TestEventTarget* target) const;

  // EventTarget:
  virtual bool CanAcceptEvent(const ui::Event& event) OVERRIDE;
  virtual EventTarget* GetParentTarget() OVERRIDE;
  virtual scoped_ptr<EventTargetIterator> GetChildIterator() const OVERRIDE;
  virtual EventTargeter* GetEventTargeter() OVERRIDE;

  // EventHandler:
  virtual void OnEvent(Event* event) OVERRIDE;

 private:
  void set_parent(TestEventTarget* parent) { parent_ = parent; }

  TestEventTarget* parent_;
  ScopedVector<TestEventTarget> children_;
  scoped_ptr<EventTargeter> targeter_;

  std::set<ui::EventType> received_;

  DISALLOW_COPY_AND_ASSIGN(TestEventTarget);
};

}  // namespace test
}  // namespace ui

#endif  // UI_EVENTS_TEST_TEST_EVENT_TARGET_H_
