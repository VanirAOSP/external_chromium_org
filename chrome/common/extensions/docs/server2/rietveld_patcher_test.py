#!/usr/bin/env python
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import posixpath
import sys
import unittest
from appengine_url_fetcher import AppEngineUrlFetcher
from extensions_paths import (
    ARTICLES_TEMPLATES, EXTENSIONS, DOCS, JSON_TEMPLATES, PUBLIC_TEMPLATES)
from fake_fetchers import ConfigureFakeFetchers
from file_system import FileNotFoundError
from rietveld_patcher import RietveldPatcher
import url_constants


def _PrefixWith(prefix, lst):
  return [posixpath.join(prefix, item) for item in lst]


class RietveldPatcherTest(unittest.TestCase):
  def setUp(self):
    ConfigureFakeFetchers()
    self._patcher = RietveldPatcher(
        '14096030',
        AppEngineUrlFetcher(url_constants.CODEREVIEW_SERVER))

  def _ReadLocalFile(self, filename):
    with open(os.path.join(sys.path[0],
                           'test_data',
                           'rietveld_patcher',
                           'expected',
                           filename), 'r') as f:
      return f.read()

  def _ApplySingle(self, path):
    return self._patcher.Apply([path], None).Get()[path]

  def testGetVersion(self):
    self.assertEqual(self._patcher.GetVersion(), '22002')

  def testGetPatchedFiles(self):
    added, deleted, modified = self._patcher.GetPatchedFiles()
    self.assertEqual(
        sorted(added),
        _PrefixWith(DOCS, ['examples/test',
                           'templates/articles/test_foo.html',
                           'templates/public/extensions/test_foo.html']))
    self.assertEqual(deleted,
                     ['%s/extensions/runtime.html' % PUBLIC_TEMPLATES])
    self.assertEqual(
        sorted(modified),
        _PrefixWith(EXTENSIONS, ['api/test.json',
                                 'docs/templates/json/extensions_sidenav.json',
                                 'manifest.h']))

  def testApply(self):
    article_path = '%s/test_foo.html' % ARTICLES_TEMPLATES

    # Apply to an added file.
    self.assertEqual(
        self._ReadLocalFile('test_foo.html'),
        self._ApplySingle('%s/extensions/test_foo.html' % PUBLIC_TEMPLATES))

    # Apply to a modified file.
    self.assertEqual(
        self._ReadLocalFile('extensions_sidenav.json'),
        self._ApplySingle('%s/extensions_sidenav.json' % JSON_TEMPLATES))

    # Applying to a deleted file doesn't throw exceptions. It just returns
    # empty content.
    # self.assertRaises(FileNotFoundError, self._ApplySingle,
    #     'docs/templates/public/extensions/runtime.html')

    # Apply to an unknown file.
    self.assertRaises(FileNotFoundError, self._ApplySingle, 'not_existing')

if __name__ == '__main__':
  unittest.main()
