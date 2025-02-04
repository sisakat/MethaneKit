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

FILE: Methane/Graphics/TextureBase.cpp
Base implementation of the texture interface.

******************************************************************************/

#include "TextureBase.h"
#include "RenderContextBase.h"

#include <Methane/Graphics/TypeFormatters.hpp>
#include <Methane/Instrumentation.h>
#include <Methane/Checks.hpp>

namespace Methane::Graphics
{

Texture::View::View(Texture& texture, const SubResource::Index& subresource_index, const SubResource::Count& subresource_count, Opt<TextureDimensionType> texture_dimension_type_opt)
    : Resource::View(texture, subresource_index, subresource_count, texture_dimension_type_opt)
    , m_texture_ptr(std::dynamic_pointer_cast<Texture>(GetResourcePtr()))
{
    META_FUNCTION_TASK();
}

Texture& Texture::View::GetTexture() const
{
    META_CHECK_ARG_NOT_NULL_DESCR(m_texture_ptr, "can not get texture from uninitialized resource view");
    return *m_texture_ptr;
}

Texture::Settings Texture::Settings::Image(const Dimensions& dimensions, const Opt<uint32_t>& array_length_opt, PixelFormat pixel_format, bool mipmapped, TextureBase::Usage usage)
{
    META_FUNCTION_TASK();

    Settings settings;
    if (dimensions.GetHeight() == 1)
        settings.dimension_type = array_length_opt ? DimensionType::Tex1DArray : DimensionType::Tex1D;
    else if (dimensions.GetDepth() == 1)
        settings.dimension_type = array_length_opt ? DimensionType::Tex2DArray : DimensionType::Tex2D;
    else
        settings.dimension_type = DimensionType::Tex3D;
    settings.type         = Type::Texture;
    settings.dimensions   = dimensions;
    settings.array_length = array_length_opt.value_or(1U);
    settings.pixel_format = pixel_format;
    settings.usage_mask   = usage;
    settings.mipmapped    = mipmapped;

    return settings;
}

Texture::Settings Texture::Settings::Cube(uint32_t dimension_size, const Opt<uint32_t>& array_length_opt, PixelFormat pixel_format, bool mipmapped, Usage usage)
{
    META_FUNCTION_TASK();

    Settings settings;
    settings.type           = Type::Texture;
    settings.dimension_type = array_length_opt ? DimensionType::CubeArray : DimensionType::Cube;
    settings.dimensions     = Dimensions(dimension_size, dimension_size, 6U);
    settings.array_length   = array_length_opt.value_or(1U);
    settings.pixel_format   = pixel_format;
    settings.usage_mask     = usage;
    settings.mipmapped      = mipmapped;

    return settings;
}

Texture::Settings Texture::Settings::FrameBuffer(const Dimensions& dimensions, PixelFormat pixel_format)
{
    META_FUNCTION_TASK();

    Settings settings;
    settings.type           = Type::FrameBuffer;
    settings.dimension_type = DimensionType::Tex2D;
    settings.usage_mask     = Usage::RenderTarget;
    settings.pixel_format   = pixel_format;
    settings.dimensions     = dimensions;

    return settings;
}

Texture::Settings Texture::Settings::DepthStencilBuffer(const Dimensions& dimensions, PixelFormat pixel_format, Usage usage_mask)
{
    META_FUNCTION_TASK();

    Settings settings;
    settings.type           = Type::DepthStencilBuffer;
    settings.dimension_type = DimensionType::Tex2D;
    settings.usage_mask     = usage_mask;
    settings.pixel_format   = pixel_format;
    settings.dimensions     = dimensions;

    return settings;
}

TextureBase::TextureBase(const ContextBase& context, const Settings& settings,
                         State initial_state, Opt<State> auto_transition_source_state_opt)
    : ResourceBase(context, Resource::Type::Texture, settings.usage_mask, initial_state, auto_transition_source_state_opt)
    , m_settings(settings)
{
    META_FUNCTION_TASK();
    META_CHECK_ARG_NOT_EQUAL_DESCR(m_settings.usage_mask, TextureBase::Usage::None, "can not create texture with 'Unknown' usage mask");
    META_CHECK_ARG_NOT_EQUAL_DESCR(m_settings.pixel_format, PixelFormat::Unknown, "can not create texture with 'Unknown' pixel format");
    META_CHECK_ARG_NOT_NULL_DESCR(m_settings.array_length, "array length should be greater than zero");

    ValidateDimensions(m_settings.dimension_type, m_settings.dimensions, m_settings.mipmapped);
    SetSubResourceCount(
        SubResource::Count(
            settings.dimensions.GetDepth(),
            settings.array_length,
            settings.mipmapped ? GetRequiredMipLevelsCount(settings.dimensions) : 1U
        )
    );
}

void TextureBase::ValidateDimensions(DimensionType dimension_type, const Dimensions& dimensions, bool mipmapped)
{
    META_FUNCTION_TASK();
    META_UNUSED(mipmapped);
    META_CHECK_ARG_NOT_ZERO_DESCR(dimensions, "all dimension sizes should be greater than zero");

    switch (dimension_type)
    {
    case DimensionType::Cube:
    case DimensionType::CubeArray:
        META_CHECK_ARG_DESCR(dimensions, dimensions.GetWidth() == dimensions.GetHeight() && dimensions.GetDepth() == 6, "cube texture must have equal width and height dimensions and depth equal to 6");
        [[fallthrough]];
    case DimensionType::Tex3D:
        META_CHECK_ARG_DESCR(dimensions.GetDepth(), !mipmapped || !(dimensions.GetDepth() % 2), "all dimensions of the mip-mapped texture should be a power of 2, but depth is not");
        [[fallthrough]];
    case DimensionType::Tex2D:
    case DimensionType::Tex2DArray:
    case DimensionType::Tex2DMultisample:
        META_CHECK_ARG_DESCR(dimensions.GetHeight(), !mipmapped || !(dimensions.GetHeight() % 2), "all dimensions of the mip-mapped texture should be a power of 2, but height is not");
        [[fallthrough]];
    case DimensionType::Tex1D:
    case DimensionType::Tex1DArray:
        META_CHECK_ARG_DESCR(dimensions.GetWidth(), !mipmapped || !(dimensions.GetWidth() % 2), "all dimensions of the mip-mapped texture should be a power of 2, but width is not");
        break;
    default:
        META_UNEXPECTED_ARG(dimension_type);
    }
}

Data::Size TextureBase::GetRequiredMipLevelsCount(const Dimensions& dimensions)
{
    META_FUNCTION_TASK();
    return 1U + static_cast<uint32_t>(std::floor(std::log2(static_cast<double>(dimensions.GetLongestSide()))));
}

Data::Size TextureBase::GetDataSize(Data::MemoryState size_type) const noexcept
{
    META_FUNCTION_TASK();
    return size_type == Data::MemoryState::Reserved
            ? m_settings.dimensions.GetPixelsCount() * GetPixelSize(m_settings.pixel_format) * m_settings.array_length
            : GetInitializedDataSize();
}

Data::Size TextureBase::CalculateSubResourceDataSize(const SubResource::Index& sub_resource_index) const
{
    META_FUNCTION_TASK();
    ValidateSubResource(sub_resource_index, {});

    const Data::Size pixel_size = GetPixelSize(m_settings.pixel_format);
    if (sub_resource_index.GetMipLevel() == 0U)
    {
        return pixel_size * static_cast<const Data::FrameSize&>(m_settings.dimensions).GetPixelsCount();
    }

    const double mip_divider = std::pow(2.0, sub_resource_index.GetMipLevel());
    const Data::FrameSize mip_frame_size(
        static_cast<uint32_t>(std::ceil(static_cast<double>(m_settings.dimensions.GetWidth()) / mip_divider)),
        static_cast<uint32_t>(std::ceil(static_cast<double>(m_settings.dimensions.GetHeight()) / mip_divider))
    );
    return pixel_size * mip_frame_size.GetPixelsCount();
}

} // namespace Methane::Graphics