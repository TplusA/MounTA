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
#ifndef DEVICE_MANAGER_HH
#define DEVICE_MANAGER_HH

#include "devices.hh"

namespace Devices
{

/*!
 * All mass storage devices known by the daemon.
 */
class AllDevices
{
  private:
    std::map<ID::value_type, Device *> devices_;

  public:
    AllDevices(const AllDevices &) = delete;
    AllDevices &operator=(const AllDevices &) = delete;

    explicit AllDevices() {}

    const Device *new_entry(const char *devname, const Volume **volume);
    void remove_entry(const char *devname);

    decltype(devices_)::const_iterator begin() { return devices_.begin(); };
    decltype(devices_)::const_iterator end()   { return devices_.end(); };
};

}

#endif /* !DEVICE_MANAGER_HH */
