// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/in_process/synchronous_compositor_factory_impl.h"

#include "content/browser/android/in_process/synchronous_compositor_output_surface.h"
#include "content/public/browser/browser_thread.h"
#include "gpu/command_buffer/client/gl_in_process_context.h"
#include "ui/gl/android/surface_texture.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/gl_surface_stub.h"
#include "webkit/common/gpu/context_provider_in_process.h"
#include "webkit/common/gpu/webgraphicscontext3d_in_process_command_buffer_impl.h"

namespace content {

namespace {

blink::WebGraphicsContext3D::Attributes GetDefaultAttribs() {
  blink::WebGraphicsContext3D::Attributes attributes;
  attributes.antialias = false;
  attributes.depth = false;
  attributes.stencil = false;
  attributes.shareResources = true;
  attributes.noAutomaticFlushes = true;

  return attributes;
}

using webkit::gpu::WebGraphicsContext3DInProcessCommandBufferImpl;

scoped_ptr<gpu::GLInProcessContext> CreateContext(
    scoped_refptr<gfx::GLSurface> surface,
    scoped_refptr<gpu::InProcessCommandBuffer::Service> service,
    gpu::GLInProcessContext* share_context) {
  const gfx::GpuPreference gpu_preference = gfx::PreferDiscreteGpu;

  if (!surface)
    surface = gfx::GLSurface::CreateOffscreenGLSurface(gfx::Size(1, 1));

  gpu::GLInProcessContextAttribs in_process_attribs;
  WebGraphicsContext3DInProcessCommandBufferImpl::ConvertAttributes(
      GetDefaultAttribs(), &in_process_attribs);
  scoped_ptr<gpu::GLInProcessContext> context(
      gpu::GLInProcessContext::CreateWithSurface(surface,
                                                 service,
                                                 share_context,
                                                 in_process_attribs,
                                                 gpu_preference));
  return context.Pass();
}

scoped_ptr<WebGraphicsContext3DInProcessCommandBufferImpl> WrapContext(
    scoped_ptr<gpu::GLInProcessContext> context) {
  if (!context.get())
    return scoped_ptr<WebGraphicsContext3DInProcessCommandBufferImpl>();

  return scoped_ptr<WebGraphicsContext3DInProcessCommandBufferImpl>(
      WebGraphicsContext3DInProcessCommandBufferImpl::WrapContext(
          context.Pass(), GetDefaultAttribs()));
}

class VideoContextProvider
    : public StreamTextureFactorySynchronousImpl::ContextProvider {
 public:
  VideoContextProvider(
      scoped_ptr<gpu::GLInProcessContext> gl_in_process_context)
      : gl_in_process_context_(gl_in_process_context.get()) {

    context_provider_ = webkit::gpu::ContextProviderInProcess::Create(
        WrapContext(gl_in_process_context.Pass()),
        "Video-Offscreen-main-thread");
    context_provider_->BindToCurrentThread();
  }

  virtual scoped_refptr<gfx::SurfaceTexture> GetSurfaceTexture(
      uint32 stream_id) OVERRIDE {
    return gl_in_process_context_->GetSurfaceTexture(stream_id);
  }

  virtual blink::WebGraphicsContext3D* Context3d() OVERRIDE {
    return context_provider_->Context3d();
  }

 private:
  friend class base::RefCountedThreadSafe<VideoContextProvider>;
  virtual ~VideoContextProvider() {}

  scoped_refptr<cc::ContextProvider> context_provider_;
  gpu::GLInProcessContext* gl_in_process_context_;

  DISALLOW_COPY_AND_ASSIGN(VideoContextProvider);
};

}  // namespace

using webkit::gpu::WebGraphicsContext3DInProcessCommandBufferImpl;

SynchronousCompositorFactoryImpl::SynchronousCompositorFactoryImpl()
    : wrapped_gl_context_for_main_thread_(NULL),
      wrapped_gl_context_for_compositor_thread_(NULL),
      num_hardware_compositors_(0) {
  SynchronousCompositorFactory::SetInstance(this);
}

SynchronousCompositorFactoryImpl::~SynchronousCompositorFactoryImpl() {}

scoped_refptr<base::MessageLoopProxy>
SynchronousCompositorFactoryImpl::GetCompositorMessageLoop() {
  return BrowserThread::GetMessageLoopProxyForThread(BrowserThread::UI);
}

scoped_ptr<cc::OutputSurface>
SynchronousCompositorFactoryImpl::CreateOutputSurface(int routing_id) {
  scoped_ptr<SynchronousCompositorOutputSurface> output_surface(
      new SynchronousCompositorOutputSurface(routing_id));
  return output_surface.PassAs<cc::OutputSurface>();
}

InputHandlerManagerClient*
SynchronousCompositorFactoryImpl::GetInputHandlerManagerClient() {
  return synchronous_input_event_filter();
}

scoped_refptr<cc::ContextProvider>
SynchronousCompositorFactoryImpl::GetOffscreenContextProviderForMainThread() {
  bool failed = false;
  if ((!offscreen_context_for_main_thread_.get() ||
       offscreen_context_for_main_thread_->DestroyedOnMainThread())) {
    scoped_ptr<gpu::GLInProcessContext> context =
        CreateContext(NULL, NULL, NULL);
    wrapped_gl_context_for_main_thread_ = context.get();
    offscreen_context_for_main_thread_ =
        webkit::gpu::ContextProviderInProcess::Create(
            WrapContext(context.Pass()),
            "Compositor-Offscreen-main-thread");
    failed = !offscreen_context_for_main_thread_.get() ||
             !offscreen_context_for_main_thread_->BindToCurrentThread();
  }

  if (failed) {
    offscreen_context_for_main_thread_ = NULL;
    wrapped_gl_context_for_main_thread_ = NULL;
  }
  return offscreen_context_for_main_thread_;
}

// This is called on both renderer main thread (offscreen context creation
// path shared between cross-process and in-process platforms) and renderer
// compositor impl thread (InitializeHwDraw) in order to support Android
// WebView synchronously enable and disable hardware mode multiple times in
// the same task. This is ok because in-process WGC3D creation may happen on
// any thread and is lightweight.
scoped_refptr<cc::ContextProvider> SynchronousCompositorFactoryImpl::
    GetOffscreenContextProviderForCompositorThread() {
  base::AutoLock lock(offscreen_context_for_compositor_thread_lock_);
  DCHECK(service_);
  bool failed = false;
  if (!offscreen_context_for_compositor_thread_.get() ||
      offscreen_context_for_compositor_thread_->DestroyedOnMainThread()) {
    scoped_ptr<gpu::GLInProcessContext> context =
        CreateContext(new gfx::GLSurfaceStub, service_, NULL);
    wrapped_gl_context_for_compositor_thread_ = context.get();
    offscreen_context_for_compositor_thread_ =
        webkit::gpu::ContextProviderInProcess::Create(
            WrapContext(context.Pass()),
            "Compositor-Offscreen-compositor-thread");
    failed = !offscreen_context_for_compositor_thread_.get() ||
             !offscreen_context_for_compositor_thread_->BindToCurrentThread();
  }
  if (failed) {
    offscreen_context_for_compositor_thread_ = NULL;
    wrapped_gl_context_for_compositor_thread_ = NULL;
  }
  return offscreen_context_for_compositor_thread_;
}

scoped_refptr<cc::ContextProvider> SynchronousCompositorFactoryImpl::
    CreateOnscreenContextProviderForCompositorThread(
        scoped_refptr<gfx::GLSurface> surface) {
  DCHECK(surface);
  DCHECK(service_);
  DCHECK(wrapped_gl_context_for_compositor_thread_);

  return webkit::gpu::ContextProviderInProcess::Create(
      WrapContext(CreateContext(
          surface, service_, wrapped_gl_context_for_compositor_thread_)),
      "Compositor-Onscreen");
}

scoped_ptr<StreamTextureFactory>
SynchronousCompositorFactoryImpl::CreateStreamTextureFactory(int view_id) {
  scoped_ptr<StreamTextureFactorySynchronousImpl> factory(
      new StreamTextureFactorySynchronousImpl(
          base::Bind(&SynchronousCompositorFactoryImpl::
                          TryCreateStreamTextureFactory,
                     base::Unretained(this)),
          view_id));
  return factory.PassAs<StreamTextureFactory>();
}

void SynchronousCompositorFactoryImpl::CompositorInitializedHardwareDraw() {
  base::AutoLock lock(num_hardware_compositor_lock_);
  num_hardware_compositors_++;
}

void SynchronousCompositorFactoryImpl::CompositorReleasedHardwareDraw() {
  base::AutoLock lock(num_hardware_compositor_lock_);
  DCHECK_GT(num_hardware_compositors_, 0u);
  num_hardware_compositors_--;
}

bool SynchronousCompositorFactoryImpl::CanCreateMainThreadContext() {
  base::AutoLock lock(num_hardware_compositor_lock_);
  return num_hardware_compositors_ > 0;
}

scoped_refptr<StreamTextureFactorySynchronousImpl::ContextProvider>
SynchronousCompositorFactoryImpl::TryCreateStreamTextureFactory() {
  scoped_refptr<StreamTextureFactorySynchronousImpl::ContextProvider>
      context_provider;
  // This check only guarantees the main thread context is created after
  // a compositor did successfully initialize hardware draw in the past.
  // In particular this does not guarantee that the main thread context
  // will fail creation when all compositors release hardware draw.
  if (CanCreateMainThreadContext() && !video_context_provider_) {
    DCHECK(service_);
    DCHECK(wrapped_gl_context_for_compositor_thread_);

    video_context_provider_ = new VideoContextProvider(
        CreateContext(new gfx::GLSurfaceStub,
                      service_,
                      wrapped_gl_context_for_compositor_thread_));
  }
  return video_context_provider_;
}

void SynchronousCompositorFactoryImpl::SetDeferredGpuService(
    scoped_refptr<gpu::InProcessCommandBuffer::Service> service) {
  DCHECK(!service_);
  gfx::GLSurface::InitializeOneOff();
  service_ = service;
}

}  // namespace content
