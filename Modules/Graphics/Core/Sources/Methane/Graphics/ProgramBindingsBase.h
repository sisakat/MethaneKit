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

FILE: Methane/Graphics/ProgramBindingsBase.h
Base implementation of the program bindings interface.

******************************************************************************/

#pragma once

#include <Methane/Graphics/ProgramBindings.h>
#include <Methane/Graphics/Resource.h>
#include <Methane/Data/Emitter.hpp>

#include "CommandListBase.h"
#include "ObjectBase.h"

#include <magic_enum.hpp>
#include <optional>

namespace Methane::Graphics
{

class ContextBase;
class CommandListBase;
class ResourceBase;

class ProgramBindingsBase
    : public ProgramBindings
    , public ObjectBase
    , public Data::Receiver<ProgramBindings::IArgumentBindingCallback>
{
public:
    class ArgumentBindingBase
        : public ArgumentBinding
        , public Data::Emitter<ProgramBindings::IArgumentBindingCallback>
        , public std::enable_shared_from_this<ArgumentBindingBase>
    {
    public:
        static Ptr<ArgumentBindingBase> CreateCopy(const ArgumentBindingBase& other_argument_binding);

        ArgumentBindingBase(const ContextBase& context, const Settings& settings);

        virtual void MergeSettings(const ArgumentBindingBase& other);

        // ArgumentBinding interface
        const Settings&        GetSettings() const noexcept override     { return m_settings; }
        const Resource::Views& GetResourceViews() const noexcept final   { return m_resource_views; }
        bool                   SetResourceViews(const Resource::Views& resource_views) override;
        explicit operator std::string() const final;

        Ptr<ArgumentBindingBase>   GetPtr() { return shared_from_this(); }

        bool IsAlreadyApplied(const Program& program,
                              const ProgramBindingsBase& applied_program_bindings,
                              bool check_binding_value_changes = true) const;

    protected:
        const ContextBase& GetContext() const noexcept { return m_context; }

    private:
        const ContextBase& m_context;
        const Settings     m_settings;
        Resource::Views    m_resource_views;
    };

    using ArgumentBindings = std::unordered_map<Program::Argument, Ptr<ArgumentBindingBase>, Program::Argument::Hash>;

    ProgramBindingsBase(const Ptr<Program>& program_ptr, const ResourceViewsByArgument& resource_views_by_argument, Data::Index frame_index);
    ProgramBindingsBase(const ProgramBindingsBase& other_program_bindings, const ResourceViewsByArgument& replace_resource_view_by_argument, const Opt<Data::Index>& frame_index);
    ProgramBindingsBase(const Ptr<Program>& program_ptr, Data::Index frame_index);
    ProgramBindingsBase(const ProgramBindingsBase& other_program_bindings, const Opt<Data::Index>& frame_index);
    ProgramBindingsBase(ProgramBindingsBase&&) noexcept = default;

    ProgramBindingsBase& operator=(const ProgramBindingsBase& other) = delete;
    ProgramBindingsBase& operator=(ProgramBindingsBase&& other) = delete;

    // ProgramBindings interface
    Program&                  GetProgram() const final;
    const Program::Arguments& GetArguments() const noexcept final     { return m_arguments; }
    Data::Index               GetFrameIndex() const noexcept final    { return m_frame_index; }
    Data::Index               GetBindingsIndex() const noexcept final { return m_bindings_index; }
    ArgumentBinding&          Get(const Program::Argument& shader_argument) const final;
    explicit operator std::string() const final;

    // ProgramBindingsBase interface
    virtual void CompleteInitialization() = 0;
    virtual void Apply(CommandListBase& command_list, ApplyBehavior apply_behavior = ApplyBehavior::AllIncremental) const = 0;

    Program::Arguments GetUnboundArguments() const;

    template<typename CommandListType>
    void ApplyResourceTransitionBarriers(CommandListType& command_list,
                                         Program::ArgumentAccessor::Type apply_access_mask = static_cast<Program::ArgumentAccessor::Type>(~0U),
                                         const CommandQueue* owner_queue_ptr = nullptr) const
    {
        if (ApplyResourceStates(apply_access_mask, owner_queue_ptr) &&
            m_resource_state_transition_barriers_ptr && !m_resource_state_transition_barriers_ptr->IsEmpty())
        {
            command_list.SetResourceBarriers(*m_resource_state_transition_barriers_ptr);
        }
    }

protected:
    // ProgramBindings::IArgumentBindingCallback
    void OnProgramArgumentBindingResourceViewsChanged(const ArgumentBinding&, const Resource::Views&, const Resource::Views&) override;

    void SetResourcesForArguments(const ResourceViewsByArgument& resource_views_by_argument);

    Program& GetProgram();
    void InitializeArgumentBindings(const ProgramBindingsBase* other_program_bindings_ptr = nullptr);
    ResourceViewsByArgument ReplaceResourceViews(const ArgumentBindings& argument_bindings,
                                                 const ResourceViewsByArgument& replace_resource_views) const;
    void VerifyAllArgumentsAreBoundToResources() const;
    const ArgumentBindings& GetArgumentBindings() const { return m_binding_by_argument; }
    const Refs<Resource>& GetResourceRefsByAccess(Program::ArgumentAccessor::Type access_type) const;

    void ClearTransitionResourceStates();
    void RemoveTransitionResourceStates(const ProgramBindings::ArgumentBinding& argument_binding, const Resource& resource);
    void AddTransitionResourceState(const ProgramBindings::ArgumentBinding& argument_binding, Resource& resource);
    void AddTransitionResourceStates(const ProgramBindings::ArgumentBinding& argument_binding);

private:
    struct ResourceAndState
    {
        Ptr<ResourceBase> resource_ptr;
        Resource::State   state;

        ResourceAndState(Ptr<ResourceBase> resource_ptr, Resource::State);
    };

    using ResourceStates = std::vector<ResourceAndState>;
    using ResourceStatesByAccess = std::array<ResourceStates, magic_enum::enum_count<Program::ArgumentAccessor::Type>()>;
    using ResourceRefsByAccess = std::array<Refs<Resource>, magic_enum::enum_count<Program::ArgumentAccessor::Type>()>;

    bool ApplyResourceStates(Program::ArgumentAccessor::Type access_types_mask, const CommandQueue* owner_queue_ptr = nullptr) const;
    void InitResourceRefsByAccess();

    const Ptr<Program>              m_program_ptr;
    Data::Index                     m_frame_index;
    Program::Arguments              m_arguments;
    ArgumentBindings                m_binding_by_argument;
    ResourceStatesByAccess          m_transition_resource_states_by_access;
    ResourceRefsByAccess            m_resource_refs_by_access;
    mutable Ptr<Resource::Barriers> m_resource_state_transition_barriers_ptr;
    Data::Index                     m_bindings_index = 0u; // index of this program bindings object between all program bindings of the program
};

} // namespace Methane::Graphics
