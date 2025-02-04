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

FILE: Methane/Graphics/DirectX12/ProgramBindingsDX.h
DirectX 12 implementation of the program bindings interface.

******************************************************************************/

#pragma once

#include "ResourceDX.h"

#include <Methane/Graphics/ProgramBindingsBase.h>

#include <wrl.h>
#include <directx/d3d12.h>

#include <magic_enum.hpp>
#include <functional>
#include <array>

namespace Methane::Graphics
{

class RenderCommandListDX;
struct ICommandListDX;

namespace wrl = Microsoft::WRL;

class ProgramBindingsDX final // NOSONAR - custom destructor is required
    : public ProgramBindingsBase
{
public:
    class ArgumentBindingDX   // NOSONAR - custom destructor is required
        : public ArgumentBindingBase
    {
    public:
        enum class Type : uint32_t
        {
            DescriptorTable = 0,
            ConstantBufferView,
            ShaderResourceView,
        };

        struct SettingsDX : Settings
        {
            Type                  type;
            D3D_SHADER_INPUT_TYPE input_type;
            uint32_t              point;
            uint32_t              space;
        };

        struct DescriptorRange
        {
            DescriptorHeapDX::Type heap_type = DescriptorHeapDX::Type::Undefined;
            uint32_t               offset    = 0;
            uint32_t               count     = 0;
        };

        ArgumentBindingDX(const ContextBase& context, const SettingsDX& settings);
        ArgumentBindingDX(const ArgumentBindingDX& other);
        ArgumentBindingDX(ArgumentBindingDX&&) noexcept = default;
        ~ArgumentBindingDX() override = default;

        ArgumentBindingDX& operator=(const ArgumentBindingDX&) = delete;
        ArgumentBindingDX& operator=(ArgumentBindingDX&&) noexcept = default;

        // ArgumentBinding interface
        bool SetResourceViews(const Resource::Views& resource_views) override;

        const SettingsDX&      GetSettingsDX() const noexcept          { return m_settings_dx; }
        uint32_t               GetRootParameterIndex() const noexcept  { return m_root_parameter_index; }
        const DescriptorRange& GetDescriptorRange() const noexcept     { return m_descriptor_range; }
        const ResourceViewsDX& GetResourceViewsDX() const noexcept     { return m_resource_views_dx; }
        DescriptorHeapDX::Type GetDescriptorHeapType() const;

        void SetRootParameterIndex(uint32_t root_parameter_index)          { m_root_parameter_index = root_parameter_index; }
        void SetDescriptorRange(const DescriptorRange& descriptor_range);
        void SetDescriptorHeapReservation(const DescriptorHeapDX::Reservation* p_reservation);

    private:
        const SettingsDX                     m_settings_dx;
        uint32_t                             m_root_parameter_index = std::numeric_limits<uint32_t>::max();;
        DescriptorRange                      m_descriptor_range;
        const DescriptorHeapDX::Reservation* m_p_descriptor_heap_reservation = nullptr;
        ResourceViewsDX                      m_resource_views_dx;
    };
    
    ProgramBindingsDX(const Ptr<Program>& program_ptr, const ResourceViewsByArgument& resource_views_by_argument, Data::Index frame_index);
    ProgramBindingsDX(const ProgramBindingsDX& other_program_bindings, const ResourceViewsByArgument& replace_resource_views_by_argument, const Opt<Data::Index>& frame_index);
    ~ProgramBindingsDX() override;

    void Initialize();

    // ProgramBindings interface
    void CompleteInitialization() override;
    void Apply(CommandListBase& command_list, ApplyBehavior apply_behavior) const override;

    void Apply(ICommandListDX& command_list_dx, const ProgramBindingsBase* applied_program_bindings_ptr, ApplyBehavior apply_behavior) const;

private:
    struct RootParameterBinding
    {
        ArgumentBindingDX&          argument_binding;
        uint32_t                    root_parameter_index = 0U;
        D3D12_GPU_DESCRIPTOR_HANDLE base_descriptor      {  };
        D3D12_GPU_VIRTUAL_ADDRESS   gpu_virtual_address  = 0U;
    };

    template<typename FuncType> // function void(ArgumentBindingDX&, const DescriptorHeapDX::Reservation*)
    void ForEachArgumentBinding(FuncType argument_binding_function) const;
    void ReserveDescriptorHeapRanges();
    void AddRootParameterBinding(const Program::ArgumentAccessor& argument_desc, const RootParameterBinding& root_parameter_binding);
    void UpdateRootParameterBindings();
    void AddRootParameterBindingsForArgument(ArgumentBindingDX& argument_binding, const DescriptorHeapDX::Reservation* p_heap_reservation);
    void ApplyRootParameterBindings(Program::ArgumentAccessor::Type access_types_mask, ID3D12GraphicsCommandList& d3d12_command_list,
                                    const ProgramBindingsBase* applied_program_bindings_ptr, bool apply_changes_only) const;
    void ApplyRootParameterBinding(const RootParameterBinding& root_parameter_binding, ID3D12GraphicsCommandList& d3d12_command_list) const;
    void CopyDescriptorsToGpu();
    void CopyDescriptorsToGpuForArgument(const wrl::ComPtr<ID3D12Device>& d3d12_device, ArgumentBindingDX& argument_binding,
                                         const DescriptorHeapDX::Reservation* p_heap_reservation) const;

    using RootParameterBindings = std::vector<RootParameterBinding>;
    using RootParameterBindingsByAccess = std::array<RootParameterBindings, magic_enum::enum_count<Program::ArgumentAccessor::Type>()>;
    RootParameterBindingsByAccess m_root_parameter_bindings_by_access;

    using DescriptorHeapReservationByType = std::array<std::optional<DescriptorHeapDX::Reservation>, magic_enum::enum_count<DescriptorHeapDX::Type>() - 1>;
    DescriptorHeapReservationByType m_descriptor_heap_reservations_by_type;
};

class DescriptorsCountByAccess
{
public:
    DescriptorsCountByAccess();

    uint32_t& operator[](Program::ArgumentAccessor::Type access_type);
    uint32_t  operator[](Program::ArgumentAccessor::Type access_type) const;

private:
    std::array<uint32_t, magic_enum::enum_count<Program::ArgumentAccessor::Type>()> m_count_by_access_type;
};

} // namespace Methane::Graphics
