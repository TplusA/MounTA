/*
 * Copyright (C) 2015, 2017, 2019--2021  T+A elektroakustik GmbH & Co. KG
 *
 * This file is part of MounTA.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA  02110-1301, USA.
 */

#ifndef DEVICES_OS_H
#define DEVICES_OS_H

#include <string>

namespace Automounter { class ExternalTools; }

namespace Devices
{

enum class DeviceType
{
    UNKNOWN,
    USB,
};

struct DeviceInfo
{
    DeviceType type;
    std::string device_uuid;
    std::string usb_port_sysfs_name;

    DeviceInfo(const DeviceInfo &) = delete;
    DeviceInfo(DeviceInfo &&) = default;
    DeviceInfo &operator=(const DeviceInfo &) = default;
    DeviceInfo &operator=(DeviceInfo &&) = default;

    DeviceInfo(): type(DeviceType::UNKNOWN) {}

    explicit DeviceInfo(std::string &&uuid, std::string &&name):
        type(DeviceType::USB),
        device_uuid(std::move(uuid)),
        usb_port_sysfs_name(std::move(name))
    {}
};

struct VolumeInfo
{
    int idx;
    std::string volume_uuid;
    std::string label;
    std::string fstype;

    VolumeInfo(const VolumeInfo &) = delete;
    VolumeInfo(VolumeInfo &&) = default;
    VolumeInfo &operator=(const VolumeInfo &) = default;
    VolumeInfo &operator=(VolumeInfo &&) = default;

    explicit VolumeInfo(): idx(-1) {}

    explicit VolumeInfo(int vol_idx, std::string &&uuid,
                        std::string &&vol_label, std::string &&vol_fstype):
        idx(vol_idx),
        volume_uuid(std::move(uuid)),
        label(std::move(vol_label)),
        fstype(std::move(vol_fstype))
    {}
};

/*!
 * Pass tool configuration.
 */
void init(const Automounter::ExternalTools &tools);

/*!
 * Get device information if possible.
 *
 * \param devlink
 *     Name of the device symlink.
 *
 * \param[out] devinfo
 *     Information about the device with given name.
 *
 * \returns
 *     True on success, false on error.
 */
bool get_device_information(const std::string &devlink, Devices::DeviceInfo &devinfo);

/*!
 * Get volume information if possible.
 *
 * \param devname
 *     Name of a block device.
 *
 * \param[out] volinfo
 *     Information about the volume stored on the given block device.
 *
 * \returns
 *     True if the given device contains a volume that could possibly be
 *     mounted, false if it doesn't (or on error).
 */
bool get_volume_information(const std::string &devname, Devices::VolumeInfo &volinfo);

}

#endif /* !DEVICES_OS_H */
