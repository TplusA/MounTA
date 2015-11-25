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

#ifndef DBUS_HANDLERS_H
#define DBUS_HANDLERS_H

#include "mounta_dbus.h"

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