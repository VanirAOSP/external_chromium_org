# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import os
import unittest

from telemetry import value
from telemetry.page import page_set
from telemetry.value import list_of_scalar_values

class TestBase(unittest.TestCase):
  def setUp(self):
    self.page_set =  page_set.PageSet.FromDict({
      "description": "hello",
      "archive_path": "foo.wpr",
      "pages": [
        {"url": "http://www.bar.com/"},
        {"url": "http://www.baz.com/"},
        {"url": "http://www.foo.com/"}
        ]
      }, os.path.dirname(__file__))

  @property
  def pages(self):
    return self.page_set.pages

class ValueTest(TestBase):
  def testListSamePageMergingWithSamePageConcatenatePolicy(self):
    page0 = self.pages[0]
    v0 = list_of_scalar_values.ListOfScalarValues(
        page0, 'x', 'unit',
        [1,2], same_page_merge_policy=value.CONCATENATE)
    v1 = list_of_scalar_values.ListOfScalarValues(
        page0, 'x', 'unit',
        [3,4], same_page_merge_policy=value.CONCATENATE)
    self.assertTrue(v1.IsMergableWith(v0))

    vM = (list_of_scalar_values.ListOfScalarValues.
          MergeLikeValuesFromSamePage([v0, v1]))
    self.assertEquals(page0, vM.page)
    self.assertEquals('x', vM.name)
    self.assertEquals('unit', vM.units)
    self.assertEquals(value.CONCATENATE, vM.same_page_merge_policy)
    self.assertEquals(True, vM.important)
    self.assertEquals([1, 2, 3, 4], vM.values)

  def testListSamePageMergingWithPickFirstPolicy(self):
    page0 = self.pages[0]
    v0 = list_of_scalar_values.ListOfScalarValues(
        page0, 'x', 'unit',
        [1,2], same_page_merge_policy=value.PICK_FIRST)
    v1 = list_of_scalar_values.ListOfScalarValues(
        page0, 'x', 'unit',
        [3,4], same_page_merge_policy=value.PICK_FIRST)
    self.assertTrue(v1.IsMergableWith(v0))

    vM = (list_of_scalar_values.ListOfScalarValues.
          MergeLikeValuesFromSamePage([v0, v1]))
    self.assertEquals(page0, vM.page)
    self.assertEquals('x', vM.name)
    self.assertEquals('unit', vM.units)
    self.assertEquals(value.PICK_FIRST, vM.same_page_merge_policy)
    self.assertEquals(True, vM.important)
    self.assertEquals([1, 2], vM.values)

  def testListDifferentPageMerging(self):
    page0 = self.pages[0]
    v0 = list_of_scalar_values.ListOfScalarValues(
        page0, 'x', 'unit',
        [1, 2], same_page_merge_policy=value.PICK_FIRST)
    v1 = list_of_scalar_values.ListOfScalarValues(
        page0, 'x', 'unit',
        [3, 4], same_page_merge_policy=value.PICK_FIRST)
    self.assertTrue(v1.IsMergableWith(v0))

    vM = (list_of_scalar_values.ListOfScalarValues.
          MergeLikeValuesFromDifferentPages([v0, v1]))
    self.assertEquals(None, vM.page)
    self.assertEquals('x', vM.name)
    self.assertEquals('unit', vM.units)
    self.assertEquals(value.PICK_FIRST, vM.same_page_merge_policy)
    self.assertEquals(True, vM.important)
    self.assertEquals([1, 2, 3, 4], vM.values)
