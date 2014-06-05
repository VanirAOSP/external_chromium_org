// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_URL_FETCHER_DOWNLOADER_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_URL_FETCHER_DOWNLOADER_H_

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/component_updater/crx_downloader.h"
#include "net/url_request/url_fetcher_delegate.h"

namespace net {
class URLFetcher;
class URLRequestContextGetter;
}

namespace component_updater {

// Implements a CRX downloader in top of the URLFetcher.
class UrlFetcherDownloader : public CrxDownloader,
                             public net::URLFetcherDelegate {
 protected:
  friend class CrxDownloader;
  UrlFetcherDownloader(scoped_ptr<CrxDownloader> successor,
                       net::URLRequestContextGetter* context_getter,
                       scoped_refptr<base::SequencedTaskRunner> task_runner,
                       const DownloadCallback& download_callback);
  virtual ~UrlFetcherDownloader();

 private:
  // Overrides for CrxDownloader.
  virtual void DoStartDownload(const GURL& url) OVERRIDE;

  // Overrides for URLFetcherDelegate.
  virtual void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE;
  virtual void OnURLFetchDownloadProgress(const net::URLFetcher* source,
                                          int64 current,
                                          int64 total) OVERRIDE;
  scoped_ptr<net::URLFetcher> url_fetcher_;
  net::URLRequestContextGetter* context_getter_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  base::Time download_start_time_;

  int64 downloaded_bytes_;
  int64 total_bytes_;

  DISALLOW_COPY_AND_ASSIGN(UrlFetcherDownloader);
};

}  // namespace component_updater

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_URL_FETCHER_DOWNLOADER_H_

