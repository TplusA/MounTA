/*
 * Copyright (C) 2015, 2017, 2019, 2020, 2022  T+A elektroakustik GmbH & Co. KG
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

#ifndef DEVICE_MANAGER_HH
#define DEVICE_MANAGER_HH

#include <stdexcept>
#include <functional>
#include <unordered_map>

#include "devices.hh"
#include "devices_os.hh"

namespace Automounter { class ExternalTools; }

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
  public:
    using DevContainerType = std::map<ID::value_type, std::shared_ptr<Devices::Device>>;

  private:
    DevContainerType devices_;
    const Automounter::ExternalTools &tools_;
    const std::string symlink_directory_;
    std::unordered_map<std::string, std::string> volume_device_for_mountpoint_;

  public:
    AllDevices(const AllDevices &) = delete;
    AllDevices &operator=(const AllDevices &) = delete;
    AllDevices(AllDevices &&) = default;

    explicit AllDevices(const Automounter::ExternalTools &tools, const std::string& symlink_directory):
        tools_(tools),
        symlink_directory_(symlink_directory)
    {}

    ~AllDevices();

    std::shared_ptr<Device> new_entry(const char *devlink, Volume *&volume,
                                      bool &have_probed_containing_device);

    std::shared_ptr<const Device> new_entry(const char *devlink,
                                            const Volume *&volume,
                                            bool &have_probed_containing_device)
    {
        return new_entry(devlink, const_cast<Devices::Volume *&>(volume),
                         have_probed_containing_device);
    }

    std::shared_ptr<Device> new_entry_by_mountpoint(const char *mountpoint_path,
                                                    Volume *&volume);
    std::string take_volume_device_for_mountpoint(const char *mountpoint_path);

    bool remove_entry(const char *devlink,
                      const std::function<void(const Device &)> &after_removal_notification = nullptr,
                      const std::function<void(const Device &)> &before_removal_notification = nullptr);
    bool remove_entry(Devices::AllDevices::DevContainerType::const_iterator devices_iter,
                      const std::function<void(const Device &)> &after_removal_notification = nullptr,
                      const std::function<void(const Device &)> &before_removal_notification = nullptr);

    decltype(devices_)::const_iterator begin() const { return devices_.begin(); };
    decltype(devices_)::const_iterator end() const   { return devices_.end(); };
    size_t get_number_of_devices() const             { return devices_.size(); }

  private:
    std::shared_ptr<Device> add_or_get_device(const char *devlink,
                                              const std::string &devname,
                                              VolumeInfo &volinfo,
                                              bool &have_info,
                                              bool &have_probed_containing_device);

    std::shared_ptr<Device> find_root_device(const char *devlink);
    std::shared_ptr<Device> get_device_by_devlink(const char *devlink);

    std::pair<std::shared_ptr<Devices::Device>, Devices::Volume *>
    add_or_get_volume(std::shared_ptr<Device> device,
                      const char *devlink, const std::string &devname,
                      const VolumeInfo &volinfo);
};

}

#endif /* !DEVICE_MANAGER_HH */
