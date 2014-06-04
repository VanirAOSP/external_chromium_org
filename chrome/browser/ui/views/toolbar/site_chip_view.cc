// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/site_chip_view.h"

#include "base/files/file_path.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_icon_image.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/favicon/favicon_tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/client_side_detection_host.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/safe_browsing/safe_browsing_tab_observer.h"
#include "chrome/browser/safe_browsing/ui_manager.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/omnibox/omnibox_view.h"
#include "chrome/browser/ui/toolbar/toolbar_model.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/manifest_handlers/icons_handler.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "extensions/common/constants.h"
#include "grit/component_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "net/base/net_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/theme_provider.h"
#include "ui/views/background.h"
#include "ui/views/button_drag_utils.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/label_button_border.h"
#include "ui/views/controls/label.h"
#include "ui/views/painter.h"


// SiteChipExtensionIcon ------------------------------------------------------

class SiteChipExtensionIcon : public extensions::IconImage::Observer {
 public:
  SiteChipExtensionIcon(LocationIconView* icon_view,
                        Profile* profile,
                        const extensions::Extension* extension);
  virtual ~SiteChipExtensionIcon();

  // IconImage::Observer:
  virtual void OnExtensionIconImageChanged(
      extensions::IconImage* image) OVERRIDE;

 private:
  LocationIconView* icon_view_;
  scoped_ptr<extensions::IconImage> icon_image_;

  DISALLOW_COPY_AND_ASSIGN(SiteChipExtensionIcon);
};

SiteChipExtensionIcon::SiteChipExtensionIcon(
    LocationIconView* icon_view,
    Profile* profile,
    const extensions::Extension* extension)
    : icon_view_(icon_view),
      icon_image_(new extensions::IconImage(
          profile,
          extension,
          extensions::IconsInfo::GetIcons(extension),
          extension_misc::EXTENSION_ICON_BITTY,
          extensions::IconsInfo::GetDefaultAppIcon(),
          this)) {
  // Forces load of the image.
  icon_image_->image_skia().GetRepresentation(1.0f);

  if (!icon_image_->image_skia().image_reps().empty())
    OnExtensionIconImageChanged(icon_image_.get());
}

SiteChipExtensionIcon::~SiteChipExtensionIcon() {
}

void SiteChipExtensionIcon::OnExtensionIconImageChanged(
    extensions::IconImage* image) {
  if (icon_view_)
    icon_view_->SetImage(&icon_image_->image_skia());
}


// SiteChipView ---------------------------------------------------------------

namespace {

const int kEdgeThickness = 5;
const int k16x16IconLeadingSpacing = 1;
const int k16x16IconTrailingSpacing = 2;
const int kIconTextSpacing = 3;
const int kTrailingLabelMargin = 0;

const SkColor kEVBackgroundColor = SkColorSetRGB(163, 226, 120);
const SkColor kMalwareBackgroundColor = SkColorSetRGB(145, 0, 0);
const SkColor kBrokenSSLBackgroundColor = SkColorSetRGB(253, 196, 36);

// Detect client-side or SB malware/phishing hits.
bool IsMalware(const GURL& url, content::WebContents* tab) {
  if (tab->GetURL() != url)
    return false;

  safe_browsing::SafeBrowsingTabObserver* sb_observer =
      safe_browsing::SafeBrowsingTabObserver::FromWebContents(tab);
  return sb_observer && sb_observer->detection_host() &&
      sb_observer->detection_host()->DidPageReceiveSafeBrowsingMatch();
}

// For selected kChromeUIScheme and kAboutScheme, return the string resource
// number for the title of the page. If we don't have a specialized title,
// returns -1.
int StringForChromeHost(const GURL& url) {
  DCHECK(url.is_empty() ||
      url.SchemeIs(chrome::kChromeUIScheme) ||
      url.SchemeIs(chrome::kAboutScheme));

  if (url.is_empty())
    return IDS_NEW_TAB_TITLE;

  // TODO(gbillock): Just get the page title and special case exceptions?
  std::string host = url.host();
  if (host == chrome::kChromeUIAppLauncherPageHost)
    return IDS_APP_DEFAULT_PAGE_NAME;
  if (host == chrome::kChromeUIBookmarksHost)
    return IDS_BOOKMARK_MANAGER_TITLE;
  if (host == chrome::kChromeUIComponentsHost)
    return IDS_COMPONENTS_TITLE;
  if (host == chrome::kChromeUICrashesHost)
    return IDS_CRASHES_TITLE;
  if (host == chrome::kChromeUIDevicesHost)
    return IDS_LOCAL_DISCOVERY_DEVICES_PAGE_TITLE;
  if (host == chrome::kChromeUIDownloadsHost)
    return IDS_DOWNLOAD_TITLE;
  if (host == chrome::kChromeUIExtensionsHost)
    return IDS_MANAGE_EXTENSIONS_SETTING_WINDOWS_TITLE;
  if (host == chrome::kChromeUIHelpHost)
    return IDS_ABOUT_TAB_TITLE;
  if (host == chrome::kChromeUIHistoryHost)
    return IDS_HISTORY_TITLE;
  if (host == chrome::kChromeUINewTabHost)
    return IDS_NEW_TAB_TITLE;
  if (host == chrome::kChromeUIPluginsHost)
    return IDS_PLUGINS_TITLE;
  if (host == chrome::kChromeUIPolicyHost)
    return IDS_POLICY_TITLE;
  if (host == chrome::kChromeUIPrintHost)
    return IDS_PRINT_PREVIEW_TITLE;
  if (host == chrome::kChromeUISettingsHost)
    return IDS_SETTINGS_TITLE;
  if (host == chrome::kChromeUIVersionHost)
    return IDS_ABOUT_VERSION_TITLE;

  return -1;
}

}  // namespace

string16 SiteChipView::SiteLabelFromURL(const GURL& provided_url) {
  // First, strip view-source: if it appears.  Note that GetContent removes
  // "view-source:" but leaves the http, https or ftp scheme.
  GURL url(provided_url);
  if (url.SchemeIs(content::kViewSourceScheme))
    url = GURL(url.GetContent());

  // Chrome built-in pages.
  if (url.is_empty() ||
      url.SchemeIs(chrome::kChromeUIScheme) ||
      url.SchemeIs(chrome::kAboutScheme)) {
    int string_ref = StringForChromeHost(url);
    if (string_ref == -1)
      return base::UTF8ToUTF16("Chrome");
    return l10n_util::GetStringUTF16(string_ref);
  }

  Profile* profile = toolbar_view_->browser()->profile();

  // For chrome-extension urls, return the extension name.
  if (url.SchemeIs(extensions::kExtensionScheme)) {
    ExtensionService* service =
        extensions::ExtensionSystem::Get(profile)->extension_service();
    const extensions::Extension* extension =
        service->extensions()->GetExtensionOrAppByURL(url);
    return extension ?
        base::UTF8ToUTF16(extension->name()) : base::UTF8ToUTF16(url.host());
  }

  if (url.SchemeIsHTTPOrHTTPS() || url.SchemeIs(content::kFtpScheme)) {
    // See ToolbarModelImpl::GetText(). Does not pay attention to any user
    // edits, and uses GetURL/net::FormatUrl -- We don't really care about
    // length or the autocomplete parser.
    // TODO(gbillock): This uses an algorithm very similar to GetText, which
    // is probably too conservative. Try out just using a simpler mechanism of
    // StripWWW() and IDNToUnicode().
    std::string languages;
    if (profile)
      languages = profile->GetPrefs()->GetString(prefs::kAcceptLanguages);

    base::string16 formatted = net::FormatUrl(url.GetOrigin(), languages,
        net::kFormatUrlOmitAll, net::UnescapeRule::NORMAL, NULL, NULL, NULL);
    // Remove scheme, "www.", and trailing "/".
    if (StartsWith(formatted, ASCIIToUTF16("http://"), false))
      formatted = formatted.substr(7);
    else if (StartsWith(formatted, ASCIIToUTF16("https://"), false))
      formatted = formatted.substr(8);
    else if (StartsWith(formatted, ASCIIToUTF16("ftp://"), false))
      formatted = formatted.substr(6);
    if (StartsWith(formatted, ASCIIToUTF16("www."), false))
      formatted = formatted.substr(4);
    if (EndsWith(formatted, ASCIIToUTF16("/"), false))
      formatted = formatted.substr(0, formatted.size()-1);
    return formatted;
  }

  // These internal-ish debugging-style schemes we don't expect users
  // to see. In these cases, the site chip will display the first
  // part of the full URL.
  if (url.SchemeIs(chrome::kBlobScheme) ||
      url.SchemeIs(chrome::kChromeDevToolsScheme) ||
      url.SchemeIs(chrome::kChromeNativeScheme) ||
      url.SchemeIs(chrome::kDataScheme) ||
      url.SchemeIs(chrome::kFileScheme) ||
      url.SchemeIs(chrome::kFileSystemScheme) ||
      url.SchemeIs(content::kGuestScheme) ||
      url.SchemeIs(content::kJavaScriptScheme) ||
      url.SchemeIs(content::kMailToScheme) ||
      url.SchemeIs(content::kMetadataScheme) ||
      url.SchemeIs(content::kSwappedOutScheme)) {
    std::string truncated_url;
    base::TruncateUTF8ToByteSize(url.spec(), 1000, &truncated_url);
    return base::UTF8ToUTF16(truncated_url);
  }

#if defined(OS_CHROMEOS)
  if (url.SchemeIs(chrome::kCrosScheme) ||
      url.SchemeIs(chrome::kDriveScheme)) {
    return base::UTF8ToUTF16(url.spec());
  }
#endif

  // If all else fails, return hostname.
  return base::UTF8ToUTF16(url.host());
}

SiteChipView::SiteChipView(ToolbarView* toolbar_view)
    : ToolbarButton(this, NULL),
      toolbar_view_(toolbar_view),
      painter_(NULL),
      showing_16x16_icon_(false) {
  scoped_refptr<SafeBrowsingService> sb_service =
      g_browser_process->safe_browsing_service();
  // May not be set for unit tests.
  if (sb_service.get() && sb_service->ui_manager())
    sb_service->ui_manager()->AddObserver(this);

  set_drag_controller(this);
}

SiteChipView::~SiteChipView() {
  scoped_refptr<SafeBrowsingService> sb_service =
      g_browser_process->safe_browsing_service();
  if (sb_service.get() && sb_service->ui_manager())
    sb_service->ui_manager()->RemoveObserver(this);
}

void SiteChipView::Init() {
  ToolbarButton::Init();
  image()->EnableCanvasFlippingForRTLUI(false);

  // TODO(gbillock): Would be nice to just use stock LabelButton stuff here.
  location_icon_view_ = new LocationIconView(toolbar_view_->location_bar());
  // Make location icon hover events count as hovering the site chip.
  location_icon_view_->set_interactive(false);

  host_label_ = new views::Label();
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  host_label_->SetFont(rb.GetFont(ui::ResourceBundle::MediumFont));

  AddChildView(location_icon_view_);
  AddChildView(host_label_);

  location_icon_view_->SetImage(GetThemeProvider()->GetImageSkiaNamed(
      IDR_LOCATION_BAR_HTTP));
  location_icon_view_->ShowTooltip(true);

  const int kEVBackgroundImages[] = IMAGE_GRID(IDR_SITE_CHIP_EV);
  ev_background_painter_.reset(
      views::Painter::CreateImageGridPainter(kEVBackgroundImages));
  const int kBrokenSSLBackgroundImages[] = IMAGE_GRID(IDR_SITE_CHIP_BROKENSSL);
  broken_ssl_background_painter_.reset(
      views::Painter::CreateImageGridPainter(kBrokenSSLBackgroundImages));
  const int kMalwareBackgroundImages[] = IMAGE_GRID(IDR_SITE_CHIP_MALWARE);
  malware_background_painter_.reset(
      views::Painter::CreateImageGridPainter(kMalwareBackgroundImages));
}

bool SiteChipView::ShouldShow() {
  return chrome::ShouldDisplayOriginChip();
}

void SiteChipView::Update(content::WebContents* web_contents) {
  if (!web_contents)
    return;

  // Note: security level can change async as the connection is made.
  GURL url = toolbar_view_->GetToolbarModel()->GetURL();
  const ToolbarModel::SecurityLevel security_level =
      toolbar_view_->GetToolbarModel()->GetSecurityLevel(true);

  bool url_malware = IsMalware(url, web_contents);

  // TODO(gbillock): We persist a malware setting while a new WebContents
  // content is loaded, meaning that we end up transiently marking a safe
  // page as malware. Need to fix that.

  if ((url == url_displayed_) &&
      (security_level == security_level_) &&
      (url_malware == url_malware_))
    return;

  url_displayed_ = url;
  url_malware_ = url_malware;
  security_level_ = security_level;

  SkColor label_background =
      GetThemeProvider()->GetColor(ThemeProperties::COLOR_TOOLBAR);
  if (url_malware_) {
    painter_ = malware_background_painter_.get();
    label_background = kMalwareBackgroundColor;
  } else if (security_level_ == ToolbarModel::SECURITY_ERROR) {
    painter_ = broken_ssl_background_painter_.get();
    label_background = kBrokenSSLBackgroundColor;
  } else if (security_level_ == ToolbarModel::EV_SECURE) {
    painter_ = ev_background_painter_.get();
    label_background = kEVBackgroundColor;
  } else {
    painter_ = NULL;
  }

  string16 host = SiteLabelFromURL(url_displayed_);
  if (security_level_ == ToolbarModel::EV_SECURE) {
    host = l10n_util::GetStringFUTF16(IDS_SITE_CHIP_EV_SSL_LABEL,
        toolbar_view_->GetToolbarModel()->GetEVCertName(),
        host);
  }

  host_label_->SetText(host);
  host_label_->SetTooltipText(host);
  host_label_->SetBackgroundColor(label_background);

  int icon = toolbar_view_->GetToolbarModel()->GetIconForSecurityLevel(
      security_level_);
  showing_16x16_icon_ = false;

  if (url_displayed_.is_empty() ||
      url_displayed_.SchemeIs(chrome::kChromeUIScheme) ||
      url_displayed_.SchemeIs(chrome::kAboutScheme)) {
    icon = IDR_PRODUCT_LOGO_16;
    showing_16x16_icon_ = true;
  }

  location_icon_view_->SetImage(GetThemeProvider()->GetImageSkiaNamed(icon));

  if (url_displayed_.SchemeIs(extensions::kExtensionScheme)) {
    icon = IDR_EXTENSIONS_FAVICON;
    showing_16x16_icon_ = true;
    location_icon_view_->SetImage(GetThemeProvider()->GetImageSkiaNamed(icon));

    ExtensionService* service =
        extensions::ExtensionSystem::Get(
            toolbar_view_->browser()->profile())->extension_service();
    const extensions::Extension* extension =
        service->extensions()->GetExtensionOrAppByURL(url_displayed_);
    extension_icon_.reset(
        new SiteChipExtensionIcon(location_icon_view_,
                                  toolbar_view_->browser()->profile(),
                                  extension));
  } else {
    extension_icon_.reset();
  }

  Layout();
  SchedulePaint();
}

void SiteChipView::OnChanged() {
  Update(toolbar_view_->GetWebContents());
  toolbar_view_->Layout();
  toolbar_view_->SchedulePaint();
  // TODO(gbillock): Also need to potentially repaint infobars to make sure the
  // arrows are pointing to the right spot. Only needed for some edge cases.
}

gfx::Size SiteChipView::GetPreferredSize() {
  gfx::Size label_size = host_label_->GetPreferredSize();
  gfx::Size icon_size = location_icon_view_->GetPreferredSize();
  int icon_spacing = showing_16x16_icon_ ?
      (k16x16IconLeadingSpacing + k16x16IconTrailingSpacing) : 0;
  return gfx::Size(kEdgeThickness + icon_size.width() + icon_spacing +
                   kIconTextSpacing + label_size.width() +
                   kTrailingLabelMargin + kEdgeThickness,
                   icon_size.height());
}

void SiteChipView::Layout() {
  // TODO(gbillock): Eventually we almost certainly want to use
  // LocationBarLayout for leading and trailing decorations.

  location_icon_view_->SetBounds(
      kEdgeThickness + (showing_16x16_icon_ ? k16x16IconLeadingSpacing : 0),
      LocationBarView::kNormalEdgeThickness,
      location_icon_view_->GetPreferredSize().width(),
      height() - 2 * LocationBarView::kNormalEdgeThickness);

  int host_label_x = location_icon_view_->x() + location_icon_view_->width() +
      kIconTextSpacing;
  host_label_x += showing_16x16_icon_ ? k16x16IconTrailingSpacing : 0;
  int host_label_width =
      width() - host_label_x - kEdgeThickness - kTrailingLabelMargin;
  host_label_->SetBounds(host_label_x,
                         LocationBarView::kNormalEdgeThickness,
                         host_label_width,
                         height() - 2 * LocationBarView::kNormalEdgeThickness);
}

void SiteChipView::OnPaint(gfx::Canvas* canvas) {
  gfx::Rect rect(GetLocalBounds());
  if (painter_)
    views::Painter::PaintPainterAt(canvas, painter_, rect);

  ToolbarButton::OnPaint(canvas);
}

// TODO(gbillock): Make the LocationBarView or OmniboxView the listener for
// this button.
void SiteChipView::ButtonPressed(views::Button* sender,
                                 const ui::Event& event) {
  // See if the event needs to be passed to the LocationIconView.
  if (event.IsMouseEvent() || (event.type() == ui::ET_GESTURE_TAP)) {
    location_icon_view_->set_interactive(true);
    const ui::LocatedEvent& located_event =
        static_cast<const ui::LocatedEvent&>(event);
    if (GetEventHandlerForPoint(located_event.location()) ==
            location_icon_view_) {
      location_icon_view_->page_info_helper()->ProcessEvent(located_event);
      location_icon_view_->set_interactive(false);
      return;
    }
    location_icon_view_->set_interactive(false);
  }

  UMA_HISTOGRAM_COUNTS("SiteChip.Pressed", 1);
  content::RecordAction(content::UserMetricsAction("SiteChipPress"));

  toolbar_view_->location_bar()->GetOmniboxView()->SetFocus();
  toolbar_view_->location_bar()->GetOmniboxView()->model()->
      SetCaretVisibility(true);
  toolbar_view_->location_bar()->GetOmniboxView()->ShowURL();
}

void SiteChipView::WriteDragDataForView(View* sender,
                                        const gfx::Point& press_pt,
                                        OSExchangeData* data) {
  // TODO(gbillock): Consolidate this with the identical logic in
  // LocationBarView.
  content::WebContents* web_contents = toolbar_view_->GetWebContents();
  FaviconTabHelper* favicon_tab_helper =
      FaviconTabHelper::FromWebContents(web_contents);
  gfx::ImageSkia favicon = favicon_tab_helper->GetFavicon().AsImageSkia();
  button_drag_utils::SetURLAndDragImage(web_contents->GetURL(),
                                        web_contents->GetTitle(),
                                        favicon,
                                        data,
                                        sender->GetWidget());
}

int SiteChipView::GetDragOperationsForView(View* sender,
                                           const gfx::Point& p) {
  return ui::DragDropTypes::DRAG_COPY | ui::DragDropTypes::DRAG_LINK;
}

bool SiteChipView::CanStartDragForView(View* sender,
                                       const gfx::Point& press_pt,
                                       const gfx::Point& p) {
  return true;
}

// Note: When OnSafeBrowsingHit would be called, OnSafeBrowsingMatch will
// have already been called.
void SiteChipView::OnSafeBrowsingHit(
    const SafeBrowsingUIManager::UnsafeResource& resource) {}

void SiteChipView::OnSafeBrowsingMatch(
    const SafeBrowsingUIManager::UnsafeResource& resource) {
  OnChanged();
}

