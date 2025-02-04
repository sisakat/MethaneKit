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

FILE: Methane/Graphics/DirectX12/FenceDX.h
DirectX 12 fence implementation.

******************************************************************************/

#pragma once

#include <Methane/Graphics/FenceBase.h>

#include <wrl.h>
#include <directx/d3d12.h>

namespace Methane::Graphics
{

namespace wrl = Microsoft::WRL;

class CommandQueueDX;

class FenceDX final // NOSONAR - custom destructor is required
    : public FenceBase
{
public:
    explicit FenceDX(CommandQueueBase& command_queue);
    FenceDX(const FenceDX&) = delete;
    FenceDX(FenceDX&&) noexcept = default;
    ~FenceDX() override;

    FenceDX& operator=(const FenceDX&) = delete;
    FenceDX& operator=(FenceDX&&) noexcept = default;

    // Fence overrides
    void Signal() override;
    void WaitOnCpu() override;
    void WaitOnGpu(CommandQueue& wait_on_command_queue) override;

    // Object override
    bool SetName(const std::string& name) override;

private:
    CommandQueueDX& GetCommandQueueDX();

    wrl::ComPtr<ID3D12Fence> m_cp_fence;
    HANDLE                   m_event = nullptr;
};

} // namespace Methane::Graphics
