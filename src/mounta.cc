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

#include <cstring>
#include <iostream>

#include <glib-object.h>
#include <glib-unix.h>

#include "fdevents.hh"
#include "automounter.hh"
#include "dbus_iface.h"
#include "messages.h"
#include "versioninfo.h"

struct parameters
{
    bool run_in_foreground;
    bool connect_to_session_dbus;
    const char *working_directory;
    const char *mount_tool;
    const char *unmount_tool;
    const char *blkid_tool;
};

static void show_version_info(void)
{
    printf("%s\n"
           "Revision %s%s\n"
           "         %s+%d, %s\n",
           PACKAGE_STRING,
           VCS_FULL_HASH, VCS_WC_MODIFIED ? " (tainted)" : "",
           VCS_TAG, VCS_TICK, VCS_DATE);
}

static void log_version_info(void)
{
    msg_info("Rev %s%s, %s+%d, %s",
             VCS_FULL_HASH, VCS_WC_MODIFIED ? " (tainted)" : "",
             VCS_TAG, VCS_TICK, VCS_DATE);
}

/*!
 * Set up logging, daemonize.
 */
static int setup(const struct parameters *parameters, GMainLoop **loop)
{
    *loop = NULL;

    msg_enable_syslog(!parameters->run_in_foreground);

    if(!parameters->run_in_foreground)
        openlog("drcpd", LOG_PID, LOG_DAEMON);

    if(!parameters->run_in_foreground)
    {
        if(daemon(0, 0) < 0)
        {
            msg_error(errno, LOG_EMERG, "Failed to run as daemon");
            return -1;
        }
    }

    log_version_info();

    *loop = g_main_loop_new(NULL, FALSE);
    if(*loop == NULL)
    {
        msg_error(ENOMEM, LOG_EMERG, "Failed creating GLib main loop");
        return -1;
    }

    osdev_init(parameters->blkid_tool, NULL);

    return 0;
}

static void usage(const char *program_name)
{
    std::cout <<
        "Usage: " << program_name << " [options]\n"
        "\n"
        "Options:\n"
        "  --help         Show this help.\n"
        "  --version      Print version information to stdout.\n"
        "  --fg           Run in foreground, don't run as daemon.\n"
        "  --workdir PATH Where the mountpoints are to be maintained.\n"
        "  --session-dbus Connect to session D-Bus.\n"
        "  --system-dbus  Connect to system D-Bus."
        << std::endl;
}

static int process_command_line(int argc, char *argv[],
                                struct parameters *parameters)
{
    parameters->run_in_foreground = false;
    parameters->connect_to_session_dbus = true;
    parameters->mount_tool = "/usr/bin/sudo /bin/mount";
    parameters->unmount_tool = "/usr/bin/sudo /bin/umount";
    parameters->blkid_tool = "/usr/bin/sudo /sbin/blkid";
    parameters->working_directory = "/run/MounTA";

#define CHECK_ARGUMENT() \
    do \
    { \
        if(i + 1 >= argc) \
        { \
            fprintf(stderr, "Option %s requires an argument.\n", argv[i]); \
            return -1; \
        } \
        ++i; \
    } \
    while(0)

    for(int i = 1; i < argc; ++i)
    {
        if(strcmp(argv[i], "--help") == 0)
            return 1;
        else if(strcmp(argv[i], "--version") == 0)
            return 2;
        else if(strcmp(argv[i], "--fg") == 0)
            parameters->run_in_foreground = true;
        else if(strcmp(argv[i], "--workdir") == 0)
        {
            CHECK_ARGUMENT();
            parameters->working_directory = argv[i];
        }
        else if(strcmp(argv[i], "--session-dbus") == 0)
            parameters->connect_to_session_dbus = true;
        else if(strcmp(argv[i], "--system-dbus") == 0)
            parameters->connect_to_session_dbus = false;
        else
        {
            std::cerr << "Unknown option \"" << argv[i]
                      << "\". Please try --help." << std::endl;
            return -1;
        }
    }

#undef CHECK_ARGUMENT

    return 0;
}

static void handle_device_changes(FdEvents::EventType ev, const char *path, void *user_data)
{
    auto *data = static_cast<std::pair<Automounter::Core, GMainLoop *> *>(user_data);

    switch(ev)
    {
      case FdEvents::NEW_DEVICE:
        data->first.handle_new_device(path);
        break;

      case FdEvents::DEVICE_GONE:
        data->first.handle_removed_device(path);
        break;

      case FdEvents::SHUTDOWN:
        data->first.shutdown();
        g_main_loop_quit(data->second);
        break;
    }
}

static gboolean handle_fd_event(gint fd, GIOCondition condition, gpointer user_data)
{
    return (static_cast<FdEvents *>(user_data)->process()
            ? G_SOURCE_CONTINUE
            : G_SOURCE_REMOVE);
}

static int setup_inotify_watch(FdEvents &ev, const char *path,
                               std::pair<Automounter::Core, GMainLoop *> *data)
{
    int fd = ev.watch(path, handle_device_changes, data);

    if(fd < 0)
        return -1;

    if(g_unix_fd_add(fd, G_IO_IN, handle_fd_event, &ev) <= 0)
        return -1;

    return 0;
}

static gboolean signal_handler(gpointer user_data)
{
    g_main_loop_quit(static_cast<GMainLoop *>(user_data));
    return G_SOURCE_REMOVE;
}

int main(int argc, char *argv[])
{
    static struct parameters parameters;

    int ret = process_command_line(argc, argv, &parameters);

    if(ret == -1)
        return EXIT_FAILURE;
    else if(ret == 1)
    {
        usage(argv[0]);
        return EXIT_SUCCESS;
    }
    else if(ret == 2)
    {
        show_version_info();
        return EXIT_SUCCESS;
    }

    static GMainLoop *loop = NULL;

    if(setup(&parameters, &loop) < 0)
        return EXIT_FAILURE;

    g_unix_signal_add(SIGINT, signal_handler, loop);
    g_unix_signal_add(SIGTERM, signal_handler, loop);

    static constexpr const char mount_options_default[] = "-o ro";

    Automounter::ExternalTools tools(parameters.mount_tool,   mount_options_default,
                                     parameters.unmount_tool, nullptr);
    auto event_data =
        std::make_pair(Automounter::Core(parameters.working_directory, tools), loop);

    if(dbus_setup(loop, parameters.connect_to_session_dbus, &event_data.first) < 0)
        return EXIT_FAILURE;

    static FdEvents ev;
    if(setup_inotify_watch(ev, "/dev/disk/by-id", &event_data) < 0)
        return EXIT_FAILURE;

    g_main_loop_run(loop);

    msg_info("Shutting down");

    dbus_shutdown(loop);

    return EXIT_SUCCESS;
}
