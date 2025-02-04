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

FILE: Methane/Graphics/DirectX12/ShaderDX.h
DirectX 12 implementation of the shader interface.

******************************************************************************/

#pragma once

#include <Methane/Graphics/ShaderBase.h>

#include <wrl.h>
#include <directx/d3d12.h>
#include <directx/d3dx12.h>
#include <d3d12shader.h>

namespace Methane::Data
{
class Chunk;
}

namespace Methane::Graphics
{

namespace wrl = Microsoft::WRL;

struct IContextDX;
class ProgramDX;

class ShaderDX final : public ShaderBase
{
public:
    ShaderDX(Type type, const ContextBase& context, const Settings& settings);

    // ShaderBase overrides
    ArgumentBindings GetArgumentBindings(const Program::ArgumentAccessors& argument_accessors) const override;

    const Data::Chunk*                    GetNativeByteCode() const noexcept { return m_byte_code_chunk_ptr.get(); }
    std::vector<D3D12_INPUT_ELEMENT_DESC> GetNativeProgramInputLayout(const ProgramDX& program) const;

private:
    const IContextDX& GetContextDX() const noexcept;

    UniquePtr<Data::Chunk>              m_byte_code_chunk_ptr;
    wrl::ComPtr<ID3DBlob>               m_cp_byte_code;
    wrl::ComPtr<ID3D12ShaderReflection> m_cp_reflection;
};

} // namespace Methane::Graphics
