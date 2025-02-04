/******************************************************************************

Copyright 2019-2021 Evgeny Gorodetskiy

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

FILE: Methane/Graphics/Vulkan/BlitCommandListVK.h
Vulkan implementation of the blit command list interface.

******************************************************************************/

#pragma once

#include "CommandListVK.hpp"

#include <Methane/Graphics/BlitCommandList.h>
#include <Methane/Graphics/CommandListBase.h>

#include <vulkan/vulkan.hpp>

namespace Methane::Graphics
{

class CommandQueueVK;

class BlitCommandListVK final
    : public CommandListVK<CommandListBase, vk::PipelineBindPoint::eGraphics>
    , public BlitCommandList
{
public:
    explicit BlitCommandListVK(CommandQueueVK& command_queue);
};

} // namespace Methane::Graphics
