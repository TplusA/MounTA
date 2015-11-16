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
#include <stdarg.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>

#include "os.h"
#include "messages.h"

void os_abort(void)
{
    abort();
}

int os_system(const char *command)
{
    msg_info("Executing external command: %s", command);

    const int ret = system(command);

    if(ret == EXIT_SUCCESS)
        msg_info("External command succeeded");
    else
        msg_error(0, LOG_ERR, "External command failed, exit code %d", ret);

    return ret;
}

int os_system_formatted(const char *format_string, ...)
{
    char buffer[1024];

    va_list va;
    va_start(va, format_string);

    (void)vsnprintf(buffer, sizeof(buffer), format_string, va);

    va_end(va);

    return os_system(buffer);
}

static bool is_valid_directory_name(const char *path)
{
    if(path[0] != '.')
        return true;

    if(path[1] == '.')
        return path[2] != '\0';
    else
        return path[1] != '\0';
}

bool os_foreach_in_path(const char *path,
                        void (*callback)(const char *path, void *user_data),
                        void *user_data)
{
    log_assert(path != NULL);
    log_assert(callback != NULL);

    DIR *dir = opendir(path);

    if(dir == NULL)
    {
        msg_error(errno, LOG_ERR, "Failed reading directory \"%s\"", path);
        return false;
    }

    bool retval = true;

    while(true)
    {
        errno = 0;
        struct dirent *result = readdir(dir);

        if(result != NULL)
        {
            if(is_valid_directory_name(result->d_name))
                callback(result->d_name, user_data);
        }
        else
        {
            retval = (errno == 0);
            break;
        }
    }

    closedir(dir);

    return retval;
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

bool os_mkdir_hierarchy(const char *path, bool must_not_exist)
{
    log_assert(path != NULL);

    if(must_not_exist)
    {
        struct stat buf;

        if(lstat(path, &buf) == 0)
        {
            msg_error(EEXIST, LOG_ERR, "Failed creating directory hierarchy %s", path);
            return false;
        }
    }

    /* oh well... */
    return os_system_formatted("mkdir -m 0750 -p %s", path) == EXIT_SUCCESS;
}

bool os_mkdir(const char *path, bool must_not_exist)
{
    log_assert(path != NULL);

    if(mkdir(path, 0750) == 0)
        return true;

    if(errno == EEXIST && !must_not_exist)
    {
        /* better make sure this is really a directory */
        struct stat buf;

        if(lstat(path, &buf) == 0 && S_ISDIR(buf.st_mode))
            return true;

        errno = EEXIST;
    }

    msg_error(errno, LOG_ERR, "Failed creating directory %s", path);

    return false;
}

bool os_rmdir(const char *path, bool must_exist)
{
    log_assert(path != NULL);

    if(rmdir(path) == 0)
        return true;

    if(must_exist)
        msg_error(errno, LOG_ERR, "Failed removing directory %s", path);

    return false;
}
