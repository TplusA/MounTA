/*
 * Copyright (C) 2015, 2019  T+A elektroakustik GmbH & Co. KG
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

#if HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <cppcutter.h>

#include "mock_devices_os.hh"

enum class Fn
{
    get_device_information,
    free_device_information,
    get_volume_information,
    free_volume_information,

    first_valid_fn_id = get_device_information,
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
      case Fn::get_device_information:
        os << "get_device_information";
        break;

      case Fn::free_device_information:
        os << "free_device_information";
        break;

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
        const Devices::DeviceInfo *arg_devinfo_;
        const Devices::VolumeInfo *arg_volinfo_;

        explicit Data(Fn fn):
            function_id_(fn),
            ret_bool_(false),
            arg_devinfo_(nullptr),
            arg_volinfo_(nullptr)
        {}
    };

    const Data d;

  private:
    /* writable reference for simple ctor code */
    Data &data_ = *const_cast<Data *>(&d);

  public:
    Expectation(const Expectation &) = delete;
    Expectation &operator=(const Expectation &) = delete;

    explicit Expectation(bool retval, const std::string &devlink, const Devices::DeviceInfo *info):
        d(Fn::get_device_information)
    {
        data_.ret_bool_ = retval;
        data_.arg_string_ = devlink;
        data_.arg_devinfo_ = info;
    }

    explicit Expectation(const Devices::DeviceInfo *info):
        d(Fn::free_device_information)
    {
        data_.arg_devinfo_ = info;
    }

    explicit Expectation(bool retval, const std::string &devname, const Devices::VolumeInfo *info):
        d(Fn::get_volume_information)
    {
        data_.ret_bool_ = retval;
        data_.arg_string_ = devname;
        data_.arg_volinfo_ = info;
    }

    explicit Expectation(const Devices::VolumeInfo *info):
        d(Fn::free_volume_information)
    {
        data_.arg_volinfo_ = info;
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

void MockDevicesOs::expect_get_device_information(const std::string &devlink,
                                                  const Devices::DeviceInfo *info)
{
    expectations_->add(Expectation(info != nullptr, devlink, info));
}

void MockDevicesOs::expect_get_volume_information(const std::string &devname,
                                                  const Devices::VolumeInfo *info)
{
    expectations_->add(Expectation(info != nullptr, devname, info));
}


MockDevicesOs *mock_devices_os_singleton = nullptr;

bool Devices::get_device_information(const std::string &devlink, Devices::DeviceInfo &info)
{
    const auto &expect(mock_devices_os_singleton->expectations_->get_next_expectation(__func__));

    cppcut_assert_equal(expect.d.function_id_, Fn::get_device_information);
    cppcut_assert_equal(expect.d.arg_string_, devlink);

    if(expect.d.arg_devinfo_ != nullptr)
        info = *expect.d.arg_devinfo_;

    return expect.d.ret_bool_;
}

bool Devices::get_volume_information(const std::string &devname, Devices::VolumeInfo &info)
{
    const auto &expect(mock_devices_os_singleton->expectations_->get_next_expectation(__func__));

    cppcut_assert_equal(expect.d.function_id_, Fn::get_volume_information);
    cppcut_assert_equal(expect.d.arg_string_, devname);

    if(expect.d.arg_volinfo_ != nullptr)
        info = *expect.d.arg_volinfo_;

    return expect.d.ret_bool_;
}

void osdev_free_volume_information(Devices::VolumeInfo *info)
{
    const auto &expect(mock_devices_os_singleton->expectations_->get_next_expectation(__func__));

    cppcut_assert_equal(expect.d.function_id_, Fn::free_volume_information);

    /*
     * The \p info pointer will be some internal pointer that we cannot access
     * here in the mock, so we resort to deep comparison. In a better solution,
     * the pointer passed to osdev_get_volume_information() would be captured
     * and compared against \p info at this point.
     */
    cppcut_assert_equal(expect.d.arg_volinfo_->idx, info->idx);
    cppcut_assert_equal(expect.d.arg_volinfo_->label, info->label);
    cppcut_assert_equal(expect.d.arg_volinfo_->fstype, info->fstype);
}
