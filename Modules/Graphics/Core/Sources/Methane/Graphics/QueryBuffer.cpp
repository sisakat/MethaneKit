/******************************************************************************

Copyright 2020 Evgeny Gorodetskiy

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

FILE: Methane/Graphics/QueryBuffer.cpp
GPU data query buffer base implementation.

******************************************************************************/

#include "QueryBuffer.h"
#include "CommandQueueBase.h"

#include <Methane/Graphics/ContextBase.h>
#include <Methane/Graphics/RenderContext.h>
#include <Methane/Data/RangeUtils.hpp>
#include <Methane/Instrumentation.h>
#include <Methane/Checks.hpp>

#include <magic_enum.hpp>

namespace Methane::Graphics
{

Query::Query(QueryBuffer& buffer, CommandListBase& command_list, Data::Index index, Range data_range)
    : m_buffer_ptr(buffer.GetPtr())
    , m_command_list(command_list)
    , m_index(index)
    , m_data_range(data_range)
{
    META_FUNCTION_TASK();
}

Query::~Query()
{
    META_FUNCTION_TASK();
    try
    {
        m_buffer_ptr->ReleaseQuery(*this);
    }
    catch(const std::exception& e)
    {
        META_UNUSED(e);
        META_LOG("WARNING: Unexpected error during Query destruction: {}", e.what());
        assert(false);
    }
}

void Query::Begin()
{
    META_FUNCTION_TASK();
    const QueryBuffer::Type query_buffer_type = GetQueryBuffer().GetType();
    META_CHECK_ARG_NOT_EQUAL_DESCR(query_buffer_type, QueryBuffer::Type::Timestamp, "timestamp query can not be begun, it can be ended only");
    META_CHECK_ARG_NOT_EQUAL_DESCR(m_state, State::Begun, "can not begin unresolved or not ended query");
    m_state = State::Begun;
}

void Query::End()
{
    META_FUNCTION_TASK();
    const QueryBuffer::Type query_buffer_type = GetQueryBuffer().GetType();
    META_UNUSED(query_buffer_type);
    META_CHECK_ARG_DESCR(m_state, query_buffer_type == QueryBuffer::Type::Timestamp || m_state == State::Begun,
                         "can not end {} query that was not begun", magic_enum::enum_name(query_buffer_type));
    m_state = State::Ended;
}

void Query::ResolveData()
{
    META_FUNCTION_TASK();
    META_CHECK_ARG_EQUAL_DESCR(m_state, State::Ended, "can not resolve data of not ended query");
    m_state = State::Resolved;
}

QueryBuffer::QueryBuffer(CommandQueueBase& command_queue, Type type,
                         Query::Count max_query_count, Query::Count slots_count_per_query,
                         Data::Size buffer_size, Data::Size query_size)
    : m_type(type)
    , m_buffer_size(buffer_size)
    , m_query_size(query_size)
    , m_slots_count_per_query(slots_count_per_query)
    , m_free_indices({ { 0U, max_query_count * slots_count_per_query } })
    , m_free_data_ranges({ { 0U, buffer_size } })
    , m_command_queue(command_queue)
    , m_context(dynamic_cast<const Context&>(command_queue.GetContext()))
{
    META_FUNCTION_TASK();
}

void QueryBuffer::ReleaseQuery(const Query& query)
{
    META_FUNCTION_TASK();
    m_free_indices.Add({ query.GetIndex(), query.GetIndex() + 1 });
    m_free_data_ranges.Add(query.GetDataRange());
}

QueryBuffer::CreateQueryArgs QueryBuffer::GetCreateQueryArguments()
{
    META_FUNCTION_TASK();
    const Data::Range<Data::Index> index_range = Data::ReserveRange(m_free_indices, m_slots_count_per_query);
    META_CHECK_ARG_DESCR(index_range, !index_range.IsEmpty(), "maximum queries count is reached");

    const Query::Range data_range = Data::ReserveRange(m_free_data_ranges, m_query_size);
    META_CHECK_ARG_DESCR(data_range, !data_range.IsEmpty(), "there is no space available for new query");

    return { index_range.GetStart(), data_range };
}

TimeDelta TimestampQueryBuffer::GetGpuTimeOffset() const noexcept
{
    META_FUNCTION_TASK();
    return static_cast<TimeDelta>(m_calibrated_timestamps.gpu_ts) - static_cast<TimeDelta>(m_calibrated_timestamps.cpu_ts);
}

void TimestampQueryBuffer::SetGpuFrequency(Frequency gpu_frequency)
{
    META_FUNCTION_TASK();
    m_gpu_frequency = gpu_frequency;
}

void TimestampQueryBuffer::SetCalibratedTimestamps(const CalibratedTimestamps& calibrated_timestamps)
{
    META_FUNCTION_TASK();
    m_calibrated_timestamps = calibrated_timestamps;
}

} // namespace Methane::Graphics