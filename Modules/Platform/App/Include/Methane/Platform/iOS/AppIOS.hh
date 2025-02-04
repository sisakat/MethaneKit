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

FILE: Methane/Platform/iOS/AppIOS.h
iOS application implementation.

******************************************************************************/

#pragma once

#include <Methane/Platform/AppBase.h>
#include <Methane/Platform/iOS/AppEnvironment.hh>

#if defined(__OBJC__) && defined(METHANE_RENDER_APP)

#else

using UIWindow = void;

#endif

namespace Methane::Platform
{

class AppIOS : public AppBase
{
public:
    static AppIOS* GetInstance();
    
    explicit AppIOS(const AppBase::Settings& settings);

    // AppBase interface
    void InitContext(const Platform::AppEnvironment& env, const Data::FrameSize& frame_size) override;
    int  Run(const RunArgs& args) override;
    void Alert(const Message& msg, bool deferred = false) override;
    void SetWindowTitle(const std::string& title_text) override;
    bool SetFullScreen(bool is_full_screen) override;
    float GetContentScalingFactor() const override;
    uint32_t GetFontResolutionDpi() const override;
    void Close() override;

    void SetWindow(UIWindow* ns_window);
    bool SetFullScreenInternal(bool is_full_screen) { return AppBase::SetFullScreen(is_full_screen); }
    UIWindow* GetWindow() { return m_ns_window; }

protected:
    // AppBase interface
    void ShowAlert(const Message& msg) override;

private:
    UIWindow* m_ns_window = nullptr;

    static AppIOS* s_instance_ptr;
};

} // namespace Methane::Platform
