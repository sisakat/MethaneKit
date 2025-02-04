/******************************************************************************

Copyright 2019-2022 Evgeny Gorodetskiy

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

FILE: Methane/Graphics/Vulkan/BufferVK.h
Vulkan implementation of the buffer interface.

******************************************************************************/

#pragma once

#include "ResourceVK.hpp"

#include <Methane/Graphics/BufferBase.h>
#include <Methane/Graphics/Types.h>

#include <vulkan/vulkan.hpp>

namespace Methane::Graphics
{

class BufferVK final // NOSONAR - inheritance hierarchy is greater than 5
    : public ResourceVK<BufferBase, vk::Buffer, true>
{
public:
    BufferVK(const ContextBase& context, const Settings& settings);

    // Resource interface
    void SetData(const SubResources& sub_resources, CommandQueue& target_cmd_queue) override;

    // Object interface
    bool SetName(const std::string& name) override;

protected:
    // ResourceVK override
    Ptr<ResourceViewVK::ViewDescriptorVariant> CreateNativeViewDescriptor(const View::Id& view_id) override;

private:
    vk::UniqueBuffer            m_vk_unique_staging_buffer;
    vk::UniqueDeviceMemory      m_vk_unique_staging_memory;
    std::vector<vk::BufferCopy> m_vk_copy_regions;
};

class BufferSetVK final : public BufferSetBase
{
public:
    BufferSetVK(Buffer::Type buffers_type, const Refs<Buffer>& buffer_refs);

    const std::vector<vk::Buffer>&     GetNativeBuffers() const noexcept { return m_vk_buffers; }
    const std::vector<vk::DeviceSize>& GetNativeOffsets() const noexcept { return m_vk_offsets; }

private:
    std::vector<vk::Buffer>     m_vk_buffers;
    std::vector<vk::DeviceSize> m_vk_offsets;
};

} // namespace Methane::Graphics
