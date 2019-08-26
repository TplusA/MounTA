/*
 * Copyright (C) 2015, 2017, 2019  T+A elektroakustik GmbH & Co. KG
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

    GVariantBuilder devices_builder;
    GVariantBuilder volumes_builder;
    g_variant_builder_init(&devices_builder, G_VARIANT_TYPE("a(qsss)"));
    g_variant_builder_init(&volumes_builder, G_VARIANT_TYPE("a(ussq)"));

    auto am = static_cast<const Automounter::Core *>(user_data);
    log_assert(am != nullptr);

    for(const auto &device : *am)
    {
        if(device.get_state() != Devices::Device::OK)
            continue;

        /* note: this duplicates #announce_new_device() */
        g_variant_builder_add(&devices_builder,
                              "(qsss)",
                              device.get_id(),
                              device.get_display_name().c_str(),
                              device.get_working_directory().str().c_str(),
                              device.get_usb_port().c_str());

        for(const auto &volume_iter : device)
        {
            const auto &volume = *volume_iter.second;

            if(volume.get_state() != Devices::Volume::MOUNTED)
                continue;

            /* note: this duplicates #announce_new_volume() */
            g_variant_builder_add(&volumes_builder,
                                  "(ussq)",
                                  volume.get_index() >= 0 ? volume.get_index() : UINT_MAX,
                                  volume.get_label().c_str(),
                                  volume.get_mountpoint_name().c_str(),
                                  volume.get_device()->get_id());
        }
    }

    GVariant *const devices = g_variant_builder_end(&devices_builder);
    GVariant *const volumes = g_variant_builder_end(&volumes_builder);

    if(devices != nullptr && volumes != nullptr)
        tdbus_moun_ta_complete_get_all(object, invocation, devices, volumes);
    else
    {
        if(devices != nullptr)
            g_variant_unref(devices);

        if(volumes != nullptr)
            g_variant_unref(volumes);

        g_dbus_method_invocation_return_error(invocation,
                                              G_DBUS_ERROR, G_DBUS_ERROR_NO_MEMORY,
                                              "Failed building answer");
    }

    return TRUE;
}

