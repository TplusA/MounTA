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

#ifndef AUTOMOUNTER_HH
#define AUTOMOUNTER_HH

#include <string>
#include <map>

#include "device_manager.hh"

namespace Automounter
{

class ExternalTools;

class FSMountOptions
{
  private:
    const std::map<const std::string, const char *const> options_;

  public:
    FSMountOptions(const FSMountOptions &) = delete;
    FSMountOptions &operator=(const FSMountOptions &) = delete;

    /*!
     * Ctor for #FSMountOptions.
     *
     * \param options
     *     A map that stores a pointer to a plain C string of extra mount
     *     options per file system. To express that a file system is supported,
     *     but no extra options are required, the file system should be
     *     explicitly mapped to \c nullptr.
     */
    explicit FSMountOptions(std::map<const std::string, const char *const> &&options):
        options_(options)
    {}

    /*!
     * Get mount options specific to given file system.
     *
     * \param fstype
     *     Name of the file system as it is called by the Linux kernel and as
     *     reported by the \c blkid tool.
     *
     * \returns
     *     A string that is safe to add to the \c mount command, guaranteed to
     *     be non-NULL. In case there are no specific options, this function
     *     returns the empty string.
     */
    const char *get_options(const std::string &fstype) const;
};

class Core
{
  private:
    const std::string working_directory_;
    const FSMountOptions &mount_options_;
    Devices::AllDevices devman_;

  public:
    Core(const Core &) = delete;
    Core &operator=(const Core &) = delete;
    Core(Core &&) = default;

    explicit Core(const char *working_directory, const ExternalTools &tools,
                  const FSMountOptions &mount_options,
                  const std::string& symlink_directory):
        working_directory_(working_directory),
        mount_options_(mount_options),
        devman_(tools, symlink_directory)
    {}

    void handle_new_device(const char *device_path);
    void handle_removed_device(const char *device_path);
    void shutdown();

    class const_iterator
    {
      private:
        Devices::AllDevices::DevContainerType::const_iterator dev_iter_;

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
