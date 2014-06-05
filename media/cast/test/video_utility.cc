// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <math.h>
#include <cstdio>

#include "media/cast/test/video_utility.h"
#include "third_party/libyuv/include/libyuv/compare.h"
#include "ui/gfx/size.h"

namespace media {
namespace cast {

double I420PSNR(const scoped_refptr<media::VideoFrame>& frame1,
                const scoped_refptr<media::VideoFrame>& frame2) {
  if (frame1->coded_size().width() != frame2->coded_size().width() ||
      frame1->coded_size().height() != frame2->coded_size().height()) return -1;

  return libyuv::I420Psnr(
      frame1->data(VideoFrame::kYPlane), frame1->stride(VideoFrame::kYPlane),
      frame1->data(VideoFrame::kUPlane), frame1->stride(VideoFrame::kUPlane),
      frame1->data(VideoFrame::kVPlane), frame1->stride(VideoFrame::kVPlane),
      frame2->data(VideoFrame::kYPlane), frame2->stride(VideoFrame::kYPlane),
      frame2->data(VideoFrame::kUPlane), frame2->stride(VideoFrame::kUPlane),
      frame2->data(VideoFrame::kVPlane), frame2->stride(VideoFrame::kVPlane),
      frame1->coded_size().width(), frame1->coded_size().height());
}

void PopulateVideoFrame(VideoFrame* frame, int start_value) {
  int width = frame->coded_size().width();
  int height = frame->coded_size().height();
  int half_width = (width + 1) / 2;
  int half_height = (height + 1) / 2;
  uint8* y_plane = frame->data(VideoFrame::kYPlane);
  uint8* u_plane = frame->data(VideoFrame::kUPlane);
  uint8* v_plane = frame->data(VideoFrame::kVPlane);

  // Set Y.
  for (int i = 0; i < width * height; ++i) {
    y_plane[i] = static_cast<uint8>(start_value + i);
  }

  // Set U.
  for (int i = 0; i < half_width * half_height; ++i) {
    u_plane[i] = static_cast<uint8>(start_value + i);
  }

  // Set V.
  for (int i = 0; i < half_width * half_height; ++i) {
    v_plane[i] = static_cast<uint8>(start_value + i);
  }
}

bool PopulateVideoFrameFromFile(VideoFrame* frame, FILE* video_file) {
  int width = frame->coded_size().width();
  int height = frame->coded_size().height();
  int half_width = (width + 1) / 2;
  int half_height = (height + 1) / 2;
  size_t frame_size = width * height + 2 * half_width * half_height;
  uint8* y_plane = frame->data(VideoFrame::kYPlane);
  uint8* u_plane = frame->data(VideoFrame::kUPlane);
  uint8* v_plane = frame->data(VideoFrame::kVPlane);

  uint8* raw_data = new uint8[frame_size];
  size_t count = fread(raw_data, 1, frame_size, video_file);
  if (count != frame_size) return false;

  memcpy(y_plane, raw_data, width * height);
  memcpy(u_plane, raw_data + width * height, half_width * half_height);
  memcpy(v_plane, raw_data + width * height +
      half_width * half_height, half_width * half_height);
  delete [] raw_data;
  return true;
}

}  // namespace cast
}  // namespace media
