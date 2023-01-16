/*
 * Copyright (C) 2015, 2017, 2019  T+A elektroakustik GmbH & Co. KG
 * Copyright (C) 2021--2023  T+A elektroakustik GmbH & Co. KG
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

#include <cstring>
#include <memory>
#include <functional>
#include <algorithm>
#include <type_traits>

#include "device_manager.hh"
#include "devices_util.h"
#include "devices_os.hh"
#include "messages.h"

Devices::AllDevices::~AllDevices()
{
    devices_.clear();
}

Devices::ID::value_type Devices::ID::next_free_id_;

struct DevnameWithVolumeNumber
{
    std::unique_ptr<char, decltype(std::free) *> devname_mem_;
    const char *devname_;
    int volume_number_;

    explicit DevnameWithVolumeNumber():
        devname_mem_{nullptr, std::free},
        devname_(nullptr),
        volume_number_(-1)
    {}
};

/*!
 * Get device name and volume number from device link.
 */
static bool get_devname_with_volume_number(DevnameWithVolumeNumber *data,
                                           const char *devlink)
{
    data->devname_mem_.reset(os_resolve_symlink(devlink));
    data->devname_ = (data->devname_mem_ != nullptr) ? data->devname_mem_.get() : devlink;
    data->volume_number_ = devname_get_volume_number(data->devname_);

    return data->volume_number_ >= 0;
}

static bool is_link_to_partition(const char *devlink_hyphen)
{
    if(devlink_hyphen == nullptr)
        return false;

    static constexpr char part[] = "part";
    if(strncmp(devlink_hyphen + 1, part, sizeof(part) - 1) != 0)
        return false;

    const char *temp = devlink_hyphen + sizeof(part);

    if(*temp == '\0')
        return false;

    for(/* nothing */; *temp != '\0'; ++temp)
    {
        if(!isdigit(*temp))
            return false;
    }

    return true;
}

const std::string mk_root_devlink_name(const char *devlink)
{
    msg_log_assert(devlink != nullptr);
    msg_log_assert(devlink[0] != '\0');

    const char *hyphen = strrchr(devlink, '-');

    if(is_link_to_partition(hyphen))
        return std::string(devlink, 0, hyphen - devlink);

    msg_error(EINVAL, LOG_ERR,
              "Malformed device link name \"%s\"", devlink);

    return "";
}

static inline Devices::AllDevices::DevContainerType::iterator
get_device_iter_by_devlink(Devices::AllDevices::DevContainerType &devices,
                           const char *devlink)
{
    return std::find_if(devices.begin(), devices.end(),
        [&devlink] (Devices::AllDevices::DevContainerType::value_type &it)
        {
            return it.second->get_devlink_name() == devlink;
        });
}

std::shared_ptr<Devices::Device> Devices::AllDevices::find_root_device(const char *devlink)
{
    const std::string temp(mk_root_devlink_name(devlink));
    const auto &dev = get_device_iter_by_devlink(devices_, temp.c_str());
    return (dev != devices_.end()) ? dev->second : nullptr;
}

std::shared_ptr<Devices::Device>
Devices::AllDevices::new_entry(const char *devlink, Devices::Volume *&volume,
                               bool &have_probed_containing_device)
{
    msg_log_assert(devlink != nullptr);

    volume = nullptr;
    have_probed_containing_device = false;

    DevnameWithVolumeNumber data;
    if(!get_devname_with_volume_number(&data, devlink))
        return nullptr;

    VolumeInfo volinfo;
    bool have_volume_info = false;

    std::shared_ptr<Device> device =
        (data.volume_number_ == 0)
        ? add_or_get_device(devlink, data.devname_, volinfo, have_volume_info,
                            have_probed_containing_device)
        : find_root_device(devlink);

    if(data.volume_number_ > 0 || have_volume_info)
    {
        if(!have_volume_info)
            have_volume_info = get_volume_information(data.devname_, volinfo);

        if(have_volume_info)
        {
            auto device_and_volume = add_or_get_volume(device, devlink, data.devname_, volinfo);

            msg_log_assert(device_and_volume.first == device ||
                           (device == nullptr && device_and_volume.first != nullptr));

            volume = device_and_volume.second;

            if(device == nullptr)
                device = device_and_volume.first;
        }
    }
    else if(device != nullptr && data.volume_number_ == 0)
    {
        if(!have_probed_containing_device)
            have_probed_containing_device = device->probe();

        volume = device->lookup_volume_by_devname(data.devname_);
    }

    return device;
}

std::shared_ptr<Devices::Device>
Devices::AllDevices::new_entry_by_mountpoint(const char *mountpoint_path,
                                             Devices::Volume *&volume)
{
    {
        Automounter::Mountpoint mp(tools_, mountpoint_path);
        if(!mp.probe(false))
        {
            msg_error(EINVAL, LOG_ERR, "Not a mountpoint: %s", mountpoint_path);
            return nullptr;
        }
    }

    const auto devlinks(map_mountpoint_path_to_device_links(mountpoint_path));

    if(devlinks.first.empty() || devlinks.second.empty())
    {
        msg_error(EINVAL, LOG_ERR,
                  "Failed mapping mountpoint %s to device links", mountpoint_path);
        return nullptr;
    }

    bool dummy;
    if(new_entry(devlinks.first.c_str(), volume, dummy) == nullptr)
        return nullptr;

    auto result = new_entry(devlinks.second.c_str(), volume, dummy);

    if(result != nullptr)
        volume_device_for_mountpoint_[mountpoint_path] = devlinks.first;

    return result;
}

std::string Devices::AllDevices::take_volume_device_for_mountpoint(const char *mountpoint_path)
{
    msg_log_assert(mountpoint_path != nullptr);

    std::string result;
    auto it(volume_device_for_mountpoint_.find(mountpoint_path));

    if(it != volume_device_for_mountpoint_.end())
    {
        result = std::move(it->second);
        volume_device_for_mountpoint_.erase(it);
    }

    MSG_BUG_IF(result.empty(), "Failed to map mountpoint to device name");
    return result;
}

bool Devices::AllDevices::remove_entry(const char *devlink,
                                       const std::function<void(const Device &)> &after_removal_notification,
                                       const std::function<void(const Device &)> &before_removal_notification)
{
    msg_log_assert(devlink != nullptr);
    return remove_entry(get_device_iter_by_devlink(devices_, devlink),
                        after_removal_notification,
                        before_removal_notification);
}

bool Devices::AllDevices::remove_entry(Devices::AllDevices::DevContainerType::const_iterator devices_iter,
                                       const std::function<void(const Device &)> &after_removal_notification,
                                       const std::function<void(const Device &)> &before_removal_notification)
{
    if(devices_iter == devices_.end())
    {
        /* react only on removal of whole devices */
        return false;
    }

    if(before_removal_notification)
        before_removal_notification(*devices_iter->second);

    devices_iter->second->drop_volumes();

    if(after_removal_notification)
        after_removal_notification(*devices_iter->second);

    devices_.erase(devices_iter);

    return true;
}

static std::shared_ptr<Devices::Device>
mk_device(Devices::AllDevices::DevContainerType &all_devices,
          const std::function<std::shared_ptr<Devices::Device>(const Devices::ID &device_id)> &alloc_device)
{
    std::shared_ptr<Devices::Device> device;

    while(true)
    {
        Devices::ID device_id;

        if(all_devices.find(device_id.value_) == all_devices.end())
        {
            device = alloc_device(device_id);
            break;
        }
    }

    if(device != nullptr)
    {
        auto result = all_devices.insert(std::make_pair(device->get_id(), device));

        if(!result.second)
        {
            MSG_BUG("Insertion of device failed");
            device = nullptr;
        }
    }
    else
        msg_out_of_memory("Device object");

    return device;
}

std::shared_ptr<Devices::Device>
Devices::AllDevices::get_device_by_devlink(const char *devlink)
{
    const auto &dev = get_device_iter_by_devlink(devices_, devlink);
    return (dev != devices_.end()) ? dev->second : nullptr;
}

std::shared_ptr<Devices::Device>
Devices::AllDevices::add_or_get_device(const char *devlink,
                                       const std::string &devname,
                                       VolumeInfo &volinfo,
                                       bool &have_info,
                                       bool &have_probed_containing_device)
{
    have_info = false;

    /* full device */
    auto dev = get_device_by_devlink(devlink);

    if(dev != nullptr)
    {
        msg_info("Device %s already registered", devlink);
        return dev;
    }

    /* maybe also add a volume for this device */
    have_info = get_volume_information(devname, volinfo);

    return mk_device(devices_,
        [this, &devlink, &have_probed_containing_device]
        (const ID &device_id)
        {
            auto d = std::make_shared<Device>(device_id, devlink, true);
            have_probed_containing_device = d->get_state() == Device::State::PROBED;
            return d;
        });
}

std::pair<std::shared_ptr<Devices::Device>, Devices::Volume *>
Devices::AllDevices::add_or_get_volume(std::shared_ptr<Devices::Device> device,
                                       const char *devlink,
                                       const std::string &devname,
                                       const VolumeInfo &volinfo)
{

    if(device == nullptr)
    {
        device = mk_device(devices_,
            [this, &devlink] (const ID &device_id)
            {
                return std::make_shared<Device>(device_id,
                                                mk_root_devlink_name(devlink),
                                                false);
            });
    }

    if(device == nullptr)
        return std::make_pair(nullptr, nullptr);

    auto *existing_volume = device->lookup_volume_by_devname(devname);

    if(existing_volume != nullptr)
    {
        msg_info("Volume %s already registered on device %s",
                 devlink, device->get_devlink_name().c_str());
        return std::make_pair(device, existing_volume);
    }

    const auto &label(!volinfo.label.empty() ? volinfo.label : volinfo.fstype);

    auto volume = std::unique_ptr<Volume>(
                        new Volume(device, volinfo.idx,
                                   label, volinfo.volume_uuid, volinfo.fstype,
                                   devname, tools_, symlink_directory_));

    existing_volume = volume.get();

    if(volume == nullptr)
    {
        msg_out_of_memory("Volume object");
        existing_volume = nullptr;
    }
    else if(!device->add_volume(std::move(volume)))
        existing_volume = nullptr;

    return std::make_pair(device, existing_volume);
}
