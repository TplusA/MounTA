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

#include <unistd.h>

#ifdef __cplusplus
extern "C" {
#endif

void os_abort(void);

/*!
 * Read destination of symlink, if any.
 *
 * \returns
 *     A string to be freed by the caller, or \c NULL if the input name is not
 *     a symlink, the symlink is broken, or any kind of error is returned from
 *     the OS.
 */
char *os_resolve_symlink(const char *link);

#ifdef __cplusplus
}
#endif

#endif /* !OS_H */
