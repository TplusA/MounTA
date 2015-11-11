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

#include "automounter.hh"
#include "messages.h"

void Automounter::Core::handle_new_device(const char *device_path)
{
    msg_info("New device \"%s\"", device_path);
    BUG("AM new device not implemented");
}

void Automounter::Core::handle_removed_device(const char *device_path)
{
    msg_info("Removed device \"%s\"", device_path);
    BUG("AM remove device not implemented");
}

void Automounter::Core::shutdown()
{
    BUG("AM shutdown not implemented");
}
