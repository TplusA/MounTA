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

#ifndef DEVICE_MANAGER_HH
#define DEVICE_MANAGER_HH

#include <functional>

#include "devices.hh"
#include "devices_os.h"

namespace Devices
{

class Exception: public std::runtime_error
{
  public:
    explicit Exception(const std::string &what_arg): std::runtime_error(what_arg) {}
    virtual ~Exception() {}
};

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
    AllDevices(AllDevices &&) = default;

    explicit AllDevices() {}
    ~AllDevices();

    Device *new_entry(const char *devlink, Volume *&volume,
                      bool &have_probed_containing_device);

    const Device *new_entry(const char *devlink, const Volume *&volume,
                            bool &have_probed_containing_device)
    {
        return new_entry(devlink, const_cast<Devices::Volume *&>(volume),
                         have_probed_containing_device);
    }

    using RemoveDeviceCallback = std::function<void(Devices::Device &)>;
    using RemoveVolumeCallback = std::function<void(Devices::Volume &)>;

    bool remove_entry(const char *devlink,
                      const RemoveDeviceCallback &remove_device,
                      const RemoveVolumeCallback &remove_volume);

    bool remove_entry(decltype(devices_)::const_iterator devices_iter,
                      const RemoveDeviceCallback &remove_device,
                      const RemoveVolumeCallback &remove_volume);

    decltype(devices_)::const_iterator begin() const { return devices_.begin(); };
    decltype(devices_)::const_iterator end() const   { return devices_.end(); };
    size_t get_number_of_devices() const             { return devices_.size(); }

  private:
    Device *add_or_get_device(const char *devlink, const char *devname,
                              struct osdev_volume_info &volinfo,
                              bool &have_info,
                              bool &have_probed_containing_device);

    Device *find_root_device(const char *devlink);
    Device *get_device_by_devlink(const char *devlink);

    std::pair<Devices::Device *, Devices::Volume *>
    add_or_get_volume(Device *device, const char *devlink, const char *devname,
                      const struct osdev_volume_info &volinfo);
};

}

#endif /* !DEVICE_MANAGER_HH */
