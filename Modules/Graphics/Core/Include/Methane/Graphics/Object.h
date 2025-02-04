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

FILE: Methane/Graphics/Object.h
Methane object interface: represents any named object.

******************************************************************************/

#pragma once

#include <Methane/Memory.hpp>
#include <Methane/Data/IEmitter.h>

#include <string>
#include <stdexcept>

namespace Methane::Graphics
{

struct Object;

struct IObjectCallback
{
    virtual void OnObjectNameChanged(Object&, const std::string& /*old_name*/) { /* does nothing by default */ }
    virtual void OnObjectDestroyed(Object&)                                    { /* does nothing by default */ }

    virtual ~IObjectCallback() = default;
};

struct Object
    : virtual Data::IEmitter<IObjectCallback> // NOSONAR
{
    struct Registry
    {
        class NameConflictException : public std::invalid_argument
        {
        public:
            explicit NameConflictException(const std::string& name);
        };

        virtual void AddGraphicsObject(Object& object) = 0;
        virtual void RemoveGraphicsObject(Object& object) = 0;
        [[nodiscard]] virtual Ptr<Object> GetGraphicsObject(const std::string& object_name) const noexcept = 0;
        [[nodiscard]] virtual bool        HasGraphicsObject(const std::string& object_name) const noexcept = 0;

        virtual ~Registry() = default;
    };

    // Object interface
    virtual bool SetName(const std::string& name) = 0;
    [[nodiscard]] virtual const std::string& GetName() const noexcept = 0;
    [[nodiscard]] virtual Ptr<Object>        GetPtr() = 0;
};

} // namespace Methane::Graphics
