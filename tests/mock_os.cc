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
    resolve_symlink,

    first_valid_os_fn_id = stdlib_abort,
    last_valid_os_fn_id = resolve_symlink,
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

      case OsFn::resolve_symlink:
        os << "resolve_symlink";
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
        std::string arg_string_;

        explicit Data(OsFn fn):
            function_id_(fn)
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

void MockOs::expect_os_resolve_symlink(const char *retval, const char *link)
{
    expectations_->add(Expectation(retval, link));
}


MockOs *mock_os_singleton = nullptr;

void os_abort(void)
{
    const auto &expect(mock_os_singleton->expectations_->get_next_expectation(__func__));

    cppcut_assert_equal(expect.d.function_id_, OsFn::stdlib_abort);
}

char *os_resolve_symlink(const char *link)
{
    const auto &expect(mock_os_singleton->expectations_->get_next_expectation(__func__));

    cppcut_assert_equal(expect.d.function_id_, OsFn::resolve_symlink);
    cppcut_assert_not_null(link);
    cppcut_assert_equal(expect.d.arg_string_.c_str(), link);

    return expect.d.ret_string_.empty() ? nullptr : strdup(expect.d.ret_string_.c_str());
}
