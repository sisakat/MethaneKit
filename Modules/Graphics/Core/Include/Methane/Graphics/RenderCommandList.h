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

FILE: Methane/Graphics/RenderCommandList.h
Methane render command list interface.

******************************************************************************/

#pragma once

#include "CommandList.h"
#include "Buffer.h"
#include "RenderState.h"

#include <Methane/Memory.hpp>

namespace Methane::Graphics
{

struct RenderPass;
struct ParallelRenderCommandList;

struct RenderCommandList : virtual CommandList // NOSONAR
{
    static constexpr Type type = Type::Render;

    enum class Primitive
    {
        Point,
        Line,
        LineStrip,
        Triangle,
        TriangleStrip
    };

    // Create RenderCommandList instance
    [[nodiscard]] static Ptr<RenderCommandList> Create(CommandQueue& command_queue, RenderPass& render_pass);
    [[nodiscard]] static Ptr<RenderCommandList> Create(ParallelRenderCommandList& parallel_command_list);
    
    // RenderCommandList interface
    [[nodiscard]] virtual bool IsValidationEnabled() const noexcept = 0;
    virtual void SetValidationEnabled(bool is_validation_enabled) = 0;
    [[nodiscard]] virtual RenderPass& GetRenderPass() const = 0;
    virtual void ResetWithState(RenderState& render_state, DebugGroup* p_debug_group = nullptr) = 0;
    virtual void ResetWithStateOnce(RenderState& render_state, DebugGroup* p_debug_group = nullptr) = 0;
    virtual void SetRenderState(RenderState& render_state, RenderState::Groups state_groups = RenderState::Groups::All) = 0;
    virtual void SetViewState(ViewState& view_state) = 0;
    virtual bool SetVertexBuffers(BufferSet& vertex_buffers, bool set_resource_barriers = true) = 0;
    virtual bool SetIndexBuffer(Buffer& index_buffer, bool set_resource_barriers = true) = 0;
    virtual void DrawIndexed(Primitive primitive, uint32_t index_count = 0, uint32_t start_index = 0, uint32_t start_vertex = 0,
                             uint32_t instance_count = 1, uint32_t start_instance = 0) = 0;
    virtual void Draw(Primitive primitive, uint32_t vertex_count, uint32_t start_vertex = 0,
                      uint32_t instance_count = 1, uint32_t start_instance = 0) = 0;
    
    using CommandList::Reset;
};

} // namespace Methane::Graphics
