/*
 * Copyright (C) 2015, 2017, 2019, 2020  T+A elektroakustik GmbH & Co. KG
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

#include <algorithm>

#include "devices_os.hh"
#include "devices_util.h"
#include "external_tools.hh"
#include "messages.h"

static const Automounter::ExternalTools *devices_os_tools;

class Tempfile
{
  private:
    static constexpr char NAME_TEMPLATE[] = "/tmp/mounta_blkid.XXXXXX";

    char name_[sizeof(NAME_TEMPLATE)];
    int fd_;
    int errno_;
    bool suceeded_;

  public:
    Tempfile(const Tempfile &) = delete;
    Tempfile &operator=(const Tempfile &) = delete;

    explicit Tempfile(bool keep_open = false):
        errno_(0),
        suceeded_(true)
    {
        std::copy(NAME_TEMPLATE, NAME_TEMPLATE + sizeof(NAME_TEMPLATE), name_);

        fd_ = mkstemp(name_);

        if(fd_ < 0)
        {
            errno_ = errno;
            suceeded_ =  false;
            name_[0] = '\0';
            msg_error(errno_, LOG_ERR, "Failed creating temporary file");
        }
        else if(!keep_open)
        {
            os_file_close(fd_);
            fd_ = -1;
        }
    }

    ~Tempfile()
    {
        if(fd_ >= 0)
            os_file_close(fd_);

        if(name_[0] != '\0')
            os_file_delete(name_);
    }

    bool created() const { return suceeded_; }
    int error_code() const { return errno_; }

    const char *name() const { return name_; }
};

constexpr char Tempfile::NAME_TEMPLATE[];

void Devices::init(const Automounter::ExternalTools &tools)
{
    devices_os_tools = &tools;
}

/*
 * Example input:
 * /devices/platform/bcm2708_usb/usb1/1-1/1-1.5/1-1.5:1.0/host6/target6:0:0/6:0:0:0/block/sda
 *
 * This function attempts to find the part before the "/host6/" part and
 * returns a full, absolute path to that location. We assume that the sysfs is
 * always mounted to "/sys".
 */
static bool parse_usb_device_id(const char *const output, size_t length,
                                Devices::DeviceInfo &info)
{
    if(length == 0 || output[0] != '/')
        return false;

    const char *from(output + 1);

    while(true)
    {
        auto to(std::find_if(from, output + length,
                             [] (const char ch) { return ch == '/'; }));

        if(to >= output + length)
            return false;

        static const char key[] = "host";

        if(std::distance(from, to) >= ssize_t(sizeof(key)) &&
           std::equal(from, from + sizeof(key) - 1, key) &&
           from[sizeof(key) - 1] >= '0' && from[sizeof(key) - 1] <= '9')
        {
            break;
        }

        from = to + 1;
    }

    static const char sysfs_mountpoint[] = "/sys";

    info.type = Devices::DeviceType::USB;
    info.usb_port_sysfs_name = sysfs_mountpoint;
    info.usb_port_sysfs_name.append(output, std::distance(output, from) - 1);

    return true;
}

bool Devices::get_device_information(const std::string &devlink, DeviceInfo &devinfo)
{
    Tempfile tempfile;

    if(!tempfile.created())
        return false;

    if(os_system_formatted(msg_is_verbose(MESSAGE_LEVEL_DEBUG),
                           "%s info --query path \"%s\" >\"%s\"",
                           devices_os_tools->udevadm_.executable_.c_str(),
                           devlink.c_str(), tempfile.name()) < 0)
        return false;

    struct os_mapped_file_data output;
    const bool retval =
        (os_map_file_to_memory(&output, tempfile.name()) == 0)
        ? parse_usb_device_id(static_cast<const char *>(output.ptr),
                              output.length, devinfo)
        : false;

    os_unmap_file(&output);

    return retval;
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

    value = "";  /* we have moved from value: better safe than sorry */
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

static bool parse_blkid_output(const char *const output, size_t length,
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

    Tempfile tempfile;

    if(!tempfile.created())
        return false;

    if(os_system_formatted(msg_is_verbose(MESSAGE_LEVEL_DEBUG),
                           "%s %s -o export %s >\"%s\"",
                           devices_os_tools->blkid_.executable_.c_str(),
                           devices_os_tools->blkid_.options_.c_str(),
                           devname.c_str(), tempfile.name()) < 0)
        return false;

    info.idx = (idx > 0) ? idx : -1;

    struct os_mapped_file_data output;
    const bool retval =
        (os_map_file_to_memory(&output, tempfile.name()) == 0)
        ? parse_blkid_output(static_cast<const char *>(output.ptr),
                             output.length, info)
        : false;

    os_unmap_file(&output);

    return retval;
}
