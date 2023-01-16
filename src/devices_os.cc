/*
 * Copyright (C) 2015, 2017, 2019--2023  T+A elektroakustik GmbH & Co. KG
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
#include <cstring>

#include "devices_os.hh"
#include "devices_util.h"
#include "external_tools.hh"
#include "messages.h"

static const Automounter::ExternalTools *devices_os_tools;

class Tempfile
{
  private:
    static constexpr char NAME_TEMPLATE[] = "/tmp/mounta_udevadm.XXXXXX";

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

static size_t skip_key(const char *const str, size_t length, size_t offset)
{
    while(offset < length && str[offset] != '=')
        ++offset;

    return offset;
}

static size_t skip_value(const char *const str, size_t length, size_t offset)
{
    while(offset < length && str[offset] != '\n')
        ++offset;

    return offset;
}

static size_t skip_line(const char *const str, size_t length, size_t offset)
{
    offset = skip_value(str, length, offset);

    if(offset < length)
        ++offset;

    return offset;
}

static std::string copy_assigned_value(const char *const str, size_t length,
                                       size_t beginning_of_value)
{
    const auto beyond_value = skip_value(str, length, beginning_of_value);

    return beyond_value > beginning_of_value
        ? std::string(str + beginning_of_value + 1, beyond_value - beginning_of_value - 1)
        : std::string();
}

/*!
 * UUID types, from worst to best.
 */
enum class UUIDType
{
    NONE,
    PARTITION_TABLE,
    PARTITION_ENTRY,
    FILE_SYSTEM,
};

enum class ParseUUIDResult
{
    BAD_INPUT,
    SKIPPED_OTHER_KEY,
    SKIPPED_EMPTY,
    SKIPPED_WORSE_UUID,
    SUCCESS,
};

static ParseUUIDResult
try_parse_part_table_uuid_or_fs_uuid(const char *const assignment, size_t length,
                                     std::string &uuid, UUIDType &uuid_type_in_info)
{
    const size_t beyond_key = skip_key(assignment, length, 0);

    if(beyond_key >= length)
        return ParseUUIDResult::BAD_INPUT;

    if(strncmp(assignment, "ID_PART_TABLE_UUID", beyond_key) == 0)
    {
        /* partition table */
        switch(uuid_type_in_info)
        {
          case UUIDType::NONE:
            break;

          case UUIDType::PARTITION_TABLE:
            MSG_BUG("Duplicate partition table UUID");
            return ParseUUIDResult::SKIPPED_WORSE_UUID;

          case UUIDType::PARTITION_ENTRY:
          case UUIDType::FILE_SYSTEM:
            return ParseUUIDResult::SKIPPED_WORSE_UUID;
        }

        uuid = copy_assigned_value(assignment, length, beyond_key);
        if(uuid.empty())
            return ParseUUIDResult::SKIPPED_EMPTY;

        uuid_type_in_info = UUIDType::PARTITION_TABLE;
        return ParseUUIDResult::SUCCESS;
    }

    if(strncmp(assignment, "ID_PART_ENTRY_UUID", beyond_key) == 0)
    {
        /* partition table entry */
        switch(uuid_type_in_info)
        {
          case UUIDType::NONE:
          case UUIDType::PARTITION_TABLE:
            break;

          case UUIDType::PARTITION_ENTRY:
            MSG_BUG("Duplicate partition entry UUID");
            return ParseUUIDResult::SKIPPED_WORSE_UUID;

          case UUIDType::FILE_SYSTEM:
            return ParseUUIDResult::SKIPPED_WORSE_UUID;
        }

        uuid = copy_assigned_value(assignment, length, beyond_key);
        if(uuid.empty())
            return ParseUUIDResult::SKIPPED_EMPTY;

        uuid_type_in_info = UUIDType::PARTITION_ENTRY;
        return ParseUUIDResult::SUCCESS;
    }

    if(strncmp(assignment, "ID_FS_UUID", beyond_key) == 0)
    {
        /* file system */
        switch(uuid_type_in_info)
        {
          case UUIDType::NONE:
          case UUIDType::PARTITION_TABLE:
          case UUIDType::PARTITION_ENTRY:
            break;

          case UUIDType::FILE_SYSTEM:
            MSG_BUG("Duplicate file system UUID");
            return ParseUUIDResult::SKIPPED_WORSE_UUID;
        }

        uuid = copy_assigned_value(assignment, length, beyond_key);
        if(uuid.empty())
            return ParseUUIDResult::SKIPPED_EMPTY;

        uuid_type_in_info = UUIDType::FILE_SYSTEM;
        return ParseUUIDResult::SUCCESS;
    }

    return ParseUUIDResult::SKIPPED_OTHER_KEY;
}

static bool parse_device_info(const char *const output, size_t length,
                              const std::string &devlink, Devices::DeviceInfo &devinfo)
{
    size_t offset = 0;
    UUIDType uuid_type = UUIDType::NONE;

    while(offset < length)
    {
        const size_t start_of_line = offset;
        offset = skip_line(output, length, offset);
        const size_t line_length = offset - start_of_line;

        if(line_length < 4 ||
           output[start_of_line + 1] != ':' || output[start_of_line + 2] != ' ')
        {
            if(line_length >= 2)
                msg_error(0, LOG_NOTICE,
                          "Skipping unexpected udevadm output for device %s",
                          devlink.c_str());
            continue;
        }

        if(output[start_of_line] == 'P')
        {
            if(!parse_usb_device_id(&output[start_of_line + 3], line_length - 3, devinfo))
                return false;
        }
        else if(output[start_of_line] == 'E')
            try_parse_part_table_uuid_or_fs_uuid(&output[start_of_line + 3],
                                                 line_length - 3,
                                                 devinfo.device_uuid, uuid_type);
    }

    switch(uuid_type)
    {
      case UUIDType::NONE:
        msg_error(0, LOG_WARNING, "Device %s has no UUID", devlink.c_str());
        devinfo.device_uuid = "DO-NOT-STORE:";
        std::transform(
            devlink.begin(), devlink.end(), std::back_inserter(devinfo.device_uuid),
            [] (const char &ch) { return ch == '/' ? '_' : ch; });
        break;

      case UUIDType::PARTITION_TABLE:
      case UUIDType::PARTITION_ENTRY:
      case UUIDType::FILE_SYSTEM:
        break;
    }

    return
        !devinfo.device_uuid.empty() &&
        devinfo.type != Devices::DeviceType::UNKNOWN;
}

bool Devices::get_device_information(const std::string &devlink, DeviceInfo &devinfo)
{
    Tempfile tempfile;

    if(!tempfile.created())
        return false;

    if(os_system_formatted(msg_is_verbose(MESSAGE_LEVEL_DEBUG),
                           "%s info --query all \"%s\" >\"%s\"",
                           devices_os_tools->udevadm_.executable_.c_str(),
                           devlink.c_str(), tempfile.name()) < 0)
        return false;

    struct os_mapped_file_data output;
    const bool retval =
        (os_map_file_to_memory(&output, tempfile.name()) == 0)
        ? parse_device_info(static_cast<const char *>(output.ptr),
                            output.length, devlink, devinfo)
        : false;

    os_unmap_file(&output);

    return retval;
}

static bool parse_volume_info(const char *const output, size_t length,
                              const std::string &devname, Devices::VolumeInfo &volinfo)
{
    size_t offset = 0;
    UUIDType uuid_type = UUIDType::NONE;

    while(offset < length)
    {
        const size_t start_of_line = offset;
        offset = skip_line(output, length, offset);
        const size_t line_length = offset - start_of_line;

        if(line_length < 4 ||
           output[start_of_line + 1] != ':' || output[start_of_line + 2] != ' ')
        {
            if(line_length >= 2)
                msg_error(0, LOG_NOTICE,
                          "Skipping unexpected udevadm output for volume %s",
                          devname.c_str());
            continue;
        }

        if(output[start_of_line] != 'E')
            continue;

        const char *assignment = &output[start_of_line + 3];
        const size_t assignment_length = line_length - 3;

        switch(try_parse_part_table_uuid_or_fs_uuid(assignment, assignment_length,
                                                    volinfo.volume_uuid, uuid_type))
        {
          case ParseUUIDResult::SKIPPED_OTHER_KEY:
            break;

          case ParseUUIDResult::BAD_INPUT:
          case ParseUUIDResult::SKIPPED_EMPTY:
          case ParseUUIDResult::SKIPPED_WORSE_UUID:
          case ParseUUIDResult::SUCCESS:
            continue;
        }

        const size_t beyond_key = skip_key(assignment, assignment_length, 0);

        if(strncmp(assignment, "ID_FS_LABEL", beyond_key) == 0)
            volinfo.label = copy_assigned_value(assignment, assignment_length, beyond_key);
        else if(strncmp(assignment, "ID_FS_TYPE", beyond_key) == 0)
            volinfo.fstype = copy_assigned_value(assignment, assignment_length, beyond_key);
    }

    switch(uuid_type)
    {
      case UUIDType::PARTITION_TABLE:
        if(volinfo.idx <= 0)
            break;

        /* fall-through */

      case UUIDType::NONE:
        msg_error(0, LOG_WARNING, "Volume %s has no UUID", devname.c_str());
        volinfo.volume_uuid = "DO-NOT-STORE:";
        std::transform(
            devname.begin(), devname.end(), std::back_inserter(volinfo.volume_uuid),
            [] (const char &ch) { return ch == '/' ? '_' : ch; });
        break;

      case UUIDType::PARTITION_ENTRY:
      case UUIDType::FILE_SYSTEM:
        break;
    }

    return
        !volinfo.volume_uuid.empty() &&
        !volinfo.fstype.empty();
}

bool Devices::get_volume_information(const std::string &devname, VolumeInfo &info)
{
    msg_log_assert(devices_os_tools != nullptr);

    const int idx = devname_get_volume_number(devname.c_str());

    if(idx < 0)
        return false;

    Tempfile tempfile;

    if(!tempfile.created())
        return false;

    if(os_system_formatted(msg_is_verbose(MESSAGE_LEVEL_DEBUG),
                           "%s info --query all \"%s\" >\"%s\"",
                           devices_os_tools->udevadm_.executable_.c_str(),
                           devname.c_str(), tempfile.name()) < 0)
        return false;

    info.idx = (idx > 0) ? idx : -1;

    struct os_mapped_file_data output;
    const bool retval =
        (os_map_file_to_memory(&output, tempfile.name()) == 0)
        ? parse_volume_info(static_cast<const char *>(output.ptr),
                            output.length, devname, info)
        : false;

    os_unmap_file(&output);

    return retval;
}

static bool get_device_and_volume_devnames(const char *path, std::string &dev_device,
                                           std::string &vol_device)
{
    Tempfile tempfile;

    if(!tempfile.created())
        return false;

    if(os_system_formatted(msg_is_verbose(MESSAGE_LEVEL_DEBUG),
            "%s %s --output SOURCE \"%s\" >\"%s\"",
            devices_os_tools->findmnt_.executable_.c_str(),
            devices_os_tools->findmnt_.options_.c_str(), path,
            tempfile.name()) < 0)
        return false;

    struct os_mapped_file_data output;
    if(os_map_file_to_memory(&output, tempfile.name()) == 0)
        dev_device.assign(static_cast<const char *>(output.ptr), output.length);
    os_unmap_file(&output);

    while(!dev_device.empty() && dev_device.back() == '\n')
        dev_device.pop_back();

    if(dev_device.empty())
        return false;

    vol_device = dev_device;

    while(!dev_device.empty() && dev_device.back() >= '0' && dev_device.back() <= '9')
        dev_device.pop_back();

    return !dev_device.empty();
}

static bool skip_token(const char *line, size_t len, size_t &offset, size_t *end_of_token)
{
    while(offset < len && line[offset] != ' ' && line[offset] != '\n')
        ++offset;

    if(end_of_token != nullptr)
        *end_of_token = offset;

    if(offset >= len)
        return true;

    if(line[offset] == '\n')
    {
        ++offset;
        return true;
    }

    return false;
}

static std::string parse_device_link_from_line(const char *line, size_t len, size_t &offset)
{
    while(offset < len)
    {
        while(offset < len && line[offset] == ' ')
            ++offset;

        static const std::string prefix = "/dev/disk/by-id/";
        const auto remainder = len - offset;
        if(remainder < prefix.length())
            break;

        const auto start_of_token = offset;
        size_t i;
        for(i = 0; prefix[i] == line[offset]; ++i, ++offset)
            ;

        if(i != prefix.length())
        {
            /* not found, skip token and return if at EOL or EOF*/
            if(skip_token(line, len, offset, nullptr))
                break;

            continue;
        }

        /* found */
        size_t end_of_token;
        if(!skip_token(line, len, offset, &end_of_token))
        {
            /* skip until EOL or EOF */
            do
            {
                while(offset < len && line[offset] == ' ')
                    ++offset;
            }
            while(!skip_token(line, len, offset, nullptr));
        }

        return std::string(line + start_of_token, line + end_of_token);
    }

    return "";
}

static bool get_device_links(const std::string &dev_device, const std::string &vol_device,
                             std::pair<std::string, std::string> &result)
{
    Tempfile tempfile;

    if(!tempfile.created())
        return false;

    if(os_system_formatted(msg_is_verbose(MESSAGE_LEVEL_DEBUG),
            "%s info --query symlink --export --root \"%s\" \"%s\" >\"%s\"",
            devices_os_tools->udevadm_.executable_.c_str(),
            dev_device.c_str(), vol_device.c_str(), tempfile.name()) < 0)
        return false;

    struct os_mapped_file_data output;
    if(os_map_file_to_memory(&output, tempfile.name()) == 0)
    {
        size_t offset = 0;
        result.first = parse_device_link_from_line(static_cast<const char *>(output.ptr),
                                                   output.length, offset);
        result.second = parse_device_link_from_line(static_cast<const char *>(output.ptr),
                                                    output.length, offset);
    }
    os_unmap_file(&output);

    if(result.first.empty() || result.second.empty())
    {
        result.first.clear();
        result.second.clear();
    }

    return !result.first.empty();
}

std::pair<std::string, std::string>
Devices::map_mountpoint_path_to_device_links(const char *path)
{
    msg_log_assert(devices_os_tools != nullptr);

    std::pair<std::string, std::string> result;

    std::string dev_device, vol_device;
    if(!get_device_and_volume_devnames(path, dev_device, vol_device))
        return result;

    get_device_links(dev_device, vol_device, result);
    return result;
}
