// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Utility functions for video testing.

#include "media/base/video_frame.h"

namespace media {
namespace cast {

// Compute and return PSNR between two frames.
double I420PSNR(const scoped_refptr<media::VideoFrame>& frame1,
                const scoped_refptr<media::VideoFrame>& frame2);

// Populate a video frame with values starting with the given start value.
// Width, height and stride should be set in advance.
// Memory is allocated within the function.
void PopulateVideoFrame(VideoFrame* frame, int start_value);

// Populate a video frame from a file.
// Returns true if frame was populated, false if not (EOF).
bool PopulateVideoFrameFromFile(VideoFrame* frame, FILE* video_file);

}  // namespace cast
}  // namespace media
