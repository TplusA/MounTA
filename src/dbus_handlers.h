/*
 * Copyright (C) 2015, 2018, 2019, 2020  T+A elektroakustik GmbH & Co. KG
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

#ifndef DBUS_HANDLERS_H
#define DBUS_HANDLERS_H

#pragma GCC diagnostic push
#ifdef __cplusplus
#pragma GCC diagnostic ignored "-Wold-style-cast"
#endif /* __cplusplus */
#pragma GCC diagnostic ignored "-Wcast-qual"
#include "de_tahifi_mounta.h"
#pragma GCC diagnostic pop

#ifdef __cplusplus
extern "C" {
#endif

gboolean dbusmethod_get_all(tdbusMounTA *object,
                            GDBusMethodInvocation *invocation,
                            void *user_data);

#ifdef __cplusplus
}
#endif

#endif /* !DBUS_HANDLERS_H */
