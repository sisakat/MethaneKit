/******************************************************************************

Copyright 2019-2021 Evgeny Gorodetskiy

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

FILE: HelloTriangleApp.cpp
Tutorial demonstrating colored triangle rendering with Methane graphics API

******************************************************************************/

#include <Methane/Kit.h>
#include <Methane/Graphics/App.hpp>
#include <Methane/Samples/AppSettings.hpp>

using namespace Methane;
using namespace Methane::Graphics;

struct HelloTriangleFrame final : AppFrame
{
    Ptr<RenderCommandList> render_cmd_list_ptr;
    Ptr<CommandListSet>    execute_cmd_list_set_ptr;
    using AppFrame::AppFrame;
};

using GraphicsApp = App<HelloTriangleFrame>;
class HelloTriangleApp final : public GraphicsApp // NOSONAR
{
private:
    Ptr<RenderState> m_render_state_ptr;

public:
    HelloTriangleApp()
        : GraphicsApp(
            []() {
                Graphics::AppSettings settings = Tutorials::GetGraphicsTutorialAppSettings("Methane Hello Triangle", Tutorials::g_default_app_options_color_only);
                settings.graphics_app.SetScreenPassAccess(RenderPass::Access::None);
                return settings;
            }(),
            "Tutorial demonstrating colored triangle rendering with Methane Kit.")
    { }

    ~HelloTriangleApp() override
    {
        WaitForRenderComplete();
    }

    void Init() override
    {
        GraphicsApp::Init();

        m_render_state_ptr = RenderState::Create(GetRenderContext(),
            RenderState::Settings
            {
                Program::Create(GetRenderContext(),
                    Program::Settings
                    {
                        Program::Shaders
                        {
                            Shader::CreateVertex(GetRenderContext(), { Data::ShaderProvider::Get(), { "HelloTriangle", "TriangleVS" } }),
                            Shader::CreatePixel(GetRenderContext(),  { Data::ShaderProvider::Get(), { "HelloTriangle", "TrianglePS" } }),
                        },
                        Program::InputBufferLayouts{ },
                        Program::ArgumentAccessors{ },
                        GetScreenRenderPattern().GetAttachmentFormats()
                    }
                ),
                GetScreenRenderPatternPtr()
            }
        );
        m_render_state_ptr->SetName("Triangle Render State");

        for (HelloTriangleFrame& frame : GetFrames())
        {
            frame.render_cmd_list_ptr      = RenderCommandList::Create(GetRenderContext().GetRenderCommandKit().GetQueue(), *frame.screen_pass_ptr);
            frame.render_cmd_list_ptr->SetName(IndexedName("Render Triangle", frame.index));
            frame.execute_cmd_list_set_ptr = CommandListSet::Create({ *frame.render_cmd_list_ptr }, frame.index);
        }

        GraphicsApp::CompleteInitialization();
    }

    bool Render() override
    {
        if (!GraphicsApp::Render())
            return false;

        const HelloTriangleFrame& frame = GetCurrentFrame();
        frame.render_cmd_list_ptr->ResetWithState(*m_render_state_ptr);
        frame.render_cmd_list_ptr->SetViewState(GetViewState());
        frame.render_cmd_list_ptr->Draw(RenderCommandList::Primitive::Triangle, 3);
        frame.render_cmd_list_ptr->Commit();

        GetRenderContext().GetRenderCommandKit().GetQueue().Execute(*frame.execute_cmd_list_set_ptr);
        GetRenderContext().Present();

        return true;
    }

    void OnContextReleased(Context& context) override
    {
        m_render_state_ptr.reset();

        GraphicsApp::OnContextReleased(context);
    }
};

int main(int argc, const char* argv[])
{
    return HelloTriangleApp().Run({ argc, argv });
}
