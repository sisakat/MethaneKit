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

FILE: Methane/Graphics/CommandList.h
Methane command list interface: this is uncreatable common command list interface,
to create instance refer to RenderCommandList, etc. for specific derived interface.

******************************************************************************/

#pragma once

#include "Object.h"
#include "ProgramBindings.h"
#include "Resource.h"

#include <Methane/Graphics/Types.h>
#include <Methane/Data/TimeRange.hpp>
#include <Methane/Data/IEmitter.h>

#include <string>
#include <functional>

namespace Methane::Graphics
{

struct CommandQueue;

struct ICommandListCallback
{
    virtual void OnCommandListStateChanged(CommandList&)        { /* does nothing by default */ };
    virtual void OnCommandListExecutionCompleted(CommandList&)  { /* does nothing by default */ };
    
    virtual ~ICommandListCallback() = default;
};

struct CommandList
    : virtual Object // NOSONAR
    , virtual Data::IEmitter<ICommandListCallback> // NOSONAR
{
    enum class Type
    {
        Blit,
        Render,
        ParallelRender,
    };

    enum class State
    {
        Pending,
        Encoding,
        Committed,
        Executing,
    };

    struct DebugGroup : virtual Object // NOSONAR
    {
        [[nodiscard]] static Ptr<DebugGroup> Create(const std::string& name);

        virtual DebugGroup& AddSubGroup(Data::Index id, const std::string& name) = 0;
        [[nodiscard]] virtual DebugGroup* GetSubGroup(Data::Index id) const noexcept = 0;
        [[nodiscard]] virtual bool        HasSubGroups() const noexcept = 0;
    };

    using CompletedCallback = std::function<void(CommandList& command_list)>;

    // CommandList interface
    [[nodiscard]] virtual Type  GetType() const noexcept = 0;
    [[nodiscard]] virtual State GetState() const noexcept = 0;
    virtual void  PushDebugGroup(DebugGroup& debug_group) = 0;
    virtual void  PopDebugGroup() = 0;
    virtual void  Reset(DebugGroup* p_debug_group = nullptr) = 0;
    virtual void  ResetOnce(DebugGroup* p_debug_group = nullptr) = 0;
    virtual void  SetProgramBindings(ProgramBindings& program_bindings,
                                     ProgramBindings::ApplyBehavior apply_behavior = ProgramBindings::ApplyBehavior::AllIncremental) = 0;
    virtual void  SetResourceBarriers(const Resource::Barriers& resource_barriers) = 0;
    virtual void  Commit() = 0;
    virtual void  WaitUntilCompleted(uint32_t timeout_ms = 0U) = 0;
    [[nodiscard]] virtual Data::TimeRange GetGpuTimeRange(bool in_cpu_nanoseconds) const = 0;
    [[nodiscard]] virtual CommandQueue& GetCommandQueue() = 0;
};

struct CommandListSet
{
    [[nodiscard]] static Ptr<CommandListSet> Create(const Refs<CommandList>& command_list_refs, Opt<Data::Index> frame_index_opt = {});

    [[nodiscard]] virtual Data::Size               GetCount() const noexcept = 0;
    [[nodiscard]] virtual const Refs<CommandList>& GetRefs() const noexcept = 0;
    [[nodiscard]] virtual CommandList&             operator[](Data::Index index) const = 0;
    [[nodiscard]] virtual const Opt<Data::Index>&  GetFrameIndex() const noexcept = 0;

    virtual ~CommandListSet() = default;
};

} // namespace Methane::Graphics

#ifdef METHANE_COMMAND_DEBUG_GROUPS_ENABLED

#define META_DEBUG_GROUP_CREATE(/*const std::string& */group_name) \
    Methane::Graphics::CommandList::DebugGroup::Create(group_name)

#define META_DEBUG_GROUP_PUSH(/*CommandList& */cmd_list, /*const std::string& */group_name) \
    { \
        const auto s_local_debug_group = META_DEBUG_GROUP_CREATE(group_name); \
        (cmd_list).PushDebugGroup(*s_local_debug_group); \
    }

#define META_DEBUG_GROUP_POP(/*CommandList& */cmd_list) \
    (cmd_list).PopDebugGroup()

#else

#define META_DEBUG_GROUP_CREATE(/*const std::string& */group_name) \
    nullptr

#define META_DEBUG_GROUP_PUSH(/*CommandList& */cmd_list, /*const std::string& */group_name) \
    META_UNUSED(cmd_list); META_UNUSED(group_name)

#define META_DEBUG_GROUP_POP(/*CommandList& */cmd_list) \
    META_UNUSED(cmd_list)

#endif

#define META_DEBUG_GROUP_CREATE_VAR(variable, /*const std::string& */group_name) \
    META_UNUSED(group_name); \
    static const Methane::Ptr<Methane::Graphics::CommandList::DebugGroup> variable = META_DEBUG_GROUP_CREATE(group_name)
