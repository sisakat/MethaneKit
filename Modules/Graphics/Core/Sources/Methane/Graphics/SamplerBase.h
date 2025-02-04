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

FILE: Methane/Graphics/SamplerBase.h
Base implementation of the sampler interface.

******************************************************************************/

#pragma once

#include "ResourceBase.h"

#include <Methane/Graphics/Sampler.h>

namespace Methane::Graphics
{

class ContextBase;

class SamplerBase
    : public Sampler
    , public ResourceBase
{
public:
    SamplerBase(const ContextBase& context, const Settings& settings,
                State initial_state = State::Undefined, Opt<State> auto_transition_source_state_opt = {});

    // Sampler interface
    const Settings& GetSettings() const override { return m_settings; }

    // Resource interface
    void        SetData(const SubResources& sub_resources, CommandQueue&) override;
    Data::Size  GetDataSize(Data::MemoryState) const noexcept override { return 0; }

private:
    Settings     m_settings;
};

} // namespace Methane::Graphics
