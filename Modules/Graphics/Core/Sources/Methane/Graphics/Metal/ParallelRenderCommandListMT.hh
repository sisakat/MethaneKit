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

FILE: Methane/Graphics/Metal/ParallelRenderCommandListMT.hh
Metal implementation of the parallel render command list interface.

******************************************************************************/

#pragma once

#include <Methane/Graphics/ParallelRenderCommandListBase.h>

#include <functional>

#import <Metal/Metal.h>

namespace Methane::Graphics
{

class CommandQueueMT;
class BufferMT;
class RenderPassMT;

class ParallelRenderCommandListMT final : public ParallelRenderCommandListBase
{
public:
    ParallelRenderCommandListMT(CommandQueueBase& command_queue, RenderPassBase& render_pass);

    // ParallelRenderCommandList interface
    void Reset(const Ptr<RenderState>& sp_render_state, DebugGroup* p_debug_group = nullptr) override;

    // CommandList interface
    void Commit() override;

    // CommandListBase interface
    void Execute(uint32_t frame_index) override;

    // Object interface
    void SetName(const std::string& label) override;

    id<MTLCommandBuffer>&                GetNativeCommandBuffer() noexcept         { return m_mtl_cmd_buffer; }
    id<MTLParallelRenderCommandEncoder>& GetNativeParallelRenderEncoder() noexcept { return m_mtl_parallel_render_encoder; }

private:
    CommandQueueMT& GetCommandQueueMT() noexcept;
    RenderPassMT&   GetPassMT();

    id<MTLCommandBuffer>                m_mtl_cmd_buffer = nil;
    id<MTLParallelRenderCommandEncoder> m_mtl_parallel_render_encoder = nil;
};

} // namespace Methane::Graphics
