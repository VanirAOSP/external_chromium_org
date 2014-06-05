// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_view_host.h"

#include "base/strings/string_piece.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/window_controller.h"
#include "chrome/browser/file_select_helper.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/common/extensions/extension_messages.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "grit/browser_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/events/keycodes/keyboard_codes.h"

using content::NativeWebKeyboardEvent;
using content::OpenURLParams;
using content::RenderViewHost;
using content::WebContents;
using content::WebContentsObserver;
using web_modal::WebContentsModalDialogManager;

namespace extensions {

// Notifies an ExtensionViewHost when a WebContents is destroyed.
class ExtensionViewHost::AssociatedWebContentsObserver
    : public WebContentsObserver {
 public:
  AssociatedWebContentsObserver(ExtensionViewHost* host,
                                WebContents* web_contents)
      : WebContentsObserver(web_contents), host_(host) {}
  virtual ~AssociatedWebContentsObserver() {}

  // content::WebContentsObserver:
  virtual void WebContentsDestroyed(WebContents* web_contents) OVERRIDE {
    // Deleting |this| from here is safe.
    host_->SetAssociatedWebContents(NULL);
  }

 private:
  ExtensionViewHost* host_;

  DISALLOW_COPY_AND_ASSIGN(AssociatedWebContentsObserver);
};

ExtensionViewHost::ExtensionViewHost(
    const Extension* extension,
    content::SiteInstance* site_instance,
    const GURL& url,
    ViewType host_type)
    : ExtensionHost(extension, site_instance, url, host_type),
      associated_web_contents_(NULL) {
  // Not used for panels, see PanelHost.
  DCHECK(host_type == VIEW_TYPE_EXTENSION_DIALOG ||
         host_type == VIEW_TYPE_EXTENSION_INFOBAR ||
         host_type == VIEW_TYPE_EXTENSION_POPUP);
}

ExtensionViewHost::~ExtensionViewHost() {
  // The hosting WebContents will be deleted in the base class, so unregister
  // this object before it deletes the attached WebContentsModalDialogManager.
  WebContentsModalDialogManager* manager =
      WebContentsModalDialogManager::FromWebContents(host_contents());
  if (manager)
    manager->SetDelegate(NULL);
}

void ExtensionViewHost::CreateView(Browser* browser) {
#if defined(TOOLKIT_VIEWS)
  view_.reset(new ExtensionViewViews(this, browser));
  // We own |view_|, so don't auto delete when it's removed from the view
  // hierarchy.
  view_->set_owned_by_client();
#elif defined(OS_MACOSX)
  view_.reset(new ExtensionViewMac(this, browser));
  view_->Init();
#elif defined(TOOLKIT_GTK)
  view_.reset(new ExtensionViewGtk(this, browser));
  view_->Init();
#else
  // TODO(port)
  NOTREACHED();
#endif
}

void ExtensionViewHost::SetAssociatedWebContents(WebContents* web_contents) {
  associated_web_contents_ = web_contents;
  if (associated_web_contents_) {
    // Observe the new WebContents for deletion.
    associated_web_contents_observer_.reset(
        new AssociatedWebContentsObserver(this, associated_web_contents_));
  } else {
    associated_web_contents_observer_.reset();
  }
}

void ExtensionViewHost::UnhandledKeyboardEvent(
    WebContents* source,
    const content::NativeWebKeyboardEvent& event) {
  Browser* browser = view_->browser();
  if (browser) {
    // Handle lower priority browser shortcuts such as Ctrl-f.
    return browser->HandleKeyboardEvent(source, event);
  } else {
#if defined(TOOLKIT_VIEWS)
    // In case there's no Browser (e.g. for dialogs), pass it to
    // ExtensionViewViews to handle accelerators. The view's FocusManager does
    // not know anything about Browser accelerators, but might know others such
    // as Ash's.
    view_->HandleKeyboardEvent(event);
#endif
  }
}

// ExtensionHost overrides:

void ExtensionViewHost::OnDidStopLoading() {
  DCHECK(did_stop_loading());
#if defined(TOOLKIT_VIEWS) || defined(OS_MACOSX)
  view_->DidStopLoading();
#endif
}

void ExtensionViewHost::OnDocumentAvailable() {
  if (extension_host_type() == VIEW_TYPE_EXTENSION_INFOBAR) {
    // No style sheet for other types, at the moment.
    InsertInfobarCSS();
  }
}

void ExtensionViewHost::LoadInitialURL() {
  if (!ExtensionSystem::GetForBrowserContext(browser_context())->
          extension_service()->IsBackgroundPageReady(extension())) {
    // Make sure the background page loads before any others.
    registrar()->Add(this,
                     chrome::NOTIFICATION_EXTENSION_BACKGROUND_PAGE_READY,
                     content::Source<Extension>(extension()));
    return;
  }

  // Popups may spawn modal dialogs, which need positioning information.
  if (extension_host_type() == VIEW_TYPE_EXTENSION_POPUP) {
    WebContentsModalDialogManager::CreateForWebContents(host_contents());
    WebContentsModalDialogManager::FromWebContents(
        host_contents())->SetDelegate(this);
  }

  ExtensionHost::LoadInitialURL();
}

bool ExtensionViewHost::IsBackgroundPage() const {
  DCHECK(view_);
  return false;
}

// content::WebContentsDelegate overrides:

WebContents* ExtensionViewHost::OpenURLFromTab(
    WebContents* source,
    const OpenURLParams& params) {
  // Whitelist the dispositions we will allow to be opened.
  switch (params.disposition) {
    case SINGLETON_TAB:
    case NEW_FOREGROUND_TAB:
    case NEW_BACKGROUND_TAB:
    case NEW_POPUP:
    case NEW_WINDOW:
    case SAVE_TO_DISK:
    case OFF_THE_RECORD: {
      // Only allow these from hosts that are bound to a browser (e.g. popups).
      // Otherwise they are not driven by a user gesture.
      Browser* browser = view_->browser();
      return browser ? browser->OpenURL(params) : NULL;
    }
    default:
      return NULL;
  }
}

bool ExtensionViewHost::PreHandleKeyboardEvent(
    WebContents* source,
    const NativeWebKeyboardEvent& event,
    bool* is_keyboard_shortcut) {
  if (extension_host_type() == VIEW_TYPE_EXTENSION_POPUP &&
      event.type == NativeWebKeyboardEvent::RawKeyDown &&
      event.windowsKeyCode == ui::VKEY_ESCAPE) {
    DCHECK(is_keyboard_shortcut != NULL);
    *is_keyboard_shortcut = true;
    return false;
  }

  // Handle higher priority browser shortcuts such as Ctrl-w.
  Browser* browser = view_->browser();
  if (browser)
    return browser->PreHandleKeyboardEvent(source, event, is_keyboard_shortcut);

  *is_keyboard_shortcut = false;
  return false;
}

void ExtensionViewHost::HandleKeyboardEvent(
    WebContents* source,
    const NativeWebKeyboardEvent& event) {
  if (extension_host_type() == VIEW_TYPE_EXTENSION_POPUP) {
    if (event.type == NativeWebKeyboardEvent::RawKeyDown &&
        event.windowsKeyCode == ui::VKEY_ESCAPE) {
      Close();
      return;
    }
  }
  UnhandledKeyboardEvent(source, event);
}

content::ColorChooser* ExtensionViewHost::OpenColorChooser(
    WebContents* web_contents,
    SkColor initial_color,
    const std::vector<content::ColorSuggestion>& suggestions) {
  // Similar to the file chooser below, opening a color chooser requires a
  // visible <input> element to click on. Therefore this code only exists for
  // extensions with a view.
  return chrome::ShowColorChooser(web_contents, initial_color);
}

void ExtensionViewHost::RunFileChooser(
    WebContents* tab,
    const content::FileChooserParams& params) {
  // For security reasons opening a file picker requires a visible <input>
  // element to click on, so this code only exists for extensions with a view.
  FileSelectHelper::RunFileChooser(tab, params);
}


void ExtensionViewHost::ResizeDueToAutoResize(WebContents* source,
                                          const gfx::Size& new_size) {
  view_->ResizeDueToAutoResize(new_size);
}

// content::WebContentsObserver overrides:

void ExtensionViewHost::RenderViewCreated(RenderViewHost* render_view_host) {
  ExtensionHost::RenderViewCreated(render_view_host);

  view_->RenderViewCreated();

  // If the host is bound to a window, then extract its id. Extensions hosted
  // in ExternalTabContainer objects may not have an associated window.
  WindowController* window = GetExtensionWindowController();
  if (window) {
    render_view_host->Send(new ExtensionMsg_UpdateBrowserWindowId(
        render_view_host->GetRoutingID(), window->GetWindowId()));
  }
}

// web_modal::WebContentsModalDialogManagerDelegate overrides:

web_modal::WebContentsModalDialogHost*
ExtensionViewHost::GetWebContentsModalDialogHost() {
  return this;
}

bool ExtensionViewHost::IsWebContentsVisible(WebContents* web_contents) {
  return platform_util::IsVisible(web_contents->GetView()->GetNativeView());
}

gfx::NativeView ExtensionViewHost::GetHostView() const {
  return view_->native_view();
}

gfx::Point ExtensionViewHost::GetDialogPosition(const gfx::Size& size) {
  if (!GetVisibleWebContents())
    return gfx::Point();
  gfx::Rect bounds = GetVisibleWebContents()->GetView()->GetViewBounds();
  return gfx::Point(
      std::max(0, (bounds.width() - size.width()) / 2),
      std::max(0, (bounds.height() - size.height()) / 2));
}

gfx::Size ExtensionViewHost::GetMaximumDialogSize() {
  if (!GetVisibleWebContents())
    return gfx::Size();
  return GetVisibleWebContents()->GetView()->GetViewBounds().size();
}

void ExtensionViewHost::AddObserver(
    web_modal::ModalDialogHostObserver* observer) {
}

void ExtensionViewHost::RemoveObserver(
    web_modal::ModalDialogHostObserver* observer) {
}

WindowController* ExtensionViewHost::GetExtensionWindowController() const {
  return view_->browser() ? view_->browser()->extension_window_controller()
                          : NULL;
}

WebContents* ExtensionViewHost::GetAssociatedWebContents() const {
  return associated_web_contents_;
}

WebContents* ExtensionViewHost::GetVisibleWebContents() const {
  if (associated_web_contents_)
    return associated_web_contents_;
  if (extension_host_type() == VIEW_TYPE_EXTENSION_POPUP)
    return host_contents();
  return NULL;
}

void ExtensionViewHost::Observe(int type,
                                const content::NotificationSource& source,
                                const content::NotificationDetails& details) {
  if (type == chrome::NOTIFICATION_EXTENSION_BACKGROUND_PAGE_READY) {
    DCHECK(ExtensionSystem::GetForBrowserContext(browser_context())->
               extension_service()->IsBackgroundPageReady(extension()));
    LoadInitialURL();
    return;
  }
  ExtensionHost::Observe(type, source, details);
}

void ExtensionViewHost::InsertInfobarCSS() {
  static const base::StringPiece css(
      ResourceBundle::GetSharedInstance().GetRawDataResource(
      IDR_EXTENSIONS_INFOBAR_CSS));

  render_view_host()->InsertCSS(base::string16(), css.as_string());
}

}  // namespace extensions
