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

FILE: Methane/Graphics/Types.cpp
Methane graphics type functions implementation.

******************************************************************************/

#include <Methane/Graphics/Types.h>
#include <Methane/Graphics/Rect.hpp>
#include <Methane/Graphics/Volume.hpp>

#include <Methane/Instrumentation.h>

#include <cassert>

namespace Methane::Graphics
{

ScissorRect GetFrameScissorRect(const FrameRect& frame_rect)
{
    META_FUNCTION_TASK();
    return {
        ScissorRect::Point(static_cast<uint32_t>(std::max(0, frame_rect.origin.GetX())),
                           static_cast<uint32_t>(std::max(0, frame_rect.origin.GetY()))),
        ScissorRect::Size(frame_rect.origin.GetX() >= 0 ? frame_rect.size.width : frame_rect.size.width + frame_rect.origin.GetX(),
                          frame_rect.origin.GetY() >= 0 ? frame_rect.size.height : frame_rect.size.height + frame_rect.origin.GetY())
    };
}

ScissorRect GetFrameScissorRect(const FrameSize& frame_size)
{
    META_FUNCTION_TASK();
    return ScissorRect {
        ScissorRect::Point(0u, 0u),
        ScissorRect::Size(frame_size.width, frame_size.height)
    };
}

Viewport GetFrameViewport(const FrameRect& frame_rect)
{
    META_FUNCTION_TASK();
    return Viewport{
        Viewport::Point(static_cast<double>(frame_rect.origin.GetX()), static_cast<double>(frame_rect.origin.GetY()), 0.0),
        Viewport::Size(static_cast<double>(frame_rect.size.width), static_cast<double>(frame_rect.size.height), 1.0)
    };
}

Viewport GetFrameViewport(const FrameSize& frame_size)
{
    META_FUNCTION_TASK();
    return Viewport {
        Viewport::Point(0.0, 0.0, 0.0),
        Viewport::Size(static_cast<double>(frame_size.width), static_cast<double>(frame_size.height), 1.0)
    };
}

Data::Size GetPixelSize(PixelFormat pixel_format) noexcept
{
    META_FUNCTION_TASK();

    switch(pixel_format)
    {
    case PixelFormat::RGBA8:
    case PixelFormat::RGBA8Unorm:
    case PixelFormat::RGBA8Unorm_sRGB:
    case PixelFormat::BGRA8Unorm:
    case PixelFormat::BGRA8Unorm_sRGB:
    case PixelFormat::R32Float:
    case PixelFormat::R32Uint:
    case PixelFormat::R32Sint:
    case PixelFormat::Depth32Float:
        return 4;

    case PixelFormat::R16Float:
    case PixelFormat::R16Uint:
    case PixelFormat::R16Sint:
    case PixelFormat::R16Unorm:
    case PixelFormat::R16Snorm:
        return 2;

    case PixelFormat::R8Uint:
    case PixelFormat::R8Sint:
    case PixelFormat::R8Unorm:
    case PixelFormat::R8Snorm:
    case PixelFormat::A8Unorm:
        return 1;

    default:
        assert(0);
    }

    return 0;
}

bool IsSrgbColorSpace(PixelFormat pixel_format) noexcept
{
    META_FUNCTION_TASK();

    switch (pixel_format)
    {
    case PixelFormat::RGBA8Unorm_sRGB:
    case PixelFormat::BGRA8Unorm_sRGB:
        return true;

    default:
        return false;
    }
}

} // namespace Methane::Graphics
