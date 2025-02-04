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

FILE: Methane/Platform/AppView.h
Methane application view used both by RenderContext in Core API
and by Methane App implementations.

******************************************************************************/

#pragma once

#ifdef __OBJC__

#ifdef APPLE_MACOS
#import "MacOS/AppViewMT.hh"
#else
#import "iOS/AppViewMT.hh"
#endif

#endif // __OBJC__

namespace Methane::Platform
{

#ifdef __OBJC__

using NativeAppView = AppViewMT;
using NativeAppViewPtr = NativeAppView* _Nonnull;

#else // __OBJC__

using NativeAppView = uint8_t;
using NativeAppViewPtr = NativeAppView*;

#endif // __OBJC__

struct AppView
{
    NativeAppViewPtr p_native_view;
};

} // namespace Methane::Platform
