/*
 * Copyright (C) 2015, 2019, 2020, 2022  T+A elektroakustik GmbH & Co. KG
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

#if HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include "mock_devices_os.hh"

MockDevicesOs::Mock *MockDevicesOs::singleton = nullptr;


bool Devices::get_device_information(const std::string &devlink, Devices::DeviceInfo &info)
{
    return MockDevicesOs::singleton->check_next<MockDevicesOs::GetDeviceInformation>(devlink, std::ref(info));
}

bool Devices::get_volume_information(const std::string &devname, Devices::VolumeInfo &info)
{
    return MockDevicesOs::singleton->check_next<MockDevicesOs::GetVolumeInformation>(devname, std::ref(info));
}

std::pair<std::string, std::string>
Devices::map_mountpoint_path_to_device_links(const char *mountpoint_path)
{
    return MockDevicesOs::singleton->check_next<MockDevicesOs::MapMountpointPathToDeviceLinks>(mountpoint_path);
}
