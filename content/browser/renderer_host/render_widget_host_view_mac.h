// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_MAC_H_
#define CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_MAC_H_

#import <Cocoa/Cocoa.h>
#include <list>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/mac/scoped_nsobject.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "content/browser/renderer_host/display_link_mac.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/browser/renderer_host/software_frame_manager.h"
#include "content/common/content_export.h"
#include "content/common/cursors/webcursor.h"
#include "content/common/edit_command.h"
#import "content/public/browser/render_widget_host_view_mac_base.h"
#include "ipc/ipc_sender.h"
#include "third_party/WebKit/public/web/WebCompositionUnderline.h"
#include "ui/base/cocoa/base_view.h"

namespace content {
class CompositingIOSurfaceMac;
class CompositingIOSurfaceContext;
class RenderWidgetHostViewMac;
class RenderWidgetHostViewMacEditCommandHelper;
class WebContents;
}

@class CompositingIOSurfaceLayer;
@class FullscreenWindowManager;
@protocol RenderWidgetHostViewMacDelegate;
@class ToolTip;

@protocol RenderWidgetHostViewMacOwner
- (content::RenderWidgetHostViewMac*)renderWidgetHostViewMac;
@end

// This is the view that lives in the Cocoa view hierarchy. In Windows-land,
// RenderWidgetHostViewWin is both the view and the delegate. We split the roles
// but that means that the view needs to own the delegate and will dispose of it
// when it's removed from the view system.
@interface RenderWidgetHostViewCocoa
    : BaseView <RenderWidgetHostViewMacBase,
                RenderWidgetHostViewMacOwner,
                NSTextInputClient> {
 @private
  scoped_ptr<content::RenderWidgetHostViewMac> renderWidgetHostView_;
  // This ivar is the cocoa delegate of the NSResponder.
  base::scoped_nsobject<NSObject<RenderWidgetHostViewMacDelegate>>
      responderDelegate_;
  BOOL canBeKeyView_;
  BOOL takesFocusOnlyOnMouseDown_;
  BOOL closeOnDeactivate_;
  scoped_ptr<content::RenderWidgetHostViewMacEditCommandHelper>
      editCommand_helper_;

  // These are part of the magic tooltip code from WebKit's WebHTMLView:
  id trackingRectOwner_;              // (not retained)
  void* trackingRectUserData_;
  NSTrackingRectTag lastToolTipTag_;
  base::scoped_nsobject<NSString> toolTip_;

  // Is YES if there was a mouse-down as yet unbalanced with a mouse-up.
  BOOL hasOpenMouseDown_;

  NSWindow* lastWindow_;  // weak

  // The cursor for the page. This is passed up from the renderer.
  base::scoped_nsobject<NSCursor> currentCursor_;

  // Variables used by our implementaion of the NSTextInput protocol.
  // An input method of Mac calls the methods of this protocol not only to
  // notify an application of its status, but also to retrieve the status of
  // the application. That is, an application cannot control an input method
  // directly.
  // This object keeps the status of a composition of the renderer and returns
  // it when an input method asks for it.
  // We need to implement Objective-C methods for the NSTextInput protocol. On
  // the other hand, we need to implement a C++ method for an IPC-message
  // handler which receives input-method events from the renderer.

  // Represents the input-method attributes supported by this object.
  base::scoped_nsobject<NSArray> validAttributesForMarkedText_;

  // Indicates if we are currently handling a key down event.
  BOOL handlingKeyDown_;

  // Indicates if there is any marked text.
  BOOL hasMarkedText_;

  // Indicates if unmarkText is called or not when handling a keyboard
  // event.
  BOOL unmarkTextCalled_;

  // The range of current marked text inside the whole content of the DOM node
  // being edited.
  // TODO(suzhe): This is currently a fake value, as we do not support accessing
  // the whole content yet.
  NSRange markedRange_;

  // The selected range, cached from a message sent by the renderer.
  NSRange selectedRange_;

  // Text to be inserted which was generated by handling a key down event.
  base::string16 textToBeInserted_;

  // Marked text which was generated by handling a key down event.
  base::string16 markedText_;

  // Underline information of the |markedText_|.
  std::vector<blink::WebCompositionUnderline> underlines_;

  // Indicates if doCommandBySelector method receives any edit command when
  // handling a key down event.
  BOOL hasEditCommands_;

  // Contains edit commands received by the -doCommandBySelector: method when
  // handling a key down event, not including inserting commands, eg. insertTab,
  // etc.
  content::EditCommands editCommands_;

  // The plugin that currently has focus (-1 if no plugin has focus).
  int focusedPluginIdentifier_;

  // Whether or not plugin IME is currently enabled active.
  BOOL pluginImeActive_;

  // Whether the previous mouse event was ignored due to hitTest check.
  BOOL mouseEventWasIgnored_;

  // Event monitor for scroll wheel end event.
  id endWheelMonitor_;

  // OpenGL Support:

  // recursive globalFrameDidChange protection:
  BOOL handlingGlobalFrameDidChange_;

  // The scale factor of the display this view is in.
  float deviceScaleFactor_;

  // If true then escape key down events are suppressed until the first escape
  // key up event. (The up event is suppressed as well). This is used by the
  // flash fullscreen code to avoid sending a key up event without a matching
  // key down event.
  BOOL suppressNextEscapeKeyUp_;
}

@property(nonatomic, readonly) NSRange selectedRange;
@property(nonatomic, readonly) BOOL suppressNextEscapeKeyUp;

- (void)setCanBeKeyView:(BOOL)can;
- (void)setTakesFocusOnlyOnMouseDown:(BOOL)b;
- (void)setCloseOnDeactivate:(BOOL)b;
- (void)setToolTipAtMousePoint:(NSString *)string;
// True for always-on-top special windows (e.g. Balloons and Panels).
- (BOOL)acceptsMouseEventsWhenInactive;
// Cancel ongoing composition (abandon the marked text).
- (void)cancelComposition;
// Confirm ongoing composition.
- (void)confirmComposition;
// Enables or disables plugin IME.
- (void)setPluginImeActive:(BOOL)active;
// Updates the current plugin focus state.
- (void)pluginFocusChanged:(BOOL)focused forPlugin:(int)pluginId;
// Evaluates the event in the context of plugin IME, if plugin IME is enabled.
// Returns YES if the event was handled.
- (BOOL)postProcessEventForPluginIme:(NSEvent*)event;
- (void)updateCursor:(NSCursor*)cursor;
- (NSRect)firstViewRectForCharacterRange:(NSRange)theRange
                             actualRange:(NSRangePointer)actualRange;
@end

@interface SoftwareLayer : CALayer {
 @private
  content::RenderWidgetHostViewMac* renderWidgetHostView_;
}

- (id)initWithRenderWidgetHostViewMac:(content::RenderWidgetHostViewMac*)r;

// Invalidate the RenderWidgetHostViewMac because it may be going away. If
// displayed again, it will draw white.
- (void)disableRendering;

@end

namespace content {
class RenderWidgetHostImpl;

///////////////////////////////////////////////////////////////////////////////
// RenderWidgetHostViewMac
//
//  An object representing the "View" of a rendered web page. This object is
//  responsible for displaying the content of the web page, and integrating with
//  the Cocoa view system. It is the implementation of the RenderWidgetHostView
//  that the cross-platform RenderWidgetHost object uses
//  to display the data.
//
//  Comment excerpted from render_widget_host.h:
//
//    "The lifetime of the RenderWidgetHost* is tied to the render process.
//     If the render process dies, the RenderWidgetHost* goes away and all
//     references to it must become NULL."
//
// RenderWidgetHostView class hierarchy described in render_widget_host_view.h.
class CONTENT_EXPORT RenderWidgetHostViewMac
    : public RenderWidgetHostViewBase,
      public IPC::Sender,
      public SoftwareFrameManagerClient {
 public:
  // The view will associate itself with the given widget. The native view must
  // be hooked up immediately to the view hierarchy, or else when it is
  // deleted it will delete this out from under the caller.
  explicit RenderWidgetHostViewMac(RenderWidgetHost* widget);
  virtual ~RenderWidgetHostViewMac();

  RenderWidgetHostViewCocoa* cocoa_view() const { return cocoa_view_; }

  // |delegate| is used to separate out the logic from the NSResponder delegate.
  // |delegate| is retained by this class.
  // |delegate| should be set at most once.
  CONTENT_EXPORT void SetDelegate(
    NSObject<RenderWidgetHostViewMacDelegate>* delegate);
  void SetAllowOverlappingViews(bool overlapping);

  // RenderWidgetHostView implementation.
  virtual bool OnMessageReceived(const IPC::Message& msg) OVERRIDE;
  virtual void InitAsChild(gfx::NativeView parent_view) OVERRIDE;
  virtual RenderWidgetHost* GetRenderWidgetHost() const OVERRIDE;
  virtual void SetSize(const gfx::Size& size) OVERRIDE;
  virtual void SetBounds(const gfx::Rect& rect) OVERRIDE;
  virtual gfx::NativeView GetNativeView() const OVERRIDE;
  virtual gfx::NativeViewId GetNativeViewId() const OVERRIDE;
  virtual gfx::NativeViewAccessible GetNativeViewAccessible() OVERRIDE;
  virtual bool HasFocus() const OVERRIDE;
  virtual bool IsSurfaceAvailableForCopy() const OVERRIDE;
  virtual void Show() OVERRIDE;
  virtual void Hide() OVERRIDE;
  virtual bool IsShowing() OVERRIDE;
  virtual gfx::Rect GetViewBounds() const OVERRIDE;
  virtual void SetShowingContextMenu(bool showing) OVERRIDE;
  virtual void SetActive(bool active) OVERRIDE;
  virtual void SetTakesFocusOnlyOnMouseDown(bool flag) OVERRIDE;
  virtual void SetWindowVisibility(bool visible) OVERRIDE;
  virtual void WindowFrameChanged() OVERRIDE;
  virtual void ShowDefinitionForSelection() OVERRIDE;
  virtual bool SupportsSpeech() const OVERRIDE;
  virtual void SpeakSelection() OVERRIDE;
  virtual bool IsSpeaking() const OVERRIDE;
  virtual void StopSpeaking() OVERRIDE;
  virtual void SetBackground(const SkBitmap& background) OVERRIDE;

  // Implementation of RenderWidgetHostViewBase.
  virtual void InitAsPopup(RenderWidgetHostView* parent_host_view,
                           const gfx::Rect& pos) OVERRIDE;
  virtual void InitAsFullscreen(
      RenderWidgetHostView* reference_host_view) OVERRIDE;
  virtual void WasShown() OVERRIDE;
  virtual void WasHidden() OVERRIDE;
  virtual void MovePluginWindows(
      const std::vector<WebPluginGeometry>& moves) OVERRIDE;
  virtual void Focus() OVERRIDE;
  virtual void Blur() OVERRIDE;
  virtual void UpdateCursor(const WebCursor& cursor) OVERRIDE;
  virtual void SetIsLoading(bool is_loading) OVERRIDE;
  virtual void TextInputTypeChanged(ui::TextInputType type,
                                    ui::TextInputMode input_mode,
                                    bool can_compose_inline) OVERRIDE;
  virtual void ImeCancelComposition() OVERRIDE;
  virtual void ImeCompositionRangeChanged(
      const gfx::Range& range,
      const std::vector<gfx::Rect>& character_bounds) OVERRIDE;
  virtual void RenderProcessGone(base::TerminationStatus status,
                                 int error_code) OVERRIDE;
  virtual void Destroy() OVERRIDE;
  virtual void SetTooltipText(const base::string16& tooltip_text) OVERRIDE;
  virtual void SelectionChanged(const base::string16& text,
                                size_t offset,
                                const gfx::Range& range) OVERRIDE;
  virtual void SelectionBoundsChanged(
      const ViewHostMsg_SelectionBounds_Params& params) OVERRIDE;
  virtual void ScrollOffsetChanged() OVERRIDE;
  virtual void CopyFromCompositingSurface(
      const gfx::Rect& src_subrect,
      const gfx::Size& dst_size,
      const base::Callback<void(bool, const SkBitmap&)>& callback,
      SkBitmap::Config config) OVERRIDE;
  virtual void CopyFromCompositingSurfaceToVideoFrame(
      const gfx::Rect& src_subrect,
      const scoped_refptr<media::VideoFrame>& target,
      const base::Callback<void(bool)>& callback) OVERRIDE;
  virtual bool CanCopyToVideoFrame() const OVERRIDE;
  virtual bool CanSubscribeFrame() const OVERRIDE;
  virtual void BeginFrameSubscription(
      scoped_ptr<RenderWidgetHostViewFrameSubscriber> subscriber) OVERRIDE;
  virtual void EndFrameSubscription() OVERRIDE;
  virtual void OnSwapCompositorFrame(
      uint32 output_surface_id, scoped_ptr<cc::CompositorFrame> frame) OVERRIDE;
  virtual void OnAcceleratedCompositingStateChange() OVERRIDE;
  virtual void AcceleratedSurfaceInitialized(int host_id,
                                             int route_id) OVERRIDE;
  virtual void CreateBrowserAccessibilityManagerIfNeeded() OVERRIDE;
  virtual gfx::Point AccessibilityOriginInScreen(const gfx::Rect& bounds)
      OVERRIDE;
  virtual void OnAccessibilitySetFocus(int acc_obj_id) OVERRIDE;
  virtual void AccessibilityShowMenu(int acc_obj_id) OVERRIDE;
  virtual bool PostProcessEventForPluginIme(
      const NativeWebKeyboardEvent& event) OVERRIDE;

  virtual void AcceleratedSurfaceBuffersSwapped(
      const GpuHostMsg_AcceleratedSurfaceBuffersSwapped_Params& params,
      int gpu_host_id) OVERRIDE;
  virtual void AcceleratedSurfacePostSubBuffer(
      const GpuHostMsg_AcceleratedSurfacePostSubBuffer_Params& params,
      int gpu_host_id) OVERRIDE;
  virtual void AcceleratedSurfaceSuspend() OVERRIDE;
  virtual void AcceleratedSurfaceRelease() OVERRIDE;
  virtual bool HasAcceleratedSurface(const gfx::Size& desired_size) OVERRIDE;
  virtual void GetScreenInfo(blink::WebScreenInfo* results) OVERRIDE;
  virtual gfx::Rect GetBoundsInRootWindow() OVERRIDE;
  virtual gfx::GLSurfaceHandle GetCompositingSurface() OVERRIDE;

  virtual void SetScrollOffsetPinning(
      bool is_pinned_to_left, bool is_pinned_to_right) OVERRIDE;
  virtual bool LockMouse() OVERRIDE;
  virtual void UnlockMouse() OVERRIDE;
  virtual void UnhandledWheelEvent(
      const blink::WebMouseWheelEvent& event) OVERRIDE;

  // IPC::Sender implementation.
  virtual bool Send(IPC::Message* message) OVERRIDE;

  // SoftwareFrameManagerClient implementation:
  virtual void SoftwareFrameWasFreed(
      uint32 output_surface_id, unsigned frame_id) OVERRIDE;
  virtual void ReleaseReferencesToSoftwareFrame() OVERRIDE;

  virtual SkBitmap::Config PreferredReadbackFormat() OVERRIDE;

  // Forwards the mouse event to the renderer.
  void ForwardMouseEvent(const blink::WebMouseEvent& event);

  void KillSelf();

  void SetTextInputActive(bool active);

  // Sends completed plugin IME notification and text back to the renderer.
  void PluginImeCompositionCompleted(const base::string16& text, int plugin_id);

  const std::string& selected_text() const { return selected_text_; }

  // Update the IOSurface to be drawn and call setNeedsDisplay on
  // |cocoa_view_|.
  void CompositorSwapBuffers(uint64 surface_handle,
                             const gfx::Size& size,
                             float scale_factor,
                             const std::vector<ui::LatencyInfo>& latency_info);

  // Draw the IOSurface by making its context current to this view.
  void DrawIOSurfaceWithoutCoreAnimation();

  // Called when a GPU error is detected. Posts a task to destroy all
  // compositing state.
  void GotAcceleratedCompositingError();

  // Sets the overlay view, which should be drawn in the same IOSurface
  // atop of this view, if both views are drawing accelerated content.
  // Overlay is stored as a weak ptr.
  void SetOverlayView(RenderWidgetHostViewMac* overlay,
                      const gfx::Point& offset);

  // Removes the previously set overlay view.
  void RemoveOverlayView();

  // Returns true and stores first rectangle for character range if the
  // requested |range| is already cached, otherwise returns false.
  // Exposed for testing.
  CONTENT_EXPORT bool GetCachedFirstRectForCharacterRange(
      NSRange range, NSRect* rect, NSRange* actual_range);

  // Returns true if there is line break in |range| and stores line breaking
  // point to |line_breaking_point|. The |line_break_point| is valid only if
  // this function returns true.
  bool GetLineBreakIndex(const std::vector<gfx::Rect>& bounds,
                         const gfx::Range& range,
                         size_t* line_break_point);

  // Returns composition character boundary rectangle. The |range| is
  // composition based range. Also stores |actual_range| which is corresponding
  // to actually used range for returned rectangle.
  gfx::Rect GetFirstRectForCompositionRange(const gfx::Range& range,
                                            gfx::Range* actual_range);

  // Converts from given whole character range to composition oriented range. If
  // the conversion failed, return gfx::Range::InvalidRange.
  gfx::Range ConvertCharacterRangeToCompositionRange(
      const gfx::Range& request_range);

  WebContents* GetWebContents();

  // These member variables should be private, but the associated ObjC class
  // needs access to them and can't be made a friend.

  // The associated Model.  Can be NULL if Destroy() is called when
  // someone (other than superview) has retained |cocoa_view_|.
  RenderWidgetHostImpl* render_widget_host_;

  // Whether last rendered frame was accelerated.
  bool last_frame_was_accelerated_;

  // The time at which this view started displaying white pixels as a result of
  // not having anything to paint (empty backing store from renderer). This
  // value returns true for is_null() if we are not recording whiteout times.
  base::TimeTicks whiteout_start_time_;

  // The time it took after this view was selected for it to be fully painted.
  base::TimeTicks web_contents_switch_paint_time_;

  // Current text input type.
  ui::TextInputType text_input_type_;
  bool can_compose_inline_;

  // The background CoreAnimation layer which is hosted by |cocoa_view_|.
  // The compositing or software layers will be added as sublayers to this.
  base::scoped_nsobject<CALayer> background_layer_;

  // The CoreAnimation layer for software compositing. This should be NULL
  // when software compositing is not in use.
  base::scoped_nsobject<SoftwareLayer> software_layer_;

  // Accelerated compositing structures. These may be dynamically created and
  // destroyed together in Create/DestroyCompositedIOSurfaceAndLayer.
  base::scoped_nsobject<CompositingIOSurfaceLayer> compositing_iosurface_layer_;
  scoped_ptr<CompositingIOSurfaceMac> compositing_iosurface_;
  scoped_refptr<CompositingIOSurfaceContext> compositing_iosurface_context_;

  // Timer used to dynamically transition the compositing layer in and out of
  // asynchronous mode.
  base::DelayTimer<RenderWidgetHostViewMac>
      compositing_iosurface_layer_async_timer_;

  // This holds the current software compositing framebuffer, if any.
  scoped_ptr<SoftwareFrameManager> software_frame_manager_;

  // Whether to allow overlapping views.
  bool allow_overlapping_views_;

  // Whether to use the CoreAnimation path to draw content.
  bool use_core_animation_;

  // Latency info to send back when the next frame appears on the
  // screen.
  std::vector<ui::LatencyInfo> pending_latency_info_;

  // When taking a screenshot when using CoreAnimation, add a delay of
  // a few frames to ensure that the contents have reached the screen
  // before reporting latency info.
  uint32 pending_latency_info_delay_;
  base::WeakPtrFactory<RenderWidgetHostViewMac>
      pending_latency_info_delay_weak_ptr_factory_;

  NSWindow* pepper_fullscreen_window() const {
    return pepper_fullscreen_window_;
  }

  CONTENT_EXPORT void release_pepper_fullscreen_window_for_testing();

  RenderWidgetHostViewMac* fullscreen_parent_host_view() const {
    return fullscreen_parent_host_view_;
  }

  RenderWidgetHostViewFrameSubscriber* frame_subscriber() const {
    return frame_subscriber_.get();
  }

  int window_number() const;

  // The scale factor for the screen that the view is currently on.
  float ViewScaleFactor() const;

  // Update the scale factor for the backing store and for any CALayers.
  void UpdateBackingStoreScaleFactor();

  // Ensure that the display link is associated with the correct display.
  void UpdateDisplayLink();

  // The scale factor of the backing store. Note that this is updated based on
  // ViewScaleFactor with some delay.
  float backing_store_scale_factor_;

  void AddPendingLatencyInfo(
      const std::vector<ui::LatencyInfo>& latency_info);
  void SendPendingLatencyInfoToHost();
  void TickPendingLatencyInfoDelay();

  void SendPendingSwapAck();

  void PauseForPendingResizeOrRepaintsAndDraw();

  // The geometric arrangement of the layers depends on cocoa_view's size, the
  // compositing IOSurface's rounded size, and the software frame size. Update
  // all of them using this function when any of those parameters changes. Also
  // update the scale factor of the layers.
  void LayoutLayers();

  bool HasPendingSwapAck() const { return pending_swap_ack_; }

 private:
  friend class RenderWidgetHostViewMacTest;

  struct PendingSwapAck {
    PendingSwapAck(int32 route_id, int gpu_host_id, int32 renderer_id)
        : route_id(route_id),
          gpu_host_id(gpu_host_id),
          renderer_id(renderer_id) {}
    int32 route_id;
    int gpu_host_id;
    int32 renderer_id;
  };
  scoped_ptr<PendingSwapAck> pending_swap_ack_;
  void AddPendingSwapAck(int32 route_id, int gpu_host_id, int32 renderer_id);

  // Returns whether this render view is a popup (autocomplete window).
  bool IsPopup() const;

  // Shuts down the render_widget_host_.  This is a separate function so we can
  // invoke it from the message loop.
  void ShutdownHost();

  void EnsureSoftwareLayer();
  void DestroySoftwareLayer();

  bool EnsureCompositedIOSurface() WARN_UNUSED_RESULT;
  void EnsureCompositedIOSurfaceLayer();
  enum DestroyCompositedIOSurfaceLayerBehavior {
    kLeaveLayerInHierarchy,
    kRemoveLayerFromHierarchy,
  };
  void DestroyCompositedIOSurfaceLayer(
      DestroyCompositedIOSurfaceLayerBehavior destroy_layer_behavior);
  enum DestroyContextBehavior {
    kLeaveContextBoundToView,
    kDestroyContext,
  };
  void DestroyCompositedIOSurfaceAndLayer(
      DestroyContextBehavior destroy_context_behavior);

  void DestroyCompositingStateOnError();

  // Unbind the GL context (if any) that is bound to |cocoa_view_|.
  void ClearBoundContextDrawable();

  // Called when a GPU SwapBuffers is received.
  void GotAcceleratedFrame();

  // Called when a software DIB is received.
  void GotSoftwareFrame();

  // Called if it has been a quarter-second since a GPU SwapBuffers has been
  // received. In this case, switch from polling for frames to pushing them.
  void TimerSinceGotAcceleratedFrameFired();

  // IPC message handlers.
  void OnPluginFocusChanged(bool focused, int plugin_id);
  void OnStartPluginIme();
  void OnDidChangeScrollbarsForMainFrame(bool has_horizontal_scrollbar,
                                         bool has_vertical_scrollbar);

  // Convert |rect| from the views coordinate (upper-left origin) into
  // the OpenGL coordinate (lower-left origin) and scale for HiDPI displays.
  gfx::Rect GetScaledOpenGLPixelRect(const gfx::Rect& rect);

  // Send updated vsync parameters to the renderer.
  void SendVSyncParametersToRenderer();

  // The associated view. This is weak and is inserted into the view hierarchy
  // to own this RenderWidgetHostViewMac object. Set to nil at the start of the
  // destructor.
  RenderWidgetHostViewCocoa* cocoa_view_;

  // Indicates if the page is loading.
  bool is_loading_;

  // The text to be shown in the tooltip, supplied by the renderer.
  base::string16 tooltip_text_;

  // Factory used to safely scope delayed calls to ShutdownHost().
  base::WeakPtrFactory<RenderWidgetHostViewMac> weak_factory_;

  // selected text on the renderer.
  std::string selected_text_;

  // The window used for popup widgets.
  base::scoped_nsobject<NSWindow> popup_window_;

  // The fullscreen window used for pepper flash.
  base::scoped_nsobject<NSWindow> pepper_fullscreen_window_;
  base::scoped_nsobject<FullscreenWindowManager> fullscreen_window_manager_;
  // Our parent host view, if this is fullscreen.  NULL otherwise.
  RenderWidgetHostViewMac* fullscreen_parent_host_view_;

  // The overlay view which is rendered above this one in the same
  // accelerated IOSurface.
  // Overlay view has |underlay_view_| set to this view.
  base::WeakPtr<RenderWidgetHostViewMac> overlay_view_;

  // Offset at which overlay view should be rendered.
  gfx::Point overlay_view_offset_;

  // The underlay view which this view is rendered above in the same
  // accelerated IOSurface.
  // Underlay view has |overlay_view_| set to this view.
  base::WeakPtr<RenderWidgetHostViewMac> underlay_view_;

  // Set to true when |underlay_view_| has drawn this view. After that point,
  // this view should not draw again until |underlay_view_| is changed.
  bool underlay_view_has_drawn_;

  // Factory used to safely reference overlay view set in SetOverlayView.
  base::WeakPtrFactory<RenderWidgetHostViewMac>
      overlay_view_weak_factory_;

  // Display link for getting vsync info.
  scoped_refptr<DisplayLinkMac> display_link_;

  // The current composition character range and its bounds.
  gfx::Range composition_range_;
  std::vector<gfx::Rect> composition_bounds_;

  // The current caret bounds.
  gfx::Rect caret_rect_;

  // Subscriber that listens to frame presentation events.
  scoped_ptr<RenderWidgetHostViewFrameSubscriber> frame_subscriber_;

  base::WeakPtrFactory<RenderWidgetHostViewMac>
      software_frame_weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(RenderWidgetHostViewMac);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_MAC_H_
