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

#ifndef FDEVENTS_HH
#define FDEVENTS_HH

#include <string>
#include <functional>

/*!
 * Small utility class that wraps inotify(7).
 */
class FdEvents
{
  public:
    enum EventType
    {
        NEW_DEVICE,
        DEVICE_GONE,
        SHUTDOWN,
    };

    using callback_type = std::function<void(EventType ev, const char *path, void *user_data)>;

  private:
    int fd_;
    int wd_;

    std::string path_buffer_;
    size_t path_buffer_prefix_length_;

    callback_type event_handler_;
    void *event_handler_user_data_;

  public:
    FdEvents(const FdEvents &) = delete;
    FdEvents &operator=(const FdEvents &) = delete;

    explicit FdEvents():
        fd_(-1),
        wd_(-1),
        path_buffer_prefix_length_(0),
        event_handler_user_data_(nullptr)
    {}

    ~FdEvents();

    /*!
     * Install a specific inotify watch.
     *
     * The watch is configured so that the \p handler is called when files
     * (including symlinks) are created or deleted inside the watched path.
     *
     * Further, if the watched path is removed or moved, the watch is removed
     * and an #FdEvents::SHUTDOWN event is sent. In this case it is not allowed
     * to call #FdEvents::process() anymore before installing a new watch using
     * #FdEvents::watch().
     *
     * \param path
     *     The directory to watch.
     *
     * \param handler
     *     The function to call for each event observed in the watched path.
     *
     * \param user_data
     *     Pointer passed to \p handler.
     *
     * \returns
     *     A file descriptor that can be used by poll() or select(), which is
     *     the only way the caller may use this fd. If there are any events on
     *     the file descriptor, call #FdEvents::process() to handle them.
     *     On error, a negative file descriptor is returned.
     */
    int watch(const char *path, const callback_type &handler, void *user_data);

    /*!
     * Process any pending events on the inotify watch.
     *
     * It is a programming error to call this function without prior call of
     * #FdEvents::watch() or after an #FdEvents::SHUTDOWN events has been
     * received.
     *
     * \returns
     *     True on success, false in case the inotify watch has been close.
     */
    bool process();

  private:
    void init_path_buffer(const char *path);
    const char *path_from_event(const struct inotify_event *event);
};

#endif /* !FDEVENTS_HH */
