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

#include <sstream>
#include <cstring>

#include "automounter.hh"
#include "dbus_iface_deep.h"
#include "messages.h"
#include "os.h"

static void announce_new_volume(const Devices::Volume &vol)
{
    tdbus_moun_ta_emit_new_volume(dbus_get_mounta_iface(),
                                  vol.get_index() >= 0 ? vol.get_index() : UINT_MAX,
                                  vol.get_label().c_str(),
                                  vol.get_mountpoint().c_str(),
                                  vol.get_device()->get_id());
}

static void announce_new_device(const Devices::Device &dev)
{
    if(!dev.get_working_directory().empty())
    {
        GVariant *usb_info =
            g_variant_new("(uu)", dev.get_usb_hub_id(), dev.get_usb_port());
        tdbus_moun_ta_emit_new_usbdevice(dbus_get_mounta_iface(),
                                         dev.get_id(),
                                         dev.get_display_name().c_str(),
                                         dev.get_working_directory().c_str(),
                                         usb_info);
    }
}

static void announce_device_removed(const Devices::Device &dev)
{
    if(!dev.get_working_directory().empty())
        tdbus_moun_ta_emit_device_removed(dbus_get_mounta_iface(),
                                          dev.get_id(),
                                          dev.get_working_directory().c_str());
}

static std::string ensure_mountpoint_directory(const std::string &wd,
                                               const Devices::Volume &volume)
{
    const auto *dev = volume.get_device();

    if(dev->get_working_directory().empty())
    {
        std::ostringstream os;

        os << wd << '/' << volume.get_device()->get_id();

        if(os_mkdir_hierarchy(os.str().c_str(), true))
            volume.set_device_working_dir(os.str());
    }

    if(volume.get_state() == Devices::Volume::PENDING &&
       !dev->get_working_directory().empty())
    {
        std::ostringstream os;

        os << dev->get_working_directory() << '/' << volume.get_index();

        if(os_mkdir(os.str().c_str(), true))
            return os.str();
    }

    return "";
}

static void do_mount_volume(Devices::Volume *volume, const std::string &path,
                            const Automounter::ExternalTools &tools)
{
    log_assert(volume->get_state() == Devices::Volume::PENDING);

    if(os_system_formatted("%s %s %s %s",
                           tools.mount_.executable_.c_str(),
                           tools.mount_.options_.c_str(),
                           volume->get_device_name().c_str(), path.c_str()) == 0)
    {
        volume->set_mounted(path);

        msg_info("Mounted %s to %s (USB hub %u, port %u)",
                 volume->get_device_name().c_str(), path.c_str(),
                 volume->get_device()->get_usb_hub_id(),
                 volume->get_device()->get_usb_port());

        announce_new_volume(*volume);
    }
    else
    {
        volume->set_unusable();

        msg_error(0, LOG_ERR, "Failed mounting %s to %s",
                  volume->get_device_name().c_str(), path.c_str());

        /* we need to clean up after ourselves */
        (void)os_rmdir(path.c_str(), true);
    }
}

static bool do_unmount_path(const char *path,
                            const Automounter::ExternalTools &tools)
{
    return os_system_formatted("%s %s %s",
                               tools.unmount_.executable_.c_str(),
                               tools.unmount_.options_.c_str(),
                               path);
}

static void do_unmount_volume(Devices::Volume &volume,
                              const Automounter::ExternalTools &tools)
{
    log_assert(volume.get_state() == Devices::Volume::MOUNTED ||
               volume.get_state() == Devices::Volume::REJECTED);

    if(do_unmount_path(volume.get_mountpoint().c_str(), tools) == 0)
        msg_info("Unmounted %s from %s",
                 volume.get_device_name().c_str(),
                 volume.get_mountpoint().c_str());
    else
        msg_info("Failed unmounting %s from %s (ignored)",
                 volume.get_device_name().c_str(),
                 volume.get_mountpoint().c_str());

    volume.set_removed();
}

/*!
 * Filter devices here.
 *
 * \todo Replace ad-hoc filter by proper configurable filter object.
 */
static void apply_device_filter(Devices::Device &dev)
{
    log_assert(dev.get_state() == Devices::Device::PROBED);

    constexpr static const char required_prefix[] = "usb-";

    if(strncmp(dev.get_display_name().c_str(),
               required_prefix, sizeof(required_prefix) - 1) != 0)
        return dev.reject();

    dev.accept();
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

void Automounter::Core::handle_new_device(const char *device_path)
{
    log_assert(device_path != nullptr);

    msg_info("New device: \"%s\"", device_path);

    Devices::Volume *vol;
    auto *dev = devman_.new_entry(device_path, &vol);

    if(dev == nullptr || vol == nullptr)
    {
        if(dev == nullptr)
            msg_error(0, LOG_NOTICE, "Failed using device %s", device_path);

        return;
    }

    bool is_device_announcement_needed = false;

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

        is_device_announcement_needed = true;

        /* fall-through */

      case Devices::Device::OK:
        break;
    }

    switch(vol->get_state())
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
    apply_volume_filter(*vol);

    const std::string mountpoint_path(ensure_mountpoint_directory(working_directory_, *vol));

    if(is_device_announcement_needed)
        announce_new_device(*dev);

    if(vol->get_state() != Devices::Volume::PENDING)
        return;

    /*
     * None of the filters kicked in, so we'll try to mount the volume now.
     */
    if(!mountpoint_path.empty())
        do_mount_volume(vol, mountpoint_path, tools_);
    else
        vol->set_unusable();
}

static void try_unmount_volume(Devices::Volume &vol,
                               const Automounter::ExternalTools *tools)
{
    log_assert(tools != nullptr);

    switch(vol.get_state())
    {
      case Devices::Volume::PENDING:
        vol.set_unusable();
        break;

      case Devices::Volume::REJECTED:
        BUG("Attempting to unmount rejected volume");

        /* fall-through */

      case Devices::Volume::MOUNTED:
        do_unmount_volume(vol, *tools);
        break;

      case Devices::Volume::UNUSABLE:
        break;

      case Devices::Volume::REMOVED:
        BUG("Attempted to unmount removed volume");
        break;
    }

    log_assert(vol.get_state() == Devices::Volume::REMOVED ||
               vol.get_state() == Devices::Volume::UNUSABLE);
}

void Automounter::Core::handle_removed_device(const char *device_path)
{
    log_assert(device_path != nullptr);

    msg_info("Removed device: \"%s\"", device_path);

    devman_.remove_entry(device_path,
                         announce_device_removed,
                         std::bind(try_unmount_volume, std::placeholders::_1, &tools_));
}

static void remove_mountpoint_the_hard_way(const char *path, void *user_data)
{
    const auto &tools = *reinterpret_cast<const Automounter::ExternalTools *>(user_data);

    if(do_unmount_path(path, tools))
        msg_error(0, LOG_NOTICE, "Unmounted \"%s\" the hard way", path);
    else
        msg_error(0, LOG_NOTICE, "Failed unmounting \"%s\" the hard way", path);

    (void)os_rmdir(path, true);
}

void Automounter::Core::shutdown()
{
    /* Attempt to clean up the nice and polite way. */
    while(devman_.remove_entry(devman_.begin(),
                               nullptr,
                               std::bind(try_unmount_volume,
                                         std::placeholders::_1, &tools_)))
        ;

    /* Remove residual mountpoints. There shouldn't be any, but we want to be
     * sure to leave the system in the most sane state possible. */
    os_foreach_in_path(working_directory_.c_str(),
                       remove_mountpoint_the_hard_way,
                       const_cast<Automounter::ExternalTools *>(&tools_));

    /* Top-level working directory should be removed as well. Will be created
     * again when devices are found. */
    os_rmdir(working_directory_.c_str(), false);
}
