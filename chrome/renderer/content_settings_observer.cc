// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/content_settings_observer.h"

#include "base/command_line.h"
#include "base/metrics/histogram.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/url_constants.h"
#include "chrome/renderer/extensions/dispatcher.h"
#include "content/public/renderer/document_state.h"
#include "content/public/renderer/navigation_state.h"
#include "content/public/renderer/render_view.h"
#include "extensions/common/constants.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/web/WebDataSource.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebFrameClient.h"
#include "third_party/WebKit/public/web/WebSecurityOrigin.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "webkit/child/weburlresponse_extradata_impl.h"

using blink::WebDataSource;
using blink::WebDocument;
using blink::WebFrame;
using blink::WebFrameClient;
using blink::WebSecurityOrigin;
using blink::WebString;
using blink::WebURL;
using blink::WebView;
using content::DocumentState;
using content::NavigationState;
using extensions::APIPermission;

namespace {

enum {
  INSECURE_CONTENT_DISPLAY = 0,
  INSECURE_CONTENT_DISPLAY_HOST_GOOGLE,
  INSECURE_CONTENT_DISPLAY_HOST_WWW_GOOGLE,
  INSECURE_CONTENT_DISPLAY_HTML,
  INSECURE_CONTENT_RUN,
  INSECURE_CONTENT_RUN_HOST_GOOGLE,
  INSECURE_CONTENT_RUN_HOST_WWW_GOOGLE,
  INSECURE_CONTENT_RUN_TARGET_YOUTUBE,
  INSECURE_CONTENT_RUN_JS,
  INSECURE_CONTENT_RUN_CSS,
  INSECURE_CONTENT_RUN_SWF,
  INSECURE_CONTENT_DISPLAY_HOST_YOUTUBE,
  INSECURE_CONTENT_RUN_HOST_YOUTUBE,
  INSECURE_CONTENT_RUN_HOST_GOOGLEUSERCONTENT,
  INSECURE_CONTENT_DISPLAY_HOST_MAIL_GOOGLE,
  INSECURE_CONTENT_RUN_HOST_MAIL_GOOGLE,
  INSECURE_CONTENT_DISPLAY_HOST_PLUS_GOOGLE,
  INSECURE_CONTENT_RUN_HOST_PLUS_GOOGLE,
  INSECURE_CONTENT_DISPLAY_HOST_DOCS_GOOGLE,
  INSECURE_CONTENT_RUN_HOST_DOCS_GOOGLE,
  INSECURE_CONTENT_DISPLAY_HOST_SITES_GOOGLE,
  INSECURE_CONTENT_RUN_HOST_SITES_GOOGLE,
  INSECURE_CONTENT_DISPLAY_HOST_PICASAWEB_GOOGLE,
  INSECURE_CONTENT_RUN_HOST_PICASAWEB_GOOGLE,
  INSECURE_CONTENT_DISPLAY_HOST_GOOGLE_READER,
  INSECURE_CONTENT_RUN_HOST_GOOGLE_READER,
  INSECURE_CONTENT_DISPLAY_HOST_CODE_GOOGLE,
  INSECURE_CONTENT_RUN_HOST_CODE_GOOGLE,
  INSECURE_CONTENT_DISPLAY_HOST_GROUPS_GOOGLE,
  INSECURE_CONTENT_RUN_HOST_GROUPS_GOOGLE,
  INSECURE_CONTENT_DISPLAY_HOST_MAPS_GOOGLE,
  INSECURE_CONTENT_RUN_HOST_MAPS_GOOGLE,
  INSECURE_CONTENT_DISPLAY_HOST_GOOGLE_SUPPORT,
  INSECURE_CONTENT_RUN_HOST_GOOGLE_SUPPORT,
  INSECURE_CONTENT_DISPLAY_HOST_GOOGLE_INTL,
  INSECURE_CONTENT_RUN_HOST_GOOGLE_INTL,
  INSECURE_CONTENT_NUM_EVENTS
};

// Constants for UMA statistic collection.
static const char kWWWDotGoogleDotCom[] = "www.google.com";
static const char kMailDotGoogleDotCom[] = "mail.google.com";
static const char kPlusDotGoogleDotCom[] = "plus.google.com";
static const char kDocsDotGoogleDotCom[] = "docs.google.com";
static const char kSitesDotGoogleDotCom[] = "sites.google.com";
static const char kPicasawebDotGoogleDotCom[] = "picasaweb.google.com";
static const char kCodeDotGoogleDotCom[] = "code.google.com";
static const char kGroupsDotGoogleDotCom[] = "groups.google.com";
static const char kMapsDotGoogleDotCom[] = "maps.google.com";
static const char kWWWDotYoutubeDotCom[] = "www.youtube.com";
static const char kDotGoogleUserContentDotCom[] = ".googleusercontent.com";
static const char kGoogleReaderPathPrefix[] = "/reader/";
static const char kGoogleSupportPathPrefix[] = "/support/";
static const char kGoogleIntlPathPrefix[] = "/intl/";
static const char kDotJS[] = ".js";
static const char kDotCSS[] = ".css";
static const char kDotSWF[] = ".swf";
static const char kDotHTML[] = ".html";

// Constants for mixed-content blocking.
static const char kGoogleDotCom[] = "google.com";

static bool IsHostInDomain(const std::string& host, const std::string& domain) {
  return (EndsWith(host, domain, false) &&
          (host.length() == domain.length() ||
           (host.length() > domain.length() &&
            host[host.length() - domain.length() - 1] == '.')));
}

GURL GetOriginOrURL(const WebFrame* frame) {
  WebString top_origin = frame->top()->document().securityOrigin().toString();
  // The the |top_origin| is unique ("null") e.g., for file:// URLs. Use the
  // document URL as the primary URL in those cases.
  if (top_origin == "null")
    return frame->top()->document().url();
  return GURL(top_origin);
}

ContentSetting GetContentSettingFromRules(
    const ContentSettingsForOneType& rules,
    const WebFrame* frame,
    const GURL& secondary_url) {
  ContentSettingsForOneType::const_iterator it;
  // If there is only one rule, it's the default rule and we don't need to match
  // the patterns.
  if (rules.size() == 1) {
    DCHECK(rules[0].primary_pattern == ContentSettingsPattern::Wildcard());
    DCHECK(rules[0].secondary_pattern == ContentSettingsPattern::Wildcard());
    return rules[0].setting;
  }
  const GURL& primary_url = GetOriginOrURL(frame);
  for (it = rules.begin(); it != rules.end(); ++it) {
    if (it->primary_pattern.Matches(primary_url) &&
        it->secondary_pattern.Matches(secondary_url)) {
      return it->setting;
    }
  }
  NOTREACHED();
  return CONTENT_SETTING_DEFAULT;
}

}  // namespace

ContentSettingsObserver::ContentSettingsObserver(
    content::RenderView* render_view,
    extensions::Dispatcher* extension_dispatcher)
    : content::RenderViewObserver(render_view),
      content::RenderViewObserverTracker<ContentSettingsObserver>(render_view),
      extension_dispatcher_(extension_dispatcher),
      allow_displaying_insecure_content_(false),
      allow_running_insecure_content_(false),
      content_setting_rules_(NULL),
      is_interstitial_page_(false),
      npapi_plugins_blocked_(false) {
  ClearBlockedContentSettings();
  render_view->GetWebView()->setPermissionClient(this);
}

ContentSettingsObserver::~ContentSettingsObserver() {
}

void ContentSettingsObserver::SetContentSettingRules(
    const RendererContentSettingRules* content_setting_rules) {
  content_setting_rules_ = content_setting_rules;
}

bool ContentSettingsObserver::IsPluginTemporarilyAllowed(
    const std::string& identifier) {
  // If the empty string is in here, it means all plug-ins are allowed.
  // TODO(bauerb): Remove this once we only pass in explicit identifiers.
  return (temporarily_allowed_plugins_.find(identifier) !=
          temporarily_allowed_plugins_.end()) ||
         (temporarily_allowed_plugins_.find(std::string()) !=
          temporarily_allowed_plugins_.end());
}

void ContentSettingsObserver::DidBlockContentType(
    ContentSettingsType settings_type) {
  if (!content_blocked_[settings_type]) {
    content_blocked_[settings_type] = true;
    Send(new ChromeViewHostMsg_ContentBlocked(routing_id(), settings_type));
  }
}

bool ContentSettingsObserver::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ContentSettingsObserver, message)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_SetAsInterstitial, OnSetAsInterstitial)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_NPAPINotSupported, OnNPAPINotSupported)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_SetAllowDisplayingInsecureContent,
                        OnSetAllowDisplayingInsecureContent)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_SetAllowRunningInsecureContent,
                        OnSetAllowRunningInsecureContent)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  if (handled)
    return true;

  // Don't swallow LoadBlockedPlugins messages, as they're sent to every
  // blocked plugin.
  IPC_BEGIN_MESSAGE_MAP(ContentSettingsObserver, message)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_LoadBlockedPlugins, OnLoadBlockedPlugins)
  IPC_END_MESSAGE_MAP()

  return false;
}

void ContentSettingsObserver::DidCommitProvisionalLoad(
    WebFrame* frame, bool is_new_navigation) {
  if (frame->parent())
    return;  // Not a top-level navigation.

  DocumentState* document_state = DocumentState::FromDataSource(
      frame->dataSource());
  NavigationState* navigation_state = document_state->navigation_state();
  if (!navigation_state->was_within_same_page()) {
    // Clear "block" flags for the new page. This needs to happen before any of
    // |allowScript()|, |allowScriptFromSource()|, |allowImage()|, or
    // |allowPlugins()| is called for the new page so that these functions can
    // correctly detect that a piece of content flipped from "not blocked" to
    // "blocked".
    ClearBlockedContentSettings();
    temporarily_allowed_plugins_.clear();
  }

  GURL url = frame->document().url();
  // If we start failing this DCHECK, please makes sure we don't regress
  // this bug: http://code.google.com/p/chromium/issues/detail?id=79304
  DCHECK(frame->document().securityOrigin().toString() == "null" ||
         !url.SchemeIs(chrome::kDataScheme));
}

bool ContentSettingsObserver::allowDatabase(WebFrame* frame,
                                            const WebString& name,
                                            const WebString& display_name,
                                            unsigned long estimated_size) {
  if (frame->document().securityOrigin().isUnique() ||
      frame->top()->document().securityOrigin().isUnique())
    return false;

  bool result = false;
  Send(new ChromeViewHostMsg_AllowDatabase(
      routing_id(), GURL(frame->document().securityOrigin().toString()),
      GURL(frame->top()->document().securityOrigin().toString()),
      name, display_name, &result));
  return result;
}

bool ContentSettingsObserver::allowFileSystem(WebFrame* frame) {
  if (frame->document().securityOrigin().isUnique() ||
      frame->top()->document().securityOrigin().isUnique())
    return false;

  bool result = false;
  Send(new ChromeViewHostMsg_AllowFileSystem(
      routing_id(), GURL(frame->document().securityOrigin().toString()),
      GURL(frame->top()->document().securityOrigin().toString()), &result));
  return result;
}

bool ContentSettingsObserver::allowImage(WebFrame* frame,
                                         bool enabled_per_settings,
                                         const WebURL& image_url) {
  bool allow = enabled_per_settings;
  if (enabled_per_settings) {
    if (is_interstitial_page_)
      return true;
    if (IsWhitelistedForContentSettings(frame))
      return true;

    if (content_setting_rules_) {
      GURL secondary_url(image_url);
      allow = GetContentSettingFromRules(
          content_setting_rules_->image_rules,
          frame, secondary_url) != CONTENT_SETTING_BLOCK;
    }
  }
  if (!allow)
    DidBlockContentType(CONTENT_SETTINGS_TYPE_IMAGES);
  return allow;
}

bool ContentSettingsObserver::allowIndexedDB(WebFrame* frame,
                                             const WebString& name,
                                             const WebSecurityOrigin& origin) {
  if (frame->document().securityOrigin().isUnique() ||
      frame->top()->document().securityOrigin().isUnique())
    return false;

  bool result = false;
  Send(new ChromeViewHostMsg_AllowIndexedDB(
      routing_id(), GURL(frame->document().securityOrigin().toString()),
      GURL(frame->top()->document().securityOrigin().toString()),
      name, &result));
  return result;
}

bool ContentSettingsObserver::allowPlugins(WebFrame* frame,
                                           bool enabled_per_settings) {
  return enabled_per_settings;
}

bool ContentSettingsObserver::allowScript(WebFrame* frame,
                                          bool enabled_per_settings) {
  if (!enabled_per_settings)
    return false;
  if (is_interstitial_page_)
    return true;

  std::map<WebFrame*, bool>::const_iterator it =
      cached_script_permissions_.find(frame);
  if (it != cached_script_permissions_.end())
    return it->second;

  // Evaluate the content setting rules before
  // |IsWhitelistedForContentSettings|; if there is only the default rule
  // allowing all scripts, it's quicker this way.
  bool allow = true;
  if (content_setting_rules_) {
    ContentSetting setting = GetContentSettingFromRules(
        content_setting_rules_->script_rules,
        frame,
        GURL(frame->document().securityOrigin().toString()));
    allow = setting != CONTENT_SETTING_BLOCK;
  }
  allow = allow || IsWhitelistedForContentSettings(frame);

  cached_script_permissions_[frame] = allow;
  return allow;
}

bool ContentSettingsObserver::allowScriptFromSource(
    WebFrame* frame,
    bool enabled_per_settings,
    const blink::WebURL& script_url) {
  if (!enabled_per_settings)
    return false;
  if (is_interstitial_page_)
    return true;

  bool allow = true;
  if (content_setting_rules_) {
    ContentSetting setting = GetContentSettingFromRules(
        content_setting_rules_->script_rules,
        frame,
        GURL(script_url));
    allow = setting != CONTENT_SETTING_BLOCK;
  }
  return allow || IsWhitelistedForContentSettings(frame);
}

bool ContentSettingsObserver::allowStorage(WebFrame* frame, bool local) {
  if (frame->document().securityOrigin().isUnique() ||
      frame->top()->document().securityOrigin().isUnique())
    return false;
  bool result = false;

  StoragePermissionsKey key(
      GURL(frame->document().securityOrigin().toString()), local);
  std::map<StoragePermissionsKey, bool>::const_iterator permissions =
      cached_storage_permissions_.find(key);
  if (permissions != cached_storage_permissions_.end())
    return permissions->second;

  Send(new ChromeViewHostMsg_AllowDOMStorage(
      routing_id(), GURL(frame->document().securityOrigin().toString()),
      GURL(frame->top()->document().securityOrigin().toString()),
      local, &result));
  cached_storage_permissions_[key] = result;
  return result;
}

bool ContentSettingsObserver::allowReadFromClipboard(WebFrame* frame,
                                                     bool default_value) {
  bool allowed = false;
  // TODO(dcheng): Should we consider a toURL() method on WebSecurityOrigin?
  Send(new ChromeViewHostMsg_CanTriggerClipboardRead(
      routing_id(), GURL(frame->document().securityOrigin().toString().utf8()),
      &allowed));
  return allowed;
}

bool ContentSettingsObserver::allowWriteToClipboard(WebFrame* frame,
                                                    bool default_value) {
  bool allowed = false;
  Send(new ChromeViewHostMsg_CanTriggerClipboardWrite(
      routing_id(), GURL(frame->document().securityOrigin().toString().utf8()),
      &allowed));
  return allowed;
}

bool ContentSettingsObserver::allowWebComponents(WebFrame* frame,
                                                 bool defaultValue) {
  if (defaultValue)
    return true;

  WebSecurityOrigin origin = frame->document().securityOrigin();
  if (EqualsASCII(origin.protocol(), chrome::kChromeUIScheme))
    return true;

  if (const extensions::Extension* extension = GetExtension(origin)) {
    if (extension->HasAPIPermission(APIPermission::kExperimental))
      return true;
  }

  return false;
}

bool ContentSettingsObserver::allowMutationEvents(WebFrame* frame,
                                                  bool default_value) {
  WebSecurityOrigin origin = frame->document().securityOrigin();
  const extensions::Extension* extension = GetExtension(origin);
  if (extension && extension->is_platform_app())
    return false;
  return default_value;
}

bool ContentSettingsObserver::allowPushState(WebFrame* frame) {
  WebSecurityOrigin origin = frame->document().securityOrigin();
  const extensions::Extension* extension = GetExtension(origin);
  return !extension || !extension->is_platform_app();
}

static void SendInsecureContentSignal(int signal) {
  UMA_HISTOGRAM_ENUMERATION("SSL.InsecureContent", signal,
                            INSECURE_CONTENT_NUM_EVENTS);
}

bool ContentSettingsObserver::allowDisplayingInsecureContent(
    blink::WebFrame* frame,
    bool allowed_per_settings,
    const blink::WebSecurityOrigin& origin,
    const blink::WebURL& resource_url) {
  SendInsecureContentSignal(INSECURE_CONTENT_DISPLAY);

  std::string origin_host(origin.host().utf8());
  GURL frame_gurl(frame->document().url());
  if (IsHostInDomain(origin_host, kGoogleDotCom)) {
    SendInsecureContentSignal(INSECURE_CONTENT_DISPLAY_HOST_GOOGLE);
    if (StartsWithASCII(frame_gurl.path(), kGoogleSupportPathPrefix, false)) {
      SendInsecureContentSignal(INSECURE_CONTENT_DISPLAY_HOST_GOOGLE_SUPPORT);
    } else if (StartsWithASCII(frame_gurl.path(),
                               kGoogleIntlPathPrefix,
                               false)) {
      SendInsecureContentSignal(INSECURE_CONTENT_DISPLAY_HOST_GOOGLE_INTL);
    }
  }

  if (origin_host == kWWWDotGoogleDotCom) {
    SendInsecureContentSignal(INSECURE_CONTENT_DISPLAY_HOST_WWW_GOOGLE);
    if (StartsWithASCII(frame_gurl.path(), kGoogleReaderPathPrefix, false))
      SendInsecureContentSignal(INSECURE_CONTENT_DISPLAY_HOST_GOOGLE_READER);
  } else if (origin_host == kMailDotGoogleDotCom) {
    SendInsecureContentSignal(INSECURE_CONTENT_DISPLAY_HOST_MAIL_GOOGLE);
  } else if (origin_host == kPlusDotGoogleDotCom) {
    SendInsecureContentSignal(INSECURE_CONTENT_DISPLAY_HOST_PLUS_GOOGLE);
  } else if (origin_host == kDocsDotGoogleDotCom) {
    SendInsecureContentSignal(INSECURE_CONTENT_DISPLAY_HOST_DOCS_GOOGLE);
  } else if (origin_host == kSitesDotGoogleDotCom) {
    SendInsecureContentSignal(INSECURE_CONTENT_DISPLAY_HOST_SITES_GOOGLE);
  } else if (origin_host == kPicasawebDotGoogleDotCom) {
    SendInsecureContentSignal(INSECURE_CONTENT_DISPLAY_HOST_PICASAWEB_GOOGLE);
  } else if (origin_host == kCodeDotGoogleDotCom) {
    SendInsecureContentSignal(INSECURE_CONTENT_DISPLAY_HOST_CODE_GOOGLE);
  } else if (origin_host == kGroupsDotGoogleDotCom) {
    SendInsecureContentSignal(INSECURE_CONTENT_DISPLAY_HOST_GROUPS_GOOGLE);
  } else if (origin_host == kMapsDotGoogleDotCom) {
    SendInsecureContentSignal(INSECURE_CONTENT_DISPLAY_HOST_MAPS_GOOGLE);
  } else if (origin_host == kWWWDotYoutubeDotCom) {
    SendInsecureContentSignal(INSECURE_CONTENT_DISPLAY_HOST_YOUTUBE);
  }

  GURL resource_gurl(resource_url);
  if (EndsWith(resource_gurl.path(), kDotHTML, false))
    SendInsecureContentSignal(INSECURE_CONTENT_DISPLAY_HTML);

  if (allowed_per_settings || allow_displaying_insecure_content_)
    return true;

  Send(new ChromeViewHostMsg_DidBlockDisplayingInsecureContent(routing_id()));

  return false;
}

bool ContentSettingsObserver::allowRunningInsecureContent(
    blink::WebFrame* frame,
    bool allowed_per_settings,
    const blink::WebSecurityOrigin& origin,
    const blink::WebURL& resource_url) {
  std::string origin_host(origin.host().utf8());
  GURL frame_gurl(frame->document().url());
  DCHECK_EQ(frame_gurl.host(), origin_host);

  bool is_google = IsHostInDomain(origin_host, kGoogleDotCom);
  if (is_google) {
    SendInsecureContentSignal(INSECURE_CONTENT_RUN_HOST_GOOGLE);
    if (StartsWithASCII(frame_gurl.path(), kGoogleSupportPathPrefix, false)) {
      SendInsecureContentSignal(INSECURE_CONTENT_RUN_HOST_GOOGLE_SUPPORT);
    } else if (StartsWithASCII(frame_gurl.path(),
                               kGoogleIntlPathPrefix,
                               false)) {
      SendInsecureContentSignal(INSECURE_CONTENT_RUN_HOST_GOOGLE_INTL);
    }
  }

  if (origin_host == kWWWDotGoogleDotCom) {
    SendInsecureContentSignal(INSECURE_CONTENT_RUN_HOST_WWW_GOOGLE);
    if (StartsWithASCII(frame_gurl.path(), kGoogleReaderPathPrefix, false))
      SendInsecureContentSignal(INSECURE_CONTENT_RUN_HOST_GOOGLE_READER);
  } else if (origin_host == kMailDotGoogleDotCom) {
    SendInsecureContentSignal(INSECURE_CONTENT_RUN_HOST_MAIL_GOOGLE);
  } else if (origin_host == kPlusDotGoogleDotCom) {
    SendInsecureContentSignal(INSECURE_CONTENT_RUN_HOST_PLUS_GOOGLE);
  } else if (origin_host == kDocsDotGoogleDotCom) {
    SendInsecureContentSignal(INSECURE_CONTENT_RUN_HOST_DOCS_GOOGLE);
  } else if (origin_host == kSitesDotGoogleDotCom) {
    SendInsecureContentSignal(INSECURE_CONTENT_RUN_HOST_SITES_GOOGLE);
  } else if (origin_host == kPicasawebDotGoogleDotCom) {
    SendInsecureContentSignal(INSECURE_CONTENT_RUN_HOST_PICASAWEB_GOOGLE);
  } else if (origin_host == kCodeDotGoogleDotCom) {
    SendInsecureContentSignal(INSECURE_CONTENT_RUN_HOST_CODE_GOOGLE);
  } else if (origin_host == kGroupsDotGoogleDotCom) {
    SendInsecureContentSignal(INSECURE_CONTENT_RUN_HOST_GROUPS_GOOGLE);
  } else if (origin_host == kMapsDotGoogleDotCom) {
    SendInsecureContentSignal(INSECURE_CONTENT_RUN_HOST_MAPS_GOOGLE);
  } else if (origin_host == kWWWDotYoutubeDotCom) {
    SendInsecureContentSignal(INSECURE_CONTENT_RUN_HOST_YOUTUBE);
  } else if (EndsWith(origin_host, kDotGoogleUserContentDotCom, false)) {
    SendInsecureContentSignal(INSECURE_CONTENT_RUN_HOST_GOOGLEUSERCONTENT);
  }

  GURL resource_gurl(resource_url);
  if (resource_gurl.host() == kWWWDotYoutubeDotCom)
    SendInsecureContentSignal(INSECURE_CONTENT_RUN_TARGET_YOUTUBE);

  if (EndsWith(resource_gurl.path(), kDotJS, false))
    SendInsecureContentSignal(INSECURE_CONTENT_RUN_JS);
  else if (EndsWith(resource_gurl.path(), kDotCSS, false))
    SendInsecureContentSignal(INSECURE_CONTENT_RUN_CSS);
  else if (EndsWith(resource_gurl.path(), kDotSWF, false))
    SendInsecureContentSignal(INSECURE_CONTENT_RUN_SWF);

  if (!allow_running_insecure_content_ && !allowed_per_settings) {
    DidBlockContentType(CONTENT_SETTINGS_TYPE_MIXEDSCRIPT);
    return false;
  }

  return true;
}

bool ContentSettingsObserver::allowWebGLDebugRendererInfo(WebFrame* frame) {
  bool allowed = false;
  Send(new ChromeViewHostMsg_IsWebGLDebugRendererInfoAllowed(
      routing_id(),
      GURL(frame->top()->document().securityOrigin().toString().utf8()),
      &allowed));
  return allowed;
}

void ContentSettingsObserver::didNotAllowPlugins(WebFrame* frame) {
  DidBlockContentType(CONTENT_SETTINGS_TYPE_PLUGINS);
}

void ContentSettingsObserver::didNotAllowScript(WebFrame* frame) {
  DidBlockContentType(CONTENT_SETTINGS_TYPE_JAVASCRIPT);
}

bool ContentSettingsObserver::AreNPAPIPluginsBlocked() const {
  return npapi_plugins_blocked_;
}

void ContentSettingsObserver::OnLoadBlockedPlugins(
    const std::string& identifier) {
  temporarily_allowed_plugins_.insert(identifier);
}

void ContentSettingsObserver::OnSetAsInterstitial() {
  is_interstitial_page_ = true;
}

void ContentSettingsObserver::OnNPAPINotSupported() {
  npapi_plugins_blocked_ = true;
}

void ContentSettingsObserver::OnSetAllowDisplayingInsecureContent(bool allow) {
  allow_displaying_insecure_content_ = allow;
  WebFrame* main_frame = render_view()->GetWebView()->mainFrame();
  if (main_frame)
    main_frame->reload();
}

void ContentSettingsObserver::OnSetAllowRunningInsecureContent(bool allow) {
  allow_running_insecure_content_ = allow;
  OnSetAllowDisplayingInsecureContent(allow);
}


void ContentSettingsObserver::ClearBlockedContentSettings() {
  for (size_t i = 0; i < arraysize(content_blocked_); ++i)
    content_blocked_[i] = false;
  cached_storage_permissions_.clear();
  cached_script_permissions_.clear();
}

const extensions::Extension* ContentSettingsObserver::GetExtension(
    const WebSecurityOrigin& origin) const {
  if (!EqualsASCII(origin.protocol(), extensions::kExtensionScheme))
    return NULL;

  const std::string extension_id = origin.host().utf8().data();
  if (!extension_dispatcher_->IsExtensionActive(extension_id))
    return NULL;

  return extension_dispatcher_->extensions()->GetByID(extension_id);
}

bool ContentSettingsObserver::IsWhitelistedForContentSettings(WebFrame* frame) {
  // Whitelist Instant processes.
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kInstantProcess))
    return true;

  // Whitelist ftp directory listings, as they require JavaScript to function
  // properly.
  webkit_glue::WebURLResponseExtraDataImpl* extra_data =
      static_cast<webkit_glue::WebURLResponseExtraDataImpl*>(
          frame->dataSource()->response().extraData());
  if (extra_data && extra_data->is_ftp_directory_listing())
    return true;
  return IsWhitelistedForContentSettings(frame->document().securityOrigin(),
                                         frame->document().url());
}

bool ContentSettingsObserver::IsWhitelistedForContentSettings(
    const WebSecurityOrigin& origin,
    const GURL& document_url) {
  if (document_url == GURL(content::kUnreachableWebDataURL))
    return true;

  if (origin.isUnique())
    return false;  // Uninitialized document?

  if (EqualsASCII(origin.protocol(), chrome::kChromeUIScheme))
    return true;  // Browser UI elements should still work.

  if (EqualsASCII(origin.protocol(), chrome::kChromeDevToolsScheme))
    return true;  // DevTools UI elements should still work.

  if (EqualsASCII(origin.protocol(), extensions::kExtensionScheme))
    return true;

  // TODO(creis, fsamuel): Remove this once the concept of swapped out
  // RenderViews goes away.
  if (document_url == GURL(content::kSwappedOutURL))
    return true;

  // If the scheme is file:, an empty file name indicates a directory listing,
  // which requires JavaScript to function properly.
  if (EqualsASCII(origin.protocol(), chrome::kFileScheme)) {
    return document_url.SchemeIs(chrome::kFileScheme) &&
           document_url.ExtractFileName().empty();
  }

  return false;
}
