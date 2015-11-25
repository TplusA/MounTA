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

#ifndef DEVICES_OS_H
#define DEVICES_OS_H

#include <stdbool.h>

enum osdev_device_type
{
    OSDEV_DEVICE_TYPE_UNKNOWN,
    OSDEV_DEVICE_TYPE_USB,
};

struct osdev_device_info
{
    enum osdev_device_type type;

    struct
    {
        unsigned int hub_id;
        unsigned int port;
    }
    usb;
};

struct osdev_volume_info
{
    int idx;
    const char *label;
    const char *fstype;
};

#ifdef __cplusplus
extern "C" {
#endif

/*!
 * Pass details about the \c blkid command.
 *
 * It is required to call this function before calling
 * ##osdev_get_volume_information().
 */
void osdev_init(const char *blkid_command, const char *blkid_options);

/*!
 * Get device information if possible.
 *
 * \param devlink
 *     Name of the device symlink.
 *
 * \param[out] devinfo
 *     Information about the device with given name. Must be freed by caller
 *     via #osdev_free_device_information().
 *
 * \returns
 *     True on success, false on error. It is safe to pass the \p devinfo
 *     structure to #osdev_free_device_information() even in case of error.
 */
bool osdev_get_device_information(const char *devlink, struct osdev_device_info *devinfo);

/*!
 * Free resources allocated by a device information structure.
 *
 * Note that the memory occupied by the \p devinfo object itself is not freed
 * by this function. The caller is responsible for managing storage of this
 * object.
 */
void osdev_free_device_information(struct osdev_device_info *devinfo);

/*!
 * Get volume information if possible.
 *
 * \param devname
 *     Name of a block device.
 *
 * \param[out] volinfo
 *     Information about the volume stored on the given block device.
 *     Must be freed by caller via #osdev_free_volume_information().
 *
 * \returns
 *     True if the given device contains a volume that could possibly be
 *     mounted, false if it doesn't (or on error). If this function returns
 *     true, then the volume information is returned in \p volinfo (which must
 *     be freed by the caller); otherwise, a blank volume information structure
 *     is returned (which is safe to be passed to
 *     #osdev_free_volume_information()).
 */
bool osdev_get_volume_information(const char *devname, struct osdev_volume_info *volinfo);

/*!
 * Free resources allocated by a volume information structure.
 *
 * Note that the memory occupied by the \p volinfo object itself is not freed
 * by this function. The caller is responsible for managing storage of this
 * object.
 */
void osdev_free_volume_information(struct osdev_volume_info *volinfo);

#ifdef __cplusplus
}
#endif

#endif /* !DEVICES_OS_H */
