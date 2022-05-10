/*
 * Copyright (C) 2017, 2019, 2022  T+A elektroakustik GmbH & Co. KG
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

#ifndef AUTODIR_HH
#define AUTODIR_HH

#include <string>
#include "messages.h"

namespace Automounter
{

class ExternalTools;

enum class FailIf
{
    NOT_FOUND,
    JUST_WATCHING,
};

class Directory
{
  private:
    std::string absolute_path_;
    bool is_created_;
    bool is_externally_managed_;

  public:
    Directory(const Directory &) = delete;
    Directory &operator=(const Directory &) = delete;
    Directory &operator=(Directory &&) = default;

    explicit Directory():
        is_created_(false),
        is_externally_managed_(false)
    {}

    explicit Directory(std::string &&path):
        absolute_path_(std::move(path)),
        is_created_(false),
        is_externally_managed_(false)
    {}

    ~Directory() { cleanup(); }

    bool create();
    bool probe(bool store_state = true);

    void set_externally_managed()
    {
        is_created_ = true;
        is_externally_managed_ = true;
    }

    bool exists(FailIf fail_if) const
    {
        switch(fail_if)
        {
          case FailIf::JUST_WATCHING:
            if(is_externally_managed_)
                return false;
            break;

          case FailIf::NOT_FOUND:
            break;
        }

        return is_created_;
    }

    const std::string &str() const { return absolute_path_; }

    void cleanup();
};

class Mountpoint
{
  private:
    Directory directory_;

    const ExternalTools &tools_;
    bool is_mounted_;

  public:
    Mountpoint(const Mountpoint &) = delete;
    Mountpoint &operator=(const Mountpoint &) = delete;

    explicit Mountpoint(const ExternalTools &tools):
        tools_(tools),
        is_mounted_(false)
    {}

    explicit Mountpoint(const ExternalTools &tools, std::string &&path):
        directory_(std::move(path)),
        tools_(tools),
        is_mounted_(false)
    {}

    ~Mountpoint() { do_cleanup(false); }

    void set(std::string &&path);
    void cleanup() { do_cleanup(true); }

    bool create() { return directory_.create(); }
    bool probe(bool store_state = true);
    bool mount(const std::string &device_name,
               const std::string &mount_options);

    bool exists(FailIf fail_if) const { return directory_.exists(fail_if); }
    bool is_mounted() const { return is_mounted_; }
    const std::string &str() const { return directory_.str(); }

  private:
    void do_cleanup(bool thoroughly);
};

}

#endif /* !AUTODIR_HH */
