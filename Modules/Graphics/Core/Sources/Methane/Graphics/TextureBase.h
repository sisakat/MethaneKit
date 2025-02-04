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

FILE: Methane/Graphics/TextureBase.h
Base implementation of the texture interface.

******************************************************************************/

#pragma once

#include <Methane/Graphics/Texture.h>

#include "ResourceBase.h"

namespace Methane::Graphics
{

class TextureBase
    : public Texture
    , public ResourceBase
{
public:
    TextureBase(const ContextBase& context, const Settings& settings,
                State initial_state = State::Undefined, Opt<State> auto_transition_source_state_opt = {});

    // Texture interface
    const Settings& GetSettings() const override { return m_settings; }
    Data::Size      GetDataSize(Data::MemoryState size_type = Data::MemoryState::Reserved) const noexcept override;

    static Data::Size GetRequiredMipLevelsCount(const Dimensions& dimensions);

protected:
    // ResourceBase overrides
    Data::Size CalculateSubResourceDataSize(const SubResource::Index& sub_resource_index) const override;

    static void ValidateDimensions(DimensionType dimension_type, const Dimensions& dimensions, bool mipmapped);

private:
    const Settings m_settings;
};

} // namespace Methane::Graphics
