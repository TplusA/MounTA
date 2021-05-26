/*
 * Copyright (C) 2015--2021  T+A elektroakustik GmbH & Co. KG
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

#include <sstream>
#include <cstring>

#include "automounter.hh"
#include "external_tools.hh"
#include "dbus_iface_deep.h"
#include "messages.h"
#include "os.h"

const char *Automounter::FSMountOptions::get_options(const std::string &fstype) const
{
    auto it = options_.find(fstype);

    if(it != options_.end())
    {
        if(it->second != nullptr)
            return it->second;
    }
    else
        msg_error(0, LOG_NOTICE,
                  "WARNING: Encountered unsupported file system \"%s\"",
                  fstype.c_str());

    return "";
}

static void announce_new_volume(const Devices::Volume &vol)
{
    /* note: this duplicates part of #dbusmethod_get_all() */
    tdbus_moun_ta_emit_new_volume(dbus_get_mounta_iface(),
                                  vol.get_index() >= 0 ? vol.get_index() : UINT_MAX,
                                  vol.get_label().c_str(),
                                  vol.get_mountpoint_name().c_str(),
                                  vol.get_device()->get_id(),
                                  vol.get_volume_uuid().c_str());
}

static void announce_new_device(const Devices::Device &dev)
{
    if(dev.get_working_directory().exists())
    {
        /* note: this duplicates part of #dbusmethod_get_all() */
        tdbus_moun_ta_emit_new_usbdevice(dbus_get_mounta_iface(),
                                         dev.get_id(),
                                         dev.get_display_name().c_str(),
                                         dev.get_device_uuid().c_str(),
                                         dev.get_working_directory().str().c_str(),
                                         dev.get_usb_port().c_str());
    }
}

/*!
 * Filter devices here.
 *
 * \todo Replace ad-hoc filter by proper configurable filter object.
 */
static void apply_device_filter(Devices::Device &dev)
{
    log_assert(dev.get_state() == Devices::Device::PROBED);

    static constexpr const std::array<const char *const, 2> allowed_prefixes
    {
        "usb-",
        "ata-",
    };

    for(const auto prefix : allowed_prefixes)
    {
        if(strncmp(dev.get_display_name().c_str(), prefix, strlen(prefix)) == 0)
            return dev.accept();
    }

    dev.reject();
}

/*!
 * We could filter volumes here.
 *
 * In case a volume should be filtered out, #Devices::Volume::reject() should
 * be called.
 */
static void apply_volume_filter(Devices::Volume &vol)
{
    log_assert(vol.get_state() == Devices::Volume::PENDING);
}

static void try_mount_volume(Devices::Volume &vol,
                             const Automounter::FSMountOptions &mount_options)
{
    switch(vol.get_state())
    {
      case Devices::Volume::PENDING:
        /* try to mount below */
        break;

      case Devices::Volume::REJECTED:
        /* devices wasn't rejected, but this volume is */
        return;

      case Devices::Volume::MOUNTED:
        BUG("Attempted to remount device");
        return;

      case Devices::Volume::UNUSABLE:
        BUG("Attempted to remount known unusable device");
        return;

      case Devices::Volume::REMOVED:
        BUG("Attempted to remount removed device");
        return;
    }

    /*
     * We announce any device which has not been filtered away as a whole, even
     * if some (or all) of its volumes are filtered. This way the user will see
     * the device, but not the filtered volumes. If all available volumes are
     * filtered, then the device will still be visible, but appear empty.
     */
    apply_volume_filter(vol);

    if(vol.get_state() != Devices::Volume::PENDING)
        return;

    if(!vol.get_device()->get_working_directory().exists())
        return;

    /*
     * None of the filters kicked in, so we'll try to mount the volume now.
     */
    if(vol.mk_mountpoint_directory() && vol.mount(mount_options))
    {
        vol.set_mounted();

        msg_info("Mounted %s to %s (USB port %s)",
                 vol.get_device_name().c_str(),
                 vol.get_mountpoint_name().c_str(),
                 vol.get_device()->get_usb_port().c_str());

        announce_new_volume(vol);
    }
    else
    {
        vol.set_unusable();

        msg_error(0, LOG_ERR, "Failed mounting device %s",
                  vol.get_device_name().c_str());
    }
}

static void mount_all_pending_volumes(Devices::Device &dev,
                                      const Automounter::FSMountOptions &mount_options)
{
    for(const auto &volinfo : dev)
    {
        if(volinfo.second == nullptr)
            continue;

        if(volinfo.second->get_state() == Devices::Volume::PENDING)
            try_mount_volume(*volinfo.second, mount_options);
    }
}

void Automounter::Core::handle_new_device(const char *device_path)
{
    log_assert(device_path != nullptr);

    msg_info("New device: \"%s\"", device_path);

    Devices::Volume *vol;
    bool have_probed_dev;
    auto dev = devman_.new_entry(device_path, vol, have_probed_dev);

    if(dev == nullptr || vol == nullptr)
    {
        if(dev == nullptr)
            msg_error(0, LOG_NOTICE, "Failed using device %s", device_path);

        if(dev == nullptr || dev->empty())
            return;
    }

    log_assert(dev != nullptr);

    switch(dev->get_state())
    {
      case Devices::Device::SYNTHETIC:
        /* handled later when the device is really available */
        return;

      case Devices::Device::BROKEN:
      case Devices::Device::REJECTED:
        /* don't care about it anymore */
        return;

      case Devices::Device::PROBED:
        apply_device_filter(*dev);

        if(dev->get_state() != Devices::Device::OK)
            return;

        /* fall-through */

      case Devices::Device::OK:
        if(!have_probed_dev)
            have_probed_dev = dev->get_working_directory().str().empty();

        break;
    }

    if(have_probed_dev)
    {
        std::ostringstream os;
        os << working_directory_ << '/' << dev->get_id();

        if(dev->mk_working_directory(os.str()))
            announce_new_device(*dev);

        mount_all_pending_volumes(*dev, mount_options_);
    }
    else if(vol != nullptr)
        try_mount_volume(*vol, mount_options_);
}

void Automounter::Core::handle_removed_device(const char *device_path)
{
    log_assert(device_path != nullptr);

    msg_info("Removed device: \"%s\"", device_path);

    devman_.remove_entry(device_path,
        [] (const Devices::Device &device)
        {
            if(device.get_working_directory().exists())
                tdbus_moun_ta_emit_device_removed(dbus_get_mounta_iface(),
                                                device.get_id(),
                                                device.get_working_directory().str().c_str());
        },
        [] (const Devices::Device &device)
        {
            if(device.get_working_directory().exists())
                tdbus_moun_ta_emit_device_will_be_removed(dbus_get_mounta_iface(),
                                                          device.get_id(),
                                                          device.get_working_directory().str().c_str());
        });
}

static int remove_mountpoint_the_hard_way(const char *path,
                                          unsigned char dtype, void *user_data)
{
    const auto &tools = *reinterpret_cast<const Automounter::ExternalTools *>(user_data);

    if(dtype != DT_DIR)
        msg_error(0, LOG_ERR,
                  "Unexpected entry in top-level directory: \"%s\"", path);
    else
    {
        Automounter::Mountpoint mp(tools, path);
        mp.probe();
    }

    return 0;
}

void Automounter::Core::shutdown()
{
    /* Attempt to clean up the nice and polite way. */
    for(auto it = devman_.begin(); it != devman_.end(); ++it)
        devman_.remove_entry(it, nullptr);

    /* Remove residual mountpoints. There shouldn't be any, but we want to be
     * sure to leave the system in the most sane state possible. */
    os_foreach_in_path(working_directory_.c_str(),
                       remove_mountpoint_the_hard_way,
                       nullptr);

    /* Top-level working directory should be removed as well. Will be created
     * again when devices are found. */
    os_rmdir(working_directory_.c_str(), false);
}
