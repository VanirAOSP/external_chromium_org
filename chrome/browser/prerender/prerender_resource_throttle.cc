// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prerender/prerender_resource_throttle.h"

#include "chrome/browser/prerender/prerender_final_status.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/prerender/prerender_tracker.h"
#include "chrome/browser/prerender/prerender_util.h"
#include "content/public/browser/resource_controller.h"
#include "content/public/browser/resource_request_info.h"
#include "net/url_request/url_request.h"

namespace prerender {

static const char kFollowOnlyWhenPrerenderShown[] =
    "follow-only-when-prerender-shown";

PrerenderResourceThrottle::PrerenderResourceThrottle(
    net::URLRequest* request,
    PrerenderTracker* tracker)
    : request_(request),
      tracker_(tracker),
      throttled_(false) {
}

void PrerenderResourceThrottle::WillStartRequest(bool* defer) {
  const content::ResourceRequestInfo* info =
      content::ResourceRequestInfo::ForRequest(request_);
  int child_id = info->GetChildID();
  int route_id = info->GetRouteID();

  // If the prerender was used since the throttle was added, leave it
  // alone.
  if (!tracker_->IsPrerenderingOnIOThread(child_id, route_id))
    return;

  // Abort any prerenders that spawn requests that use unsupported HTTP methods
  // or schemes.
  if (!prerender::PrerenderManager::IsValidHttpMethod(request_->method()) &&
      tracker_->TryCancelOnIOThread(
          child_id, route_id, prerender::FINAL_STATUS_INVALID_HTTP_METHOD)) {
    controller()->Cancel();
    return;
  }
  if (!prerender::PrerenderManager::DoesSubresourceURLHaveValidScheme(
          request_->url()) &&
      tracker_->TryCancelOnIOThread(
          child_id, route_id, prerender::FINAL_STATUS_UNSUPPORTED_SCHEME)) {
    ReportUnsupportedPrerenderScheme(request_->url());
    controller()->Cancel();
    return;
  }
}

void PrerenderResourceThrottle::WillRedirectRequest(const GURL& new_url,
                                                    bool* defer) {
  DCHECK(!throttled_);

  const content::ResourceRequestInfo* info =
      content::ResourceRequestInfo::ForRequest(request_);
  int child_id = info->GetChildID();
  int route_id = info->GetRouteID();

  // If the prerender was used since the throttle was added, leave it
  // alone.
  if (!tracker_->IsPrerenderingOnIOThread(child_id, route_id))
    return;

  // Abort any prerenders with requests which redirect to invalid schemes.
  if (!prerender::PrerenderManager::DoesURLHaveValidScheme(new_url) &&
      tracker_->TryCancel(
          child_id, route_id, prerender::FINAL_STATUS_UNSUPPORTED_SCHEME)) {
    ReportUnsupportedPrerenderScheme(new_url);
    controller()->Cancel();
    return;
  }

  // Only defer redirects with the Follow-Only-When-Prerender-Shown
  // header.
  std::string header;
  request_->GetResponseHeaderByName(kFollowOnlyWhenPrerenderShown, &header);
  if (header != "1")
    return;

  // Do not defer redirects on main frame loads.
  if (info->GetResourceType() == ResourceType::MAIN_FRAME)
    return;

  if (!info->IsAsync()) {
    // Cancel on deferred synchronous requests. Those will
    // indefinitely hang up a renderer process.
    //
    // If the TryCancelOnIOThread fails, the UI thread won a race to
    // use the prerender, so let the request through.
    if (tracker_->TryCancelOnIOThread(child_id, route_id,
                                      FINAL_STATUS_BAD_DEFERRED_REDIRECT)) {
      controller()->Cancel();
    }
    return;
  }

  // Defer the redirect until the prerender is used or
  // canceled. It is possible for the UI thread to used the
  // prerender at the same time. But then |tracker_| will resume
  // the request soon in
  // PrerenderTracker::RemovePrerenderOnIOThread.
  *defer = true;
  throttled_ = true;
  tracker_->AddResourceThrottleOnIOThread(child_id, route_id,
                                          this->AsWeakPtr());
}

const char* PrerenderResourceThrottle::GetNameForLogging() const {
  return "PrerenderResourceThrottle";
}

void PrerenderResourceThrottle::Resume() {
  DCHECK(throttled_);

  throttled_ = false;
  controller()->Resume();
}

void PrerenderResourceThrottle::Cancel() {
  DCHECK(throttled_);

  throttled_ = false;
  controller()->Cancel();
}

}  // namespace prerender
