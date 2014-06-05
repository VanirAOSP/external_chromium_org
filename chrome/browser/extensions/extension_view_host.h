// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_VIEW_HOST_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_VIEW_HOST_H_

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/extensions/extension_host.h"
#include "components/web_modal/web_contents_modal_dialog_host.h"
#include "components/web_modal/web_contents_modal_dialog_manager_delegate.h"

#if defined(TOOLKIT_VIEWS)
#include "chrome/browser/ui/views/extensions/extension_view_views.h"
#elif defined(OS_MACOSX)
#include "chrome/browser/ui/cocoa/extensions/extension_view_mac.h"
#elif defined(TOOLKIT_GTK)
#include "chrome/browser/ui/gtk/extensions/extension_view_gtk.h"
#elif defined(OS_ANDROID)
#include "chrome/browser/ui/android/extensions/extension_view_android.h"
#endif

class Browser;

namespace content {
class SiteInstance;
class WebContents;
}

namespace extensions {

// The ExtensionHost for an extension that backs a view in the browser UI. For
// example, this could be an extension popup, infobar or dialog, but not a
// background page.
class ExtensionViewHost
    : public ExtensionHost,
      public web_modal::WebContentsModalDialogManagerDelegate,
      public web_modal::WebContentsModalDialogHost {
 public:
  ExtensionViewHost(const Extension* extension,
                    content::SiteInstance* site_instance,
                    const GURL& url,
                    ViewType host_type);
  virtual ~ExtensionViewHost();

  // TODO(jamescook): Create platform specific subclasses?
#if defined(TOOLKIT_VIEWS)
  typedef ExtensionViewViews PlatformExtensionView;
#elif defined(OS_MACOSX)
  typedef ExtensionViewMac PlatformExtensionView;
#elif defined(TOOLKIT_GTK)
  typedef ExtensionViewGtk PlatformExtensionView;
#elif defined(OS_ANDROID)
  // Android does not support extensions.
  typedef ExtensionViewAndroid PlatformExtensionView;
#endif

  PlatformExtensionView* view() { return view_.get(); }

  // Create an ExtensionView and tie it to this host and |browser|.  Note NULL
  // is a valid argument for |browser|.  Extension views may be bound to
  // tab-contents hosted in ExternalTabContainer objects, which do not
  // instantiate Browser objects.
  void CreateView(Browser* browser);

  void SetAssociatedWebContents(content::WebContents* web_contents);

  // Handles keyboard events that were not handled by HandleKeyboardEvent().
  // Platform specific implementation may override this method to handle the
  // event in platform specific way.
  virtual void UnhandledKeyboardEvent(
      content::WebContents* source,
      const content::NativeWebKeyboardEvent& event);

  // ExtensionHost
  virtual void OnDidStopLoading() OVERRIDE;
  virtual void OnDocumentAvailable() OVERRIDE;
  virtual void LoadInitialURL() OVERRIDE;
  virtual bool IsBackgroundPage() const OVERRIDE;

  // content::WebContentsDelegate
  virtual content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params) OVERRIDE;
  virtual bool PreHandleKeyboardEvent(
      content::WebContents* source,
      const content::NativeWebKeyboardEvent& event,
      bool* is_keyboard_shortcut) OVERRIDE;
  virtual void HandleKeyboardEvent(
      content::WebContents* source,
      const content::NativeWebKeyboardEvent& event) OVERRIDE;
  virtual content::ColorChooser* OpenColorChooser(
      content::WebContents* web_contents,
      SkColor color,
      const std::vector<content::ColorSuggestion>& suggestions) OVERRIDE;
  virtual void RunFileChooser(
      content::WebContents* tab,
      const content::FileChooserParams& params) OVERRIDE;
  virtual void ResizeDueToAutoResize(content::WebContents* source,
                                     const gfx::Size& new_size) OVERRIDE;

  // content::WebContentsObserver
  virtual void RenderViewCreated(
      content::RenderViewHost* render_view_host) OVERRIDE;

  // web_modal::WebContentsModalDialogManagerDelegate
  virtual web_modal::WebContentsModalDialogHost*
      GetWebContentsModalDialogHost() OVERRIDE;
  virtual bool IsWebContentsVisible(
      content::WebContents* web_contents) OVERRIDE;

  // web_modal::WebContentsModalDialogHost
  virtual gfx::NativeView GetHostView() const OVERRIDE;
  virtual gfx::Point GetDialogPosition(const gfx::Size& size) OVERRIDE;
  virtual gfx::Size GetMaximumDialogSize() OVERRIDE;
  virtual void AddObserver(
      web_modal::ModalDialogHostObserver* observer) OVERRIDE;
  virtual void RemoveObserver(
      web_modal::ModalDialogHostObserver* observer) OVERRIDE;

  // ExtensionFunctionDispatcher::Delegate
  virtual WindowController* GetExtensionWindowController() const OVERRIDE;
  virtual content::WebContents* GetAssociatedWebContents() const OVERRIDE;
  virtual content::WebContents* GetVisibleWebContents() const OVERRIDE;

  // content::NotificationObserver
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 private:
  // Insert a default style sheet for Extension Infobars.
  void InsertInfobarCSS();

  // Optional view that shows the rendered content in the UI.
  scoped_ptr<PlatformExtensionView> view_;

  // The relevant WebContents associated with this ExtensionViewHost, if any.
  content::WebContents* associated_web_contents_;

  // Observer to detect when the associated web contents is destroyed.
  class AssociatedWebContentsObserver;
  scoped_ptr<AssociatedWebContentsObserver> associated_web_contents_observer_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionViewHost);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_VIEW_HOST_H_
