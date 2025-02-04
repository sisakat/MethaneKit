/******************************************************************************

Copyright 2019-2022 Evgeny Gorodetskiy

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

FILE: Methane/Graphics/Vulkan/RenderContextVK.hh
Vulkan implementation of the render context interface.

******************************************************************************/

#pragma once

#include "ContextVK.hpp"

#include <Methane/Graphics/RenderContextBase.h>
#include <Methane/Platform/AppEnvironment.h>
#include <Methane/Data/Emitter.hpp>

#include <vulkan/vulkan.hpp>

#ifdef __APPLE__
#ifdef __OBJC__
#import <Methane/Platform/MacOS/AppViewMT.hh>
#else
using AppViewMT = void;
#endif
#endif

namespace Methane::Graphics
{

class RenderContextVK;

struct IRenderContextVKCallback
{
    virtual void OnRenderContextVKSwapchainChanged(RenderContextVK& context) = 0;

    virtual ~IRenderContextVKCallback() = default;
};

class RenderContextVK final // NOSONAR - this class requires destructor
    : public ContextVK<RenderContextBase>
    , public Data::Emitter<IRenderContextVKCallback>
{
public:
    RenderContextVK(const Platform::AppEnvironment& app_env, DeviceVK& device, tf::Executor& parallel_executor, const RenderContext::Settings& settings);
    ~RenderContextVK() override;

    // Context interface
    void WaitForGpu(WaitFor wait_for) override;

    // RenderContext interface
    bool     ReadyToRender() const override;
    void     Resize(const FrameSize& frame_size) override;
    void     Present() override;
    bool     SetVSyncEnabled(bool vsync_enabled) override;
    bool     SetFrameBuffersCount(uint32_t frame_buffers_count) override;
    Platform::AppView GetAppView() const override { return { }; }

    // ContextBase overrides
    void Initialize(DeviceBase& device, bool is_callback_emitted = true) override;
    void Release() override;

    // ObjectBase overrides
    bool SetName(const std::string& name) override;

    const vk::SurfaceKHR&   GetNativeSurface() const noexcept     { return m_vk_unique_surface.get(); }
    const vk::SwapchainKHR& GetNativeSwapchain() const noexcept   { return m_vk_unique_swapchain.get(); }
    const vk::Extent2D&     GetNativeFrameExtent() const noexcept { return m_vk_frame_extent; }
    vk::Format              GetNativeFrameFormat() const noexcept { return m_vk_frame_format; }
    const vk::Image&        GetNativeFrameImage(uint32_t frame_buffer_index) const;
    const vk::Semaphore&    GetNativeFrameImageAvailableSemaphore(uint32_t frame_buffer_index) const;
    const vk::Semaphore&    GetNativeFrameImageAvailableSemaphore() const;

protected:
    // RenderContextBase overrides
    uint32_t GetNextFrameBufferIndex() override;

private:
    vk::SurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& available_formats) const;
    vk::PresentModeKHR ChooseSwapPresentMode(const std::vector<vk::PresentModeKHR>& available_present_modes) const;
    vk::Extent2D ChooseSwapExtent(const vk::SurfaceCapabilitiesKHR& surface_caps) const;
    void InitializeNativeSwapchain();
    void ReleaseNativeSwapchainResources();
    void ResetNativeSwapchain();
    void ResetNativeObjectNames() const;

    const vk::Device m_vk_device;
#ifdef __APPLE__
    // MacOS metal app view with swap-chain implementation to work via MoltenVK
    AppViewMT* m_metal_view;
#endif
    const vk::UniqueSurfaceKHR       m_vk_unique_surface;
    vk::UniqueSwapchainKHR           m_vk_unique_swapchain;
    vk::Format                       m_vk_frame_format;
    vk::Extent2D                     m_vk_frame_extent;
    std::vector<vk::Image>           m_vk_frame_images;
    std::vector<vk::UniqueSemaphore> m_vk_frame_semaphores_pool;
    std::vector<vk::Semaphore>       m_vk_frame_image_available_semaphores;
};

} // namespace Methane::Graphics
