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

#include "devices_os.h"
#include "messages.h"

bool osdev_get_volume_information(const char *devname, struct osdev_volume_info *info)
{
    log_assert(devname != NULL);
    log_assert(info != NULL);

    BUG("osdev_get_volume_information() not implemented");
    return false;
}

void osdev_free_volume_information(struct osdev_volume_info *info)
{
    log_assert(info != NULL);

    BUG("osdev_get_volume_information() not implemented");
}
