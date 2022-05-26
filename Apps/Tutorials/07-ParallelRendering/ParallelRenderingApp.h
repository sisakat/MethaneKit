/******************************************************************************

Copyright 2022 Evgeny Gorodetskiy

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

FILE: ParallelRenderingApp.h
Tutorial demonstrating parallel rendering with Methane graphics API

******************************************************************************/

#pragma once

#include <Methane/Kit.h>
#include <Methane/UserInterface/App.hpp>

namespace hlslpp // NOSONAR
{
#pragma pack(push, 16)
#include "Shaders/ParallelRenderingUniforms.h" // NOSONAR
#pragma pack(pop)
}

namespace Methane::Tutorials
{

namespace gfx = Methane::Graphics;

struct ParallelRenderingFrame final : Graphics::AppFrame
{
    gfx::InstancedMeshBufferBindings    cubes_array;
    Ptr<gfx::ParallelRenderCommandList> parallel_render_cmd_list_ptr;
    Ptr<gfx::CommandListSet>            execute_cmd_list_set_ptr;

    using gfx::AppFrame::AppFrame;
};

using UserInterfaceApp = UserInterface::App<ParallelRenderingFrame>;

class ParallelRenderingApp final : public UserInterfaceApp // NOSONAR
{
public:
    ParallelRenderingApp();
    ~ParallelRenderingApp() override;

    // GraphicsApp overrides
    void Init() override;
    bool Resize(const gfx::FrameSize& frame_size, bool is_minimized) override;
    bool Update() override;
    bool Render() override;

protected:
    // IContextCallback override
    void OnContextReleased(gfx::Context& context) override;

private:
    bool Animate(double elapsed_seconds, double delta_seconds);

    using MeshBuffers = gfx::MeshBuffers<hlslpp::Uniforms>;

    const float           m_model_scale;
    hlslpp::float4x4      m_model_matrix;
    gfx::Camera           m_camera;
    Ptr<gfx::RenderState> m_render_state_ptr;
    Ptr<gfx::Texture>     m_texture_array_ptr;
    Ptr<gfx::Sampler>     m_texture_sampler_ptr;
    Ptr<MeshBuffers>      m_cube_buffers_ptr;
};

} // namespace Methane::Tutorials
