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
#include <ctype.h>
#include <errno.h>

#include "devices_os.h"
#include "devices_util.h"
#include "messages.h"

static struct
{
    const char *cmd;
    const char *opts;
}
osdev_blkid_details;

void osdev_init(const char *blkid_command, const char *blkid_options)
{
    log_assert(blkid_command != NULL);

    osdev_blkid_details.cmd = blkid_command;
    osdev_blkid_details.opts = (blkid_options != NULL) ? blkid_options : "";
}

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

static size_t skip_whitespace(const char *const str, size_t length,
                            size_t *offset)
{
    while(*offset < length && isblank(str[*offset]))
        ++*offset;

    return *offset;
}

static size_t skip_key(const char *const str, size_t length, size_t *offset)
{
    while(*offset < length && isupper(str[*offset]))
        ++*offset;

    return *offset;
}

static size_t skip_value(const char *const str, size_t length, size_t *offset)
{
    while(*offset < length && str[*offset] != '\n')
        ++*offset;

    return *offset;
}

static size_t skip_line(const char *const str, size_t length, size_t *offset)
{
    skip_value(str, length, offset);

    if(*offset < length)
        ++*offset;

    return *offset;
}

static bool try_get_value(const char *const key, const char **value,
                          const char *const str,
                          size_t key_begin, size_t key_beyond,
                          size_t value_begin, size_t value_beyond)
{
    if(strncmp(key, str + key_begin, key_beyond - key_begin) != 0)
        return false;

    char *const old_value = (char *)*value;

    *value = strndup(str + value_begin, value_beyond - value_begin);

    if(old_value != NULL)
    {
        msg_error(0, LOG_NOTICE, "Got key \"%s\" multiple times from blkid", key);
        msg_error(0, LOG_NOTICE, "Old value: \"%s\"", old_value);
        msg_error(0, LOG_NOTICE, "New value: \"%s\"", *value);
        free(old_value);
    }

    if(*value == NULL)
        msg_out_of_memory("value from blkid output");

    return *value != NULL;
}

static bool parse_output(const char *const output, size_t length,
                         struct osdev_volume_info *info)
{
    info->label = NULL;
    info->fstype = NULL;

    size_t offset = 0;

    while(offset < length)
    {
        const size_t key_begin = skip_whitespace(output, length, &offset);
        const size_t key_beyond = skip_key(output, length, &offset);

        skip_whitespace(output, length, &offset);

        if(offset >= length || output[offset++] != '=')
        {
            skip_line(output, length, &offset);
            continue;
        }

        const size_t value_begin = skip_whitespace(output, length, &offset);
        const size_t value_beyond = skip_value(output, length, &offset);

        if(key_begin < key_beyond && value_begin < value_beyond)
        {
            if(!try_get_value("LABEL", &info->label, output,
                              key_begin, key_beyond, value_begin, value_beyond))
                try_get_value("TYPE", &info->fstype, output,
                              key_begin, key_beyond, value_begin, value_beyond);
        }

        skip_line(output, length, &offset);
    }

    if(info->fstype == NULL)
        osdev_free_volume_information(info);

    return info->fstype != NULL;
}

bool osdev_get_volume_information(const char *devname, struct osdev_volume_info *info)
{
    log_assert(devname != NULL);
    log_assert(info != NULL);
    log_assert(osdev_blkid_details.cmd != NULL);
    log_assert(osdev_blkid_details.opts != NULL);

    const int idx = devname_get_volume_number(devname);

    if(idx < 0)
        return false;

    char tempfile_name[] = "/tmp/mounta_blkid.XXXXXX";
    const int fd = mkstemp(tempfile_name);

    if(fd < 0)
    {
        msg_error(errno, LOG_ERR, "Failed creating temporary file");
        return false;
    }

    os_file_close(fd);

    if(os_system_formatted("%s %s -o export %s >\"%s\"",
                           osdev_blkid_details.cmd, osdev_blkid_details.opts,
                           devname, tempfile_name) < 0)
        return false;

    info->idx = (idx > 0) ? idx : -1;

    struct os_mapped_file_data output;
    const bool retval =
        (os_map_file_to_memory(&output, tempfile_name) == 0)
        ? parse_output(output.ptr, output.length, info)
        : false;

    os_file_delete(tempfile_name);
    os_unmap_file(&output);

    return retval;
}

void osdev_free_volume_information(struct osdev_volume_info *info)
{
    log_assert(info != NULL);

    free((void *)info->label);
    free((void *)info->fstype);
    memset(info, 0, sizeof(*info));
}
