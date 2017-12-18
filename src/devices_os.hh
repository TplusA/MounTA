/*
 * Copyright (C) 2015, 2017  T+A elektroakustik GmbH & Co. KG
 *
 * This file is part of MounTA.
 *
 * MounTA is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 3 as
 * published by the Free Software Foundation.
 *
 * MounTA is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with MounTA.  If not, see <http://www.gnu.org/licenses/>.
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

    struct
    {
        unsigned int hub_id;
        unsigned int port;
    }
    usb;
};

struct VolumeInfo
{
    int idx;
    std::string label;
    std::string fstype;
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
