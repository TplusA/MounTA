/*
 * Copyright (C) 2015, 2017  T+A elektroakustik GmbH & Co. KG
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

#include <climits>
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

void FdEvents::init_path_buffer(const char *path)
{
    path_buffer_ = path;
    path_buffer_ += '/';
    path_buffer_prefix_length_ = path_buffer_.length();
}

int FdEvents::watch(const char *path,
                    const callback_type &handler, void *user_data)
{
    log_assert(path != nullptr);
    log_assert(handler != nullptr);

    close_fd_and_wd(fd_, wd_);

    init_path_buffer(path);

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

static ssize_t try_fill_buffer(int fd, uint8_t *event_buffer, size_t buffer_size)
{
    if(fd < 0)
    {
        BUG("Attempted to process events on closed inotify instance");
        return -1;
    }

    ssize_t len;

    while((len = read(fd, event_buffer, buffer_size)) < 0 &&
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
    path_buffer_.erase(path_buffer_prefix_length_);

    if(event->len > 0)
    {
        std::copy(event->name, event->name + event->len,
                  std::back_inserter(path_buffer_));
        return path_buffer_.c_str();
    }
    else
        return nullptr;
}

bool FdEvents::process()
{
    std::aligned_storage<sizeof(struct inotify_event) + NAME_MAX + 1,
                         alignof(struct inotify_event)>::type aligned_event_buffer[16];
    auto *const event_buffer = reinterpret_cast<uint8_t *>(aligned_event_buffer);

    const ssize_t len =
        try_fill_buffer(fd_, event_buffer, sizeof(aligned_event_buffer));

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
