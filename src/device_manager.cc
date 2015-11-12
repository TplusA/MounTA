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

#include <cstring>
#include <climits>
#include <functional>
#include <algorithm>
#include <type_traits>

#include "device_manager.hh"
#include "devices_os.h"
#include "messages.h"

Devices::AllDevices::~AllDevices()
{
    for(auto dev : devices_)
        delete dev.second;

    devices_.clear();
}

Devices::ID::value_type Devices::ID::next_free_id_;

static const char *find_trailing_number(const char *devname)
{
    const char *p;

    for(p = devname + strlen(devname) - 1; p > devname; --p)
    {
        if(!isdigit(*p))
        {
            ++p;
            break;
        }
    }

    return (*p != '\0') ? p : nullptr;
}

/*!
 * Parse volume number for block device name.
 *
 * \param devname
 *     A device name such as `/dev/sda`, `/dev/sdb5`, or `/dev/sdx123`.
 *
 * \returns
 *     The volume number, or -1 if the number could not be parsed. Note that 0
 *     will be returned in case the device name does not end with a number.
 *     Number -1 is only returned for names that start with a digit, end with a
 *     number that causes an integer overflow, or on internal fault.
 */
static int devname_get_volume_number(const char *devname)
{
    log_assert(devname != nullptr);
    log_assert(devname[0] != '\0');

    if(isdigit(devname[0]))
    {
        msg_error(EINVAL, LOG_NOTICE, "Invalid device name: \"%s\"", devname);
        return -1;
    }

    const char *const start_of_number = find_trailing_number(devname);

    if(start_of_number == nullptr)
        return 0;

    char *endptr;
    const unsigned long temp = strtoul(start_of_number, &endptr, 10);

    if(*endptr == '\0')
        return temp;

    if(temp > INT_MAX || (temp == ULONG_MAX && errno == ERANGE))
        msg_error(ERANGE, LOG_NOTICE,
                  "Number in device name out of range: \"%s\"", devname);
    else
        BUG("Failed parsing validated number");

    return -1;
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
            return strcmp(it.second->get_name().c_str(), devlink) == 0;
        });
}

Devices::Device *Devices::AllDevices::find_root_device(const char *devlink)
{
    const std::string temp(mk_root_devlink_name(devlink));
    const auto &dev = get_device_iter_by_devlink(devices_, temp.c_str());
    return (dev != devices_.end()) ? dev->second : nullptr;
}

const Devices::Device *Devices::AllDevices::new_entry(const char *devlink,
                                                      const Devices::Volume **volume)
{
    log_assert(devlink != nullptr);

    if(volume != nullptr)
        *volume = nullptr;

    char *devname_mem = os_resolve_symlink(devlink);
    const char *devname = (devname_mem != nullptr) ? devname_mem : devlink;

    const int volume_number = devname_get_volume_number(devname);

    if(volume_number < 0)
    {
        free(devname_mem);
        return nullptr;
    }

    struct osdev_volume_info volinfo;
    bool have_volume_info = false;

    Device *device =
        (volume_number == 0)
        ? add_or_get_device(devlink, devname, volinfo, have_volume_info)
        : find_root_device(devlink);

    if(volume_number > 0 || have_volume_info)
    {
        if(!have_volume_info)
            have_volume_info = osdev_get_volume_information(devname, &volinfo);

        if(have_volume_info)
        {
            auto device_and_volume = add_or_get_volume(device, devlink, devname, volinfo);

            log_assert(device_and_volume.first == device ||
                       (device == nullptr && device_and_volume.first != nullptr));

            if(volume != nullptr)
                *volume = device_and_volume.second;

            if(device == nullptr)
                device = device_and_volume.first;
        }
    }
    else if(device != nullptr && volume_number == 0 && volume != nullptr)
        *volume = device->lookup_volume_by_devname(devname);

    free(devname_mem);

    if(have_volume_info)
        osdev_free_volume_information(&volinfo);

    return device;
}

bool Devices::AllDevices::remove_entry(const char *devlink)
{
    log_assert(devlink != nullptr);

    auto dev_it = get_device_iter_by_devlink(devices_, devlink);
    if(dev_it == devices_.end())
    {
        msg_error(EINVAL, LOG_NOTICE,
                  "Cannot remove non-existent device \"%s\"", devlink);
        BUG("Requested to remove non-existent entry");
        return false;
    }

    Devices::Device *device = dev_it->second;

    devices_.erase(dev_it);

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
            return new Devices::Device(device_id,
                                       devlink, "/some/where", USBHubID(5), 10);
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
                                           mk_root_devlink_name(devlink));
            });
    }

    if(device == nullptr)
        return std::pair<Devices::Device *, Devices::Volume *>(nullptr, nullptr);

    auto *volume = device->lookup_volume_by_devname(devname);

    if(volume != nullptr)
    {
        msg_info("Volume %s already registered on device %s",
                 devlink, device->get_name().c_str());
        return std::pair<Devices::Device *, Devices::Volume *>(device, volume);
    }

    const char *label = ((volinfo.label != nullptr && volinfo.label[0] != '\0')
                         ? volinfo.label
                         : volinfo.fstype);

    volume = new Devices::Volume(*device, volinfo.idx, label, devname);

    if(volume == nullptr)
        msg_out_of_memory("Volume object");
    else if(!device->add_volume(*volume))
    {
        delete volume;
        volume = nullptr;
    }

    return std::pair<Devices::Device *, Devices::Volume *>(device, volume);
}
