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

FILE: ShadowCubeApp.cpp
Tutorial demonstrating shadow-pass rendering with Methane graphics API

******************************************************************************/

#include "ShadowCubeApp.h"

#include <Methane/Samples/AppSettings.hpp>
#include <Methane/Graphics/CubeMesh.hpp>
#include <Methane/Data/TimeAnimation.h>

#include <magic_enum.hpp>

namespace Methane::Tutorials
{

struct Vertex
{
    gfx::Mesh::Position position;
    gfx::Mesh::Normal   normal;
    gfx::Mesh::TexCoord texcoord;

    inline static const gfx::Mesh::VertexLayout layout{
        gfx::Mesh::VertexField::Position,
        gfx::Mesh::VertexField::Normal,
        gfx::Mesh::VertexField::TexCoord,
    };
};

static const gfx::FrameSize g_shadow_map_size(1024, 1024);

ShadowCubeApp::ShadowCubeApp()
    : UserInterfaceApp(
        GetGraphicsTutorialAppSettings("Methane Shadow Cube", g_default_app_options_color_with_depth_and_anim),
        GetUserInterfaceTutorialAppSettings(g_default_app_options_color_with_depth_and_anim),
        "Methane tutorial of shadow pass rendering")
{
    m_view_camera.ResetOrientation({ { 15.0F, 22.5F, -15.0F }, { 0.0F, 7.5F, 0.0F }, { 0.0F, 1.0F, 0.0F } });

    m_light_camera.ResetOrientation({ { 0.0F,  25.0F, -25.0F }, { 0.0F, 7.5F, 0.0F }, { 0.0F, 1.0F, 0.0F } });
    m_light_camera.SetProjection(gfx::Camera::Projection::Orthogonal);
    m_light_camera.SetParameters({ -300, 300.F, 90.F });
    m_light_camera.Resize(Data::FloatSize(80.F, 80.F));

    // Setup animations
    GetAnimations().emplace_back(std::make_shared<Data::TimeAnimation>(std::bind(&ShadowCubeApp::Animate, this, std::placeholders::_1, std::placeholders::_2)));
}

ShadowCubeApp::~ShadowCubeApp()
{
    // Wait for GPU rendering is completed to release resources
    WaitForRenderComplete();
}

void ShadowCubeApp::Init()
{
    UserInterfaceApp::Init();

    gfx::CommandQueue& render_cmd_queue = GetRenderContext().GetRenderCommandKit().GetQueue();
    const gfx::RenderContext::Settings& context_settings = GetRenderContext().GetSettings();
    m_view_camera.Resize(context_settings.frame_size);

    const gfx::Mesh::VertexLayout mesh_layout(Vertex::layout);
    const gfx::CubeMesh<Vertex>   cube_mesh(mesh_layout, 1.F, 1.F, 1.F);
    const gfx::QuadMesh<Vertex>   floor_mesh(mesh_layout, 7.F, 7.F, 0.F, 0, gfx::QuadMesh<Vertex>::FaceType::XZ);

    // Load textures, vertex and index buffers for cube and floor meshes
    using namespace magic_enum::bitwise_operators;
    const gfx::ImageLoader::Options image_options = gfx::ImageLoader::Options::Mipmapped
                                                  | gfx::ImageLoader::Options::SrgbColorSpace;

    m_cube_buffers_ptr  = std::make_unique<TexturedMeshBuffers>(render_cmd_queue, cube_mesh, "Cube");
    m_cube_buffers_ptr->SetTexture(GetImageLoader().LoadImageToTexture2D(render_cmd_queue, "MethaneBubbles.jpg", image_options, "Cube Face Texture"));

    m_floor_buffers_ptr = std::make_unique<TexturedMeshBuffers>(render_cmd_queue, floor_mesh, "Floor");
    m_floor_buffers_ptr->SetTexture(GetImageLoader().LoadImageToTexture2D(render_cmd_queue, "MarbleWhite.jpg", image_options, "Floor Texture"));

    const auto constants_data_size      = static_cast<Data::Size>(sizeof(hlslpp::Constants));
    const auto scene_uniforms_data_size = static_cast<Data::Size>(sizeof(hlslpp::SceneUniforms));
    const auto mesh_uniforms_data_size  = static_cast<Data::Size>(sizeof(hlslpp::MeshUniforms));

    // Create constants buffer for frame rendering
    m_const_buffer_ptr = gfx::Buffer::CreateConstantBuffer(GetRenderContext(), constants_data_size);
    m_const_buffer_ptr->SetName("Constants Buffer");
    m_const_buffer_ptr->SetData(
        { { reinterpret_cast<Data::ConstRawPtr>(&m_scene_constants), sizeof(m_scene_constants) } }, // NOSONAR
        render_cmd_queue
    );

    // Create sampler for cube and floor textures sampling
    m_texture_sampler_ptr = gfx::Sampler::Create(GetRenderContext(),
        gfx::Sampler::Settings
        {
            gfx::Sampler::Filter  { gfx::Sampler::Filter::MinMag::Linear },
            gfx::Sampler::Address { gfx::Sampler::Address::Mode::ClampToEdge }
        }
    );
    m_texture_sampler_ptr->SetName("Texture Sampler");

    // Create sampler for shadow-map texture
    m_shadow_sampler_ptr = gfx::Sampler::Create(GetRenderContext(),
        gfx::Sampler::Settings
        {
            gfx::Sampler::Filter  { gfx::Sampler::Filter::MinMag::Linear },
            gfx::Sampler::Address { gfx::Sampler::Address::Mode::ClampToEdge }
        }
    );
    m_shadow_sampler_ptr->SetName("Shadow Map Sampler");

    // ========= Final Pass Render & View States =========

    const gfx::Shader::EntryFunction    vs_main{ "ShadowCube", "CubeVS" };
    const gfx::Shader::EntryFunction    ps_main{ "ShadowCube", "CubePS" };
    const gfx::Shader::MacroDefinitions textured_shadows_definitions{ { "ENABLE_SHADOWS", "" }, { "ENABLE_TEXTURING", "" } };

    // Create final pass rendering state with program
    gfx::RenderState::Settings final_state_settings;
    final_state_settings.program_ptr = gfx::Program::Create(GetRenderContext(),
        gfx::Program::Settings
        {
            gfx::Program::Shaders
            {
                gfx::Shader::CreateVertex(GetRenderContext(), { Data::ShaderProvider::Get(), vs_main, textured_shadows_definitions }),
                gfx::Shader::CreatePixel(GetRenderContext(),  { Data::ShaderProvider::Get(), ps_main, textured_shadows_definitions }),
            },
            gfx::Program::InputBufferLayouts
            {
                gfx::Program::InputBufferLayout
                {
                    gfx::Program::InputBufferLayout::ArgumentSemantics { cube_mesh.GetVertexLayout().GetSemantics() }
                }
            },
            gfx::Program::ArgumentAccessors
            {
                { { gfx::Shader::Type::Vertex, "g_mesh_uniforms"  }, gfx::Program::ArgumentAccessor::Type::Mutable       },
                { { gfx::Shader::Type::Pixel,  "g_scene_uniforms" }, gfx::Program::ArgumentAccessor::Type::FrameConstant },
                { { gfx::Shader::Type::Pixel,  "g_constants"      }, gfx::Program::ArgumentAccessor::Type::Constant      },
                { { gfx::Shader::Type::Pixel,  "g_shadow_map"     }, gfx::Program::ArgumentAccessor::Type::FrameConstant },
                { { gfx::Shader::Type::Pixel,  "g_shadow_sampler" }, gfx::Program::ArgumentAccessor::Type::Constant      },
                { { gfx::Shader::Type::Pixel,  "g_texture"        }, gfx::Program::ArgumentAccessor::Type::Mutable       },
                { { gfx::Shader::Type::Pixel,  "g_texture_sampler"}, gfx::Program::ArgumentAccessor::Type::Constant      },
            },
            GetScreenRenderPattern().GetAttachmentFormats()
        }
    );
    final_state_settings.render_pattern_ptr = GetScreenRenderPatternPtr();
    final_state_settings.program_ptr->SetName("Textured, Shadows & Lighting");
    final_state_settings.depth.enabled = true;

    m_final_pass.render_state_ptr = gfx::RenderState::Create(GetRenderContext(), final_state_settings);
    m_final_pass.render_state_ptr->SetName("Final pass render state");
    m_final_pass.view_state_ptr = GetViewStatePtr();

    // ========= Shadow Pass Render & View States =========

    // Create shadow-pass render pattern
    m_shadow_pass_pattern_ptr = gfx::RenderPattern::Create(GetRenderContext(), {
        { }, // No color attachments
        gfx::RenderPattern::DepthAttachment(
            0U, context_settings.depth_stencil_format, 1U,
            gfx::RenderPass::Attachment::LoadAction::Clear,
            gfx::RenderPass::Attachment::StoreAction::Store,
            context_settings.clear_depth_stencil->first
        ),
        std::nullopt, // No stencil attachment
        gfx::RenderPass::Access::ShaderResources,
        false // intermediate render pass
    });

    // Create shadow-pass rendering state with program
    const gfx::Shader::MacroDefinitions textured_definitions{ { "ENABLE_TEXTURING", "" } };
    gfx::RenderState::Settings shadow_state_settings;
    shadow_state_settings.program_ptr = gfx::Program::Create(GetRenderContext(),
        gfx::Program::Settings
        {
            gfx::Program::Shaders
            {
                gfx::Shader::CreateVertex(GetRenderContext(), { Data::ShaderProvider::Get(), vs_main, textured_definitions }),
            },
            final_state_settings.program_ptr->GetSettings().input_buffer_layouts,
            gfx::Program::ArgumentAccessors
            {
                { { gfx::Shader::Type::All, "g_mesh_uniforms"  }, gfx::Program::ArgumentAccessor::Type::Mutable },
            },
            m_shadow_pass_pattern_ptr->GetAttachmentFormats()
        }
    );
    shadow_state_settings.render_pattern_ptr = m_shadow_pass_pattern_ptr;
    shadow_state_settings.program_ptr->SetName("Vertex Only: Textured, Lighting");
    shadow_state_settings.render_pattern_ptr = m_shadow_pass_pattern_ptr;
    shadow_state_settings.depth.enabled = true;

    m_shadow_pass.render_state_ptr = gfx::RenderState::Create(GetRenderContext(), shadow_state_settings);
    m_shadow_pass.render_state_ptr->SetName("Shadow-map render state");
    m_shadow_pass.view_state_ptr = gfx::ViewState::Create({
        { gfx::GetFrameViewport(g_shadow_map_size)    },
        { gfx::GetFrameScissorRect(g_shadow_map_size) }
    });

    // ========= Per-Frame Data =========

    using namespace magic_enum::bitwise_operators;
    const gfx::Texture::Settings shadow_texture_settings = gfx::Texture::Settings::DepthStencilBuffer(
        gfx::Dimensions(g_shadow_map_size),
        context_settings.depth_stencil_format,
        gfx::Texture::Usage::RenderTarget | gfx::Texture::Usage::ShaderRead
    );

    for(ShadowCubeFrame& frame : GetFrames())
    {
        // Create uniforms buffer with volatile parameters for the whole scene rendering
        frame.scene_uniforms_buffer_ptr = gfx::Buffer::CreateConstantBuffer(GetRenderContext(), scene_uniforms_data_size, false, true);
        frame.scene_uniforms_buffer_ptr->SetName(IndexedName("Scene Uniforms Buffer", frame.index));

        // ========= Shadow Pass Resources =========

        // Create uniforms buffer for Cube rendering in Shadow pass
        frame.shadow_pass.cube.uniforms_buffer_ptr = gfx::Buffer::CreateConstantBuffer(GetRenderContext(), mesh_uniforms_data_size, false, true);
        frame.shadow_pass.cube.uniforms_buffer_ptr->SetName(IndexedName("Cube Uniforms Buffer for Shadow Pass", frame.index));

        // Create uniforms buffer for Floor rendering in Shadow pass
        frame.shadow_pass.floor.uniforms_buffer_ptr = gfx::Buffer::CreateConstantBuffer(GetRenderContext(), mesh_uniforms_data_size, false, true);
        frame.shadow_pass.floor.uniforms_buffer_ptr->SetName(IndexedName("Floor Uniforms Buffer for Shadow Pass", frame.index));

        // Shadow-pass resource bindings for cube rendering
        frame.shadow_pass.cube.program_bindings_ptr = gfx::ProgramBindings::Create(shadow_state_settings.program_ptr, {
            { { gfx::Shader::Type::All, "g_mesh_uniforms"  }, { { *frame.shadow_pass.cube.uniforms_buffer_ptr } } },
        }, frame.index);
        frame.shadow_pass.cube.program_bindings_ptr->SetName(IndexedName("Cube Shadow-Pass Bindings {}", frame.index));

        // Shadow-pass resource bindings for floor rendering
        frame.shadow_pass.floor.program_bindings_ptr = gfx::ProgramBindings::Create(shadow_state_settings.program_ptr, {
            { { gfx::Shader::Type::All, "g_mesh_uniforms"  }, { { *frame.shadow_pass.floor.uniforms_buffer_ptr } } },
        }, frame.index);
        frame.shadow_pass.floor.program_bindings_ptr->SetName(IndexedName("Floor Shadow-Pass Bindings {}", frame.index));

        // Create depth texture for shadow map rendering
        frame.shadow_pass.rt_texture_ptr = gfx::Texture::CreateRenderTarget(GetRenderContext(), shadow_texture_settings);
        frame.shadow_pass.rt_texture_ptr->SetName(IndexedName("Shadow Map", frame.index));
        
        // Create shadow pass configuration with depth attachment
        frame.shadow_pass.render_pass_ptr = gfx::RenderPass::Create(*m_shadow_pass_pattern_ptr, {
            { *frame.shadow_pass.rt_texture_ptr },
            shadow_texture_settings.dimensions.AsRectSize()
        });
        
        // Create render pass and command list for shadow pass rendering
        frame.shadow_pass.cmd_list_ptr = gfx::RenderCommandList::Create(GetRenderContext().GetRenderCommandKit().GetQueue(), *frame.shadow_pass.render_pass_ptr);
        frame.shadow_pass.cmd_list_ptr->SetName(IndexedName("Shadow-Map Rendering", frame.index));

        // ========= Final Pass Resources =========

        // Create uniforms buffer for Cube rendering in Final pass
        frame.final_pass.cube.uniforms_buffer_ptr = gfx::Buffer::CreateConstantBuffer(GetRenderContext(), mesh_uniforms_data_size, false, true);
        frame.final_pass.cube.uniforms_buffer_ptr->SetName(IndexedName("Cube Uniforms Buffer for Final Pass", frame.index));

        // Create uniforms buffer for Floor rendering in Final pass
        frame.final_pass.floor.uniforms_buffer_ptr = gfx::Buffer::CreateConstantBuffer(GetRenderContext(), mesh_uniforms_data_size, false, true);
        frame.final_pass.floor.uniforms_buffer_ptr->SetName(IndexedName("Floor Uniforms Buffer for Final Pass", frame.index));

        // Final-pass resource bindings for cube rendering
        frame.final_pass.cube.program_bindings_ptr = gfx::ProgramBindings::Create(final_state_settings.program_ptr, {
            { { gfx::Shader::Type::Vertex, "g_mesh_uniforms"  }, { { *frame.final_pass.cube.uniforms_buffer_ptr  } } },
            { { gfx::Shader::Type::Pixel,  "g_scene_uniforms" }, { { *frame.scene_uniforms_buffer_ptr            } } },
            { { gfx::Shader::Type::Pixel,  "g_constants"      }, { { *m_const_buffer_ptr                         } } },
            { { gfx::Shader::Type::Pixel,  "g_shadow_map"     }, { { *frame.shadow_pass.rt_texture_ptr           } } },
            { { gfx::Shader::Type::Pixel,  "g_shadow_sampler" }, { { *m_shadow_sampler_ptr                       } } },
            { { gfx::Shader::Type::Pixel,  "g_texture"        }, { { m_cube_buffers_ptr->GetTexture()            } } },
            { { gfx::Shader::Type::Pixel,  "g_texture_sampler"}, { { *m_texture_sampler_ptr                      } } },
        }, frame.index);
        frame.final_pass.cube.program_bindings_ptr->SetName(IndexedName("Cube Final-Pass Bindings {}", frame.index));

        // Final-pass resource bindings for floor rendering - patched a copy of cube bindings
        frame.final_pass.floor.program_bindings_ptr = gfx::ProgramBindings::CreateCopy(*frame.final_pass.cube.program_bindings_ptr, {
            { { gfx::Shader::Type::Vertex, "g_mesh_uniforms"  }, { { *frame.final_pass.floor.uniforms_buffer_ptr } } },
            { { gfx::Shader::Type::Pixel,  "g_texture"        }, { { m_floor_buffers_ptr->GetTexture()           } } },
        }, frame.index);
        frame.final_pass.floor.program_bindings_ptr->SetName(IndexedName("Floor Final-Pass Bindings {}", frame.index));

        // Bind final pass RT texture and pass to the frame buffer texture and final pass.
        frame.final_pass.rt_texture_ptr  = frame.screen_texture_ptr;
        frame.final_pass.render_pass_ptr = frame.screen_pass_ptr;
        
        // Create render pass and command list for final pass rendering
        frame.final_pass.cmd_list_ptr = gfx::RenderCommandList::Create(GetRenderContext().GetRenderCommandKit().GetQueue(), *frame.final_pass.render_pass_ptr);
        frame.final_pass.cmd_list_ptr->SetName(IndexedName("Final Scene Rendering", frame.index));

        // Rendering command lists sequence
        frame.execute_cmd_list_set_ptr = gfx::CommandListSet::Create({
            *frame.shadow_pass.cmd_list_ptr,
            *frame.final_pass.cmd_list_ptr
        }, frame.index);
    }

    UserInterfaceApp::CompleteInitialization();
}

bool ShadowCubeApp::Resize(const gfx::FrameSize& frame_size, bool is_minimized)
{
    // Resize screen color and depth textures
    for (ShadowCubeFrame& frame : GetFrames())
        frame.final_pass.rt_texture_ptr.reset();

    const bool is_resized = UserInterfaceApp::Resize(frame_size, is_minimized);

    for (ShadowCubeFrame& frame : GetFrames())
        frame.final_pass.rt_texture_ptr = frame.screen_texture_ptr;
    
    if (!is_resized)
        return false;

    m_view_camera.Resize(frame_size);
    return true;
}

bool ShadowCubeApp::Animate(double, double delta_seconds)
{
    m_view_camera.Rotate(m_view_camera.GetOrientation().up, static_cast<float>(delta_seconds * 360.F / 8.F));
    m_light_camera.Rotate(m_light_camera.GetOrientation().up, static_cast<float>(delta_seconds * 360.F / 4.F));
    return true;
}

bool ShadowCubeApp::Update()
{
    if (!UserInterfaceApp::Update())
        return false;

    // Prepare homogenous [-1,1] to texture [0,1] coordinates transformation matrix
    static const hlslpp::float4x4 s_homogen_to_texture_coords_matrix = hlslpp::mul(hlslpp::float4x4::scale(0.5F, -0.5F, 1.F), hlslpp::float4x4::translation(0.5F, 0.5F, 0.F));

    // Update scene uniforms
    m_scene_uniforms.eye_position    = hlslpp::float4(m_view_camera.GetOrientation().eye, 1.F);
    m_scene_uniforms.light_position  = m_light_camera.GetOrientation().eye;

    hlslpp::float4x4 scale_matrix = hlslpp::float4x4::scale(m_scene_scale);

    // Cube model matrix
    hlslpp::float4x4 cube_model_matrix = hlslpp::mul(hlslpp::float4x4::translation(0.F, 0.5F, 0.F), scale_matrix); // move up by half of cube model height

    // Update Cube uniforms
    m_cube_buffers_ptr->SetFinalPassUniforms(hlslpp::MeshUniforms{
        hlslpp::transpose(cube_model_matrix),
        hlslpp::transpose(hlslpp::mul(cube_model_matrix, m_view_camera.GetViewProjMatrix())),
        hlslpp::transpose(hlslpp::mul(hlslpp::mul(cube_model_matrix, m_light_camera.GetViewProjMatrix()), s_homogen_to_texture_coords_matrix))
    });
    m_cube_buffers_ptr->SetShadowPassUniforms(hlslpp::MeshUniforms{
        hlslpp::transpose(cube_model_matrix),
        hlslpp::transpose(hlslpp::mul(cube_model_matrix, m_light_camera.GetViewProjMatrix())),
        hlslpp::float4x4()
    });

    // Update Floor uniforms
    m_floor_buffers_ptr->SetFinalPassUniforms(hlslpp::MeshUniforms{
        hlslpp::transpose(scale_matrix),
        hlslpp::transpose(hlslpp::mul(scale_matrix, m_view_camera.GetViewProjMatrix())),
        hlslpp::transpose(hlslpp::mul(hlslpp::mul(scale_matrix, m_light_camera.GetViewProjMatrix()), s_homogen_to_texture_coords_matrix))
    });
    m_floor_buffers_ptr->SetShadowPassUniforms(hlslpp::MeshUniforms{
        hlslpp::transpose(scale_matrix),
        hlslpp::transpose(hlslpp::mul(scale_matrix, m_light_camera.GetViewProjMatrix())),
        hlslpp::float4x4()
    });
    
    return true;
}

bool ShadowCubeApp::Render()
{
    if (!UserInterfaceApp::Render())
        return false;

    // Upload uniform buffers to GPU
    const ShadowCubeFrame& frame = GetCurrentFrame();
    gfx::CommandQueue& render_cmd_queue = GetRenderContext().GetRenderCommandKit().GetQueue();
    frame.scene_uniforms_buffer_ptr->SetData(m_scene_uniforms_subresources, render_cmd_queue);
    frame.shadow_pass.floor.uniforms_buffer_ptr->SetData(m_floor_buffers_ptr->GetShadowPassUniformsSubresources(), render_cmd_queue);
    frame.shadow_pass.cube.uniforms_buffer_ptr->SetData(m_cube_buffers_ptr->GetShadowPassUniformsSubresources(), render_cmd_queue);
    frame.final_pass.floor.uniforms_buffer_ptr->SetData(m_floor_buffers_ptr->GetFinalPassUniformsSubresources(), render_cmd_queue);
    frame.final_pass.cube.uniforms_buffer_ptr->SetData(m_cube_buffers_ptr->GetFinalPassUniformsSubresources(), render_cmd_queue);

    // Record commands for shadow & final render passes
    RenderScene(m_shadow_pass, frame.shadow_pass);
    RenderScene(m_final_pass, frame.final_pass);

    // Execute rendering commands and present frame to screen
    render_cmd_queue.Execute(*frame.execute_cmd_list_set_ptr);
    GetRenderContext().Present();
    
    return true;
}

void ShadowCubeApp::RenderScene(const RenderPassState& render_pass, const ShadowCubeFrame::PassResources& render_pass_resources) const
{
    gfx::RenderCommandList& cmd_list = *render_pass_resources.cmd_list_ptr;

    // Reset command list with initial rendering state
    cmd_list.ResetWithState(*render_pass.render_state_ptr, render_pass.debug_group_ptr.get());
    cmd_list.SetViewState(*render_pass.view_state_ptr);

    // Draw scene with cube and floor
    m_cube_buffers_ptr->Draw(cmd_list, *render_pass_resources.cube.program_bindings_ptr);
    m_floor_buffers_ptr->Draw(cmd_list, *render_pass_resources.floor.program_bindings_ptr);

    if (render_pass.is_final_pass)
    {
        RenderOverlay(cmd_list);
    }

    cmd_list.Commit();
}

void ShadowCubeApp::OnContextReleased(gfx::Context& context)
{
    m_final_pass.Release();
    m_shadow_pass.Release();

    m_floor_buffers_ptr.reset();
    m_cube_buffers_ptr.reset();
    m_shadow_sampler_ptr.reset();
    m_texture_sampler_ptr.reset();
    m_const_buffer_ptr.reset();
    m_shadow_pass_pattern_ptr.reset();

    UserInterfaceApp::OnContextReleased(context);
}

ShadowCubeApp::RenderPassState::RenderPassState(bool is_final_pass, const std::string& debug_group_name)
    : is_final_pass(is_final_pass)
    , debug_group_ptr(META_DEBUG_GROUP_CREATE(debug_group_name)) // NOSONAR
{
    META_UNUSED(debug_group_name);
}

void ShadowCubeApp::RenderPassState::Release()
{
    render_state_ptr.reset();
    view_state_ptr.reset();
}

} // namespace Methane::Tutorials

int main(int argc, const char* argv[])
{
    return Methane::Tutorials::ShadowCubeApp().Run({ argc, argv });
}
