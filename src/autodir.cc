/*
 * Copyright (C) 2017  T+A elektroakustik GmbH & Co. KG
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

#include "autodir.hh"
#include "external_tools.hh"
#include "os.h"

bool Automounter::Directory::create()
{
    if(is_created_)
    {
        BUG("Directory \"%s\" already created", absolute_path_.c_str());
        return false;
    }

    if(absolute_path_.empty())
    {
        BUG("Cannot create directory, name is empty");
        return false;
    }

    is_created_ = os_mkdir_hierarchy(absolute_path_.c_str(), true);

    return is_created_;
}

void Automounter::Directory::cleanup()
{
    if(!is_created_)
    {
        absolute_path_.clear();
        return;
    }

    for(int i = 20; i >= 0; --i)
    {
        if(i < 20)
        {
            /* wait for 250 ms */
            static const struct timespec t =
            {
                .tv_sec = 0,
                .tv_nsec = 250L * 1000L * 1000L,
            };

            os_nanosleep(&t);
        }

        if(os_rmdir(absolute_path_.c_str(), i == 0))
            break;
    }

    is_created_ = false;
    absolute_path_.clear();
}

void Automounter::Mountpoint::set(std::string &&path)
{
    if(directory_.exists())
        BUG("Overwriting mountpoint path");

    cleanup();
    directory_ = std::move(Directory(std::move(path)));
}

void Automounter::Mountpoint::probe()
{
}

bool Automounter::Mountpoint::mount(const std::string &device_name,
                                    const std::string &mount_options)
{
    if(directory_.str().empty())
    {
        BUG("Cannot mount empty mointpoint");
        return false;
    }

    if(!directory_.exists())
    {
        BUG("Mointpoint \"%s\" does not exist", directory_.str().c_str());
        return false;
    }

    if(is_mounted_)
    {
        BUG("Mointpoint \"%s\" already mounted", directory_.str().c_str());
        return false;
    }

    return os_system_formatted(msg_is_verbose(MESSAGE_LEVEL_NORMAL),
                               "%s %s %s %s \"%s\"",
                               tools_.mount_.executable_.c_str(),
                               tools_.mount_.options_.c_str(),
                               mount_options.c_str(),
                               device_name.c_str(), directory_.str().c_str()) == 0;
}

void Automounter::Mountpoint::do_cleanup(bool thoroughly)
{
    if(is_mounted_)
    {
        is_mounted_ = false;

        if(os_system_formatted(msg_is_verbose(MESSAGE_LEVEL_NORMAL),
                               "%s %s \"%s\"",
                               tools_.unmount_.executable_.c_str(),
                               tools_.unmount_.options_.c_str(),
                               directory_.str().c_str()) == 0)
            msg_vinfo(MESSAGE_LEVEL_DIAG,
                      "Unmounted %s", directory_.str().c_str());
        else
            msg_error(0, LOG_ERR,
                      "Failed unmounting %s (ignored)", directory_.str().c_str());
    }

    if(thoroughly)
        directory_.cleanup();
}
