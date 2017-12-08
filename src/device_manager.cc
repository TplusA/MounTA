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
#include "devices_os.h"
#include "messages.h"

Devices::AllDevices::~AllDevices()
{
    for(auto dev : devices_)
        delete dev.second;

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

static inline auto
get_device_iter_by_devlink(std::map<Devices::ID::value_type, Devices::Device *> &devices,
                           const char *devlink)
    -> std::remove_reference<decltype(devices)>::type::iterator
{
    return std::find_if(devices.begin(), devices.end(),
        [&devlink] (std::remove_reference<decltype(devices)>::type::value_type &it)
        {
            return strcmp(it.second->get_devlink_name().c_str(), devlink) == 0;
        });
}

Devices::Device *Devices::AllDevices::find_root_device(const char *devlink)
{
    const std::string temp(mk_root_devlink_name(devlink));
    const auto &dev = get_device_iter_by_devlink(devices_, temp.c_str());
    return (dev != devices_.end()) ? dev->second : nullptr;
}

Devices::Device *Devices::AllDevices::new_entry(const char *devlink,
                                                Devices::Volume *&volume)
{
    log_assert(devlink != nullptr);

    volume = nullptr;

    DevnameWithVolumeNumber data;
    if(!get_devname_with_volume_number(&data, devlink))
        return nullptr;

    struct osdev_volume_info volinfo;
    bool have_volume_info = false;

    Device *device =
        (data.volume_number_ == 0)
        ? add_or_get_device(devlink, data.devname_, volinfo, have_volume_info)
        : find_root_device(devlink);

    if(data.volume_number_ > 0 || have_volume_info)
    {
        if(!have_volume_info)
            have_volume_info = osdev_get_volume_information(data.devname_, &volinfo);

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
        device->probe();
        volume = device->lookup_volume_by_devname(data.devname_);
    }

    if(have_volume_info)
        osdev_free_volume_information(&volinfo);

    return device;
}

bool Devices::AllDevices::remove_entry(const char *devlink,
                                       const Devices::AllDevices::RemoveDeviceCallback &remove_device,
                                       const Devices::AllDevices::RemoveVolumeCallback &remove_volume)
{
    log_assert(devlink != nullptr);

    return remove_entry(get_device_iter_by_devlink(devices_, devlink),
                        remove_device, remove_volume);
}

bool Devices::AllDevices::remove_entry(decltype(devices_)::const_iterator devices_iter,
                                       const Devices::AllDevices::RemoveDeviceCallback &remove_device,
                                       const Devices::AllDevices::RemoveVolumeCallback &remove_volume)
{
    if(devices_iter == devices_.end())
    {
        /*!
         * BUG: Could leak mountpoint directories here.
         *
         * Situation:
         *     Assume there is a block device named ADev mounted to ADir, maybe
         *     even mounted there by us. Then our daemon dies for some reason
         *     (crash, killed by package manager, whatever). On startup, we try
         *     to unmount all mounts in our working directory, but this may
         *     fail if some process has a file open in ADir. Later still,
         *     device ADev may get pulled by the user, so we'll see an unplug
         *     event for a device we don't know at this exact point in code.
         *     Directory ADir will be left around in this case.
         *
         * We should try to find a directory that matches the device name, make
         * sure the directory is not associated with some volume object and
         * that nothing is mounted there, then remove it.
         */
        return false;
    }

    Devices::Device *device = devices_iter->second;

    if(remove_volume != nullptr)
    {
        for(auto &vol_it : *device)
        {
            log_assert(vol_it.second != nullptr);
            remove_volume(*vol_it.second);
        }
    }

    if(remove_device != nullptr)
        remove_device(*device);

    devices_.erase(devices_iter);

    log_assert(device != nullptr);
    delete device;

    return true;
}

static Devices::Device *
mk_device(std::map<Devices::ID::value_type, Devices::Device *> &all_devices,
          const std::function<Devices::Device *(const Devices::ID &device_id)> &alloc_device)
{
    Devices::Device *device = nullptr;

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
            delete device;
            device = nullptr;
        }
    }
    else
        msg_out_of_memory("Device object");

    return device;
}

Devices::Device *
Devices::AllDevices::get_device_by_devlink(const char *devlink)
{
    const auto &dev = get_device_iter_by_devlink(devices_, devlink);
    return (dev != devices_.end()) ? dev->second : nullptr;
}

Devices::Device *
Devices::AllDevices::add_or_get_device(const char *devlink,
                                       const char *devname,
                                       struct osdev_volume_info &volinfo,
                                       bool &have_info)
{
    have_info = false;

    /* full device */
    auto *dev = get_device_by_devlink(devlink);

    if(dev != nullptr)
    {
        msg_info("Device %s already registered", devlink);
        return dev;
    }

    /* maybe also add a volume for this device */
    have_info = osdev_get_volume_information(devname, &volinfo);

    return mk_device(devices_,
        [&devlink] (const Devices::ID &device_id)
        {
            return new Devices::Device(device_id, devlink, true);
        });
}

std::pair<Devices::Device *, Devices::Volume *>
Devices::AllDevices::add_or_get_volume(Devices::Device *device,
                                       const char *devlink,
                                       const char *devname,
                                       const struct osdev_volume_info &volinfo)
{

    if(device == nullptr)
    {
        device = mk_device(devices_,
            [&devlink] (const Devices::ID &device_id)
            {
                return new Devices::Device(device_id,
                                           mk_root_devlink_name(devlink),
                                           false);
            });
    }

    if(device == nullptr)
        return std::pair<Devices::Device *, Devices::Volume *>(nullptr, nullptr);

    auto *volume = device->lookup_volume_by_devname(devname);

    if(volume != nullptr)
    {
        msg_info("Volume %s already registered on device %s",
                 devlink, device->get_devlink_name().c_str());
        return std::pair<Devices::Device *, Devices::Volume *>(device, volume);
    }

    const char *label = ((volinfo.label != nullptr && volinfo.label[0] != '\0')
                         ? volinfo.label
                         : volinfo.fstype);

    volume = new Devices::Volume(*device, volinfo.idx,
                                 label, volinfo.fstype, devname);

    if(volume == nullptr)
        msg_out_of_memory("Volume object");
    else if(!device->add_volume(*volume))
    {
        delete volume;
        volume = nullptr;
    }

    return std::pair<Devices::Device *, Devices::Volume *>(device, volume);
}
