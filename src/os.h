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

#ifndef OS_H
#define OS_H

#include <stdbool.h>
#include <unistd.h>

/*!
 * Data for keeping track of memory-mapped files.
 */
struct os_mapped_file_data
{
    int fd;
    void *ptr;
    size_t length;
};

#ifdef __cplusplus
extern "C" {
#endif

void os_abort(void);

int os_system(const char *command);
int os_system_formatted(const char *format_string, ...)
    __attribute__ ((format (printf, 1, 2)));

bool os_foreach_in_path(const char *path,
                        void (*callback)(const char *path, void *user_data),
                        void *user_data);

/*!
 * Read destination of symlink, if any.
 *
 * \returns
 *     A string to be freed by the caller, or \c NULL if the input name is not
 *     a symlink, the symlink is broken, or any kind of error is returned from
 *     the OS.
 */
char *os_resolve_symlink(const char *link);

bool os_mkdir_hierarchy(const char *path, bool must_not_exist);
bool os_mkdir(const char *path, bool must_not_exist);
bool os_rmdir(const char *path, bool must_exist);

int os_file_new(const char *filename);
void os_file_close(int fd);
void os_file_delete(const char *filename);

int os_map_file_to_memory(struct os_mapped_file_data *mapped,
                          const char *filename);
void os_unmap_file(struct os_mapped_file_data *mapped);

#ifdef __cplusplus
}
#endif

#endif /* !OS_H */
