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

FILE: Methane/Graphics/Metal/ProgramBindingsMT.mm
Metal implementation of the program bindings interface.

******************************************************************************/

#include "ProgramBindingsMT.hh"
#include "BufferMT.hh"
#include "TextureMT.hh"
#include "SamplerMT.hh"
#include "RenderCommandListMT.hh"

#include <Methane/Instrumentation.h>
#include <Methane/Checks.hpp>

#include <magic_enum.hpp>

namespace Methane::Graphics
{

using NativeBuffers       = ProgramBindingsMT::ArgumentBindingMT::NativeBuffers;
using NativeTextures      = ProgramBindingsMT::ArgumentBindingMT::NativeTextures;
using NativeSamplerStates = ProgramBindingsMT::ArgumentBindingMT::NativeSamplerStates;
using NativeOffsets       = ProgramBindingsMT::ArgumentBindingMT::NativeOffsets;

template<typename TMetalResource>
void SetMetalResource(Shader::Type shader_type, const id<MTLRenderCommandEncoder>& mtl_cmd_encoder, TMetalResource mtl_buffer, uint32_t arg_index, NSUInteger buffer_offset);

template<typename TMetalResource>
void SetMetalResources(Shader::Type shader_type, const id<MTLRenderCommandEncoder>& mtl_cmd_encoder, const std::vector<TMetalResource>& mtl_buffer, uint32_t arg_index, const std::vector<NSUInteger>& buffer_offsets);

template<>
void SetMetalResource(Shader::Type shader_type, const id<MTLRenderCommandEncoder>& mtl_cmd_encoder, __unsafe_unretained id<MTLBuffer> mtl_buffer, uint32_t arg_index, NSUInteger buffer_offset)
{
    META_FUNCTION_TASK();
    switch(shader_type)
    {
        case Shader::Type::Vertex: [mtl_cmd_encoder setVertexBuffer:mtl_buffer   offset:buffer_offset atIndex:arg_index]; break;
        case Shader::Type::Pixel:  [mtl_cmd_encoder setFragmentBuffer:mtl_buffer offset:buffer_offset atIndex:arg_index]; break;
        default:                   META_UNEXPECTED_ARG(shader_type);
    }
}

template<>
void SetMetalResources(Shader::Type shader_type, const id<MTLRenderCommandEncoder>& mtl_cmd_encoder, const NativeBuffers& mtl_buffers,
                      uint32_t arg_index, const std::vector<NSUInteger>& buffer_offsets)
{
    META_FUNCTION_TASK();
    const NSRange args_range = NSMakeRange(arg_index, mtl_buffers.size());
    switch(shader_type)
    {
    case Shader::Type::Vertex: [mtl_cmd_encoder setVertexBuffers:mtl_buffers.data()   offsets:buffer_offsets.data() withRange:args_range]; break;
    case Shader::Type::Pixel:  [mtl_cmd_encoder setFragmentBuffers:mtl_buffers.data() offsets:buffer_offsets.data() withRange:args_range]; break;
    default:                   META_UNEXPECTED_ARG(shader_type);
    }
}

template<>
void SetMetalResource(Shader::Type shader_type, const id<MTLRenderCommandEncoder>& mtl_cmd_encoder, __unsafe_unretained id<MTLTexture> mtl_texture, uint32_t arg_index, NSUInteger)
{
    META_FUNCTION_TASK();
    switch(shader_type)
    {
        case Shader::Type::Vertex: [mtl_cmd_encoder setVertexTexture:mtl_texture   atIndex:arg_index]; break;
        case Shader::Type::Pixel:  [mtl_cmd_encoder setFragmentTexture:mtl_texture atIndex:arg_index]; break;
        default:                   META_UNEXPECTED_ARG(shader_type);
    }
}

template<>
void SetMetalResources(Shader::Type shader_type, const id<MTLRenderCommandEncoder>& mtl_cmd_encoder, const NativeTextures& mtl_textures,
                       uint32_t arg_index, const std::vector<NSUInteger>&)
{
    META_FUNCTION_TASK();
    const NSRange args_range = NSMakeRange(arg_index, mtl_textures.size());
    switch(shader_type)
    {
    case Shader::Type::Vertex: [mtl_cmd_encoder setVertexTextures:mtl_textures.data()   withRange:args_range]; break;
    case Shader::Type::Pixel:  [mtl_cmd_encoder setFragmentTextures:mtl_textures.data() withRange:args_range]; break;
    default:                   META_UNEXPECTED_ARG(shader_type);
    }
}

template<>
void SetMetalResource(Shader::Type shader_type, const id<MTLRenderCommandEncoder>& mtl_cmd_encoder, __unsafe_unretained id<MTLSamplerState> mtl_sampler, uint32_t arg_index, NSUInteger)
{
    META_FUNCTION_TASK();
    switch(shader_type)
    {
        case Shader::Type::Vertex: [mtl_cmd_encoder setVertexSamplerState:mtl_sampler   atIndex:arg_index]; break;
        case Shader::Type::Pixel:  [mtl_cmd_encoder setFragmentSamplerState:mtl_sampler atIndex:arg_index]; break;
        default:                   META_UNEXPECTED_ARG(shader_type);
    }
}

template<>
void SetMetalResources(Shader::Type shader_type, const id<MTLRenderCommandEncoder>& mtl_cmd_encoder, const NativeSamplerStates& mtl_samplers,
                      uint32_t arg_index, const std::vector<NSUInteger>&)
{
    META_FUNCTION_TASK();
    const NSRange args_range = NSMakeRange(arg_index, mtl_samplers.size());
    switch(shader_type)
    {
    case Shader::Type::Vertex: [mtl_cmd_encoder setVertexSamplerStates:mtl_samplers.data()   withRange:args_range]; break;
    case Shader::Type::Pixel:  [mtl_cmd_encoder setFragmentSamplerStates:mtl_samplers.data() withRange:args_range]; break;
    default:                   META_UNEXPECTED_ARG(shader_type);
    }
}

template<typename TMetalResource>
void SetMetalResourcesForAll(Shader::Type shader_type, const Program& program, const id<MTLRenderCommandEncoder>& mtl_cmd_encoder,
                                   const std::vector<TMetalResource>& mtl_resources, uint32_t arg_index,
                                   const std::vector<NSUInteger>& offsets = std::vector<NSUInteger>())
{
    META_FUNCTION_TASK();
    META_CHECK_ARG_NOT_EMPTY(mtl_resources);

    if (shader_type == Shader::Type::All)
    {
        if (mtl_resources.size() > 1)
        {
            for (Shader::Type specific_shader_type : program.GetShaderTypes())
            {
                SetMetalResources(specific_shader_type, mtl_cmd_encoder, mtl_resources, arg_index, offsets);
            }
        }
        else
        {
            for (Shader::Type specific_shader_type : program.GetShaderTypes())
            {
                SetMetalResource(specific_shader_type, mtl_cmd_encoder, mtl_resources.back(), arg_index,
                                 offsets.empty() ? 0 : offsets.back());
            }
        }
    }
    else
    {
        if (mtl_resources.size() > 1)
        {
            SetMetalResources(shader_type, mtl_cmd_encoder, mtl_resources, arg_index, offsets);
        }
        else
        {
            SetMetalResource(shader_type, mtl_cmd_encoder, mtl_resources.back(), arg_index,
                             offsets.empty() ? 0 : offsets.back());
        }
    }
}

Ptr<ProgramBindings> ProgramBindings::Create(const Ptr<Program>& program_ptr, const ResourceViewsByArgument& resource_views_by_argument, Data::Index frame_index)
{
    META_FUNCTION_TASK();
    return std::make_shared<ProgramBindingsMT>(program_ptr, resource_views_by_argument, frame_index);
}

Ptr<ProgramBindings> ProgramBindings::CreateCopy(const ProgramBindings& other_program_bindings, const ResourceViewsByArgument& replace_resource_views_by_argument, const Opt<Data::Index>& frame_index)
{
    META_FUNCTION_TASK();
    return std::make_shared<ProgramBindingsMT>(static_cast<const ProgramBindingsMT&>(other_program_bindings), replace_resource_views_by_argument, frame_index);
}

Ptr<ProgramBindingsBase::ArgumentBindingBase> ProgramBindingsBase::ArgumentBindingBase::CreateCopy(const ArgumentBindingBase& other_argument_binding)
{
    META_FUNCTION_TASK();
    return std::make_shared<ProgramBindingsMT::ArgumentBindingMT>(static_cast<const ProgramBindingsMT::ArgumentBindingMT&>(other_argument_binding));
}

ProgramBindingsMT::ArgumentBindingMT::ArgumentBindingMT(const ContextBase& context, const SettingsMT& settings)
    : ArgumentBindingBase(context, settings)
    , m_settings_mt(settings)
{
    META_FUNCTION_TASK();
}

bool ProgramBindingsMT::ArgumentBindingMT::SetResourceViews(const Resource::Views& resource_views)
{
    META_FUNCTION_TASK();
    if (!ArgumentBindingBase::SetResourceViews(resource_views))
        return false;

    m_mtl_sampler_states.clear();
    m_mtl_textures.clear();
    m_mtl_buffers.clear();
    m_mtl_buffer_offsets.clear();

    switch(m_settings_mt.resource_type)
    {
    case Resource::Type::Sampler:
        m_mtl_sampler_states.reserve(resource_views.size());
        std::transform(resource_views.begin(), resource_views.end(), std::back_inserter(m_mtl_sampler_states),
                       [](const Resource::View& resource_view)
                       { return dynamic_cast<const SamplerMT&>(resource_view.GetResource()).GetNativeSamplerState(); });
        break;

    case Resource::Type::Texture:
        m_mtl_textures.reserve(resource_views.size());
        std::transform(resource_views.begin(), resource_views.end(), std::back_inserter(m_mtl_textures),
                       [](const Resource::View& resource_view)
                       { return dynamic_cast<const TextureMT&>(resource_view.GetResource()).GetNativeTexture(); });
        break;

    case Resource::Type::Buffer:
        m_mtl_buffers.reserve(resource_views.size());
        m_mtl_buffer_offsets.reserve(resource_views.size());
        for (const Resource::View& resource_view : resource_views)
        {
            m_mtl_buffers.push_back(dynamic_cast<const BufferMT&>(resource_view.GetResource()).GetNativeBuffer());
            m_mtl_buffer_offsets.push_back(static_cast<NSUInteger>(resource_view.GetOffset()));
        }
        break;

    default: META_UNEXPECTED_ARG(m_settings_mt.resource_type);
    }
    return true;
}

ProgramBindingsMT::ProgramBindingsMT(const Ptr<Program>& program_ptr, const ResourceViewsByArgument& resource_views_by_argument, Data::Index frame_index)
    : ProgramBindingsBase(program_ptr, resource_views_by_argument, frame_index)
{
    META_FUNCTION_TASK();
}

ProgramBindingsMT::ProgramBindingsMT(const ProgramBindingsMT& other_program_bindings, const ResourceViewsByArgument& replace_resource_views_by_argument, const Opt<Data::Index>& frame_index)
    : ProgramBindingsBase(other_program_bindings, replace_resource_views_by_argument, frame_index)
{
    META_FUNCTION_TASK();
}

void ProgramBindingsMT::Apply(CommandListBase& command_list, ApplyBehavior apply_behavior) const
{
    META_FUNCTION_TASK();
    using namespace magic_enum::bitwise_operators;

    RenderCommandListMT& metal_command_list = static_cast<RenderCommandListMT&>(command_list);
    const id<MTLRenderCommandEncoder>& mtl_cmd_encoder = metal_command_list.GetNativeCommandEncoder();
    
    for(const auto& binding_by_argument : GetArgumentBindings())
    {
        const Program::Argument& program_argument = binding_by_argument.first;
        const ArgumentBindingMT& metal_argument_binding = static_cast<const ArgumentBindingMT&>(*binding_by_argument.second);

        if ((static_cast<bool>(apply_behavior & ApplyBehavior::ConstantOnce) ||
             static_cast<bool>(apply_behavior & ApplyBehavior::ChangesOnly)) &&
            metal_command_list.GetProgramBindingsPtr() &&
            metal_argument_binding.IsAlreadyApplied(GetProgram(), *metal_command_list.GetProgramBindingsPtr(),
                                                    static_cast<bool>(apply_behavior & ApplyBehavior::ChangesOnly)))
            continue;

        const uint32_t arg_index = metal_argument_binding.GetSettingsMT().argument_index;

        switch(metal_argument_binding.GetSettingsMT().resource_type)
        {
            case Resource::Type::Buffer:
                SetMetalResourcesForAll(program_argument.GetShaderType(), GetProgram(), mtl_cmd_encoder, metal_argument_binding.GetNativeBuffers(), arg_index,
                                       metal_argument_binding.GetBufferOffsets());
                break;

            case Resource::Type::Texture:
                SetMetalResourcesForAll(program_argument.GetShaderType(), GetProgram(), mtl_cmd_encoder, metal_argument_binding.GetNativeTextures(), arg_index);
                break;

            case Resource::Type::Sampler:
                SetMetalResourcesForAll(program_argument.GetShaderType(), GetProgram(), mtl_cmd_encoder, metal_argument_binding.GetNativeSamplerStates(), arg_index);
                break;

            default: META_UNEXPECTED_ARG(metal_argument_binding.GetSettingsMT().resource_type);
        }
    }
}

} // namespace Methane::Graphics
