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
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

#include "os.h"
#include "messages.h"

void os_abort(void)
{
    abort();
}

char *os_resolve_symlink(const char *link)
{
    log_assert(link != NULL);

    char dummy;

    if(readlink(link, &dummy, sizeof(dummy)) < 0)
    {
        if(errno == EINVAL)
            msg_error(errno, LOG_NOTICE,
                      "Path \"%s\" is not a symlink", link);
        else
            msg_error(errno, LOG_NOTICE,
                      "readlink() failed for path \"%s\"", link);

        return NULL;
    }

    char *const result = realpath(link, NULL);

    if(result == NULL)
        msg_error(errno, LOG_NOTICE,
                  "Failed resolving symlink \"%s\"", link);

    return result;
}
