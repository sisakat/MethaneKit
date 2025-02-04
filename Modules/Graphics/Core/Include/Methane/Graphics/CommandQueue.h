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

FILE: Methane/Graphics/CommandQueue.h
Methane command queue interface: queues are used to execute command lists.

******************************************************************************/

#pragma once

#include "Object.h"
#include "CommandList.h"
#include "Context.h"

#include <Methane/Memory.hpp>

namespace Methane::Graphics
{

struct CommandQueue : virtual Object // NOSONAR
{
    // Create CommandQueue instance
    [[nodiscard]] static Ptr<CommandQueue> Create(const Context& context, CommandList::Type command_lists_type);

    // CommandQueue interface
    [[nodiscard]] virtual const Context&    GetContext() const noexcept = 0;
    [[nodiscard]] virtual CommandList::Type GetCommandListType() const noexcept = 0;
    [[nodiscard]] virtual uint32_t          GetFamilyIndex() const noexcept = 0;
    virtual void Execute(CommandListSet& command_lists, const CommandList::CompletedCallback& completed_callback = {}) = 0;
};

} // namespace Methane::Graphics
