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

#include "dbus_handlers.h"
#include "automounter.hh"
#include "messages.h"

gboolean dbusmethod_get_all(tdbusMounTA *object,
                            GDBusMethodInvocation *invocation,
                            void *user_data)
{
    static const char iface_name[] = "de.tahifi.MounTA";

    msg_info("%s method invocation from '%s': %s",
             iface_name, g_dbus_method_invocation_get_sender(invocation),
             g_dbus_method_invocation_get_method_name(invocation));

    auto am = static_cast<const Automounter::Core *>(user_data);
    msg_info("AM: %p", am);

    g_dbus_method_invocation_return_error(invocation,
                                          G_DBUS_ERROR, G_DBUS_ERROR_NOT_SUPPORTED,
                                          "not implemented");

    return TRUE;
}

