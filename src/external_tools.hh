/*
 * Copyright (C) 2017, 2019, 2021  T+A elektroakustik GmbH & Co. KG
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

#ifndef EXTERNAL_TOOLS_HH
#define EXTERNAL_TOOLS_HH

#include <string>

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
    const Command mountpoint_;
    const Command udevadm_;

    ExternalTools(const ExternalTools &) = delete;
    ExternalTools &operator=(const ExternalTools &) = delete;

    explicit ExternalTools(Command &&mount, Command &&unmount,
                           Command &&mountpoint, Command &&udevadm):
        mount_(std::move(mount)),
        unmount_(std::move(unmount)),
        mountpoint_(std::move(mountpoint)),
        udevadm_(std::move(udevadm))
    {}
};

}

#endif /* !EXTERNAL_TOOLS_HH */
