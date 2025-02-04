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

FILE: Methane/Graphics/DirectX12/ProgramDX.h
DirectX 12 implementation of the program interface.

******************************************************************************/

#pragma once

#include <Methane/Graphics/ProgramBase.h>

#include "ShaderDX.h"
#include "ProgramBindingsDX.h"

#include <wrl.h>
#include <directx/d3d12.h>

#include <functional>

namespace Methane::Graphics
{

struct IContextDX;
struct IResourceDX;

namespace wrl = Microsoft::WRL;

class ProgramDX final : public ProgramBase // NOSONAR - this class requires destructor
{
    friend class ProgramBindingsDX;

public:
    ProgramDX(const ContextBase& context, const Settings& settings);
    ~ProgramDX() override;

    // Object interface
    bool SetName(const std::string& name) override;

    ShaderDX& GetVertexShaderDX() const;
    ShaderDX& GetPixelShaderDX() const;

    const wrl::ComPtr<ID3D12RootSignature>& GetNativeRootSignature() const noexcept { return m_cp_root_signature; }
    D3D12_INPUT_LAYOUT_DESC                 GetNativeInputLayoutDesc() const noexcept;

    const IContextDX& GetContextDX() const noexcept;

private:
    void InitRootSignature();
    DescriptorHeapDX::Range ReserveDescriptorRange(DescriptorHeapDX& heap, ArgumentAccessor::Type access_type, uint32_t range_length);

    struct DescriptorHeapReservation
    {
        Ref<DescriptorHeapDX>   heap;
        DescriptorHeapDX::Range range;
    };

    using DescriptorRangeByHeapAndAccessType = std::map<std::pair<DescriptorHeapDX::Type, ArgumentAccessor::Type>, DescriptorHeapReservation>;

    wrl::ComPtr<ID3D12RootSignature>              m_cp_root_signature;
    mutable std::vector<D3D12_INPUT_ELEMENT_DESC> m_dx_vertex_input_layout;

    DescriptorRangeByHeapAndAccessType m_constant_descriptor_range_by_heap_and_access_type;
    TracyLockable(std::mutex,          m_constant_descriptor_ranges_reservation_mutex)
};

} // namespace Methane::Graphics
