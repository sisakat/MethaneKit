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

FILE: Methane/Graphics/DirectX12/DescriptorHeapDX.cpp
Descriptor Heap is a platform abstraction of DirectX 12 descriptor heaps.

******************************************************************************/

#include "DescriptorHeapDX.h"
#include "DeviceDX.h"

#include <Methane/Graphics/ResourceBase.h>
#include <Methane/Graphics/ContextBase.h>
#include <Methane/Graphics/Windows/DirectXErrorHandling.h>
#include <Methane/Data/RangeUtils.hpp>
#include <Methane/Instrumentation.h>
#include <Methane/Checks.hpp>

namespace Methane::Graphics
{

static D3D12_DESCRIPTOR_HEAP_TYPE GetNativeHeapType(DescriptorHeapDX::Type type)
{
    META_FUNCTION_TASK();
    switch (type)
    {
    case DescriptorHeapDX::Type::ShaderResources: return D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    case DescriptorHeapDX::Type::Samplers:        return D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
    case DescriptorHeapDX::Type::RenderTargets:   return D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    case DescriptorHeapDX::Type::DepthStencil:    return D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    default:                                    META_UNEXPECTED_ARG_RETURN(type, D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES);
    }
}

DescriptorHeapDX::Reservation::Reservation(const Ref<DescriptorHeapDX>& heap)
    : heap(heap)
{
    META_FUNCTION_TASK();
    std::fill(ranges.begin(), ranges.end(), DescriptorHeapDX::Range(0, 0));
}

DescriptorHeapDX::Reservation::Reservation(const Ref<DescriptorHeapDX>& heap, const Ranges& ranges)
    : heap(heap)
    , ranges(ranges)
{
    META_FUNCTION_TASK();
}

DescriptorHeapDX::DescriptorHeapDX(const ContextBase& context, const Settings& settings)
    : m_context(context)
    , m_settings(settings)
    , m_deferred_size(settings.size)
    , m_descriptor_heap_type(GetNativeHeapType(settings.type))
    , m_descriptor_size(GetContextDX().GetDeviceDX().GetNativeDevice()->GetDescriptorHandleIncrementSize(m_descriptor_heap_type))
{
    META_FUNCTION_TASK();
    if (m_deferred_size > 0)
    {
        m_resources.reserve(m_deferred_size);
        m_free_ranges.Add({ 0, m_deferred_size });
    }
    if (m_settings.size > 0)
    {
        Allocate();
    }
}

DescriptorHeapDX::~DescriptorHeapDX()
{
    META_FUNCTION_TASK();
    std::scoped_lock lock_guard(m_modification_mutex);

    // All descriptor ranges must be released when heap is destroyed
    assert((!m_deferred_size && m_free_ranges.IsEmpty()) ||
           m_free_ranges == RangeSet({ { 0, m_deferred_size } }));
}

Data::Index DescriptorHeapDX::AddResource(const ResourceBase& resource)
{
    META_FUNCTION_TASK();
    std::scoped_lock lock_guard(m_modification_mutex);

    if (!m_settings.deferred_allocation)
    {
        META_CHECK_ARG_LESS_DESCR(m_resources.size(), m_settings.size + 1,
                                  "{} descriptor heap is full, no free space to add a resource",
                                  magic_enum::enum_name(m_settings.type));
    }
    else if (m_resources.size() >= m_settings.size)
    {
        m_deferred_size++;
        Allocate();
    }

    m_resources.push_back(&resource);

    const auto resource_index = static_cast<Data::Index>(m_resources.size() - 1);
    m_free_ranges.Remove(Range(resource_index, resource_index + 1));

    return static_cast<int32_t>(resource_index);
}

Data::Index DescriptorHeapDX::ReplaceResource(const ResourceBase& resource, Data::Index at_index)
{
    META_FUNCTION_TASK();
    std::scoped_lock lock_guard(m_modification_mutex);

    META_CHECK_ARG_LESS(at_index, m_resources.size());
    m_resources[at_index] = &resource;
    return at_index;
}

void DescriptorHeapDX::RemoveResource(Data::Index at_index)
{
    META_FUNCTION_TASK();
    std::scoped_lock lock_guard(m_modification_mutex);

    META_CHECK_ARG_LESS(at_index, m_resources.size());
    m_resources[at_index] = nullptr;
    m_free_ranges.Add(Range(at_index, at_index + 1));
}

DescriptorHeapDX::Range DescriptorHeapDX::ReserveRange(Data::Size length)
{
    META_FUNCTION_TASK();
    META_CHECK_ARG_NOT_ZERO_DESCR(length, "unable to reserve empty descriptor range");
    std::scoped_lock lock_guard(m_modification_mutex);

    if (const Range reserved_range = Data::ReserveRange(m_free_ranges, length);
        reserved_range || !m_settings.deferred_allocation)
        return reserved_range;

    Range deferred_range(m_deferred_size, m_deferred_size + length);
    m_deferred_size += length;
    return deferred_range;
}

void DescriptorHeapDX::ReleaseRange(const Range& range)
{
    META_FUNCTION_TASK();
    std::scoped_lock lock_guard(m_modification_mutex);
    m_free_ranges.Add(range);
}

void DescriptorHeapDX::SetDeferredAllocation(bool deferred_allocation)
{
    META_FUNCTION_TASK();
    m_settings.deferred_allocation = deferred_allocation;
}

D3D12_CPU_DESCRIPTOR_HANDLE DescriptorHeapDX::GetNativeCpuDescriptorHandle(uint32_t descriptor_index) const
{
    META_FUNCTION_TASK();
    META_CHECK_ARG_NOT_NULL(m_cp_descriptor_heap);
    META_CHECK_ARG_LESS(descriptor_index, GetAllocatedSize());
    return CD3DX12_CPU_DESCRIPTOR_HANDLE(m_cp_descriptor_heap->GetCPUDescriptorHandleForHeapStart(), descriptor_index, m_descriptor_size);
}

D3D12_GPU_DESCRIPTOR_HANDLE DescriptorHeapDX::GetNativeGpuDescriptorHandle(uint32_t descriptor_index) const
{
    META_FUNCTION_TASK();
    META_CHECK_ARG_NOT_NULL(m_cp_descriptor_heap);
    META_CHECK_ARG_LESS(descriptor_index, GetAllocatedSize());
    return CD3DX12_GPU_DESCRIPTOR_HANDLE(m_cp_descriptor_heap->GetGPUDescriptorHandleForHeapStart(), descriptor_index, m_descriptor_size);
}

void DescriptorHeapDX::Allocate()
{
    META_FUNCTION_TASK();
    const Data::Size allocated_size = GetAllocatedSize();
    const Data::Size deferred_size  = GetDeferredSize();

    if (allocated_size == deferred_size)
        return;

    const wrl::ComPtr<ID3D12Device>&  cp_device = GetContextDX().GetDeviceDX().GetNativeDevice();
    META_CHECK_ARG_NOT_NULL(cp_device);

    const bool is_shader_visible_heap = GetSettings().shader_visible;
    wrl::ComPtr<ID3D12DescriptorHeap> cp_old_descriptor_heap = m_cp_descriptor_heap;

    D3D12_DESCRIPTOR_HEAP_DESC heap_desc{};
    heap_desc.NumDescriptors = deferred_size;
    heap_desc.Type           = m_descriptor_heap_type;
    heap_desc.Flags          = is_shader_visible_heap
                             ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE
                             : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

    // Allocate new descriptor heap of deferred size
    ThrowIfFailed(cp_device->CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(&m_cp_descriptor_heap)), cp_device.Get());

    if (!is_shader_visible_heap && cp_old_descriptor_heap && allocated_size > 0)
    {
        // Copy descriptors from old heap to the new one. It works for non-shader-visible CPU heaps only.
        // Shader-visible heaps must be re-filled with updated descriptors
        // using ProgramBindings::CompleteInitialization() & DescriptorManagerDX::CompleteInitialization()
        cp_device->CopyDescriptorsSimple(allocated_size,
                                         m_cp_descriptor_heap->GetCPUDescriptorHandleForHeapStart(),
                                         cp_old_descriptor_heap->GetCPUDescriptorHandleForHeapStart(),
                                         m_descriptor_heap_type);
    }

    m_allocated_size = m_deferred_size;
    Emit(&IDescriptorHeapCallback::OnDescriptorHeapAllocated, std::ref(*this));
}

const IContextDX& DescriptorHeapDX::GetContextDX() const noexcept
{
    META_FUNCTION_TASK();
    return static_cast<const IContextDX&>(m_context);
}

} // namespace Methane::Graphics
