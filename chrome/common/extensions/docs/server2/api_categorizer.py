# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
from compiled_file_system import SingleFile
from extensions_paths import PUBLIC_TEMPLATES


class APICategorizer(object):
  ''' This class gets api category from documented apis.
  '''

  def __init__(self, file_system, compiled_fs_factory):
    self._file_system = file_system
    self._cache = compiled_fs_factory.Create(file_system,
                                             self._CollectDocumentedAPIs,
                                             APICategorizer)

  def _GenerateAPICategories(self, platform):
    return self._cache.GetFromFileListing('%s/%s' % (PUBLIC_TEMPLATES,
                                                     platform)).Get()
  @SingleFile
  def _CollectDocumentedAPIs(self, base_dir, files):
    public_templates = []
    for root, _, files in self._file_system.Walk(base_dir):
      public_templates.extend(
          ('%s/%s' % (root, name)).lstrip('/') for name in files)
    template_names = set(os.path.splitext(name)[0].replace('_', '.')
                         for name in public_templates)
    return template_names

  def GetCategory(self, platform, api_name):
    '''Return the type of api.'Chrome' means the public apis,
    private means the api only used by chrome, and experimental means
    the apis with "experimental" prefix.
    '''
    documented_apis = self._GenerateAPICategories(platform)
    if (api_name.endswith('Private') or
        api_name not in documented_apis):
      return 'private'
    if api_name.startswith('experimental.'):
      return 'experimental'
    return 'chrome'

  def IsDocumented(self, platform, api_name):
    return (api_name in self._GenerateAPICategories(platform))