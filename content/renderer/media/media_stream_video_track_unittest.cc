// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "content/child/child_process.h"
#include "content/renderer/media/media_stream_video_track.h"
#include "content/renderer/media/mock_media_stream_video_sink.h"
#include "content/renderer/media/mock_media_stream_video_source.h"
#include "media/base/video_frame.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

ACTION_P(RunClosure, closure) {
  closure.Run();
}

class MediaStreamVideoTrackTest : public ::testing::Test {
 public:
  MediaStreamVideoTrackTest()
      : child_process_(new ChildProcess()),
        mock_source_(new MockMediaStreamVideoSource(false)),
        source_started_(false) {
    blink_source_.initialize(base::UTF8ToUTF16("dummy_source_id"),
                              blink::WebMediaStreamSource::TypeVideo,
                              base::UTF8ToUTF16("dummy_source_name"));
    blink_source_.setExtraData(mock_source_);
  }

  virtual ~MediaStreamVideoTrackTest() {
  }

  void DeliverVideoFrameAndWaitForRenderer(MockMediaStreamVideoSink* sink) {
    base::RunLoop run_loop;
    base::Closure quit_closure = run_loop.QuitClosure();
    EXPECT_CALL(*sink, OnVideoFrame()).WillOnce(
        RunClosure(quit_closure));
    scoped_refptr<media::VideoFrame> frame =
        media::VideoFrame::CreateBlackFrame(
            gfx::Size(MediaStreamVideoSource::kDefaultWidth,
                      MediaStreamVideoSource::kDefaultHeight));
    mock_source()->DeliverVideoFrame(frame);
    run_loop.Run();
  }

 protected:
  base::MessageLoop* io_message_loop() const {
    return child_process_->io_message_loop();
  }

  // Create a track that's associated with |mock_source_|.
  blink::WebMediaStreamTrack CreateTrack() {
    blink::WebMediaConstraints constraints;
    constraints.initialize();
    bool enabled = true;
    blink::WebMediaStreamTrack track =
        MediaStreamVideoTrack::CreateVideoTrack(
            mock_source_, constraints,
            MediaStreamSource::ConstraintsCallback(), enabled);
    if (!source_started_) {
      mock_source_->StartMockedSource();
      source_started_ = true;
    }
    return track;
  }

  MockMediaStreamVideoSource* mock_source() { return mock_source_; }
  const blink::WebMediaStreamSource& blink_source() const {
    return blink_source_;
  }

 private:
  scoped_ptr<ChildProcess> child_process_;
  base::MessageLoopForUI message_loop_;
  blink::WebMediaStreamSource blink_source_;
  // |mock_source_| is owned by |webkit_source_|.
  MockMediaStreamVideoSource* mock_source_;
  bool source_started_;
};

TEST_F(MediaStreamVideoTrackTest, AddAndRemoveSink) {
  MockMediaStreamVideoSink sink;
  blink::WebMediaStreamTrack track = CreateTrack();
  MediaStreamVideoSink::AddToVideoTrack(&sink, track);

  DeliverVideoFrameAndWaitForRenderer(&sink);
  EXPECT_EQ(1, sink.number_of_frames());

  DeliverVideoFrameAndWaitForRenderer(&sink);

  MediaStreamVideoSink::RemoveFromVideoTrack(&sink, track);

  scoped_refptr<media::VideoFrame> frame =
      media::VideoFrame::CreateBlackFrame(
          gfx::Size(MediaStreamVideoSource::kDefaultWidth,
                    MediaStreamVideoSource::kDefaultHeight));
  mock_source()->DeliverVideoFrame(frame);
  // Wait for the IO thread to complete delivering frames.
  io_message_loop()->RunUntilIdle();
  EXPECT_EQ(2, sink.number_of_frames());
}

TEST_F(MediaStreamVideoTrackTest, SetEnabled) {
  MockMediaStreamVideoSink sink;
  blink::WebMediaStreamTrack track = CreateTrack();
  MediaStreamVideoSink::AddToVideoTrack(&sink, track);

  MediaStreamVideoTrack* video_track =
      MediaStreamVideoTrack::GetVideoTrack(track);

  DeliverVideoFrameAndWaitForRenderer(&sink);
  EXPECT_EQ(1, sink.number_of_frames());

  video_track->SetEnabled(false);
  EXPECT_FALSE(sink.enabled());

  scoped_refptr<media::VideoFrame> frame =
      media::VideoFrame::CreateBlackFrame(
          gfx::Size(MediaStreamVideoSource::kDefaultWidth,
                    MediaStreamVideoSource::kDefaultHeight));
  mock_source()->DeliverVideoFrame(frame);
  // Wait for the IO thread to complete delivering frames.
  io_message_loop()->RunUntilIdle();
  EXPECT_EQ(1, sink.number_of_frames());

  video_track->SetEnabled(true);
  EXPECT_TRUE(sink.enabled());
  mock_source()->DeliverVideoFrame(frame);
  DeliverVideoFrameAndWaitForRenderer(&sink);
  EXPECT_EQ(2, sink.number_of_frames());
  MediaStreamVideoSink::RemoveFromVideoTrack(&sink, track);
}

TEST_F(MediaStreamVideoTrackTest, SourceStopped) {
  MockMediaStreamVideoSink sink;
  blink::WebMediaStreamTrack track = CreateTrack();
  MediaStreamVideoSink::AddToVideoTrack(&sink, track);
  EXPECT_EQ(blink::WebMediaStreamSource::ReadyStateLive, sink.state());

  mock_source()->StopSource();
  EXPECT_EQ(blink::WebMediaStreamSource::ReadyStateEnded, sink.state());
  MediaStreamVideoSink::RemoveFromVideoTrack(&sink, track);
}

TEST_F(MediaStreamVideoTrackTest, StopLastTrack) {
  MockMediaStreamVideoSink sink1;
  blink::WebMediaStreamTrack track1 = CreateTrack();
  MediaStreamVideoSink::AddToVideoTrack(&sink1, track1);
  EXPECT_EQ(blink::WebMediaStreamSource::ReadyStateLive, sink1.state());

  EXPECT_EQ(blink::WebMediaStreamSource::ReadyStateLive,
            blink_source().readyState());

  MockMediaStreamVideoSink sink2;
  blink::WebMediaStreamTrack track2 = CreateTrack();
  MediaStreamVideoSink::AddToVideoTrack(&sink2, track2);
  EXPECT_EQ(blink::WebMediaStreamSource::ReadyStateLive, sink2.state());

  MediaStreamVideoTrack* native_track1 =
      MediaStreamVideoTrack::GetVideoTrack(track1);
  native_track1->Stop();
  EXPECT_EQ(blink::WebMediaStreamSource::ReadyStateEnded, sink1.state());
  EXPECT_EQ(blink::WebMediaStreamSource::ReadyStateLive,
              blink_source().readyState());
  MediaStreamVideoSink::RemoveFromVideoTrack(&sink1, track1);

  MediaStreamVideoTrack* native_track2 =
        MediaStreamVideoTrack::GetVideoTrack(track2);
  native_track2->Stop();
  EXPECT_EQ(blink::WebMediaStreamSource::ReadyStateEnded, sink2.state());
  EXPECT_EQ(blink::WebMediaStreamSource::ReadyStateEnded,
            blink_source().readyState());
  MediaStreamVideoSink::RemoveFromVideoTrack(&sink2, track2);
}

}  // namespace content
