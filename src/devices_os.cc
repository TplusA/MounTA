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

#if HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include "devices_os.hh"
#include "devices_util.h"
#include "external_tools.hh"
#include "messages.h"

static const Automounter::ExternalTools *devices_os_tools;

void Devices::init(const Automounter::ExternalTools &tools)
{
    devices_os_tools = &tools;
}

bool Devices::get_device_information(const std::string &devlink, DeviceInfo &devinfo)
{
    /* FIXME: These are fake information. */
    devinfo.type = DeviceType::USB;
    devinfo.usb.hub_id = 25;
    devinfo.usb.port = 3;

    return true;
}

static size_t skip_whitespace(const char *const str, size_t length,
                              size_t &offset)
{
    while(offset < length && isblank(str[offset]))
        ++offset;

    return offset;
}

static size_t skip_key(const char *const str, size_t length, size_t &offset)
{
    while(offset < length && isupper(str[offset]))
        ++offset;

    return offset;
}

static size_t skip_value(const char *const str, size_t length, size_t &offset)
{
    while(offset < length && str[offset] != '\n')
        ++offset;

    return offset;
}

static size_t skip_line(const char *const str, size_t length, size_t &offset)
{
    skip_value(str, length, offset);

    if(offset < length)
        ++offset;

    return offset;
}

static bool try_get_value(const std::string &key, std::string &value,
                          const char *const str,
                          size_t key_begin, size_t key_beyond,
                          size_t value_begin, size_t value_beyond)
{
    const size_t key_len = key_beyond - key_begin;

    if(key.length() != key_len ||
       key.compare(0, std::string::npos, str + key_begin, key_len) != 0)
        return false;

    std::string old_value(std::move(value));

    value.assign(str + value_begin, value_beyond - value_begin);

    if(!old_value.empty())
    {
        msg_error(0, LOG_NOTICE, "Got key \"%s\" multiple times from blkid", key.c_str());
        msg_error(0, LOG_NOTICE, "Old value: \"%s\"", old_value.c_str());
        msg_error(0, LOG_NOTICE, "New value: \"%s\"", value.c_str());
    }

    if(value.empty())
        msg_out_of_memory("value from blkid output");

    return !value.empty();
}

static bool parse_output(const char *const output, size_t length,
                         Devices::VolumeInfo &info)
{
    info.label.clear();
    info.fstype.clear();

    size_t offset = 0;

    while(offset < length)
    {
        const size_t key_begin = skip_whitespace(output, length, offset);
        const size_t key_beyond = skip_key(output, length, offset);

        skip_whitespace(output, length, offset);

        if(offset >= length || output[offset++] != '=')
        {
            skip_line(output, length, offset);
            continue;
        }

        const size_t value_begin = skip_whitespace(output, length, offset);
        const size_t value_beyond = skip_value(output, length, offset);

        if(key_begin < key_beyond && value_begin < value_beyond)
        {
            if(!try_get_value("LABEL", info.label, output,
                              key_begin, key_beyond, value_begin, value_beyond))
                try_get_value("TYPE", info.fstype, output,
                              key_begin, key_beyond, value_begin, value_beyond);
        }

        skip_line(output, length, offset);
    }

    return !info.fstype.empty();
}

bool Devices::get_volume_information(const std::string &devname, VolumeInfo &info)
{
    log_assert(devices_os_tools != nullptr);

    const int idx = devname_get_volume_number(devname.c_str());

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

    if(os_system_formatted(msg_is_verbose(MESSAGE_LEVEL_DEBUG),
                           "%s %s -o export %s >\"%s\"",
                           devices_os_tools->blkid_.executable_.c_str(),
                           devices_os_tools->blkid_.options_.c_str(),
                           devname.c_str(), tempfile_name) < 0)
        return false;

    info.idx = (idx > 0) ? idx : -1;

    struct os_mapped_file_data output;
    const bool retval =
        (os_map_file_to_memory(&output, tempfile_name) == 0)
        ? parse_output(static_cast<const char *>(output.ptr), output.length,
                       info)
        : false;

    os_file_delete(tempfile_name);
    os_unmap_file(&output);

    return retval;
}
