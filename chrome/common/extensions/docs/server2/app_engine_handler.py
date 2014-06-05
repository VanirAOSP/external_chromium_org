# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from appengine_wrappers import webapp2
from handler import Handler
from servlet import Request

class AppEngineHandler(webapp2.RequestHandler):
  '''Top-level handler for AppEngine requests. Just converts them into our
  internal Servlet architecture.
  '''

  def get(self):
    profile_mode = self.request.get('profile')
    if profile_mode:
      import cProfile, pstats, StringIO
      pr = cProfile.Profile()
      pr.enable()

    try:
      request = Request(self.request.path,
                        self.request.url[:-len(self.request.path)],
                        self.request.headers)
      response = Handler(request).Get()
    finally:
      if profile_mode:
        pr.disable()
        s = StringIO.StringIO()
        pstats.Stats(pr, stream=s).sort_stats(profile_mode).print_stats()
        self.response.out.write(s.getvalue())
        self.response.headers['Content-Type'] = 'text/plain'
        self.response.status = 200
      else:
        self.response.out.write(response.content.ToString())
        self.response.headers.update(response.headers)
        self.response.status = response.status
