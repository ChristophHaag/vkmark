/*
 * Copyright © 2017 Collabora Ltd.
 *
 * This file is part of vkmark.
 *
 * vkmark is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation, either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * vkmark is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with vkmark. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *   Alexandros Frantzis <alexandros.frantzis@collabora.com>
 */

#include "copy_buffer.h"

#include "one_time_command_buffer.h"

void vkutil::copy_buffer(
    VulkanState& vulkan,
    vk::Buffer src,
    vk::Buffer dst,
    vk::DeviceSize size)
{
    OneTimeCommandBuffer otcb{vulkan};

    auto const region = vk::BufferCopy{}.setSize(size);
    otcb.command_buffer().copyBuffer(src, dst, region);

    otcb.submit();
}
