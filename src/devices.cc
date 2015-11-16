/*
 * Copyright (C) 2015  T+A elektroakustik GmbH & Co. KG
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

#if HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <algorithm>
#include <cstring>

#include "devices.hh"
#include "devices_os.h"
#include "messages.h"

Devices::Device::~Device()
{
    for(auto vol : volumes_)
        delete vol.second;

    volumes_.clear();
}

Devices::Volume *
Devices::Device::lookup_volume_by_devname(const char *devname) const
{
    log_assert(devname != nullptr);

    const auto &vol =
        std::find_if(volumes_.begin(), volumes_.end(),
            [&devname] (const decltype(volumes_)::value_type &it) -> bool
            {
                return strcmp(it.second->get_device_name().c_str(), devname) == 0;
            });

    return (vol != volumes_.end()) ? vol->second : nullptr;
}

bool Devices::Device::add_volume(Devices::Volume &volume)
{
    auto result = volumes_.insert(std::make_pair(volume.get_index(), &volume));

    if(!result.second)
        BUG("Insertion of volume failed");

    return result.second;
}

void Devices::Device::probe()
{
    if(state_ == SYNTHETIC)
        return do_probe();
}

void Devices::Device::do_probe()
{
    log_assert(state_ == SYNTHETIC);

    struct osdev_device_info devinfo;

    const char *name = strrchr(devlink_name_.c_str(), '/');
    device_name_ = (name != nullptr) ? (name + 1) : devlink_name_.c_str();

    if(device_name_.empty() ||
       !osdev_get_device_information(devlink_name_.c_str(), &devinfo))
    {
        state_ = BROKEN;
        return;
    }

    switch(devinfo.type)
    {
      case OSDEV_DEVICE_TYPE_UNKNOWN:
        state_ = BROKEN;
        break;

      case OSDEV_DEVICE_TYPE_USB:
        root_hub_id_ = Devices::USBHubID(devinfo.usb.hub_id);
        hub_port_ = devinfo.usb.port;
        state_ = PROBED;
        break;
    }

    osdev_free_device_information(&devinfo);
}
