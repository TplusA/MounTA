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

#ifndef AUTOMOUNTER_HH
#define AUTOMOUNTER_HH

#include <string>

#include "device_manager.hh"

namespace Automounter
{

class ExternalTools
{
  public:
    class Command
    {
      public:
        const std::string executable_;
        const std::string options_;

        Command(const Command &) = delete;
        Command &operator=(const Command &) = delete;
        Command(Command &&) = default;

        explicit Command(const char *executable, const char *options):
            executable_(executable),
            options_(options != nullptr ? options : "")
        {}
    };

    const Command mount_;
    const Command unmount_;

    ExternalTools(const ExternalTools &) = delete;
    ExternalTools &operator=(const ExternalTools &) = delete;
    ExternalTools(ExternalTools &&) = default;

    explicit ExternalTools(const char *mount,   const char *mount_default_options,
                           const char *unmount, const char *unmount_default_options):
        mount_(mount,     mount_default_options != nullptr ? mount_default_options : "-o ro"),
        unmount_(unmount, unmount_default_options)
    {}
};

class Core
{
  private:
    const std::string working_directory_;
    const ExternalTools &tools_;
    Devices::AllDevices devman_;

  public:
    Core(const Core &) = delete;
    Core &operator=(const Core &) = delete;
    Core(Core &&) = default;

    explicit Core(const char *working_directory, const ExternalTools &tools):
        working_directory_(working_directory),
        tools_(std::move(tools))
    {}

    void handle_new_device(const char *device_path);
    void handle_removed_device(const char *device_path);
    void shutdown();

    class const_iterator
    {
      private:
        std::map<Devices::ID::value_type, Devices::Device *>::const_iterator dev_iter_;

      public:
        explicit constexpr const_iterator(decltype(dev_iter_) &&dev_iter):
            dev_iter_(dev_iter)
        {}

        const Devices::Device &operator*() const
        {
            return *dev_iter_->second;
        }

        bool operator!=(const const_iterator &it) const
        {
            return dev_iter_ != it.dev_iter_;
        }

        const_iterator &operator++()
        {
            ++dev_iter_;
            return *this;
        }
    };

    const_iterator begin() const { return const_iterator(devman_.begin()); }
    const_iterator end() const   { return const_iterator(devman_.end()); }
};

}

#endif /* !AUTOMOUNTER_HH */
