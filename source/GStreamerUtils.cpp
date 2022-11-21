/*
 * Copyright (C) 2022 Sky UK
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "GStreamerUtils.h"

GstMappedBuffer::GstMappedBuffer(GstBuffer *buffer, GstMapFlags flags)
    : m_buffer(buffer), m_isMapped(gst_buffer_map(m_buffer, &m_info, flags))
{
}

GstMappedBuffer::~GstMappedBuffer()
{
    if (m_isMapped)
    {
        gst_buffer_unmap(m_buffer, &m_info);
    }
}

uint8_t *GstMappedBuffer::data()
{
    if (m_isMapped)
    {
        return static_cast<uint8_t *>(m_info.data);
    }
    else
    {
        return nullptr;
    }
}

size_t GstMappedBuffer::size() const
{
    if (m_isMapped)
    {
        return static_cast<size_t>(m_info.size);
    }
    else
    {
        return 0;
    }
}

GstMappedBuffer::operator bool() const
{
    return m_isMapped;
}