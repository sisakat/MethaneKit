/******************************************************************************

Copyright 2019-2020 Evgeny Gorodetskiy

Licensed under the Apache License, Version 2.0 (the "License"),
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

*******************************************************************************

FILE: Methane/Graphics/Metal/RenderContextMT.mm
Metal implementation of the render context interface.

******************************************************************************/

#include "RenderContextMT.hh"
#include "RenderPassMT.hh"
#include "CommandQueueMT.hh"
#include "TypesMT.hh"
#include "RenderContextAppViewMT.hh"

#include <Methane/Instrumentation.h>
#include <Methane/Platform/Utils.h>
#include <Methane/Platform/Apple/Types.hh>

// Either use dispatch queue semaphore or fence primitives for CPU-GPU frames rendering synchronization
// NOTE: when fences are used for frames synchronization,
// application runs slower than expected when started from XCode, but runs normally when started from Finder
//#define USE_DISPATCH_QUEUE_SEMAPHORE

// Enables automatic capture of all initialization commands before the first frame rendering
//#define CAPTURE_INITIALIZATION_SCOPE

namespace Methane::Graphics
{

Ptr<RenderContext> RenderContext::Create(const Platform::AppEnvironment& env, Device& device, tf::Executor& parallel_executor, const RenderContext::Settings& settings)
{
    META_FUNCTION_TASK();
    DeviceBase& device_base = static_cast<DeviceBase&>(device);
    Ptr<RenderContextMT> render_context_ptr = std::make_shared<RenderContextMT>(env, device_base, parallel_executor, settings);
    render_context_ptr->Initialize(device_base, true);
    return render_context_ptr;
}

RenderContextMT::RenderContextMT(const Platform::AppEnvironment& env, DeviceBase& device, tf::Executor& parallel_executor, const RenderContext::Settings& settings)
    : ContextMT<RenderContextBase>(device, parallel_executor, settings)
    , m_app_view(CreateRenderContextAppView(env, settings))
    , m_frame_capture_scope([[MTLCaptureManager sharedCaptureManager] newCaptureScopeWithDevice:ContextMT<RenderContextBase>::GetDeviceMT().GetNativeDevice()])
#ifdef USE_DISPATCH_QUEUE_SEMAPHORE
    , m_dispatch_semaphore(dispatch_semaphore_create(settings.frame_buffers_count))
#endif
{
    META_FUNCTION_TASK();
    META_UNUSED(m_dispatch_semaphore);

    m_frame_capture_scope.label = Methane::MacOS::ConvertToNsType<std::string, NSString*>(device.GetName() + " Frame Scope");
    [MTLCaptureManager sharedCaptureManager].defaultCaptureScope = m_frame_capture_scope;

#ifdef CAPTURE_INITIALIZATION_SCOPE
    // Begin frame capture scope at context creation to enable capturing all initialization commands
    Capture(m_frame_capture_scope);
    BeginFrameCaptureScope();
#endif

    // Start redrawing main view
    m_app_view.redrawing = YES;
}

RenderContextMT::~RenderContextMT()
{
    META_FUNCTION_TASK();

#ifdef USE_DISPATCH_QUEUE_SEMAPHORE
    dispatch_release(m_dispatch_semaphore);
#endif
}

void RenderContextMT::Release()
{
    META_FUNCTION_TASK();
    
    m_app_view.redrawing = NO;
    
#ifdef USE_DISPATCH_QUEUE_SEMAPHORE
    dispatch_release(m_dispatch_semaphore);
#endif

    ContextMT<RenderContextBase>::Release();
}

void RenderContextMT::Initialize(DeviceBase& device, bool is_callback_emitted)
{
    META_FUNCTION_TASK();

    ContextMT<RenderContextBase>::Initialize(device, is_callback_emitted);
    
#ifdef USE_DISPATCH_QUEUE_SEMAPHORE
    m_dispatch_semaphore = dispatch_semaphore_create(GetSettings().frame_buffers_count);
#endif
    
    m_app_view.redrawing = YES;
}

bool RenderContextMT::ReadyToRender() const
{
    META_FUNCTION_TASK();
    return m_app_view.redrawing;
}

void RenderContextMT::WaitForGpu(WaitFor wait_for)
{
    META_FUNCTION_TASK();
    
#ifdef USE_DISPATCH_QUEUE_SEMAPHORE
    if (wait_for != WaitFor::FramePresented)
        ContextMT<RenderContextBase>::WaitForGpu(wait_for);
#else
    ContextMT<RenderContextBase>::WaitForGpu(wait_for);
#endif
    
    if (wait_for == WaitFor::FramePresented)
    {
#ifdef USE_DISPATCH_QUEUE_SEMAPHORE
        OnGpuWaitStart(wait_for);
        dispatch_semaphore_wait(m_dispatch_semaphore, DISPATCH_TIME_FOREVER);
        OnGpuWaitComplete(wait_for);
#endif
        BeginFrameCaptureScope();
    }
}

void RenderContextMT::Resize(const FrameSize& frame_size)
{
    META_FUNCTION_TASK();
    ContextMT<RenderContextBase>::Resize(frame_size);
}

void RenderContextMT::Present()
{
    META_FUNCTION_TASK();
    ContextMT<RenderContextBase>::Present();

    id<MTLCommandBuffer> mtl_cmd_buffer = [GetDefaultCommandQueueMT(CommandList::Type::Render).GetNativeCommandQueue() commandBuffer];
    mtl_cmd_buffer.label = [NSString stringWithFormat:@"%@ Present Command", GetNsName()];
#ifdef USE_DISPATCH_QUEUE_SEMAPHORE
    [mtl_cmd_buffer addCompletedHandler:^(id<MTLCommandBuffer> _Nonnull) {
        dispatch_semaphore_signal(m_dispatch_semaphore);
    }];
#endif
    [mtl_cmd_buffer presentDrawable:GetNativeDrawable()];
    [mtl_cmd_buffer commit];

    EndFrameCaptureScope();

#ifdef USE_DISPATCH_QUEUE_SEMAPHORE
    ContextMT<RenderContextBase>::OnCpuPresentComplete(false);
#else
    ContextMT<RenderContextBase>::OnCpuPresentComplete(true);
#endif
    
    UpdateFrameBufferIndex();
}

bool RenderContextMT::SetVSyncEnabled(bool vsync_enabled)
{
    META_FUNCTION_TASK();
    if (ContextMT<RenderContextBase>::SetVSyncEnabled(vsync_enabled))
    {
        m_app_view.vsyncEnabled = vsync_enabled;
        return true;
    }
    return false;
}

bool RenderContextMT::SetFrameBuffersCount(uint32_t frame_buffers_count)
{
    META_FUNCTION_TASK();
    frame_buffers_count = std::min(std::max(2U, frame_buffers_count), 3U); // Metal supports only 2 or 3 drawable buffers
    if (ContextMT<RenderContextBase>::SetFrameBuffersCount(frame_buffers_count))
    {
        m_app_view.drawableCount = frame_buffers_count;
        return true;
    }
    return false;
}

void RenderContextMT::BeginFrameCaptureScope()
{
    META_FUNCTION_TASK();
    if (m_frame_capture_scope_begun)
    {
        [m_frame_capture_scope endScope];
    }
    else
    {
        m_frame_capture_scope_begun = true;
    }

    [m_frame_capture_scope beginScope];
}

void RenderContextMT::EndFrameCaptureScope()
{
    META_FUNCTION_TASK();
    META_CHECK_ARG_TRUE_DESCR(m_frame_capture_scope_begun, "Metal frame capture scope was not begun");

    [m_frame_capture_scope endScope];
    m_frame_capture_scope_begun = false;
}

void RenderContextMT::Capture(const id<MTLCaptureScope>& mtl_capture_scope)
{
    META_FUNCTION_TASK();
    META_CHECK_ARG_NOT_NULL(mtl_capture_scope);
    
    MTLCaptureManager*    mtl_capture_manager = [MTLCaptureManager sharedCaptureManager];
    MTLCaptureDescriptor* mtl_capture_desc    = [[MTLCaptureDescriptor alloc] init];
    mtl_capture_desc.captureObject = mtl_capture_scope;

    NSError* ns_error = nil;
    const bool capture_success = [mtl_capture_manager startCaptureWithDescriptor:mtl_capture_desc error:&ns_error];
    META_CHECK_ARG_TRUE_DESCR(capture_success, "failed to capture Metal scope '{}', error: {}",
                              MacOS::ConvertFromNsType<NSString, std::string>(mtl_capture_scope.label),
                              MacOS::ConvertFromNsType<NSString, std::string>([ns_error localizedDescription]));
}

} // namespace Methane::Graphics
