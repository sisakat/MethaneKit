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

FILE: Methane/Graphics/Vulkan/ProgramVK.h
Vulkan implementation of the program interface.

******************************************************************************/

#include "ProgramVK.h"
#include "ShaderVK.h"
#include "BufferVK.h"
#include "ContextVK.h"
#include "DeviceVK.h"
#include "RenderCommandListVK.h"

#include <Methane/Graphics/ContextBase.h>
#include <Methane/Instrumentation.h>

namespace Methane::Graphics
{

static vk::PipelineLayout CreateVulkanPipelineLayout(const vk::Device& vk_device)
{
    META_FUNCTION_TASK();
    return vk_device.createPipelineLayout(vk::PipelineLayoutCreateInfo());
}

Ptr<Program> Program::Create(const Context& context, const Settings& settings)
{
    META_FUNCTION_TASK();
    return std::make_shared<ProgramVK>(dynamic_cast<const ContextBase&>(context), settings);
}

ProgramVK::ProgramVK(const ContextBase& context, const Settings& settings)
    : ProgramBase(context, settings)
    , m_vk_pipeline_layout(CreateVulkanPipelineLayout(GetContextVK().GetDeviceVK().GetNativeDevice()))
{
    META_FUNCTION_TASK();
}

ProgramVK::~ProgramVK()
{
    META_FUNCTION_TASK();
    GetContextVK().GetDeviceVK().GetNativeDevice().destroy(m_vk_pipeline_layout);
}

const IContextVK& ProgramVK::GetContextVK() const noexcept
{
    META_FUNCTION_TASK();
    return static_cast<const IContextVK&>(GetContext());
}

ShaderVK& ProgramVK::GetShaderVK(Shader::Type shader_type) noexcept
{
    META_FUNCTION_TASK();
    return static_cast<ShaderVK&>(GetShaderRef(shader_type));
}

std::vector<vk::PipelineShaderStageCreateInfo> ProgramVK::GetNativeShaderStageCreateInfos() const
{
    META_FUNCTION_TASK();
    std::vector<vk::PipelineShaderStageCreateInfo> vk_stage_create_infos;
    for(Shader::Type shader_type : GetShaderTypes())
    {
        ShaderVK& shader = static_cast<ShaderVK&>(GetShaderRef(shader_type));
        vk_stage_create_infos.emplace_back(shader.GetNativeStageCreateInfo());
    }
    return vk_stage_create_infos;
}

vk::PipelineVertexInputStateCreateInfo ProgramVK::GetNativeVertexInputStateCreateInfo() const
{
    META_FUNCTION_TASK();
    ShaderVK& vertex_shader = static_cast<ShaderVK&>(GetShaderRef(Shader::Type::Vertex));
    return vertex_shader.GetNativeVertexInputStateCreateInfo(*this);
}

} // namespace Methane::Graphics
