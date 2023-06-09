/*
 * Copyright (C) 2015, 2019, 2020, 2023  T+A elektroakustik GmbH & Co. KG
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

#include <string.h>

#include "dbus_iface.h"
#include "dbus_iface_deep.h"
#include "dbus_handlers.h"
#include "de_tahifi_mounta.h"
#include "messages.h"

struct dbus_data
{
    guint owner_id;
    int acquired;

    bool connect_to_session_bus;
    tdbusMounTA *mounta_iface;
    void *mounta_iface_user_data;
};

static int handle_dbus_error(GError **error)
{
    if(*error == NULL)
        return 0;

    msg_error(0, LOG_EMERG, "%s", (*error)->message);
    g_error_free(*error);
    *error = NULL;

    return -1;
}

static void bus_acquired(GDBusConnection *connection,
                         const gchar *name, gpointer user_data)
{
    struct dbus_data *data = user_data;

    msg_info("D-Bus \"%s\" acquired (%s bus)",
             name, data->connect_to_session_bus ? "session" : "system");

    data->mounta_iface = tdbus_moun_ta_skeleton_new();

    g_signal_connect(data->mounta_iface, "handle-get-all",
                     G_CALLBACK(dbusmethod_get_all),
                     data->mounta_iface_user_data);

    GError *error = NULL;
    g_dbus_interface_skeleton_export(G_DBUS_INTERFACE_SKELETON(data->mounta_iface),
                                     connection, "/de/tahifi/MounTA", &error);
    (void)handle_dbus_error(&error);
}

static void name_acquired(GDBusConnection *connection,
                          const gchar *name, gpointer user_data)
{
    struct dbus_data *data = user_data;

    msg_info("D-Bus name \"%s\" acquired", name);
    data->acquired = 1;
}

static void name_lost(GDBusConnection *connection,
                      const gchar *name, gpointer user_data)
{
    struct dbus_data *data = user_data;

    msg_info("D-Bus name \"%s\" lost", name);
    data->acquired = -1;
}

static void destroy_notification(gpointer data)
{
    msg_info("Bus destroyed.");
}

static struct dbus_data dbus_data;

int dbus_setup(GMainLoop *loop, bool connect_to_session_bus,
               void *automounter_for_dbus_handlers)
{
    memset(&dbus_data, 0, sizeof(dbus_data));

    dbus_data.connect_to_session_bus = connect_to_session_bus;
    dbus_data.mounta_iface_user_data = automounter_for_dbus_handlers;

    GBusType bus_type =
        connect_to_session_bus ? G_BUS_TYPE_SESSION : G_BUS_TYPE_SYSTEM;

    static const char bus_name[] = "de.tahifi.MounTA";

    dbus_data.owner_id =
        g_bus_own_name(bus_type, bus_name, G_BUS_NAME_OWNER_FLAGS_NONE,
                       bus_acquired, name_acquired, name_lost, &dbus_data,
                       destroy_notification);

    while(dbus_data.owner_id != 0 && dbus_data.acquired == 0)
    {
        /* do whatever has to be done behind the scenes until one of the
         * guaranteed callbacks gets called */
        g_main_context_iteration(NULL, TRUE);
    }

    if(dbus_data.owner_id > 0 && dbus_data.acquired < 0)
    {
        msg_error(0, LOG_EMERG, "Failed acquiring D-Bus name");
        return -1;
    }

    msg_log_assert(dbus_data.mounta_iface != NULL);

    g_main_loop_ref(loop);

    return 0;
}

void dbus_shutdown(GMainLoop *loop)
{
    if(loop == NULL)
        return;

    g_bus_unown_name(dbus_data.owner_id);
    g_main_loop_unref(loop);

    g_object_unref(dbus_data.mounta_iface);
}

tdbusMounTA *dbus_get_mounta_iface(void)
{
    return dbus_data.mounta_iface;
}
