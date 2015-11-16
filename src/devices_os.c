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

#include <stdlib.h>
#include <string.h>

#include "devices_os.h"
#include "devices_util.h"
#include "messages.h"

bool osdev_get_device_information(const char *devlink, struct osdev_device_info *devinfo)
{
    log_assert(devlink != NULL);
    log_assert(devinfo != NULL);

    /* FIXME: These are fake information. */
    devinfo->type = OSDEV_DEVICE_TYPE_USB;
    devinfo->usb.hub_id = 25;
    devinfo->usb.port = 3;

    return true;
}

void osdev_free_device_information(struct osdev_device_info *devinfo)
{
    log_assert(devinfo != NULL);

    /*
     * Nothing to do at the moment.
     *
     * We just make sure that this function gets called to avoid leaks
     * introduced by future extensions.
     */
}

bool osdev_get_volume_information(const char *devname, struct osdev_volume_info *info)
{
    log_assert(devname != NULL);
    log_assert(info != NULL);

    const int idx = devname_get_volume_number(devname);

    if(idx < 0)
        return false;

    /* FIXME: This should rely on the outcome of \c blkid.
     *        Fail if there is no volume. */
    if(idx == 0)
        return false;

    info->idx = (idx > 0) ? idx : -1;
    info->label = strdup("<BUG: get label not implemented yet>");
    info->fstype = strdup("<BUG: get fstype not implemented yet>");

    return true;
}

void osdev_free_volume_information(struct osdev_volume_info *info)
{
    log_assert(info != NULL);

    free((void *)info->label);
    free((void *)info->fstype);
    memset(info, 0, sizeof(*info));
}
