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

FILE: Methane/Graphics/DirectX12/SamplerDX.h
DirectX 12 implementation of the sampler interface.

******************************************************************************/

#pragma once

#include "ResourceDX.hpp"

#include <Methane/Graphics/SamplerBase.h>

#include <wrl.h>
#include <directx/d3d12.h>

namespace Methane::Graphics
{

namespace wrl = Microsoft::WRL;

class ContextBase;

class SamplerDX final // NOSONAR - inheritance hierarchy is greater than 5
    : public ResourceDX<SamplerBase>
{
public:
    SamplerDX(const ContextBase& context, const Settings& settings);

    // IResourceDX override
    Opt<Descriptor> InitializeNativeViewDescriptor(const ViewDX::Id& view_id) override;
};

} // namespace Methane::Graphics
