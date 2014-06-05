// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/layer_tree_json_parser.h"

#include "base/test/values_test_util.h"
#include "base/values.h"
#include "cc/layers/content_layer.h"
#include "cc/layers/layer.h"
#include "cc/layers/nine_patch_layer.h"
#include "cc/layers/picture_layer.h"
#include "cc/layers/solid_color_layer.h"
#include "cc/layers/texture_layer.h"

namespace cc {

namespace {

scoped_refptr<Layer> ParseTreeFromValue(base::Value* val,
                                        ContentLayerClient* content_client) {
  DictionaryValue* dict;
  bool success = true;
  success &= val->GetAsDictionary(&dict);
  std::string layer_type;
  success &= dict->GetString("LayerType", &layer_type);
  ListValue* list;
  success &= dict->GetList("Bounds", &list);
  int width, height;
  success &= list->GetInteger(0, &width);
  success &= list->GetInteger(1, &height);
  success &= dict->GetList("Position", &list);
  double position_x, position_y;
  success &= list->GetDouble(0, &position_x);
  success &= list->GetDouble(1, &position_y);

  bool draws_content;
  success &= dict->GetBoolean("DrawsContent", &draws_content);

  scoped_refptr<Layer> new_layer;
  if (layer_type == "SolidColorLayer") {
    new_layer = SolidColorLayer::Create();
  } else if (layer_type == "ContentLayer") {
    new_layer = ContentLayer::Create(content_client);
  } else if (layer_type == "NinePatchLayer") {
    success &= dict->GetList("ImageAperture", &list);
    int aperture_x, aperture_y, aperture_width, aperture_height;
    success &= list->GetInteger(0, &aperture_x);
    success &= list->GetInteger(1, &aperture_y);
    success &= list->GetInteger(2, &aperture_width);
    success &= list->GetInteger(3, &aperture_height);

    ListValue* bounds;
    success &= dict->GetList("ImageBounds", &bounds);
    double image_width, image_height;
    success &= bounds->GetDouble(0, &image_width);
    success &= bounds->GetDouble(1, &image_height);

    success &= dict->GetList("Border", &list);
    int border_x, border_y, border_width, border_height;
    success &= list->GetInteger(0, &border_x);
    success &= list->GetInteger(1, &border_y);
    success &= list->GetInteger(2, &border_width);
    success &= list->GetInteger(3, &border_height);

    bool fill_center;
    success &= dict->GetBoolean("FillCenter", &fill_center);

    scoped_refptr<NinePatchLayer> nine_patch_layer = NinePatchLayer::Create();

    SkBitmap bitmap;
    bitmap.setConfig(SkBitmap::kARGB_8888_Config, image_width, image_height);
    bitmap.allocPixels(NULL, NULL);
    bitmap.setImmutable();
    nine_patch_layer->SetBitmap(bitmap);
    nine_patch_layer->SetAperture(
        gfx::Rect(aperture_x, aperture_y, aperture_width, aperture_height));
    nine_patch_layer->SetBorder(
        gfx::Rect(border_x, border_y, border_width, border_height));
    nine_patch_layer->SetFillCenter(fill_center);

    new_layer = nine_patch_layer;
  } else if (layer_type == "TextureLayer") {
    new_layer = TextureLayer::CreateForMailbox(NULL);
  } else if (layer_type == "PictureLayer") {
    new_layer = PictureLayer::Create(content_client);
  } else {  // Type "Layer" or "unknown"
    new_layer = Layer::Create();
  }
  new_layer->SetAnchorPoint(gfx::Point());
  new_layer->SetPosition(gfx::PointF(position_x, position_y));
  new_layer->SetBounds(gfx::Size(width, height));
  new_layer->SetIsDrawable(draws_content);

  double opacity;
  if (dict->GetDouble("Opacity", &opacity))
    new_layer->SetOpacity(opacity);

  bool contents_opaque;
  if (dict->GetBoolean("ContentsOpaque", &contents_opaque))
    new_layer->SetContentsOpaque(contents_opaque);

  bool scrollable;
  if (dict->GetBoolean("Scrollable", &scrollable))
    new_layer->SetScrollable(scrollable);

  bool wheel_handler;
  if (dict->GetBoolean("WheelHandler", &wheel_handler))
    new_layer->SetHaveWheelEventHandlers(wheel_handler);

  if (dict->HasKey("TouchRegion")) {
    success &= dict->GetList("TouchRegion", &list);
    Region touch_region;
    for (size_t i = 0; i < list->GetSize(); ) {
      int rect_x, rect_y, rect_width, rect_height;
      success &= list->GetInteger(i++, &rect_x);
      success &= list->GetInteger(i++, &rect_y);
      success &= list->GetInteger(i++, &rect_width);
      success &= list->GetInteger(i++, &rect_height);
      touch_region.Union(gfx::Rect(rect_x, rect_y, rect_width, rect_height));
    }
    new_layer->SetTouchEventHandlerRegion(touch_region);
  }

  success &= dict->GetList("DrawTransform", &list);
  double transform[16];
  for (int i = 0; i < 16; ++i)
    success &= list->GetDouble(i, &transform[i]);

  gfx::Transform layer_transform;
  layer_transform.matrix().setColMajord(transform);
  new_layer->SetTransform(layer_transform);

  success &= dict->GetList("Children", &list);
  for (ListValue::const_iterator it = list->begin();
       it != list->end(); ++it) {
    new_layer->AddChild(ParseTreeFromValue(*it, content_client));
  }

  if (!success)
    return NULL;

  return new_layer;
}

}  // namespace

scoped_refptr<Layer> ParseTreeFromJson(std::string json,
                                       ContentLayerClient* content_client) {
  scoped_ptr<base::Value> val = base::test::ParseJson(json);
  return ParseTreeFromValue(val.get(), content_client);
}

}  // namespace cc
