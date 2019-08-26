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

#ifndef DEVICES_UTIL_H
#define DEVICES_UTIL_H

#ifdef __cplusplus
extern "C" {
#endif

/*!
 * Parse volume number for block device name.
 *
 * \param devname
 *     A device name such as `/dev/sda`, `/dev/sdb5`, or `/dev/sdx123`.
 *
 * \returns
 *     The volume number, or -1 if the number could not be parsed. Note that 0
 *     will be returned in case the device name does not end with a number.
 *     Number -1 is only returned for names that start with a digit, end with a
 *     number that causes an integer overflow, or on internal fault.
 */
int devname_get_volume_number(const char *devname);

#ifdef __cplusplus
}
#endif

#endif /* !DEVICES_UTIL_H */
