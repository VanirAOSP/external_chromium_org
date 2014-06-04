// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/autofill/autofill_textfield.h"

#include <algorithm>
#include <cmath>

#include "base/logging.h"
#include "ui/gfx/scoped_ns_graphics_context_save_gstate_mac.h"

namespace {

const CGFloat kGap = 6.0;  // gap between icon and text.
const CGFloat kMinimumHeight = 27.0;  // Enforced minimum height for text cells.

}  // namespace

@interface AutofillTextFieldCell (Internal)

- (NSRect)textFrameForFrame:(NSRect)frame;

@end

@implementation AutofillTextField

@synthesize inputDelegate = inputDelegate_;

+ (Class)cellClass {
  return [AutofillTextFieldCell class];
}

- (id)initWithFrame:(NSRect)frame {
  if (self = [super initWithFrame:frame])
    [super setDelegate:self];
  return self;
}

- (BOOL)becomeFirstResponder {
  BOOL result = [super becomeFirstResponder];
  if (result && inputDelegate_) {
    [inputDelegate_ fieldBecameFirstResponder:self];
    shouldFilterClick_ = YES;
  }
  return result;
}

- (void)onEditorMouseDown:(id)sender {
  // Since the dialog does not care about clicks that gave firstResponder
  // status, swallow those.
  if (!handlingFirstClick_)
    [inputDelegate_ onMouseDown: self];
}

- (NSRect)decorationFrame {
  return [[self cell] decorationFrameForFrame:[self frame]];
}

- (void)mouseDown:(NSEvent*)theEvent {
  // mouseDown: is only invoked for a click that actually gave firstResponder
  // status to the NSTextField, and clicks to the border area. Further clicks
  // into the content are are handled by the field editor instead.
  handlingFirstClick_ = shouldFilterClick_;
  [super mouseDown:theEvent];
  handlingFirstClick_ = NO;
  shouldFilterClick_ = NO;
}

- (void)controlTextDidEndEditing:(NSNotification*)notification {
  if (inputDelegate_)
    [inputDelegate_ didEndEditing:self];
}

- (void)controlTextDidChange:(NSNotification*)aNotification {
  if (inputDelegate_)
    [inputDelegate_ didChange:self];
}

- (NSString*)fieldValue {
  return [[self cell] fieldValue];
}

- (void)setFieldValue:(NSString*)fieldValue {
  [[self cell] setFieldValue:fieldValue];
}

- (NSString*)defaultValue {
  return [[self cell] defaultValue];
}

- (void)setDefaultValue:(NSString*)defaultValue {
  [[self cell] setDefaultValue:defaultValue];
}

- (BOOL)isDefault {
  return [[[self cell] fieldValue] isEqualToString:[[self cell] defaultValue]];
}

- (NSString*)validityMessage {
  return validityMessage_;
}

- (void)setValidityMessage:(NSString*)validityMessage {
  validityMessage_.reset([validityMessage copy]);
  [[self cell] setInvalid:[self invalid]];
}

- (BOOL)invalid {
  return [validityMessage_ length] != 0;
}

@end


@implementation AutofillTextFieldCell

@synthesize invalid = invalid_;
@synthesize defaultValue = defaultValue_;
@synthesize decorationSize = decorationSize_;

- (void)setInvalid:(BOOL)invalid {
  invalid_ = invalid;
  [[self controlView] setNeedsDisplay:YES];
}

- (NSImage*) icon{
  return icon_;
}

- (void)setIcon:(NSImage*)icon {
  icon_.reset([icon retain]);
  [self setDecorationSize:[icon_ size]];
  [[self controlView] setNeedsDisplay:YES];
}

- (NSString*)fieldValue {
  return [self stringValue];
}

- (void)setFieldValue:(NSString*)fieldValue {
  [self setStringValue:fieldValue];
}

- (NSRect)textFrameForFrame:(NSRect)frame {
  // Ensure text height is original cell height, and the text frame is centered
  // vertically in the cell frame.
  NSSize originalSize = [super cellSize];
  if (originalSize.height < NSHeight(frame)) {
    CGFloat delta = NSHeight(frame) - originalSize.height;
    frame.origin.y += std::floor(delta / 2.0);
    frame.size.height -= delta;
  }
  DCHECK_EQ(originalSize.height, NSHeight(frame));

  if (decorationSize_.width > 0) {
    NSRect textFrame, decorationFrame;
    NSDivideRect(frame, &decorationFrame, &textFrame,
                 kGap + decorationSize_.width, NSMaxXEdge);
    return textFrame;
  }
  return frame;
}

- (NSRect)decorationFrameForFrame:(NSRect)frame {
  NSRect decorationFrame;
  if (decorationSize_.width > 0) {
    NSRect textFrame;
    NSDivideRect(frame, &decorationFrame, &textFrame,
                 kGap + decorationSize_.width, NSMaxXEdge);
    decorationFrame.size = decorationSize_;
    decorationFrame.origin.y +=
        roundf((NSHeight(frame) - NSHeight(decorationFrame)) / 2.0);
  }
  return decorationFrame;
}

- (NSSize)cellSize {
  NSSize cellSize = [super cellSize];

  if (decorationSize_.width > 0) {
    cellSize.width += kGap + decorationSize_.width;
    cellSize.height = std::max(cellSize.height, decorationSize_.height);
  }
  cellSize.height = std::max(cellSize.height, kMinimumHeight);
  return cellSize;
}

- (void)editWithFrame:(NSRect)cellFrame
               inView:(NSView *)controlView
               editor:(NSText *)editor
             delegate:(id)delegate
                event:(NSEvent *)event {
  [super editWithFrame:[self textFrameForFrame:cellFrame]
                inView:controlView
                editor:editor
              delegate:delegate
                 event:event];
}

- (void)selectWithFrame:(NSRect)cellFrame
                 inView:(NSView *)controlView
                 editor:(NSText *)editor
               delegate:(id)delegate
                  start:(NSInteger)start
                 length:(NSInteger)length {
  [super selectWithFrame:[self textFrameForFrame:cellFrame]
                  inView:controlView
                  editor:editor
                delegate:delegate
                   start:start
                  length:length];
}

- (void)drawInteriorWithFrame:(NSRect)cellFrame inView:(NSView*)controlView {
  NSRect textFrame = [self textFrameForFrame:cellFrame];
  [super drawInteriorWithFrame:textFrame inView:controlView];
}

- (void)drawWithFrame:(NSRect)cellFrame inView:(NSView*)controlView {
  [super drawWithFrame:cellFrame inView:controlView];

  if (icon_) {
    NSRect iconFrame = [self decorationFrameForFrame:cellFrame];
    [icon_ drawInRect:iconFrame
             fromRect:NSZeroRect
            operation:NSCompositeSourceOver
             fraction:1.0
       respectFlipped:YES
                hints:nil];
  }

  if (invalid_) {
    gfx::ScopedNSGraphicsContextSaveGState state;

    // Render red border for invalid fields.
    [[NSColor colorWithDeviceRed:1.0 green:0.0 blue:0.0 alpha:1.0] setStroke];
    [[NSBezierPath bezierPathWithRect:NSInsetRect(cellFrame, 0.5, 0.5)] stroke];
  }
}

@end
