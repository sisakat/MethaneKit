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

FILE: Methane/Graphics/DirectX12/ProgramDX.cpp
DirectX 12 implementation of the program interface.

******************************************************************************/

#include "DeviceDX.h"
#include "ProgramDX.h"
#include "ProgramBindingsDX.h"
#include "ShaderDX.h"
#include "RenderCommandListDX.h"

#include <Methane/Graphics/ContextBase.h>
#include <Methane/Instrumentation.h>
#include <Methane/Graphics/Windows/DirectXErrorHandling.h>

#include <directx/d3dx12.h>
#include <d3dcompiler.h>

#include <nowide/convert.hpp>
#include <iomanip>

namespace Methane::Graphics
{

[[nodiscard]]
static D3D12_DESCRIPTOR_RANGE_TYPE GetDescriptorRangeTypeByShaderInputType(D3D_SHADER_INPUT_TYPE input_type)
{
    META_FUNCTION_TASK();
    switch (input_type)
    {
    case D3D_SIT_CBUFFER:
        return D3D12_DESCRIPTOR_RANGE_TYPE_CBV;

    case D3D_SIT_SAMPLER:
        return D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;

    case D3D_SIT_TBUFFER:
    case D3D_SIT_TEXTURE:
    case D3D_SIT_STRUCTURED:
    case D3D_SIT_BYTEADDRESS:
        return D3D12_DESCRIPTOR_RANGE_TYPE_SRV;

    case D3D_SIT_UAV_RWTYPED:
    case D3D_SIT_UAV_RWSTRUCTURED:
    case D3D_SIT_UAV_RWBYTEADDRESS:
    case D3D_SIT_UAV_APPEND_STRUCTURED:
    case D3D_SIT_UAV_CONSUME_STRUCTURED:
    case D3D_SIT_UAV_RWSTRUCTURED_WITH_COUNTER:
        return D3D12_DESCRIPTOR_RANGE_TYPE_UAV;

    default:
        META_UNEXPECTED_ARG_RETURN(input_type, D3D12_DESCRIPTOR_RANGE_TYPE_SRV);
    }
}

[[nodiscard]]
static DescriptorHeapDX::Type GetDescriptorHeapTypeByRangeType(D3D12_DESCRIPTOR_RANGE_TYPE range_type) noexcept
{
    META_FUNCTION_TASK();
    if (range_type == D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER)
        return DescriptorHeapDX::Type::Samplers;
    else
        return DescriptorHeapDX::Type::ShaderResources;
}

[[nodiscard]]
static D3D12_SHADER_VISIBILITY GetShaderVisibilityByType(Shader::Type shader_type)
{
    META_FUNCTION_TASK();
    switch (shader_type)
    {
    case Shader::Type::All:    return D3D12_SHADER_VISIBILITY_ALL;
    case Shader::Type::Vertex: return D3D12_SHADER_VISIBILITY_VERTEX;
    case Shader::Type::Pixel:  return D3D12_SHADER_VISIBILITY_PIXEL;
    default:                   META_UNEXPECTED_ARG_RETURN(shader_type, D3D12_SHADER_VISIBILITY_ALL);
    }
};

static void InitArgumentAsDescriptorTable(std::vector<CD3DX12_DESCRIPTOR_RANGE1>& descriptor_ranges, std::vector<CD3DX12_ROOT_PARAMETER1>& root_parameters,
                                          std::map<DescriptorHeapDX::Type, DescriptorsCountByAccess>& descriptor_offset_by_heap_type,
                                          ProgramBindingsDX::ArgumentBindingDX& argument_binding,
                                          const ProgramBindingsDX::ArgumentBindingDX::SettingsDX& bind_settings,
                                          const D3D12_SHADER_VISIBILITY& shader_visibility)
{
    const D3D12_DESCRIPTOR_RANGE_TYPE  range_type             = GetDescriptorRangeTypeByShaderInputType(bind_settings.input_type);
    const D3D12_DESCRIPTOR_RANGE_FLAGS descriptor_range_flags = bind_settings.argument.IsConstant()
                                                              ? D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC
                                                              : D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE;
    const D3D12_DESCRIPTOR_RANGE_FLAGS range_flags            = range_type == D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER
                                                              ? D3D12_DESCRIPTOR_RANGE_FLAG_NONE
                                                              : descriptor_range_flags;

    descriptor_ranges.emplace_back(range_type, bind_settings.resource_count, bind_settings.point, bind_settings.space, range_flags);
    root_parameters.back().InitAsDescriptorTable(1, &descriptor_ranges.back(), shader_visibility);

    const DescriptorHeapDX::Type heap_type = GetDescriptorHeapTypeByRangeType(range_type);
    DescriptorsCountByAccess& descriptor_offsets = descriptor_offset_by_heap_type[heap_type];
    uint32_t& descriptor_offset = descriptor_offsets[bind_settings.argument.GetAccessorType()];
    argument_binding.SetDescriptorRange({ heap_type, descriptor_offset, bind_settings.resource_count });

    descriptor_offset += bind_settings.resource_count;
}

Ptr<Program> Program::Create(const Context& context, const Settings& settings)
{
    META_FUNCTION_TASK();
    return std::make_shared<ProgramDX>(dynamic_cast<const ContextBase&>(context), settings);
}

ProgramDX::ProgramDX(const ContextBase& context, const Settings& settings)
    : ProgramBase(context, settings)
{
    META_FUNCTION_TASK();
    InitArgumentBindings(settings.argument_accessors);
    InitRootSignature();
}

ProgramDX::~ProgramDX()
{
    META_FUNCTION_TASK();
    std::scoped_lock lock_guard(m_constant_descriptor_ranges_reservation_mutex);
    for (const auto& [heap_and_access_type, heap_reservation] : m_constant_descriptor_range_by_heap_and_access_type)
    {
        if (heap_reservation.range.IsEmpty())
            continue;

        heap_reservation.heap.get().ReleaseRange(heap_reservation.range);
    }
}

bool ProgramDX::SetName(const std::string& name)
{
    META_FUNCTION_TASK();
    if (!ProgramBase::SetName(name))
        return false;

    META_CHECK_ARG_NOT_NULL(m_cp_root_signature);
    m_cp_root_signature->SetName(nowide::widen(name).c_str());
    return true;
}

void ProgramDX::InitRootSignature()
{
    META_FUNCTION_TASK();
    using ArgumentBindingDX = ProgramBindingsDX::ArgumentBindingDX;

    std::vector<CD3DX12_DESCRIPTOR_RANGE1> descriptor_ranges;
    std::vector<CD3DX12_ROOT_PARAMETER1>   root_parameters;

    const ProgramBindingsBase::ArgumentBindings& binding_by_argument = GetArgumentBindings();
    descriptor_ranges.reserve(binding_by_argument.size());
    root_parameters.reserve(binding_by_argument.size());

    std::map<DescriptorHeapDX::Type, DescriptorsCountByAccess> descriptor_offset_by_heap_type;
    for (const auto& [program_argument, argument_binding_ptr] : binding_by_argument)
    {
        META_CHECK_ARG_NOT_NULL(argument_binding_ptr);
        auto&                             argument_binding = static_cast<ArgumentBindingDX&>(*argument_binding_ptr);
        const ArgumentBindingDX::SettingsDX& bind_settings = argument_binding.GetSettingsDX();
        const D3D12_SHADER_VISIBILITY    shader_visibility = GetShaderVisibilityByType(program_argument.GetShaderType());

        argument_binding.SetRootParameterIndex(static_cast<uint32_t>(root_parameters.size()));
        root_parameters.emplace_back();

        switch (bind_settings.type)
        {
        case ArgumentBindingDX::Type::DescriptorTable:
            InitArgumentAsDescriptorTable(descriptor_ranges, root_parameters, descriptor_offset_by_heap_type, argument_binding, bind_settings, shader_visibility);
            break;

        case ArgumentBindingDX::Type::ConstantBufferView:
            root_parameters.back().InitAsConstantBufferView(bind_settings.point, bind_settings.space, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC, shader_visibility);
            break;

        case ArgumentBindingDX::Type::ShaderResourceView:
            root_parameters.back().InitAsShaderResourceView(bind_settings.point, bind_settings.space, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC, shader_visibility);
            break;

        default:
            META_UNEXPECTED_ARG(bind_settings.type);
        }
    }

    // Replicate descriptor ranges for all frame-constant argument binding instances
    for (const auto& [program_argument, frame_argument_bindings] : GetFrameArgumentBindings())
    {
        META_CHECK_ARG_NOT_EMPTY(frame_argument_bindings);
        const auto& initial_frame_binding = static_cast<ProgramBindingsDX::ArgumentBindingDX&>(*frame_argument_bindings.front());
        const ProgramBindingsDX::ArgumentBindingDX::DescriptorRange& descriptor_range = initial_frame_binding.GetDescriptorRange();

        for(size_t frame_index = 1; frame_index < frame_argument_bindings.size(); ++frame_index)
        {
            auto& argument_binding_dx = static_cast<ProgramBindingsDX::ArgumentBindingDX&>(*frame_argument_bindings[frame_index]);
            argument_binding_dx.SetRootParameterIndex(initial_frame_binding.GetRootParameterIndex());
            argument_binding_dx.SetDescriptorRange(descriptor_range);
        }
    }

    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC root_signature_desc;
    root_signature_desc.Init_1_1(static_cast<UINT>(root_parameters.size()), root_parameters.data(), 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    D3D12_FEATURE_DATA_ROOT_SIGNATURE feature_data{};
    feature_data.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;

    const wrl::ComPtr<ID3D12Device>& cp_native_device = GetContextDX().GetDeviceDX().GetNativeDevice();
    if (FAILED(cp_native_device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &feature_data, sizeof(feature_data))))
    {
        feature_data.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
    }

    wrl::ComPtr<ID3DBlob> root_signature_blob;
    wrl::ComPtr<ID3DBlob> error_blob;
    ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&root_signature_desc, feature_data.HighestVersion, &root_signature_blob, &error_blob), error_blob);
    ThrowIfFailed(cp_native_device->CreateRootSignature(0, root_signature_blob->GetBufferPointer(), root_signature_blob->GetBufferSize(), IID_PPV_ARGS(&m_cp_root_signature)), cp_native_device.Get());
}

DescriptorHeapDX::Range ProgramDX::ReserveDescriptorRange(DescriptorHeapDX& heap, ArgumentAccessor::Type access_type, uint32_t range_length)
{
    META_FUNCTION_TASK();
    if (access_type == ArgumentAccessor::Type::Mutable)
    {
        DescriptorHeapDX::Range descriptor_range = heap.ReserveRange(range_length);
        META_CHECK_ARG_NOT_ZERO_DESCR(descriptor_range, "descriptor heap does not have enough space to reserve descriptor range for a program");
        return descriptor_range;
    }

    const DescriptorHeapDX::Type heap_type = heap.GetSettings().type;
    const auto heap_and_access_type      = std::make_pair(heap_type, access_type);

    std::scoped_lock lock_guard(m_constant_descriptor_ranges_reservation_mutex);
    if (auto constant_descriptor_range_it = m_constant_descriptor_range_by_heap_and_access_type.find(heap_and_access_type);
        constant_descriptor_range_it != m_constant_descriptor_range_by_heap_and_access_type.end())
    {
        const DescriptorHeapReservation& heap_reservation = constant_descriptor_range_it->second;
        META_CHECK_ARG_NAME_DESCR("heap", std::addressof(heap) == std::addressof(heap_reservation.heap.get()),
                                  "constant descriptor range was previously reserved for the program on a different descriptor heap of the same type");
        META_CHECK_ARG_EQUAL_DESCR(range_length, heap_reservation.range.GetLength(),
                                   "constant descriptor range previously reserved for the program differs in length from requested reservation");
        return heap_reservation.range;
    }

    DescriptorHeapDX::Range descriptor_range = heap.ReserveRange(range_length);
    META_CHECK_ARG_NOT_ZERO_DESCR(descriptor_range, "descriptor heap does not have enough space to reserve descriptor range for a program");
    m_constant_descriptor_range_by_heap_and_access_type.try_emplace(heap_and_access_type, DescriptorHeapReservation{ heap, descriptor_range });
    return descriptor_range;
}

const IContextDX& ProgramDX::GetContextDX() const noexcept
{
    META_FUNCTION_TASK();
    return static_cast<const IContextDX&>(GetContext());
}

ShaderDX& ProgramDX::GetVertexShaderDX() const
{
    META_FUNCTION_TASK();
    return static_cast<ShaderDX&>(GetShaderRef(Shader::Type::Vertex));
}

ShaderDX& ProgramDX::GetPixelShaderDX() const
{
    META_FUNCTION_TASK();
    return static_cast<ShaderDX&>(GetShaderRef(Shader::Type::Pixel));
}

D3D12_INPUT_LAYOUT_DESC ProgramDX::GetNativeInputLayoutDesc() const noexcept
{
    META_FUNCTION_TASK();
    if (m_dx_vertex_input_layout.empty())
        m_dx_vertex_input_layout = GetVertexShaderDX().GetNativeProgramInputLayout(*this);

    return {
        m_dx_vertex_input_layout.data(),
        static_cast<UINT>(m_dx_vertex_input_layout.size())
    };
}

} // namespace Methane::Graphics
