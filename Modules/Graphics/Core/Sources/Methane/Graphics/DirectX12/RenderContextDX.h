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

FILE: Methane/Graphics/DirectX12/RenderContextDX.cpp
DirectX 12 implementation of the render context interface.

******************************************************************************/

#pragma once

#include "ContextDX.hpp"

#include <Methane/Graphics/RenderContextBase.h>

#include <dxgi1_4.h>
#include <vector>

namespace Methane::Graphics
{

class RenderContextDX final // NOSONAR - manual destructor is required, inheritance hierarchy is greater than 5
    : public ContextDX<RenderContextBase>
{
public:
    RenderContextDX(const Platform::AppEnvironment& env, DeviceBase& device, tf::Executor& parallel_executor, const RenderContext::Settings& settings);
    ~RenderContextDX() override;

    // Context interface
    void WaitForGpu(WaitFor wait_for) override;

    // RenderContext interface
    bool ReadyToRender() const override { return true; }
    void Resize(const FrameSize& frame_size) override;
    void Present() override;
    Platform::AppView GetAppView() const override { return { nullptr }; }

    // ContextBase interface
    void Initialize(DeviceBase& device, bool is_callback_emitted = true) override;
    void Release() override;

    const wrl::ComPtr<IDXGISwapChain3>& GetNativeSwapChain() const { return m_cp_swap_chain; }

protected:
    // RenderContextBase overrides
    uint32_t GetNextFrameBufferIndex() override;

private:
    uint32_t GetPresentVSyncInterval() const noexcept { return GetSettings().vsync_enabled ? 1 : 0; }
    uint32_t GetPresentFlags() const noexcept         { return !GetSettings().vsync_enabled && m_is_tearing_supported ? DXGI_PRESENT_ALLOW_TEARING : 0; }

    void WaitForSwapChainLatency();

    const Platform::AppEnvironment m_platform_env;
    wrl::ComPtr<IDXGISwapChain3>   m_cp_swap_chain;
    HANDLE                         m_frame_latency_waitable_object {};
    bool                           m_is_tearing_supported = false;
};

} // namespace Methane::Graphics
