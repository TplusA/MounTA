/*
 * Copyright (C) 2015, 2019  T+A elektroakustik GmbH & Co. KG
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

#if HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <errno.h>

#include "devices_util.h"
#include "messages.h"

static const char *find_trailing_number(const char *devname)
{
    const char *p;

    for(p = devname + strlen(devname) - 1; p > devname; --p)
    {
        if(!isdigit(*p))
        {
            ++p;
            break;
        }
    }

    return (*p != '\0') ? p : NULL;
}

int devname_get_volume_number(const char *devname)
{
    log_assert(devname != NULL);
    log_assert(devname[0] != '\0');

    if(isdigit(devname[0]))
    {
        msg_error(EINVAL, LOG_NOTICE, "Invalid device name: \"%s\"", devname);
        return -1;
    }

    const char *const start_of_number = find_trailing_number(devname);

    if(start_of_number == NULL)
        return 0;

    char *endptr;
    const unsigned long temp = strtoul(start_of_number, &endptr, 10);

    if(*endptr == '\0')
        return temp;

    if(temp > INT_MAX || (temp == ULONG_MAX && errno == ERANGE))
        msg_error(ERANGE, LOG_NOTICE,
                  "Number in device name out of range: \"%s\"", devname);
    else
        BUG("Failed parsing validated number");

    return -1;
}
