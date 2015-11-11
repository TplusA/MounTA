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
#include <sys/inotify.h>

#include "fdevents.hh"
#include "messages.h"

static void close_fd(int &fd)
{
    if(fd < 0)
        return;

    int ret;
    while((ret = close(fd)) == -1 && errno == EINTR)
        ;

    if(ret == -1 && errno != EINTR)
        msg_error(errno, LOG_ERR, "Failed to close file descriptor %d", fd);

    fd = -1;
}

static void close_fd_and_wd(int &fd, int &wd)
{
    if(wd >= 0)
    {
        if(inotify_rm_watch(fd, wd) < 0)
            msg_error(errno, LOG_ERR,
                      "Failed to remove inotify watch %d from fd %d", wd, fd);

        wd = -1;
    }

    close_fd(fd);
}

FdEvents::~FdEvents()
{
    close_fd_and_wd(fd_, wd_);
}

bool FdEvents::init_path_buffer(const char *path)
{
    const size_t len = strlen(path);

    if(len + 64 > sizeof(path_buffer_))
        return false;

    memcpy(path_buffer_, path, len);

    path_suffix_ = path_buffer_ + len;
    *path_suffix_++ = '/';
    path_suffix_[0] = '\0';

    path_suffix_max_chars_ = sizeof(path_buffer_) - (path_suffix_ - path_buffer_) - 1;

    log_assert(path_suffix_max_chars_ > 0);

    return true;
}

int FdEvents::watch(const char *path,
                    const callback_type &handler, void *user_data)
{
    log_assert(path != nullptr);
    log_assert(handler != nullptr);

    close_fd_and_wd(fd_, wd_);

    if(!init_path_buffer(path))
    {
        msg_error(ENAMETOOLONG, LOG_CRIT, "Internal path buffer too small");
        return -1;
    }

    fd_ = inotify_init1(IN_NONBLOCK);

    if(fd_ < 0)
    {
        msg_error(errno, LOG_CRIT, "Failed to create inotify instance");
        return -1;
    }

    wd_ = inotify_add_watch(fd_, path,
                            IN_CREATE | IN_DELETE | IN_DELETE_SELF |
                            IN_MOVE_SELF | IN_DONT_FOLLOW | IN_ONLYDIR);

    if(wd_ < 0)
    {
        msg_error(errno, LOG_CRIT,
                  "Failed to create inotify watch on fd %d", fd_);
        close_fd(fd_);
    }

    event_handler_ = handler;
    event_handler_user_data_ = user_data;

    return fd_;
}

template <size_t N>
static ssize_t try_fill_buffer(int fd, uint8_t (&event_buffer)[N])
{
    if(fd < 0)
    {
        BUG("Attempted to process events on closed inotify instance");
        return -1;
    }

    ssize_t len;

    while((len = read(fd, event_buffer, N)) < 0 &&
          errno == EINTR)
        ;

    if(len > 0)
        return len;

    if(len == 0 || (len < 0 && errno == EAGAIN))
    {
        BUG("Attempted to process inotify events, but have no events");
        return 0;
    }
    else
    {
        msg_error(errno, LOG_CRIT,
                  "Failed to read events from inotify watch fd %d", fd);
        return -1;
    }
}

const char *FdEvents::path_from_event(const struct inotify_event *event)
{
    if(event->len > 0 && event->len <= path_suffix_max_chars_)
    {
        memcpy(path_suffix_, event->name, event->len + 1);
        return path_buffer_;
    }

    if(event->len > 0)
        msg_error(ENAMETOOLONG, LOG_ERR,
                  "Buffer too small for path \"%s\"", event->name);

    path_suffix_[0] = '\0';

    return nullptr;
}

bool FdEvents::process()
{
    uint8_t event_buffer[2048]
        __attribute__ ((aligned(__alignof__(struct inotify_event))));

    const ssize_t len = try_fill_buffer(fd_, event_buffer);

    if(len < 0)
    {
        close_fd(fd_);
        return false;
    }
    else if(len == 0)
        return true;

    const struct inotify_event *event = nullptr;

    for(const uint8_t *ptr = event_buffer;
        ptr < event_buffer + len;
        // cppcheck-suppress nullPointer
        ptr += sizeof(*event) + event->len)
    {
        event = reinterpret_cast<const struct inotify_event *>(ptr);

        if(event->mask & IN_ISDIR)
            continue;

        if(event->mask & IN_CREATE)
            event_handler_(NEW_DEVICE,
                           path_from_event(event), event_handler_user_data_);

        if(event->mask & IN_DELETE)
            event_handler_(DEVICE_GONE,
                           path_from_event(event), event_handler_user_data_);

        if(event->mask & (IN_DELETE_SELF | IN_MOVE_SELF))
        {
            event_handler_(SHUTDOWN,
                           nullptr, event_handler_user_data_);
            close_fd_and_wd(fd_, wd_);
            return false;
        }
    }

    return true;
}
