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

#ifndef FDEVENTS_HH
#define FDEVENTS_HH

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

    char path_buffer_[1024];
    char *path_suffix_;
    size_t path_suffix_max_chars_;

    callback_type event_handler_;
    void *event_handler_user_data_;

  public:
    FdEvents(const FdEvents &) = delete;
    FdEvents &operator=(const FdEvents &) = delete;

    explicit FdEvents():
        fd_(-1),
        wd_(-1),
        path_suffix_(nullptr),
        path_suffix_max_chars_(0),
        event_handler_user_data_(nullptr)
    {
        path_buffer_[0] = '\0';
    }

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
    bool init_path_buffer(const char *path);
    const char *path_from_event(const struct inotify_event *event);
};

#endif /* !FDEVENTS_HH */
