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
