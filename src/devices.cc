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
