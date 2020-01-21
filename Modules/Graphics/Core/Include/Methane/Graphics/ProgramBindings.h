/******************************************************************************

Copyright 2019-2020 Evgeny Gorodetskiy

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

*******************************************************************************

FILE: Methane/Graphics/ProgramBindings.h
Methane program bindings interface for resources binding to program arguments.

******************************************************************************/

#pragma once

#include "Program.h"

#include <Methane/Memory.hpp>

#include <string>
#include <unordered_map>

namespace Methane::Graphics
{

struct ProgramBindings
{
    struct ArgumentBinding
    {
        static Ptr<ArgumentBinding> CreateCopy(const ArgumentBinding& other_resource_binging);

        // ResourceBinding interface
        virtual Shader::Type               GetShaderType() const = 0;
        virtual const std::string&         GetArgumentName() const = 0;
        virtual bool                       IsConstant() const = 0;
        virtual bool                       IsAddressable() const = 0;
        virtual uint32_t                   GetResourceCount() const = 0;
        virtual const Resource::Locations& GetResourceLocations() const = 0;
        virtual void                       SetResourceLocations(const Resource::Locations& resource_locations) = 0;

        virtual ~ArgumentBinding() = default;
    };

    struct ApplyBehavior
    {
        using Mask = uint32_t;
        enum Value : Mask
        {
            Indifferent    = 0u,        // All bindings will be applied indifferently of the previous binding values
            ConstantOnce   = 1u << 0,   // Constant program arguments will be applied only once for each command list
            ChangesOnly    = 1u << 1,   // Only changed program argument values will be applied in command sequence
            StateBarriers  = 1u << 2,   // Resource state barriers will be automatically evaluated and set for command list
            AllIncremental = ~0u        // All binding values will be applied incrementally along with resource state barriers
        };

        ApplyBehavior() = delete;
    };

    using ResourceLocationsByArgument = std::unordered_map<Program::Argument, Resource::Locations, Program::Argument::Hash>;

    // Create ResourceBindings instance
    static Ptr<ProgramBindings> Create(Program& program, const ResourceLocationsByArgument& resource_locations_by_argument);
    static Ptr<ProgramBindings> CreateCopy(const ProgramBindings& other_resource_bingings, const ResourceLocationsByArgument& replace_resource_locations_by_argument = {});

    // ResourceBindings interface
    virtual const Ptr<ArgumentBinding>& Get(const Program::Argument& shader_argument) const = 0;
    virtual void Apply(CommandList& command_list, ApplyBehavior::Mask apply_behavior = ApplyBehavior::AllIncremental) const = 0;

    virtual ~ProgramBindings() = default;
};

} // namespace Methane::Graphics
