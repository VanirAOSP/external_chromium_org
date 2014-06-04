// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/chrome_render_view_observer.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/debug/trace_event.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/prerender_messages.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/url_constants.h"
#include "chrome/renderer/chrome_render_process_observer.h"
#include "chrome/renderer/external_host_bindings.h"
#include "chrome/renderer/prerender/prerender_helper.h"
#include "chrome/renderer/safe_browsing/phishing_classifier_delegate.h"
#include "chrome/renderer/translate/translate_helper.h"
#include "chrome/renderer/webview_color_overlay.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_view.h"
#include "extensions/common/constants.h"
#include "extensions/common/stack_frame.h"
#include "net/base/data_url.h"
#include "skia/ext/image_operations.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/WebKit/public/platform/WebCString.h"
#include "third_party/WebKit/public/platform/WebRect.h"
#include "third_party/WebKit/public/platform/WebSize.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebURLRequest.h"
#include "third_party/WebKit/public/platform/WebVector.h"
#include "third_party/WebKit/public/web/WebAXObject.h"
#include "third_party/WebKit/public/web/WebDataSource.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebElement.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "third_party/WebKit/public/web/WebNode.h"
#include "third_party/WebKit/public/web/WebNodeList.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "ui/base/ui_base_switches_util.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/size.h"
#include "ui/gfx/size_f.h"
#include "ui/gfx/skbitmap_operations.h"
#include "v8/include/v8-testing.h"

using blink::WebAXObject;
using blink::WebCString;
using blink::WebDataSource;
using blink::WebDocument;
using blink::WebElement;
using blink::WebFrame;
using blink::WebGestureEvent;
using blink::WebIconURL;
using blink::WebNode;
using blink::WebNodeList;
using blink::WebRect;
using blink::WebSecurityOrigin;
using blink::WebSize;
using blink::WebString;
using blink::WebTouchEvent;
using blink::WebURL;
using blink::WebURLRequest;
using blink::WebView;
using blink::WebVector;
using blink::WebWindowFeatures;

// Delay in milliseconds that we'll wait before capturing the page contents
// and thumbnail.
static const int kDelayForCaptureMs = 500;

// Typically, we capture the page data once the page is loaded.
// Sometimes, the page never finishes to load, preventing the page capture
// To workaround this problem, we always perform a capture after the following
// delay.
static const int kDelayForForcedCaptureMs = 6000;

// define to write the time necessary for thumbnail/DOM text retrieval,
// respectively, into the system debug log
// #define TIME_TEXT_RETRIEVAL

// maximum number of characters in the document to index, any text beyond this
// point will be clipped
static const size_t kMaxIndexChars = 65535;

// Constants for UMA statistic collection.
static const char kTranslateCaptureText[] = "Translate.CaptureText";

namespace {

GURL StripRef(const GURL& url) {
  GURL::Replacements replacements;
  replacements.ClearRef();
  return url.ReplaceComponents(replacements);
}

// If the source image is null or occupies less area than
// |thumbnail_min_area_pixels|, we return the image unmodified.  Otherwise, we
// scale down the image so that the width and height do not exceed
// |thumbnail_max_size_pixels|, preserving the original aspect ratio.
SkBitmap Downscale(blink::WebImage image,
                   int thumbnail_min_area_pixels,
                   gfx::Size thumbnail_max_size_pixels) {
  if (image.isNull())
    return SkBitmap();

  gfx::Size image_size = image.size();

  if (image_size.GetArea() < thumbnail_min_area_pixels)
    return image.getSkBitmap();

  if (image_size.width() <= thumbnail_max_size_pixels.width() &&
      image_size.height() <= thumbnail_max_size_pixels.height())
    return image.getSkBitmap();

  gfx::SizeF scaled_size = image_size;

  if (scaled_size.width() > thumbnail_max_size_pixels.width()) {
    scaled_size.Scale(thumbnail_max_size_pixels.width() / scaled_size.width());
  }

  if (scaled_size.height() > thumbnail_max_size_pixels.height()) {
    scaled_size.Scale(
        thumbnail_max_size_pixels.height() / scaled_size.height());
  }

  return skia::ImageOperations::Resize(image.getSkBitmap(),
                                       skia::ImageOperations::RESIZE_GOOD,
                                       static_cast<int>(scaled_size.width()),
                                       static_cast<int>(scaled_size.height()));
}

// The delimiter for a stack trace provided by WebKit.
const char kStackFrameDelimiter[] = "\n    at ";

// Get a stack trace from a WebKit console message.
// There are three possible scenarios:
// 1. WebKit gives us a stack trace in |stack_trace|.
// 2. The stack trace is embedded in the error |message| by an internal
//    script. This will be more useful than |stack_trace|, since |stack_trace|
//    will include the internal bindings trace, instead of a developer's code.
// 3. No stack trace is included. In this case, we should mock one up from
//    the given line number and source.
// |message| will be populated with the error message only (i.e., will not
// include any stack trace).
extensions::StackTrace GetStackTraceFromMessage(
    base::string16* message,
    const base::string16& source,
    const base::string16& stack_trace,
    int32 line_number) {
  extensions::StackTrace result;
  std::vector<base::string16> pieces;
  size_t index = 0;

  if (message->find(base::UTF8ToUTF16(kStackFrameDelimiter)) !=
          base::string16::npos) {
    base::SplitStringUsingSubstr(*message,
                                 base::UTF8ToUTF16(kStackFrameDelimiter),
                                 &pieces);
    *message = pieces[0];
    index = 1;
  } else if (!stack_trace.empty()) {
    base::SplitStringUsingSubstr(stack_trace,
                                 base::UTF8ToUTF16(kStackFrameDelimiter),
                                 &pieces);
  }

  // If we got a stack trace, parse each frame from the text.
  if (index < pieces.size()) {
    for (; index < pieces.size(); ++index) {
      scoped_ptr<extensions::StackFrame> frame =
          extensions::StackFrame::CreateFromText(pieces[index]);
      if (frame.get())
        result.push_back(*frame);
    }
  }

  if (result.empty()) {  // If we don't have a stack trace, mock one up.
    result.push_back(
        extensions::StackFrame(line_number,
                               1u,  // column number
                               source,
                               base::string16() /* no function name */ ));
  }

  return result;
}

}  // namespace

ChromeRenderViewObserver::ChromeRenderViewObserver(
    content::RenderView* render_view,
    ChromeRenderProcessObserver* chrome_render_process_observer)
    : content::RenderViewObserver(render_view),
      chrome_render_process_observer_(chrome_render_process_observer),
      translate_helper_(new TranslateHelper(render_view)),
      phishing_classifier_(NULL),
      last_indexed_page_id_(-1),
      capture_timer_(false, false) {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  if (!command_line.HasSwitch(switches::kDisableClientSidePhishingDetection))
    OnSetClientSidePhishingDetection(true);
}

ChromeRenderViewObserver::~ChromeRenderViewObserver() {
}

bool ChromeRenderViewObserver::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ChromeRenderViewObserver, message)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_WebUIJavaScript, OnWebUIJavaScript)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_HandleMessageFromExternalHost,
                        OnHandleMessageFromExternalHost)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_JavaScriptStressTestControl,
                        OnJavaScriptStressTestControl)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_SetClientSidePhishingDetection,
                        OnSetClientSidePhishingDetection)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_SetVisuallyDeemphasized,
                        OnSetVisuallyDeemphasized)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_RequestThumbnailForContextNode,
                        OnRequestThumbnailForContextNode)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_GetFPS, OnGetFPS)
#if defined(OS_ANDROID)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_UpdateTopControlsState,
                        OnUpdateTopControlsState)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_RetrieveWebappInformation,
                        OnRetrieveWebappInformation)
#endif
    IPC_MESSAGE_HANDLER(ChromeViewMsg_SetWindowFeatures, OnSetWindowFeatures)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void ChromeRenderViewObserver::OnWebUIJavaScript(
    const base::string16& frame_xpath,
    const base::string16& jscript,
    int id,
    bool notify_result) {
  webui_javascript_.reset(new WebUIJavaScript());
  webui_javascript_->frame_xpath = frame_xpath;
  webui_javascript_->jscript = jscript;
  webui_javascript_->id = id;
  webui_javascript_->notify_result = notify_result;
}

void ChromeRenderViewObserver::OnHandleMessageFromExternalHost(
    const std::string& message,
    const std::string& origin,
    const std::string& target) {
  if (message.empty())
    return;
  GetExternalHostBindings()->ForwardMessageFromExternalHost(message, origin,
                                                            target);
}

void ChromeRenderViewObserver::OnJavaScriptStressTestControl(int cmd,
                                                             int param) {
  if (cmd == kJavaScriptStressTestSetStressRunType) {
    v8::Testing::SetStressRunType(static_cast<v8::Testing::StressType>(param));
  } else if (cmd == kJavaScriptStressTestPrepareStressRun) {
    v8::Testing::PrepareStressRun(param);
  }
}

#if defined(OS_ANDROID)
void ChromeRenderViewObserver::OnUpdateTopControlsState(
    content::TopControlsState constraints,
    content::TopControlsState current,
    bool animate) {
  render_view()->UpdateTopControlsState(constraints, current, animate);
}

void ChromeRenderViewObserver::OnRetrieveWebappInformation(
    const GURL& expected_url) {
  WebFrame* main_frame = render_view()->GetWebView()->mainFrame();
  WebDocument document =
      main_frame ? main_frame->document() : WebDocument();

  WebElement head = document.isNull() ? WebElement() : document.head();
  GURL document_url = document.isNull() ? GURL() : GURL(document.url());

  // Make sure we're checking the right page.
  bool success = document_url == expected_url;

  bool is_mobile_webapp_capable = false;
  bool is_apple_mobile_webapp_capable = false;

  // Search the DOM for the webapp <meta> tags.
  if (!head.isNull()) {
    WebNodeList children = head.childNodes();
    for (unsigned i = 0; i < children.length(); ++i) {
      WebNode child = children.item(i);
      if (!child.isElementNode())
        continue;
      WebElement elem = child.to<WebElement>();

      if (elem.hasTagName("meta") && elem.hasAttribute("name")) {
        std::string name = elem.getAttribute("name").utf8();
        WebString content = elem.getAttribute("content");
        if (LowerCaseEqualsASCII(content, "yes")) {
          if (name == "mobile-web-app-capable") {
            is_mobile_webapp_capable = true;
          } else if (name == "apple-mobile-web-app-capable") {
            is_apple_mobile_webapp_capable = true;
          }
        }
      }
    }
  } else {
    success = false;
  }

  bool is_only_apple_mobile_webapp_capable =
      is_apple_mobile_webapp_capable && !is_mobile_webapp_capable;
  if (main_frame && is_only_apple_mobile_webapp_capable) {
    blink::WebConsoleMessage message(
        blink::WebConsoleMessage::LevelWarning,
        "<meta name=\"apple-mobile-web-app-capable\" content=\"yes\"> is "
        "deprecated. Please include <meta name=\"mobile-web-app-capable\" "
        "content=\"yes\"> - "
        "http://developers.google.com/chrome/mobile/docs/installtohomescreen");
    main_frame->addMessageToConsole(message);
  }

  Send(new ChromeViewHostMsg_DidRetrieveWebappInformation(
      routing_id(),
      success,
      is_mobile_webapp_capable,
      is_apple_mobile_webapp_capable,
      expected_url));
}
#endif

void ChromeRenderViewObserver::OnSetWindowFeatures(
    const WebWindowFeatures& window_features) {
  render_view()->GetWebView()->setWindowFeatures(window_features);
}

void ChromeRenderViewObserver::Navigate(const GURL& url) {
  // Execute cache clear operations that were postponed until a navigation
  // event (including tab reload).
  if (chrome_render_process_observer_)
    chrome_render_process_observer_->ExecutePendingClearCache();
}

void ChromeRenderViewObserver::OnSetClientSidePhishingDetection(
    bool enable_phishing_detection) {
#if defined(FULL_SAFE_BROWSING) && !defined(OS_CHROMEOS)
  phishing_classifier_ = enable_phishing_detection ?
      safe_browsing::PhishingClassifierDelegate::Create(
          render_view(), NULL) :
      NULL;
#endif
}

void ChromeRenderViewObserver::OnSetVisuallyDeemphasized(bool deemphasized) {
  bool already_deemphasized = !!dimmed_color_overlay_.get();
  if (already_deemphasized == deemphasized)
    return;

  if (deemphasized) {
    // 70% opaque grey.
    SkColor greyish = SkColorSetARGB(178, 0, 0, 0);
    dimmed_color_overlay_.reset(
        new WebViewColorOverlay(render_view(), greyish));
  } else {
    dimmed_color_overlay_.reset();
  }
}

void ChromeRenderViewObserver::OnRequestThumbnailForContextNode(
    int thumbnail_min_area_pixels, gfx::Size thumbnail_max_size_pixels) {
  WebNode context_node = render_view()->GetContextMenuNode();
  SkBitmap thumbnail;
  gfx::Size original_size;
  if (!context_node.isNull() && context_node.isElementNode()) {
    blink::WebImage image = context_node.to<WebElement>().imageContents();
    original_size = image.size();
    thumbnail = Downscale(image,
                          thumbnail_min_area_pixels,
                          thumbnail_max_size_pixels);
  }
  Send(new ChromeViewHostMsg_RequestThumbnailForContextNode_ACK(
      routing_id(), thumbnail, original_size));
}

void ChromeRenderViewObserver::OnGetFPS() {
  float fps = (render_view()->GetFilteredTimePerFrame() > 0.0f)?
      1.0f / render_view()->GetFilteredTimePerFrame() : 0.0f;
  Send(new ChromeViewHostMsg_FPS(routing_id(), fps));
}

void ChromeRenderViewObserver::DidStartLoading() {
  if ((render_view()->GetEnabledBindings() & content::BINDINGS_POLICY_WEB_UI) &&
      webui_javascript_.get()) {
    render_view()->EvaluateScript(webui_javascript_->frame_xpath,
                                  webui_javascript_->jscript,
                                  webui_javascript_->id,
                                  webui_javascript_->notify_result);
    webui_javascript_.reset();
  }
}

void ChromeRenderViewObserver::DidStopLoading() {
  WebFrame* main_frame = render_view()->GetWebView()->mainFrame();
  GURL osd_url = main_frame->document().openSearchDescriptionURL();
  if (!osd_url.is_empty()) {
    Send(new ChromeViewHostMsg_PageHasOSDD(
        routing_id(), render_view()->GetPageId(), osd_url,
        search_provider::AUTODETECTED_PROVIDER));
  }

  // Don't capture pages including refresh meta tag.
  if (HasRefreshMetaTag(main_frame))
    return;

  CapturePageInfoLater(
      render_view()->GetPageId(),
      false,  // preliminary_capture
      base::TimeDelta::FromMilliseconds(
          render_view()->GetContentStateImmediately() ?
              0 : kDelayForCaptureMs));
}

void ChromeRenderViewObserver::DidCommitProvisionalLoad(
    WebFrame* frame, bool is_new_navigation) {
  // Don't capture pages being not new, or including refresh meta tag.
  if (!is_new_navigation || HasRefreshMetaTag(frame))
    return;

  CapturePageInfoLater(
      render_view()->GetPageId(),
      true,  // preliminary_capture
      base::TimeDelta::FromMilliseconds(kDelayForForcedCaptureMs));
}

void ChromeRenderViewObserver::DidClearWindowObject(WebFrame* frame) {
  if (render_view()->GetEnabledBindings() &
          content::BINDINGS_POLICY_EXTERNAL_HOST) {
    GetExternalHostBindings()->BindToJavascript(frame, "externalHost");
  }
}

void ChromeRenderViewObserver::DetailedConsoleMessageAdded(
    const base::string16& message,
    const base::string16& source,
    const base::string16& stack_trace_string,
    int32 line_number,
    int32 severity_level) {
  base::string16 trimmed_message = message;
  extensions::StackTrace stack_trace = GetStackTraceFromMessage(
      &trimmed_message,
      source,
      stack_trace_string,
      line_number);
  Send(new ChromeViewHostMsg_DetailedConsoleMessageAdded(routing_id(),
                                                         trimmed_message,
                                                         source,
                                                         stack_trace,
                                                         severity_level));
}

void ChromeRenderViewObserver::CapturePageInfoLater(int page_id,
                                                    bool preliminary_capture,
                                                    base::TimeDelta delay) {
  capture_timer_.Start(
      FROM_HERE,
      delay,
      base::Bind(&ChromeRenderViewObserver::CapturePageInfo,
                 base::Unretained(this),
                 page_id,
                 preliminary_capture));
}

void ChromeRenderViewObserver::CapturePageInfo(int page_id,
                                               bool preliminary_capture) {
  // If |page_id| is obsolete, we should stop indexing and capturing a page.
  if (render_view()->GetPageId() != page_id)
    return;

  if (!render_view()->GetWebView())
    return;

  WebFrame* main_frame = render_view()->GetWebView()->mainFrame();
  if (!main_frame)
    return;

  // Don't index/capture pages that are in view source mode.
  if (main_frame->isViewSourceModeEnabled())
    return;

  // Don't index/capture pages that failed to load.  This only checks the top
  // level frame so the thumbnail may contain a frame that failed to load.
  WebDataSource* ds = main_frame->dataSource();
  if (ds && ds->hasUnreachableURL())
    return;

  // Don't index/capture pages that are being prerendered.
  if (prerender::PrerenderHelper::IsPrerendering(
          render_view()->GetMainRenderFrame())) {
    return;
  }

  // Retrieve the frame's full text (up to kMaxIndexChars), and pass it to the
  // translate helper for language detection and possible translation.
  base::string16 contents;
  base::TimeTicks capture_begin_time = base::TimeTicks::Now();
  CaptureText(main_frame, &contents);
  UMA_HISTOGRAM_TIMES(kTranslateCaptureText,
                      base::TimeTicks::Now() - capture_begin_time);
  if (translate_helper_)
    translate_helper_->PageCaptured(page_id, contents);

  // TODO(shess): Is indexing "Full text search" indexing?  In that
  // case more of this can go.
  // Skip indexing if this is not a new load.  Note that the case where
  // page_id == last_indexed_page_id_ is more complicated, since we need to
  // reindex if the toplevel URL has changed (such as from a redirect), even
  // though this may not cause the page id to be incremented.
  if (page_id < last_indexed_page_id_)
    return;

  bool same_page_id = last_indexed_page_id_ == page_id;
  if (!preliminary_capture)
    last_indexed_page_id_ = page_id;

  // Get the URL for this page.
  GURL url(main_frame->document().url());
  if (url.is_empty()) {
    if (!preliminary_capture)
      last_indexed_url_ = GURL();
    return;
  }

  // If the page id is unchanged, check whether the URL (ignoring fragments)
  // has changed.  If so, we need to reindex.  Otherwise, assume this is a
  // reload, in-page navigation, or some other load type where we don't want to
  // reindex.  Note: subframe navigations after onload increment the page id,
  // so these will trigger a reindex.
  GURL stripped_url(StripRef(url));
  if (same_page_id && stripped_url == last_indexed_url_)
    return;

  if (!preliminary_capture)
    last_indexed_url_ = stripped_url;

  TRACE_EVENT0("renderer", "ChromeRenderViewObserver::CapturePageInfo");

#if defined(FULL_SAFE_BROWSING)
  // Will swap out the string.
  if (phishing_classifier_)
    phishing_classifier_->PageCaptured(&contents, preliminary_capture);
#endif
}

void ChromeRenderViewObserver::CaptureText(WebFrame* frame,
                                           base::string16* contents) {
  contents->clear();
  if (!frame)
    return;

#ifdef TIME_TEXT_RETRIEVAL
  double begin = time_util::GetHighResolutionTimeNow();
#endif

  // get the contents of the frame
  *contents = frame->contentAsText(kMaxIndexChars);

#ifdef TIME_TEXT_RETRIEVAL
  double end = time_util::GetHighResolutionTimeNow();
  char buf[128];
  sprintf_s(buf, "%d chars retrieved for indexing in %gms\n",
            contents.size(), (end - begin)*1000);
  OutputDebugStringA(buf);
#endif

  // When the contents are clipped to the maximum, we don't want to have a
  // partial word indexed at the end that might have been clipped. Therefore,
  // terminate the string at the last space to ensure no words are clipped.
  if (contents->size() == kMaxIndexChars) {
    size_t last_space_index = contents->find_last_of(base::kWhitespaceUTF16);
    if (last_space_index == base::string16::npos)
      return;  // don't index if we got a huge block of text with no spaces
    contents->resize(last_space_index);
  }
}

ExternalHostBindings* ChromeRenderViewObserver::GetExternalHostBindings() {
  if (!external_host_bindings_.get()) {
    external_host_bindings_.reset(new ExternalHostBindings(
        render_view(), routing_id()));
  }
  return external_host_bindings_.get();
}

bool ChromeRenderViewObserver::HasRefreshMetaTag(WebFrame* frame) {
  if (!frame)
    return false;
  WebElement head = frame->document().head();
  if (head.isNull() || !head.hasChildNodes())
    return false;

  const WebString tag_name(ASCIIToUTF16("meta"));
  const WebString attribute_name(ASCIIToUTF16("http-equiv"));

  WebNodeList children = head.childNodes();
  for (size_t i = 0; i < children.length(); ++i) {
    WebNode node = children.item(i);
    if (!node.isElementNode())
      continue;
    WebElement element = node.to<WebElement>();
    if (!element.hasTagName(tag_name))
      continue;
    WebString value = element.getAttribute(attribute_name);
    if (value.isNull() || !LowerCaseEqualsASCII(value, "refresh"))
      continue;
    return true;
  }
  return false;
}
