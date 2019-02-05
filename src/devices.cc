/*
 * Copyright (C) 2015, 2017, 2019  T+A elektroakustik GmbH & Co. KG
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

#include <algorithm>
#include <cstring>
#include <sstream>
#include <dirent.h>
#include "os.hh"

#include "devices.hh"
#include "devices_os.hh"
#include "automounter.hh"
#include "messages.h"

static int do_remove_residual_directories(const char *path,
                                          unsigned char dtype, void *user_data)
{
    if(dtype == DT_DIR)
    {
        const auto &amdir(*static_cast<const Automounter::Directory *>(user_data));
        auto temp(amdir.str() + '/' + path);
        msg_error(0, LOG_NOTICE, "Removing residual directory \"%s\"", temp.c_str());
        (void)os_rmdir(temp.c_str(), true);
    }
    else
        msg_error(0, LOG_ERR,
                  "Found unexpected directory entry: \"%s\"", path);

    return 0;
}

Devices::Device::~Device()
{
    /* we need to destroy all volumes first so that our mountpoint directory
     * becomes empty */
    volumes_.clear();

    if(mountpoint_container_path_.exists())
        os_foreach_in_path(mountpoint_container_path_.str().c_str(),
                           do_remove_residual_directories,
                           &mountpoint_container_path_);
}

Devices::Volume *
Devices::Device::lookup_volume_by_devname(const std::string &devname) const
{
    const auto &vol =
        std::find_if(volumes_.begin(), volumes_.end(),
            [&devname] (const decltype(volumes_)::value_type &it) -> bool
            {
                return it.second->get_device_name() == devname;
            });

    return (vol != volumes_.end()) ? vol->second.get() : nullptr;
}

bool Devices::Device::add_volume(std::unique_ptr<Devices::Volume> &&volume)
{
    auto idx = volume->get_index();
    auto result = volumes_.insert(std::make_pair(idx, std::move(volume)));

    if(!result.second)
        BUG("Insertion of volume failed");

    return result.second;
}

void Devices::Device::drop_volumes()
{
    volumes_.clear();
}

bool Devices::Device::mk_working_directory(std::string &&path)
{
    log_assert(!path.empty());
    log_assert(state_ == OK);

    if(mountpoint_container_path_.exists())
        BUG("Overwriting device mountpoint container");

    if(path == mountpoint_container_path_.str())
        return true;
    else
    {
        mountpoint_container_path_ = std::move(Automounter::Directory(std::move(path)));
        return mountpoint_container_path_.create();
    }
}

bool Devices::Device::probe()
{
    return state_ == SYNTHETIC ? do_probe() : false;
}

bool Devices::Device::do_probe()
{
    log_assert(state_ == SYNTHETIC);

    DeviceInfo devinfo;

    const char *name = strrchr(devlink_name_.c_str(), '/');
    device_name_ = (name != nullptr) ? (name + 1) : devlink_name_.c_str();

    if(device_name_.empty() ||
       !get_device_information(devlink_name_, devinfo))
    {
        state_ = BROKEN;
        return false;
    }

    switch(devinfo.type)
    {
      case DeviceType::UNKNOWN:
        state_ = BROKEN;
        break;

      case DeviceType::USB:
        usb_port_ = devinfo.usb_port_sysfs_name;
        state_ = PROBED;
        return true;
    }

    return false;
}

bool Devices::Volume::mk_mountpoint_directory()
{
    if(containing_device_ == nullptr)
        return false;

    std::ostringstream os;
    os << containing_device_->get_working_directory().str() << '/' << index_;

    mountpoint_.set(std::move(os.str()));

    return mountpoint_.create();
}

bool Devices::Volume::mount(const Automounter::FSMountOptions &mount_options)
{
    if(!mountpoint_.mount(devname_, mount_options.get_options(fstype_)))
        return false;

    if(!symlink_directory_.empty())
    {
        std::string linkabspath = symlink_directory_ + "/" + label_;
        auto file_exists = [] (const std::string &s) -> bool
        {
            OS::SuppressErrorsGuard g;
            struct stat buffer;
            return os_stat(s.c_str(), &buffer) == 0;
        };

        for(unsigned int i = 2; file_exists(linkabspath); ++i)
            linkabspath = linkabspath + "-" + std::to_string(i);

        msg_info("Creating symlink %s to %s", linkabspath.c_str(), mountpoint_.str().c_str());
        //TODO: Create os_symlink for testing.
        if(symlink(mountpoint_.str().c_str(), linkabspath.c_str()) != 0)
            msg_error(errno, LOG_ERR, "Failed to create symbolic link.");
        else
            symlink_ = linkabspath;
    }

    return true;
}

void Devices::Volume::set_mounted()
{
    log_assert(state_ == PENDING);
    state_ = MOUNTED;
}

void Devices::Volume::set_removed()
{
    log_assert(state_ == MOUNTED || state_ == REJECTED);

    set_eol_state_and_cleanup(REMOVED, true);
}

void Devices::Volume::set_unusable()
{
    log_assert(state_ == PENDING);

    set_eol_state_and_cleanup(UNUSABLE, true);
}

void Devices::Volume::set_eol_state_and_cleanup(State state,
                                                bool not_expecting_failure)
{
    state_ = state;
    mountpoint_.cleanup();

    if (!symlink_.empty())
    {
      msg_info("Deleting symlink %s", symlink_.c_str());
      if (os_file_delete(symlink_.c_str())==0)
          symlink_.clear();
      else
          msg_error(errno, LOG_ERR, "Failed to delete symbolic link.");
    }
}

Devices::Volume::~Volume()
{
    set_eol_state_and_cleanup(UNUSABLE, true);
}
