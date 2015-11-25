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

#include <cppcutter.h>
#include <string>

#include "mock_os.hh"

enum class OsFn
{
    stdlib_abort,
    stdlib_system,
    system_formatted,
    foreach_in_path,
    resolve_symlink,
    mkdir_hierarchy,
    unix_mkdir,
    unix_rmdir,
    file_new,
    file_close,
    file_delete,
    map_file_to_memory,
    unmap_file,

    first_valid_os_fn_id = stdlib_abort,
    last_valid_os_fn_id = unmap_file,
};


static std::ostream &operator<<(std::ostream &os, const OsFn id)
{
    if(id < OsFn::first_valid_os_fn_id ||
       id > OsFn::last_valid_os_fn_id)
    {
        os << "INVALID";
        return os;
    }

    switch(id)
    {
      case OsFn::stdlib_abort:
        os << "abort";
        break;

      case OsFn::stdlib_system:
        os << "stdlib_system";
        break;

      case OsFn::system_formatted:
        os << "system_formatted";
        break;

      case OsFn::foreach_in_path:
        os << "foreach_in_path";
        break;

      case OsFn::resolve_symlink:
        os << "resolve_symlink";
        break;

      case OsFn::mkdir_hierarchy:
        os << "mkdir_hierarchy";
        break;

      case OsFn::unix_mkdir:
        os << "unix_mkdir";
        break;

      case OsFn::unix_rmdir:
        os << "unix_rmdir";
        break;

      case OsFn::file_new:
        os << "file_new";
        break;

      case OsFn::file_close:
        os << "file_close";
        break;

      case OsFn::file_delete:
        os << "file_delete";
        break;

      case OsFn::map_file_to_memory:
        os << "map_file_to_memory";
        break;

      case OsFn::unmap_file:
        os << "unmap_file";
        break;
    }

    os << "()";

    return os;
}

class MockOs::Expectation
{
  public:
    struct Data
    {
        const OsFn function_id_;
        std::string ret_string_;
        bool ret_int_;
        bool ret_bool_;
        std::string arg_string_;
        bool arg_bool_;
        int arg_fd_;
        struct os_mapped_file_data *arg_mapped_pointer_;
        const struct os_mapped_file_data *arg_mapped_template_;
        bool arg_pointer_expect_concrete_value_;
        bool arg_pointer_shall_be_null_;

        explicit Data(OsFn fn):
            function_id_(fn),
            ret_int_(-1),
            ret_bool_(false),
            arg_bool_(false),
            arg_fd_(-1),
            arg_mapped_pointer_(nullptr),
            arg_mapped_template_(nullptr),
            arg_pointer_expect_concrete_value_(false),
            arg_pointer_shall_be_null_(false)
        {}
    };

    const Data d;

  private:
    /* writable reference for simple ctor code */
    Data &data_ = *const_cast<Data *>(&d);

  public:
    Expectation(const Expectation &) = delete;
    Expectation &operator=(const Expectation &) = delete;

    explicit Expectation(const char *ret_string, const char *arg_string):
        d(OsFn::resolve_symlink)
    {
        data_.ret_string_ = ret_string;
        data_.arg_string_ = arg_string;
    }

    explicit Expectation(OsFn fn, int ret_int, const char *arg_string):
        d(fn)
    {
        data_.ret_int_ = ret_int;
        data_.arg_string_ = arg_string;
    }

    explicit Expectation(OsFn fn, bool ret_bool, const char *arg_string):
        d(fn)
    {
        data_.ret_bool_ = ret_bool;
        data_.arg_string_ = arg_string;
    }

    explicit Expectation(OsFn fn, bool ret_bool, const char *arg_string, bool arg_bool):
        d(fn)
    {
        data_.ret_bool_ = ret_bool;
        data_.arg_string_ = arg_string;
        data_.arg_bool_ = arg_bool;
    }

    explicit Expectation(int ret, const char *filename):
        d(OsFn::file_new)
    {
        data_.ret_bool_ = ret;
        data_.arg_string_ = filename;
    }

    explicit Expectation(OsFn fn, const char *filename):
        d(fn)
    {
        data_.arg_string_ = filename;
    }

    explicit Expectation(OsFn fn, int fd):
        d(fn)
    {
        data_.arg_fd_ = fd;
    }

    explicit Expectation(int ret, struct os_mapped_file_data *mapped,
                         const char *filename):
        d(OsFn::map_file_to_memory)
    {
        data_.ret_int_ = ret;
        data_.arg_mapped_pointer_ = mapped;
        data_.arg_pointer_shall_be_null_ = (mapped == nullptr);
        data_.arg_string_ = filename;
    }

    explicit Expectation(const struct os_mapped_file_data *mapped,
                         const char *filename):
        d(OsFn::map_file_to_memory)
    {
        data_.ret_int_ = (mapped != nullptr && mapped->fd >= 0 && mapped->ptr != NULL) ? 0 : -1;
        data_.arg_mapped_template_ = mapped;
        data_.arg_string_ = filename;
    }

    explicit Expectation(int ret, bool expect_null_pointer,
                         const char *filename):
        d(OsFn::map_file_to_memory)
    {
        data_.ret_int_ = ret;
        data_.arg_pointer_shall_be_null_ = expect_null_pointer;
        data_.arg_string_ = filename;
    }

    explicit Expectation(struct os_mapped_file_data *mapped):
        d(OsFn::unmap_file)
    {
        data_.arg_mapped_pointer_ = mapped;
        data_.arg_pointer_shall_be_null_ = (mapped == nullptr);
    }

    explicit Expectation(const struct os_mapped_file_data *mapped):
        d(OsFn::unmap_file)
    {
        data_.arg_mapped_template_ = mapped;
    }

    explicit Expectation(OsFn fn):
        d(fn)
    {}

    Expectation(Expectation &&) = default;
};

MockOs::MockOs()
{
    expectations_ = new MockExpectations();
}

MockOs::~MockOs()
{
    delete expectations_;
}

void MockOs::init()
{
    cppcut_assert_not_null(expectations_);
    expectations_->init();
}

void MockOs::check() const
{
    cppcut_assert_not_null(expectations_);
    expectations_->check();
}

void MockOs::expect_os_abort(void)
{
    expectations_->add(Expectation(OsFn::stdlib_abort));
}

void MockOs::expect_os_system(int retval, const char *command)
{
    expectations_->add(Expectation(OsFn::stdlib_system, retval, command));
}

void MockOs::expect_os_system_formatted(int retval, const char *string)
{
    expectations_->add(Expectation(OsFn::system_formatted, retval, string));
}

void MockOs::expect_os_system_formatted_formatted(int retval, const char *string)
{
    expectations_->add(Expectation(OsFn::system_formatted, retval, string));
}

void MockOs::expect_os_foreach_in_path(bool retval, const char *path)
{
    expectations_->add(Expectation(OsFn::foreach_in_path, retval, path));
}

void MockOs::expect_os_resolve_symlink(const char *retval, const char *link)
{
    expectations_->add(Expectation(retval, link));
}

void MockOs::expect_os_mkdir_hierarchy(bool retval, const char *path, bool must_not_exist)
{
    expectations_->add(Expectation(OsFn::mkdir_hierarchy, retval, path, must_not_exist));
}

void MockOs::expect_os_mkdir(bool retval, const char *path, bool must_not_exist)
{
    expectations_->add(Expectation(OsFn::unix_mkdir, retval, path, must_not_exist));
}

void MockOs::expect_os_rmdir(bool retval, const char *path, bool must_exist)
{
    expectations_->add(Expectation(OsFn::unix_rmdir, retval, path, must_exist));
}

void MockOs::expect_os_file_new(int ret, const char *filename)
{
    expectations_->add(Expectation(ret, filename));
}

void MockOs::expect_os_file_close(int fd)
{
    expectations_->add(Expectation(OsFn::file_close, fd));
}

void MockOs::expect_os_file_delete(const char *filename)
{
    expectations_->add(Expectation(OsFn::file_delete, filename));
}

void MockOs::expect_os_map_file_to_memory(int ret, struct os_mapped_file_data *mapped,
                                          const char *filename)
{
    expectations_->add(Expectation(ret, mapped, filename));
}

void MockOs::expect_os_map_file_to_memory(int ret, bool expect_null_pointer,
                                          const char *filename)
{
    if(expect_null_pointer)
        expectations_->add(Expectation(ret, nullptr, filename));
    else
        expectations_->add(Expectation(ret, false, filename));
}

void MockOs::expect_os_map_file_to_memory(const struct os_mapped_file_data *mapped,
                                          const char *filename)
{
    expectations_->add(Expectation(mapped, filename));
}

void MockOs::expect_os_unmap_file(struct os_mapped_file_data *mapped)
{
    expectations_->add(Expectation(mapped));
}

void MockOs::expect_os_unmap_file(const struct os_mapped_file_data *mapped)
{
    expectations_->add(Expectation(mapped));
}

void MockOs::expect_os_unmap_file(bool expect_null_pointer)
{
    if(expect_null_pointer)
        expectations_->add(Expectation(static_cast<struct os_mapped_file_data *>(nullptr)));
    else
        expectations_->add(Expectation(OsFn::unmap_file, false));
}


MockOs *mock_os_singleton = nullptr;

void os_abort(void)
{
    const auto &expect(mock_os_singleton->expectations_->get_next_expectation(__func__));

    cppcut_assert_equal(expect.d.function_id_, OsFn::stdlib_abort);
}

int os_system(const char *command)
{
    const auto &expect(mock_os_singleton->expectations_->get_next_expectation(__func__));

    cppcut_assert_equal(expect.d.function_id_, OsFn::stdlib_system);
    cppcut_assert_equal(expect.d.arg_string_.c_str(), command);

    return expect.d.ret_int_;
}

int os_system_formatted(const char *format_string, ...)
{
    const auto &expect(mock_os_singleton->expectations_->get_next_expectation(__func__));

    cppcut_assert_equal(expect.d.function_id_, OsFn::stdlib_system);

    va_list va;
    char buffer[512];
    va_start(va, format_string);
    (void)vsnprintf(buffer, sizeof(buffer), format_string, va);
    va_end(va);

    cppcut_assert_equal(expect.d.arg_string_.c_str(), buffer);

    return expect.d.ret_int_;
}

bool os_foreach_in_path(const char *path,
                        void (*callback)(const char *path, void *user_data),
                        void *user_data)
{
    const auto &expect(mock_os_singleton->expectations_->get_next_expectation(__func__));

    cppcut_assert_equal(expect.d.function_id_, OsFn::foreach_in_path);
    cppcut_assert_equal(expect.d.arg_string_.c_str(), path);
    cppcut_assert_not_null(reinterpret_cast<void *>(callback));

    return expect.d.ret_bool_;
}

char *os_resolve_symlink(const char *link)
{
    const auto &expect(mock_os_singleton->expectations_->get_next_expectation(__func__));

    cppcut_assert_equal(expect.d.function_id_, OsFn::resolve_symlink);
    cppcut_assert_not_null(link);
    cppcut_assert_equal(expect.d.arg_string_.c_str(), link);

    return expect.d.ret_string_.empty() ? nullptr : strdup(expect.d.ret_string_.c_str());
}

bool os_mkdir_hierarchy(const char *path, bool must_not_exist)
{
    const auto &expect(mock_os_singleton->expectations_->get_next_expectation(__func__));

    cppcut_assert_equal(expect.d.function_id_, OsFn::mkdir_hierarchy);
    cppcut_assert_equal(expect.d.arg_string_.c_str(), path);
    cppcut_assert_equal(expect.d.arg_bool_, must_not_exist);

    return expect.d.ret_bool_;
}

bool os_mkdir(const char *path, bool must_not_exist)
{
    const auto &expect(mock_os_singleton->expectations_->get_next_expectation(__func__));

    cppcut_assert_equal(expect.d.function_id_, OsFn::unix_mkdir);
    cppcut_assert_equal(expect.d.arg_string_.c_str(), path);
    cppcut_assert_equal(expect.d.arg_bool_, must_not_exist);

    return expect.d.ret_bool_;
}

bool os_rmdir(const char *path, bool must_exist)
{
    const auto &expect(mock_os_singleton->expectations_->get_next_expectation(__func__));

    cppcut_assert_equal(expect.d.function_id_, OsFn::unix_rmdir);
    cppcut_assert_equal(expect.d.arg_string_.c_str(), path);
    cppcut_assert_equal(expect.d.arg_bool_, must_exist);

    return expect.d.ret_bool_;
}

int os_file_new(const char *filename)
{
    const auto &expect(mock_os_singleton->expectations_->get_next_expectation(__func__));

    cppcut_assert_equal(expect.d.function_id_, OsFn::file_new);
    cppcut_assert_equal(expect.d.arg_string_, std::string(filename));
    return expect.d.ret_int_;
}

void os_file_close(int fd)
{
    const auto &expect(mock_os_singleton->expectations_->get_next_expectation(__func__));

    cppcut_assert_equal(expect.d.function_id_, OsFn::file_close);
    cppcut_assert_equal(expect.d.arg_fd_, fd);
}

void os_file_delete(const char *filename)
{
    const auto &expect(mock_os_singleton->expectations_->get_next_expectation(__func__));

    cppcut_assert_equal(expect.d.function_id_, OsFn::file_delete);
    cppcut_assert_equal(expect.d.arg_string_, std::string(filename));
}

int os_map_file_to_memory(struct os_mapped_file_data *mapped,
                          const char *filename)
{
    const auto &expect(mock_os_singleton->expectations_->get_next_expectation(__func__));

    cppcut_assert_equal(expect.d.function_id_, OsFn::map_file_to_memory);

    if(expect.d.arg_mapped_template_ != nullptr)
        *mapped = *expect.d.arg_mapped_template_;
    else
    {
        if(expect.d.arg_pointer_expect_concrete_value_)
            cppcut_assert_equal(expect.d.arg_mapped_pointer_, mapped);
        else if(expect.d.arg_pointer_shall_be_null_)
            cppcut_assert_null(mapped);
        else
            cppcut_assert_not_null(mapped);
    }

    cppcut_assert_equal(expect.d.arg_string_, std::string(filename));

    return expect.d.ret_int_;
}

void os_unmap_file(struct os_mapped_file_data *mapped)
{
    const auto &expect(mock_os_singleton->expectations_->get_next_expectation(__func__));

    cppcut_assert_equal(expect.d.function_id_, OsFn::unmap_file);

    if(expect.d.arg_mapped_template_ != nullptr)
        *mapped = *expect.d.arg_mapped_template_;
    else
    {
        if(expect.d.arg_pointer_expect_concrete_value_)
            cppcut_assert_equal(expect.d.arg_mapped_pointer_, mapped);
        else if(expect.d.arg_pointer_shall_be_null_)
            cppcut_assert_null(mapped);
        else
            cppcut_assert_not_null(mapped);
    }
}
