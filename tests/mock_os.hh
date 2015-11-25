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

#ifndef MOCK_OS_HH
#define MOCK_OS_HH

#include "os.h"
#include "mock_expectation.hh"

class MockOs
{
  public:
    MockOs(const MockOs &) = delete;
    MockOs &operator=(const MockOs &) = delete;

    class Expectation;
    typedef MockExpectationsTemplate<Expectation> MockExpectations;
    MockExpectations *expectations_;

    explicit MockOs();
    ~MockOs();

    void init();
    void check() const;

    void expect_os_abort(void);
    void expect_os_system(int retval, const char *command);
    void expect_os_system_formatted(int retval, const char *string);
    void expect_os_system_formatted_formatted(int retval, const char *string);
    void expect_os_foreach_in_path(bool retval, const char *path);
    void expect_os_resolve_symlink(const char *retval, const char *link);
    void expect_os_mkdir_hierarchy(bool retval, const char *path, bool must_not_exist);
    void expect_os_mkdir(bool retval, const char *path, bool must_not_exist);
    void expect_os_rmdir(bool retval, const char *path, bool must_exist);
    void expect_os_file_new(int ret, const char *filename);
    void expect_os_file_close(int fd);
    void expect_os_file_delete(const char *filename);
    void expect_os_map_file_to_memory(int ret, struct os_mapped_file_data *mapped,
                                      const char *filename);
    void expect_os_map_file_to_memory(const struct os_mapped_file_data *mapped,
                                      const char *filename);
    void expect_os_map_file_to_memory(int ret, bool expect_null_pointer,
                                      const char *filename);
    void expect_os_unmap_file(struct os_mapped_file_data *mapped);
    void expect_os_unmap_file(const struct os_mapped_file_data *mapped);
    void expect_os_unmap_file(bool expect_null_pointer);
};

extern MockOs *mock_os_singleton;

#endif /* !MOCK_OS_HH */
