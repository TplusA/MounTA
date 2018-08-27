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
    log_assert(devlink != nullptr);
    log_assert(devlink[0] != '\0');

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
    log_assert(devlink != nullptr);

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

            log_assert(device_and_volume.first == device ||
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

bool Devices::AllDevices::remove_entry(const char *devlink,
                                       const std::function<void(const Device &)> &removal_notification)
{
    log_assert(devlink != nullptr);
    return remove_entry(get_device_iter_by_devlink(devices_, devlink),
                        removal_notification);
}

bool Devices::AllDevices::remove_entry(Devices::AllDevices::DevContainerType::const_iterator devices_iter,
                                       const std::function<void(const Device &)> &removal_notification)
{
    if(devices_iter == devices_.end())
    {
        /* react only on removal of whole devices */
        return false;
    }

    devices_iter->second->drop_volumes();

    if(removal_notification != nullptr)
        removal_notification(*devices_iter->second);

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
            BUG("Insertion of device failed");
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
                                   label, volinfo.fstype, devname, tools_,
                                   symlink_directory_));

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
