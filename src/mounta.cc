/*
 * Copyright (C) 2015, 2017, 2019, 2020  T+A elektroakustik GmbH & Co. KG
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

#include <cstring>
#include <iostream>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
#include <glib-object.h>
#include <glib-unix.h>
#pragma GCC diagnostic pop

#include "fdevents.hh"
#include "automounter.hh"
#include "external_tools.hh"
#include "dbus_iface.h"
#include "messages.h"
#include "versioninfo.h"

ssize_t (*os_read)(int fd, void *dest, size_t count) = read;
ssize_t (*os_write)(int fd, const void *buf, size_t count) = write;

struct Parameters
{
    bool run_in_foreground;
    bool connect_to_session_dbus;
    const char *working_directory;
    const char *symlink_directory;
    const char *mount_tool;
    const char *unmount_tool;
    const char *mpoint_tool;
    const char *blkid_tool;
    const char *udevadm_tool;
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
static int setup(const Parameters &parameters, GMainLoop *&loop)
{
    loop = NULL;

    msg_enable_syslog(!parameters.run_in_foreground);

    if(!parameters.run_in_foreground)
    {
        openlog("mounta", LOG_PID, LOG_DAEMON);

        if(daemon(0, 0) < 0)
        {
            msg_error(errno, LOG_EMERG, "Failed to run as daemon");
            return -1;
        }
    }

    log_version_info();

    loop = g_main_loop_new(NULL, FALSE);
    if(loop == NULL)
    {
        msg_error(ENOMEM, LOG_EMERG, "Failed creating GLib main loop");
        return -1;
    }

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

static int process_command_line(int argc, char *argv[], Parameters &parameters)
{
    parameters.run_in_foreground = false;
    parameters.connect_to_session_dbus = true;
    parameters.mount_tool = "/usr/bin/sudo /bin/mount";
    parameters.unmount_tool = "/usr/bin/sudo /bin/umount";
    parameters.mpoint_tool = "/usr/bin/mountpoint";
    parameters.blkid_tool = "/usr/bin/sudo /sbin/blkid";
    parameters.udevadm_tool = "/bin/udevadm";
    parameters.working_directory = "/run/MounTA";
    parameters.symlink_directory = "/run/mount-by-label";

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
            parameters.run_in_foreground = true;
        else if(strcmp(argv[i], "--workdir") == 0)
        {
            CHECK_ARGUMENT();
            parameters.working_directory = argv[i];
        }
        else if(strcmp(argv[i], "--session-dbus") == 0)
            parameters.connect_to_session_dbus = true;
        else if(strcmp(argv[i], "--system-dbus") == 0)
            parameters.connect_to_session_dbus = false;
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

using UnmountMountpointData =
    std::pair<const Automounter::Directory &, const Automounter::ExternalTools &>;

static int unmount_mountpoint(const char *path, unsigned char dtype, void *user_data)
{
    if(dtype != DT_DIR)
        return 0;

    auto &data(*static_cast<const UnmountMountpointData *>(user_data));

    Automounter::Mountpoint mp(std::get<1>(data), std::get<0>(data).str() + '/' + path);
    mp.probe();

    return 0;
}

using CleanupMountRootData =
    std::pair<const char *const, const Automounter::ExternalTools &>;

static int cleanup_mount_root(const char *path, unsigned char dtype, void *user_data)
{
    if(dtype != DT_DIR)
        return 0;

    auto &data(*static_cast<const CleanupMountRootData *>(user_data));

    Automounter::Directory dir(std::string(std::get<0>(data)) + '/' + path);

    if(dir.probe())
    {
        UnmountMountpointData d(dir, std::get<1>(data));
        os_foreach_in_path(dir.str().c_str(), unmount_mountpoint, &d);
    }

    return 0;
}

void cleanup_working_directory(const char *working_directory,
                               const Automounter::ExternalTools &tools)
{
    CleanupMountRootData data(working_directory, tools);
    os_foreach_in_path(working_directory, cleanup_mount_root, &data);
}

static void handle_device_changes(FdEvents::EventType ev, const char *path, void *user_data)
{
    auto &data = *static_cast<std::pair<Automounter::Core, GMainLoop *> *>(user_data);

    switch(ev)
    {
      case FdEvents::NEW_DEVICE:
        data.first.handle_new_device(path);
        break;

      case FdEvents::DEVICE_GONE:
        data.first.handle_removed_device(path);
        break;

      case FdEvents::SHUTDOWN:
        data.first.shutdown();
        g_main_loop_quit(data.second);
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
                               std::pair<Automounter::Core, GMainLoop *> &data)
{
    int fd = ev.watch(path, handle_device_changes, &data);

    if(fd < 0)
        return -1;

    if(g_unix_fd_add(fd, G_IO_IN, handle_fd_event, &ev) <= 0)
        return -1;

    return 0;
}

using CollectDevicesData =
    std::pair<std::pair<Automounter::Core, GMainLoop *> &, const char *const>;

static int collect_devices(const char *path, unsigned char dtype,
                           void *user_data)
{
    auto &data = *static_cast<CollectDevicesData *>(user_data);

    std::string full_path(data.second);
    full_path += '/';
    full_path += path;

    handle_device_changes(FdEvents::NEW_DEVICE, full_path.c_str(), &data.first);

    return 0;
}

static gboolean signal_handler(gpointer user_data)
{
    g_main_loop_quit(static_cast<GMainLoop *>(user_data));
    return G_SOURCE_REMOVE;
}

int main(int argc, char *argv[])
{
    static Parameters parameters;

    int ret = process_command_line(argc, argv, parameters);

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

    if(setup(parameters, loop) < 0)
        return EXIT_FAILURE;

    g_unix_signal_add(SIGINT, signal_handler, loop);
    g_unix_signal_add(SIGTERM, signal_handler, loop);

    static constexpr const char mount_options_default[] = "-o ro,noexec,nosuid,nodev,user";

    static constexpr const char mount_options_ext234[] = "-o errors=continue";
    static constexpr const char mount_options_fatish[] = "-o umask=222,utf8";
    static constexpr const char mount_options_hfs[] = "-o umask=222";

    static const Automounter::FSMountOptions
    mount_options(std::map<const std::string, const char *const>
    {
        { "ext2",    mount_options_ext234 },
        { "ext3",    mount_options_ext234 },
        { "ext4",    mount_options_ext234 },
        { "jfs",     mount_options_ext234 },
        { "xfs",     nullptr },
        { "btrfs",   nullptr },
        { "msdos",   mount_options_fatish },
        { "vfat",    mount_options_fatish },
        { "ntfs",    mount_options_fatish },
        { "hfs",     mount_options_hfs },
        { "hfsplus", mount_options_hfs },
        { "iso9660", nullptr },
    });

    Automounter::ExternalTools tools(
        Automounter::ExternalTools::Command(parameters.mount_tool,   mount_options_default),
        Automounter::ExternalTools::Command(parameters.unmount_tool, nullptr),
        Automounter::ExternalTools::Command(parameters.mpoint_tool,  "-q"),
        Automounter::ExternalTools::Command(parameters.blkid_tool,   nullptr),
        Automounter::ExternalTools::Command(parameters.udevadm_tool, nullptr));

    Devices::init(tools);
    cleanup_working_directory(parameters.working_directory, tools);

    auto event_data =
        std::make_pair(Automounter::Core(parameters.working_directory, tools,
                                         mount_options,
                                         parameters.symlink_directory),
                       loop);

    if(dbus_setup(loop, parameters.connect_to_session_dbus, &event_data.first) < 0)
        return EXIT_FAILURE;

    static const char watched_directory[] = "/dev/disk/by-id";

    /* install inotify watch first to make sure are not losing anything */
    static FdEvents ev;
    if(setup_inotify_watch(ev, watched_directory, event_data) < 0)
        return EXIT_FAILURE;

    /* after the inotify watch has been installed, we check the directory of
     * block devices for entries which have already there when we were started;
     * we are not going to lose any inotify events, but events may occur while
     * the directory is inspected, possibly leading to events for entries we've
     * already seen */
    {
        CollectDevicesData data(std::ref(event_data), watched_directory);

        if(os_foreach_in_path(watched_directory, collect_devices, &data) < 0)
            return EXIT_FAILURE;
    }

    /* any inotify events already received from kernel, if any, will be
     * processed within a GLib main loop */
    g_main_loop_run(loop);

    msg_info("Shutting down");

    dbus_shutdown(loop);

    return EXIT_SUCCESS;
}
