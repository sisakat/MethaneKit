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

FILE: Methane/Graphics/DirectX12/ContextDX.h
DirectX 12 context accessor interface for template class ContextDX<ContextBaseT>

******************************************************************************/

#pragma once

#include <Methane/Graphics/CommandList.h>

#include <directx/d3d12.h>

namespace Methane::Graphics
{

class CommandQueueDX;
class DeviceDX;
class DescriptorManagerDX;

struct IContextDX
{
    virtual const DeviceDX& GetDeviceDX() const noexcept = 0;
    virtual CommandQueueDX& GetDefaultCommandQueueDX(CommandList::Type type) = 0;
    virtual ID3D12QueryHeap& GetNativeQueryHeap(D3D12_QUERY_HEAP_TYPE type, uint32_t max_query_count = 1U << 15U) const = 0;
    virtual DescriptorManagerDX& GetDescriptorManagerDX() const noexcept = 0;

    virtual ~IContextDX() = default;
};

} // namespace Methane::Graphics
