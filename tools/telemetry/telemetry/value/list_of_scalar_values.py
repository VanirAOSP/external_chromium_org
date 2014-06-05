# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import numbers

from telemetry import value as value_module

def _Mean(values):
  return float(sum(values)) / len(values) if len(values) > 0 else 0.0

class ListOfScalarValues(value_module.Value):
  def __init__(self, page, name, units, values,
               important=True, same_page_merge_policy=value_module.CONCATENATE):
    super(ListOfScalarValues, self).__init__(page, name, units, important)
    assert len(values) > 0
    assert isinstance(values, list)
    for v in values:
      assert isinstance(v, numbers.Number)
    self.values = values
    self.same_page_merge_policy = same_page_merge_policy

  def __repr__(self):
    if self.page:
      page_name = self.page.url
    else:
      page_name = None
    if self.same_page_merge_policy == value_module.CONCATENATE:
      merge_policy = 'CONCATENATE'
    else:
      merge_policy = 'PICK_FIRST'
    return ('ListOfScalarValues(%s, %s, %s, %s, ' +
            'important=%s, same_page_merge_policy=%s)') % (
              page_name,
              self.name, self.units,
              repr(self.values),
              self.important,
              merge_policy)

  def GetBuildbotDataType(self, output_context):
    if self._IsImportantGivenOutputIntent(output_context):
      return 'default'
    return 'unimportant'

  def GetBuildbotValue(self):
    return self.values

  def GetRepresentativeNumber(self):
    return _Mean(self.values)

  def GetRepresentativeString(self):
    return repr(self.values)

  def IsMergableWith(self, that):
    return (super(ListOfScalarValues, self).IsMergableWith(that) and
            self.same_page_merge_policy == that.same_page_merge_policy)

  @classmethod
  def MergeLikeValuesFromSamePage(cls, values):
    assert len(values) > 0
    v0 = values[0]

    if v0.same_page_merge_policy == value_module.PICK_FIRST:
      return ListOfScalarValues(
          v0.page, v0.name, v0.units,
          values[0].values,
          important=v0.important,
          same_page_merge_policy=v0.same_page_merge_policy)

    assert v0.same_page_merge_policy == value_module.CONCATENATE
    all_values = []
    for v in values:
      all_values.extend(v.values)
    return ListOfScalarValues(
        v0.page, v0.name, v0.units,
        all_values,
        important=v0.important,
        same_page_merge_policy=v0.same_page_merge_policy)

  @classmethod
  def MergeLikeValuesFromDifferentPages(cls, values,
                                        group_by_name_suffix=False):
    assert len(values) > 0
    v0 = values[0]
    all_values = []
    for v in values:
      all_values.extend(v.values)
    if not group_by_name_suffix:
      name = v0.name
    else:
      name = v0.name_suffix
    return ListOfScalarValues(
        None, name, v0.units,
        all_values,
        important=v0.important,
        same_page_merge_policy=v0.same_page_merge_policy)
