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

FILE: Methane/Graphics/CommandListBase.h
Base implementation of the command list interface.

******************************************************************************/

#pragma once

#include "ObjectBase.h"
#include "ResourceBase.h"

#include <Methane/Graphics/Program.h>
#include <Methane/Graphics/CommandList.h>
#include <Methane/Graphics/CommandQueue.h>
#include <Methane/Memory.hpp>
#include <Methane/TracyGpu.hpp>
#include <Methane/Instrumentation.h>

#include <stack>
#include <set>
#include <mutex>
#include <condition_variable>

namespace Methane::Graphics
{

class CommandQueueBase;
class ProgramBindingsBase;

class CommandListBase
    : public ObjectBase
    , public virtual CommandList
{
    friend class CommandQueueBase;

public:
    struct CommandState final
    {
        Ptr<ProgramBindingsBase> sp_program_bindings;
        Ptrs<ObjectBase>         retained_resources;
    };

    class DebugGroupBase
        : public DebugGroup
        , public ObjectBase
    {
    public:
        DebugGroupBase(std::string name);

        // Object overrides
        void SetName(const std::string&) override { throw std::logic_error("Debug Group can not be renamed"); }

        // DebugGroup interface
        DebugGroup& AddSubGroup(Data::Index id, std::string name) final;
        DebugGroup* GetSubGroup(Data::Index id) const noexcept final;
        bool        HasSubGroups() const noexcept final { return !m_sub_groups.empty(); }

    private:
        Ptrs<DebugGroup> m_sub_groups;
    };

    CommandListBase(CommandQueueBase& command_queue, Type type);
    ~CommandListBase() override;

    // CommandList interface
    Type  GetType() const noexcept override                         { return m_type; }
    State GetState() const noexcept override                        { return m_state; }
    void  PushDebugGroup(DebugGroup& debug_group) override;
    void  PopDebugGroup() override;
    void  Reset(DebugGroup* p_debug_group = nullptr) override;
    void  SetProgramBindings(ProgramBindings& program_bindings, ProgramBindings::ApplyBehavior::Mask apply_behavior) override;
    void  Commit() override;
    void  WaitUntilCompleted(uint32_t timeout_ms = 0u) override;
    Data::TimeRange GetGpuTimeRange(bool) const override { return { 0u, 0u }; }
    CommandQueue& GetCommandQueue() override;

    // CommandListBase interface
    virtual void SetResourceBarriers(const ResourceBase::Barriers& resource_barriers) = 0;
    virtual void Execute(uint32_t frame_index, const CompletedCallback& completed_callback = {});
    virtual void Complete(uint32_t frame_index); // Called from another thread, which is tracking GPU execution

    DebugGroupBase* GetTopOpenDebugGroup() const;
    void PushOpenDebugGroup(DebugGroup& debug_group);
    void ClearOpenDebugGroups();

    CommandQueueBase&               GetCommandQueueBase() noexcept;
    const CommandQueueBase&         GetCommandQueueBase() const noexcept;
    const Ptr<ProgramBindingsBase>& GetProgramBindings() const noexcept  { return GetCommandState().sp_program_bindings; }
    Ptr<CommandListBase>            GetCommandListPtr()                  { return std::static_pointer_cast<CommandListBase>(GetBasePtr()); }

    inline void RetainResource(Ptr<ObjectBase>&& sp_resource) { if (sp_resource) m_command_state.retained_resources.emplace_back(std::move(sp_resource)); }
    inline void RetainResource(ObjectBase& resource)          { m_command_state.retained_resources.emplace_back(resource.GetBasePtr()); }

    template<typename T, typename = std::enable_if_t<std::is_base_of_v<ObjectBase, T>>>
    inline void RetainResources(const Ptrs<T>& resource_ptrs)
    {
        for(const Ptr<T>& sp_resource : resource_ptrs)
            RetainResource(std::static_pointer_cast<ObjectBase>(sp_resource));
    }

protected:
    virtual void ResetCommandState();

    CommandState&       GetCommandState()           { return m_command_state; }
    const CommandState& GetCommandState() const     { return m_command_state; }

    void        SetCommandListState(State state);
    void        SetCommandListStateNoLock(State state);
    bool        IsExecutingOnAnyFrame() const               { return m_state == State::Executing; }
    bool        IsCommitted(uint32_t frame_index) const     { return m_state == State::Committed && m_committed_frame_index == frame_index; }
    bool        IsCommitted() const                         { return IsCommitted(GetCurrentFrameIndex()); }
    bool        IsExecuting(uint32_t frame_index) const     { return m_state == State::Executing && m_committed_frame_index == frame_index; }
    bool        IsExecuting() const                         { return IsExecuting(GetCurrentFrameIndex()); }
    uint32_t    GetCurrentFrameIndex() const;
    std::string GetTypeName() const noexcept                { return GetTypeName(m_type); }

    inline void VerifyEncodingState() const
    {
        if (m_state != State::Encoding)
            throw std::logic_error(GetTypeName() + " Command list encoding is not possible in \"" + GetStateName(m_state) + "\" state.");
    }

    static std::string GetTypeName(Type type) noexcept;
    static std::string GetStateName(State state) noexcept;

private:
    using DebugGroupStack  = std::stack<Ptr<DebugGroupBase>>;

    const Type                  m_type;
    Ptr<CommandQueue>           m_sp_command_queue;
    CommandState                m_command_state;
    DebugGroupStack             m_open_debug_groups;
    uint32_t                    m_committed_frame_index = 0;
    CompletedCallback           m_completed_callback;
    State                       m_state = State::Pending;
    mutable TracyLockable(std::mutex, m_state_mutex);
    TracyLockable(std::mutex,   m_state_change_mutex);
    std::condition_variable_any m_state_change_condition_var;

    TRACY_GPU_SCOPE_TYPE                  m_tracy_gpu_scope;
    UniquePtr<TRACE_SOURCE_LOCATION_TYPE> m_sp_tracy_construct_location;
    UniquePtr<TRACE_SOURCE_LOCATION_TYPE> m_sp_tracy_reset_location;
};

class CommandListSetBase
    : public CommandListSet
    , public std::enable_shared_from_this<CommandListSetBase>
{
public:
    CommandListSetBase(Refs<CommandList> command_list_refs);

    // CommandListSet overrides
    Data::Size               GetCount() const noexcept override { return static_cast<Data::Size>(m_refs.size()); }
    const Refs<CommandList>& GetRefs() const noexcept override  { return m_refs; }
    CommandList&             operator[](Data::Index index) const override;

    // CommandListSetBase interface
    virtual void Execute(Data::Index frame_index, const CommandList::CompletedCallback& completed_callback);
    
    void Complete() noexcept;

    Ptr<CommandListSetBase>      GetPtr()                                   { return shared_from_this(); }
    const Refs<CommandListBase>& GetBaseRefs() const noexcept               { return m_base_refs; }
    Data::Index                  GetExecutingOnFrameIndex() const noexcept  { return m_executing_on_frame_index; }
    const CommandListBase&       GetCommandListBase(Data::Index index) const;
    CommandQueueBase&            GetCommandQueueBase() noexcept             { return m_base_refs.back().get().GetCommandQueueBase(); }
    const CommandQueueBase&      GetCommandQueueBase() const noexcept       { return m_base_refs.back().get().GetCommandQueueBase(); }

private:
    Refs<CommandList>      m_refs;
    Refs<CommandListBase>  m_base_refs;
    Ptrs<CommandListBase>  m_base_ptrs;
    Data::Index            m_executing_on_frame_index = 0u;
};

} // namespace Methane::Graphics
