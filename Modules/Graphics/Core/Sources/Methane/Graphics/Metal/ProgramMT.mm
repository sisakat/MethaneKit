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

FILE: Methane/Graphics/Metal/ProgramMT.hh
Metal implementation of the program interface.

******************************************************************************/

#include "ProgramMT.hh"
#include "ShaderMT.hh"
#include "ContextMT.h"
#include "DeviceMT.hh"
#include "TypesMT.hh"

#include <Methane/Graphics/ContextBase.h>
#include <Methane/Platform/Apple/Types.hh>
#include <Methane/Instrumentation.h>
#include <Methane/Checks.hpp>

namespace Methane::Graphics
{

Ptr<Program> Program::Create(const Context& context, const Settings& settings)
{
    META_FUNCTION_TASK();
    return std::make_shared<ProgramMT>(dynamic_cast<const ContextBase&>(context), settings);
}

ProgramMT::ProgramMT(const ContextBase& context, const Settings& settings)
    : ProgramBase(context, settings)
    , m_mtl_vertex_desc(GetShaderMT(Shader::Type::Vertex).GetNativeVertexDescriptor(*this))
{
    META_FUNCTION_TASK();

    // Create dummy pipeline state to get program reflection of vertex and fragment shader arguments
    MTLRenderPipelineDescriptor* mtl_reflection_state_desc = [MTLRenderPipelineDescriptor new];
    mtl_reflection_state_desc.vertexDescriptor = m_mtl_vertex_desc;
    mtl_reflection_state_desc.vertexFunction   = GetNativeShaderFunction(Shader::Type::Vertex);
    mtl_reflection_state_desc.fragmentFunction = GetNativeShaderFunction(Shader::Type::Pixel);

    // Fill state color attachment descriptors matching program's pixel shader output
    // NOTE: even when program has no pixel shaders render, render state must have at least one color format to be valid
    uint32_t attachment_index = 0;
    for(PixelFormat color_format : settings.attachment_formats.colors)
    {
        mtl_reflection_state_desc.colorAttachments[attachment_index++].pixelFormat = TypeConverterMT::DataFormatToMetalPixelType(color_format);
    }
    mtl_reflection_state_desc.colorAttachments[attachment_index].pixelFormat = MTLPixelFormatInvalid;
    mtl_reflection_state_desc.depthAttachmentPixelFormat   = TypeConverterMT::DataFormatToMetalPixelType(settings.attachment_formats.depth);
    mtl_reflection_state_desc.stencilAttachmentPixelFormat = TypeConverterMT::DataFormatToMetalPixelType(settings.attachment_formats.stencil);
    
    const IContextMT& metal_context = dynamic_cast<const IContextMT&>(context);
    
    NSError* ns_error = nil;
    const id<MTLDevice>& mtl_device = metal_context.GetDeviceMT().GetNativeDevice();

    MTLRenderPipelineReflection* mtl_render_pipeline_reflection = nil;
    m_mtl_dummy_pipeline_state_for_reflection = [mtl_device newRenderPipelineStateWithDescriptor:mtl_reflection_state_desc
                                                                                         options:MTLPipelineOptionArgumentInfo
                                                                                      reflection:&mtl_render_pipeline_reflection
                                                                                           error:&ns_error];

    META_CHECK_ARG_NOT_NULL_DESCR(m_mtl_dummy_pipeline_state_for_reflection,
                                  "Failed to create dummy pipeline state for program reflection: {}",
                                  MacOS::ConvertFromNsType<NSString, std::string>([ns_error localizedDescription]));

    if (mtl_render_pipeline_reflection)
    {
        SetNativeShaderArguments(Shader::Type::Vertex, mtl_render_pipeline_reflection.vertexArguments);
        SetNativeShaderArguments(Shader::Type::Pixel,  mtl_render_pipeline_reflection.fragmentArguments);
        InitArgumentBindings(settings.argument_accessors);
    }
}

const IContextMT& ProgramMT::GetContextMT() const noexcept
{
    META_FUNCTION_TASK();
    return static_cast<const IContextMT&>(GetContext());
}

ShaderMT& ProgramMT::GetShaderMT(Shader::Type shader_type) noexcept
{
    META_FUNCTION_TASK();
    return static_cast<ShaderMT&>(GetShaderRef(shader_type));
}

id<MTLFunction> ProgramMT::GetNativeShaderFunction(Shader::Type shader_type) noexcept
{
    META_FUNCTION_TASK();
    return HasShader(shader_type) ? static_cast<ShaderMT&>(GetShaderRef(shader_type)).GetNativeFunction() : nil;
}

void ProgramMT::SetNativeShaderArguments(Shader::Type shader_type, NSArray<MTLArgument*>* mtl_arguments) noexcept
{
    META_FUNCTION_TASK();
    if (HasShader(shader_type))
    {
        GetShaderMT(shader_type).SetNativeArguments(mtl_arguments);
    }
}

} // namespace Methane::Graphics
