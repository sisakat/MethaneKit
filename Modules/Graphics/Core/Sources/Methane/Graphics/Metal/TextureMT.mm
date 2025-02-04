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

FILE: Methane/Graphics/Metal/TextureMT.mm
Metal implementation of the texture interface.

******************************************************************************/

#include "TextureMT.hh"
#include "RenderContextMT.hh"
#include "BlitCommandListMT.hh"
#include "TypesMT.hh"

#include <Methane/Graphics/CommandKit.h>
#include <Methane/Platform/Apple/Types.hh>
#include <Methane/Instrumentation.h>
#include <Methane/Checks.hpp>

#include <magic_enum.hpp>
#include <algorithm>

namespace Methane::Graphics
{

static MTLTextureType GetNativeTextureType(Texture::DimensionType dimension_type)
{
    META_FUNCTION_TASK();
    switch(dimension_type)
    {
    case Texture::DimensionType::Tex1D:             return MTLTextureType1D;
    case Texture::DimensionType::Tex1DArray:        return MTLTextureType1DArray;
    case Texture::DimensionType::Tex2D:             return MTLTextureType2D;
    case Texture::DimensionType::Tex2DArray:        return MTLTextureType2DArray;
    case Texture::DimensionType::Tex2DMultisample:  return MTLTextureType2DMultisample;
    // TODO: add support for MTLTextureType2DMultisampleArray
    case Texture::DimensionType::Cube:              return MTLTextureTypeCube;
    case Texture::DimensionType::CubeArray:         return MTLTextureTypeCubeArray;
    case Texture::DimensionType::Tex3D:             return MTLTextureType3D;
    // TODO: add support for MTLTextureTypeTextureBuffer
    default:                                        META_UNEXPECTED_ARG_RETURN(dimension_type, MTLTextureType1D);
    }
}

static MTLRegion GetTextureRegion(const Dimensions& dimensions, Texture::DimensionType dimension_type)
{
    META_FUNCTION_TASK();
    switch(dimension_type)
    {
    case Texture::DimensionType::Tex1D:
    case Texture::DimensionType::Tex1DArray:
             return MTLRegionMake1D(0, dimensions.GetWidth());
    case Texture::DimensionType::Tex2D:
    case Texture::DimensionType::Tex2DArray:
    case Texture::DimensionType::Tex2DMultisample:
    case Texture::DimensionType::Cube:
    case Texture::DimensionType::CubeArray:
             return MTLRegionMake2D(0, 0, dimensions.GetWidth(), dimensions.GetHeight());
    case Texture::DimensionType::Tex3D:
             return MTLRegionMake3D(0, 0, 0, dimensions.GetWidth(), dimensions.GetHeight(), dimensions.GetDepth());
    default: META_UNEXPECTED_ARG_RETURN(dimension_type, MTLRegion{});
    }
}

Ptr<Texture> Texture::CreateRenderTarget(const RenderContext& context, const Settings& settings)
{
    META_FUNCTION_TASK();
    return std::make_shared<TextureMT>(dynamic_cast<const ContextBase&>(context), settings);
}

Ptr<Texture> Texture::CreateFrameBuffer(const RenderContext& context, FrameBufferIndex /*frame_buffer_index*/)
{
    META_FUNCTION_TASK();
    const RenderContext::Settings& context_settings = context.GetSettings();
    const Settings texture_settings = Settings::FrameBuffer(Dimensions(context_settings.frame_size), context_settings.color_format);
    return std::make_shared<TextureMT>(dynamic_cast<const RenderContextBase&>(context), texture_settings);
}

Ptr<Texture> Texture::CreateDepthStencilBuffer(const RenderContext& context)
{
    META_FUNCTION_TASK();
    const RenderContext::Settings& context_settings = context.GetSettings();
    const Settings texture_settings = Settings::DepthStencilBuffer(Dimensions(context_settings.frame_size), context_settings.depth_stencil_format);
    return std::make_shared<TextureMT>(dynamic_cast<const RenderContextBase&>(context), texture_settings);
}

Ptr<Texture> Texture::CreateImage(const Context& context, const Dimensions& dimensions, const Opt<uint32_t>& array_length_opt, PixelFormat pixel_format, bool mipmapped)
{
    META_FUNCTION_TASK();
    const Settings texture_settings = Settings::Image(dimensions, array_length_opt, pixel_format, mipmapped, Usage::ShaderRead);
    return std::make_shared<TextureMT>(dynamic_cast<const ContextBase&>(context), texture_settings);
}

Ptr<Texture> Texture::CreateCube(const Context& context, uint32_t dimension_size, const Opt<uint32_t>& array_length_opt, PixelFormat pixel_format, bool mipmapped)
{
    META_FUNCTION_TASK();
    const Settings texture_settings = Settings::Cube(dimension_size, array_length_opt, pixel_format, mipmapped, Usage::ShaderRead);
    return std::make_shared<TextureMT>(dynamic_cast<const ContextBase&>(context), texture_settings);
}

TextureMT::TextureMT(const ContextBase& context, const Settings& settings)
    : ResourceMT(context, settings)
    , m_mtl_texture(settings.type == Texture::Type::FrameBuffer
                      ? nil // actual frame buffer texture descriptor is set in UpdateFrameBuffer()
                      : [GetContextMT().GetDeviceMT().GetNativeDevice()  newTextureWithDescriptor:GetNativeTextureDescriptor()])
{
    META_FUNCTION_TASK();
}

bool TextureMT::SetName(const std::string& name)
{
    META_FUNCTION_TASK();
    if (!ResourceMT::SetName(name))
        return false;

    m_mtl_texture.label = [[NSString alloc] initWithUTF8String:name.data()];
    return true;
}

void TextureMT::SetData(const SubResources& sub_resources, CommandQueue& target_cmd_queue)
{
    META_FUNCTION_TASK();
    META_CHECK_ARG_NOT_NULL(m_mtl_texture);
    META_CHECK_ARG_EQUAL(m_mtl_texture.storageMode, MTLStorageModePrivate);

    ResourceMT::SetData(sub_resources, target_cmd_queue);

    BlitCommandListMT& blit_command_list = dynamic_cast<BlitCommandListMT&>(GetContextBase().GetUploadCommandKit().GetListForEncoding());
    blit_command_list.RetainResource(*this);

    const id<MTLBlitCommandEncoder>& mtl_blit_encoder = blit_command_list.GetNativeCommandEncoder();
    META_CHECK_ARG_NOT_NULL(mtl_blit_encoder);

    const Settings& settings        = GetSettings();
    const uint32_t  bytes_per_row   = settings.dimensions.GetWidth()  * GetPixelSize(settings.pixel_format);
    const uint32_t  bytes_per_image = settings.dimensions.GetHeight() * bytes_per_row;
    const MTLRegion texture_region  = GetTextureRegion(settings.dimensions, settings.dimension_type);

    for(const SubResource& sub_resource : sub_resources)
    {
        uint32_t slice = 0;
        switch(settings.dimension_type)
        {
            case Texture::DimensionType::Tex1DArray:
            case Texture::DimensionType::Tex2DArray:
                slice = sub_resource.GetIndex().GetArrayIndex();
                break;
            case Texture::DimensionType::Cube:
                slice = sub_resource.GetIndex().GetDepthSlice();
                break;
            case Texture::DimensionType::CubeArray:
                slice = sub_resource.GetIndex().GetDepthSlice() + sub_resource.GetIndex().GetArrayIndex() * 6;
                break;
            default:
                slice = 0;
        }

        [mtl_blit_encoder copyFromBuffer:GetUploadSubresourceBuffer(sub_resource)
                            sourceOffset:0
                       sourceBytesPerRow:bytes_per_row
                     sourceBytesPerImage:bytes_per_image
                              sourceSize:texture_region.size
                               toTexture:m_mtl_texture
                        destinationSlice:slice
                        destinationLevel:sub_resource.GetIndex().GetMipLevel()
                       destinationOrigin:texture_region.origin];
    }

    if (settings.mipmapped && sub_resources.size() < GetSubresourceCount().GetRawCount())
    {
        GenerateMipLevels(blit_command_list);
    }

    GetContextBase().RequestDeferredAction(Context::DeferredAction::UploadResources);
}

void TextureMT::UpdateFrameBuffer()
{
    META_FUNCTION_TASK();
    META_CHECK_ARG_EQUAL_DESCR(GetSettings().type, Texture::Type::FrameBuffer, "unable to update frame buffer on non-FB texture");
    m_mtl_texture = [GetRenderContextMT().GetNativeDrawable() texture];
}

void TextureMT::GenerateMipLevels(BlitCommandListMT& blit_command_list)
{
    META_FUNCTION_TASK();
    META_DEBUG_GROUP_CREATE_VAR(s_debug_group, "Texture MIPs Generation");
    blit_command_list.Reset(s_debug_group.get());

    const id<MTLBlitCommandEncoder>& mtl_blit_encoder = blit_command_list.GetNativeCommandEncoder();
    META_CHECK_ARG_NOT_NULL(mtl_blit_encoder);
    META_CHECK_ARG_NOT_NULL(m_mtl_texture);

    [mtl_blit_encoder generateMipmapsForTexture: m_mtl_texture];
}

const RenderContextMT& TextureMT::GetRenderContextMT() const
{
    META_FUNCTION_TASK();
    META_CHECK_ARG_EQUAL_DESCR(GetContextBase().GetType(), Context::Type::Render, "incompatible context type");
    return static_cast<const RenderContextMT&>(GetContextMT());
}

MTLTextureUsage TextureMT::GetNativeTextureUsage()
{
    META_FUNCTION_TASK();
    using namespace magic_enum::bitwise_operators;

    NSUInteger texture_usage = MTLTextureUsageUnknown;
    const Settings& settings = GetSettings();
    
    if (static_cast<bool>(settings.usage_mask & TextureBase::Usage::ShaderRead))
        texture_usage |= MTLTextureUsageShaderRead;
    
    if (static_cast<bool>(settings.usage_mask & TextureBase::Usage::ShaderWrite))
        texture_usage |= MTLTextureUsageShaderWrite;
    
    if (static_cast<bool>(settings.usage_mask & TextureBase::Usage::RenderTarget))
        texture_usage |= MTLTextureUsageRenderTarget;

    return texture_usage;
}

MTLTextureDescriptor* TextureMT::GetNativeTextureDescriptor()
{
    META_FUNCTION_TASK();

    const Settings& settings = GetSettings();
    const MTLPixelFormat mtl_pixel_format = TypeConverterMT::DataFormatToMetalPixelType(settings.pixel_format);
    const BOOL is_tex_mipmapped = MacOS::ConvertToNsType<bool, BOOL>(settings.mipmapped);

    MTLTextureDescriptor* mtl_tex_desc = nil;
    switch(settings.dimension_type)
    {
    case Texture::DimensionType::Tex2D:
        mtl_tex_desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:mtl_pixel_format
                                                                          width:settings.dimensions.GetWidth()
                                                                         height:settings.dimensions.GetHeight()
                                                                      mipmapped:is_tex_mipmapped];
        break;

    case Texture::DimensionType::Cube:
        mtl_tex_desc = [MTLTextureDescriptor textureCubeDescriptorWithPixelFormat:mtl_pixel_format
                                                                             size:settings.dimensions.GetWidth()
                                                                        mipmapped:is_tex_mipmapped];
        break;

    case Texture::DimensionType::Tex1D:
    case Texture::DimensionType::Tex1DArray:
    case Texture::DimensionType::Tex2DArray:
    case Texture::DimensionType::Tex2DMultisample:
    case Texture::DimensionType::CubeArray:
    case Texture::DimensionType::Tex3D:
        mtl_tex_desc                    = [[MTLTextureDescriptor alloc] init];
        mtl_tex_desc.pixelFormat        = mtl_pixel_format;
        mtl_tex_desc.textureType        = GetNativeTextureType(settings.dimension_type);
        mtl_tex_desc.width              = settings.dimensions.GetWidth();
        mtl_tex_desc.height             = settings.dimensions.GetHeight();
        mtl_tex_desc.depth              = settings.dimension_type == Texture::DimensionType::Tex3D
                                        ? settings.dimensions.GetDepth()
                                        : 1U;
        mtl_tex_desc.arrayLength        = settings.array_length;
        mtl_tex_desc.mipmapLevelCount   = GetSubresourceCount().GetMipLevelsCount();
        break;

    default: META_UNEXPECTED_ARG(settings.dimension_type);
    }

    if (!mtl_tex_desc)
        return nil;

    mtl_tex_desc.resourceOptions = MTLResourceStorageModePrivate;
    mtl_tex_desc.usage = GetNativeTextureUsage();

    return mtl_tex_desc;
}

} // namespace Methane::Graphics
