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

#include "mock_devices_os.hh"

enum class Fn
{
    get_volume_information,
    free_volume_information,

    first_valid_fn_id = get_volume_information,
    last_valid_fn_id = free_volume_information,
};

static std::ostream &operator<<(std::ostream &os, const Fn id)
{
    if(id < Fn::first_valid_fn_id ||
       id > Fn::last_valid_fn_id)
    {
        os << "INVALID";
        return os;
    }

    switch(id)
    {
      case Fn::get_volume_information:
        os << "get_volume_information";
        break;

      case Fn::free_volume_information:
        os << "free_volume_information";
        break;
    }

    os << "()";

    return os;
}

class MockDevicesOs::Expectation
{
  public:
    struct Data
    {
        const Fn function_id_;
        bool ret_bool_;
        std::string arg_string_;
        const struct osdev_volume_info *arg_info_;

        explicit Data(Fn fn):
            function_id_(fn),
            ret_bool_(false),
            arg_info_(nullptr)
        {}
    };

    const Data d;

  private:
    /* writable reference for simple ctor code */
    Data &data_ = *const_cast<Data *>(&d);

  public:
    Expectation(const Expectation &) = delete;
    Expectation &operator=(const Expectation &) = delete;

    explicit Expectation(bool retval, const char *devname, const struct osdev_volume_info *info):
        d(Fn::get_volume_information)
    {
        data_.ret_bool_ = retval;
        data_.arg_string_ = devname;
        data_.arg_info_ = info;
    }

    explicit Expectation(const struct osdev_volume_info *info):
        d(Fn::free_volume_information)
    {
        data_.arg_info_ = info;
    }

    Expectation(Expectation &&) = default;
};

MockDevicesOs::MockDevicesOs()
{
    expectations_ = new MockExpectations();
}

MockDevicesOs::~MockDevicesOs()
{
    delete expectations_;
}

void MockDevicesOs::init()
{
    cppcut_assert_not_null(expectations_);
    expectations_->init();
}

void MockDevicesOs::check() const
{
    cppcut_assert_not_null(expectations_);
    expectations_->check();
}

void MockDevicesOs::expect_osdev_get_volume_information(const char *devname, const struct osdev_volume_info *info)
{
    expectations_->add(Expectation(info != nullptr, devname, info));
}

void MockDevicesOs::expect_osdev_free_volume_information(const struct osdev_volume_info *info)
{
    expectations_->add(Expectation(info));
}


MockDevicesOs *mock_devices_os_singleton = nullptr;

bool osdev_get_volume_information(const char *devname, struct osdev_volume_info *info)
{
    const auto &expect(mock_devices_os_singleton->expectations_->get_next_expectation(__func__));

    cppcut_assert_equal(expect.d.function_id_, Fn::get_volume_information);
    cppcut_assert_equal(expect.d.arg_string_.c_str(), devname);
    cppcut_assert_not_null(info);

    if(expect.d.arg_info_ != nullptr)
        memcpy(info, expect.d.arg_info_, sizeof(*info));

    return expect.d.ret_bool_;
}

void osdev_free_volume_information(struct osdev_volume_info *info)
{
    const auto &expect(mock_devices_os_singleton->expectations_->get_next_expectation(__func__));

    cppcut_assert_equal(expect.d.function_id_, Fn::free_volume_information);

    /*
     * The \p info pointer will be some internal pointer that we cannot access
     * here in the mock, so we resort to deep comparison. In a better solution,
     * the pointer passed to osdev_get_volume_information() would be captured
     * and compared against \p info at this point.
     */
    cppcut_assert_equal(expect.d.arg_info_->idx, info->idx);
    cppcut_assert_equal(expect.d.arg_info_->label, info->label);
    cppcut_assert_equal(expect.d.arg_info_->fstype, info->fstype);
}
