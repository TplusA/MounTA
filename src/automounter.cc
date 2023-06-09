/*
 * Copyright (C) 2015--2023  T+A elektroakustik GmbH & Co. KG
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
#include <climits>
#include <algorithm>
#include <array>

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
    if(dev.get_working_directory().exists(Automounter::FailIf::NOT_FOUND))
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
 * Check whether or not a device symlink name is of interest to us.
 */
static bool is_device_name_acceptable(const char *device_name,
                                      bool check_for_tempfiles)
{
    /* allow only certain prefixes */
    static const std::array<const std::string, 2> allowed_prefixes
    {
        "usb-",
        "ata-",
    };

    if(check_for_tempfiles)
    {
        const char *temp = strrchr(device_name, '/');
        if(temp != nullptr)
            device_name = temp + 1;
    }

    if(std::none_of(allowed_prefixes.begin(), allowed_prefixes.end(),
            [&device_name] (const auto &prefix)
            {
                return strncmp(device_name, prefix.c_str(), prefix.length()) == 0;
            }))
        return false;

    /* filter out systemd temporary symlink names for block devices (for
     * example, "usb-NAME-0:0-part1.tmp-b8:17" is a temporary file for the
     * final name "usb-NAME-0:0-part1", so we need to detect the
     * ".tmp-bnnn:mmm" suffix) */
    if(!check_for_tempfiles)
        return true;

    const char *s = strrchr(device_name, '.');
    if(s == nullptr)
        return true;

    static const char block_dev_tmpname[] = "tmp-b";
    if(strncmp(s + 1, block_dev_tmpname, sizeof(block_dev_tmpname) - 1) != 0)
        return true;

    for(s += sizeof(block_dev_tmpname); *s >= '0' && *s <= '9'; ++s)
        ;

    if(*s != ':')
        return true;

    const char *after_colon = s + 1;
    for(s = after_colon; *s >= '0' && *s <= '9'; ++s)
        ;

    return s == after_colon || *s != '\0';
}

/*!
 * Filter devices here.
 */
static void apply_device_filter(Devices::Device &dev)
{
    msg_log_assert(dev.get_state() == Devices::Device::PROBED);

    if(is_device_name_acceptable(dev.get_display_name().c_str(), false))
        dev.accept();
    else
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
    msg_log_assert(vol.get_state() == Devices::Volume::PENDING);
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
        /* device wasn't rejected, but this volume is */
        return;

      case Devices::Volume::MOUNTED:
        MSG_BUG("Attempted to remount device");
        return;

      case Devices::Volume::UNUSABLE:
        MSG_BUG("Attempted to remount known unusable device");
        return;

      case Devices::Volume::REMOVED:
        MSG_BUG("Attempted to remount removed device");
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

    if(!vol.get_device()->get_working_directory().exists(Automounter::FailIf::NOT_FOUND))
        return;

    /*
     * None of the filters kicked in, so we'll try to mount the volume now.
     */
    if(!vol.get_device()->get_working_directory().exists(Automounter::FailIf::JUST_WATCHING))
    {
        /* watch mode */
        vol.set_unmanaged_mountpoint_directory();
        vol.set_mounted();

        msg_info("Added mounted volume %s to %s (USB port %s)",
                 vol.get_device_name().c_str(),
                 vol.get_mountpoint_name().c_str(),
                 vol.get_device()->get_usb_port().c_str());

        announce_new_volume(vol);
    }
    else if(vol.mk_mountpoint_directory() && vol.mount(mount_options))
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
    msg_log_assert(device_path != nullptr);

    if(!is_device_name_acceptable(device_path, true))
    {
        msg_vinfo(MESSAGE_LEVEL_DIAG,
                  "Rejected device (bad name): \"%s\"", device_path);
        return;
    }

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

    msg_log_assert(dev != nullptr);

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
    msg_log_assert(device_path != nullptr);

    msg_info("Removed device: \"%s\"", device_path);

    devman_.remove_entry(device_path,
        [] (const Devices::Device &device)
        {
            if(device.get_working_directory().exists(FailIf::NOT_FOUND))
                tdbus_moun_ta_emit_device_removed(dbus_get_mounta_iface(),
                                                device.get_id(),
                                                device.get_device_uuid().c_str(),
                                                device.get_working_directory().str().c_str());
        },
        [] (const Devices::Device &device)
        {
            if(device.get_working_directory().exists(FailIf::NOT_FOUND))
                tdbus_moun_ta_emit_device_will_be_removed(dbus_get_mounta_iface(),
                                                          device.get_id(),
                                                          device.get_device_uuid().c_str(),
                                                          device.get_working_directory().str().c_str());
        });
}

void Automounter::Core::handle_new_unmanaged_mountpoint(const char *mountpoint_path)
{
    msg_log_assert(mountpoint_path != nullptr);

    msg_info("New mountpoint: \"%s\"", mountpoint_path);
    static const struct timespec small_delay = {0, 500L * 1000L * 1000L};
    nanosleep(&small_delay, nullptr);

    Devices::Volume *vol;
    auto dev = devman_.new_entry_by_mountpoint(mountpoint_path, vol);

    if(dev == nullptr || dev->empty() || vol == nullptr)
    {
        msg_error(0, LOG_NOTICE, "Failed probing mountpoint %s", mountpoint_path);
        return;
    }

    switch(dev->get_state())
    {
      case Devices::Device::SYNTHETIC:
      case Devices::Device::BROKEN:
      case Devices::Device::REJECTED:
        return;

      case Devices::Device::PROBED:
        apply_device_filter(*dev);

        if(dev->get_state() != Devices::Device::OK)
            return;

        /* fall-through */

      case Devices::Device::OK:
        dev->set_mountpoint_directory(mountpoint_path);
        announce_new_device(*dev);
        try_mount_volume(*vol, mount_options_);
        break;
    }
}

void Automounter::Core::handle_removed_unmanaged_mountpoint(const char *mountpoint_path)
{
    msg_log_assert(mountpoint_path != nullptr);

    msg_info("Removed mountpoint: \"%s\"", mountpoint_path);

    /* cannot call Devices::map_mountpoint_path_to_device_links() here because
     * the mountpoint and the device are gone already */
    const auto dev(devman_.take_volume_device_for_mountpoint(mountpoint_path));
    if(!dev.empty())
        handle_removed_device(dev.c_str());
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
                       &const_cast<ExternalTools &>(tools_));

    /* Top-level working directory should be removed as well. Will be created
     * again when devices are found. */
    os_rmdir(working_directory_.c_str(), false);
}
