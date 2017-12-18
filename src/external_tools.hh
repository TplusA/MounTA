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
    const Command blkid_;

    ExternalTools(const ExternalTools &) = delete;
    ExternalTools &operator=(const ExternalTools &) = delete;

    explicit ExternalTools(Command &&mount, Command &&unmount,
                           Command &&mountpoint, Command &&blkid):
        mount_(std::move(mount)),
        unmount_(std::move(unmount)),
        mountpoint_(std::move(mountpoint)),
        blkid_(std::move(blkid))
    {}
};

}

#endif /* !EXTERNAL_TOOLS_HH */
