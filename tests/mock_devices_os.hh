/*
 * Copyright (C) 2015, 2019, 2020  T+A elektroakustik GmbH & Co. KG
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

#ifndef MOCK_DEVICES_OS_HH
#define MOCK_DEVICES_OS_HH

#include "devices_os.hh"
#include "mock_expectation.hh"

namespace MockDevicesOs
{

/*! Base class for expectations. */
class Expectation
{
  public:
    Expectation(const Expectation &) = delete;
    Expectation(Expectation &&) = default;
    Expectation &operator=(const Expectation &) = delete;
    Expectation &operator=(Expectation &&) = default;
    Expectation() {}
    virtual ~Expectation() {}
};

class Mock
{
  private:
    MockExpectationsTemplate<Expectation> expectations_;

  public:
    Mock(const Mock &) = delete;
    Mock &operator=(const Mock &) = delete;

    explicit Mock():
        expectations_("MockDevicesOs")
    {}

    ~Mock() = default;

    void expect(std::unique_ptr<Expectation> expectation)
    {
        expectations_.add(std::move(expectation));
    }

    void expect(Expectation *expectation)
    {
        expectations_.add(std::unique_ptr<Expectation>(expectation));
    }

    template <typename T>
    void ignore(std::unique_ptr<T> default_result)
    {
        expectations_.ignore<T>(std::move(default_result));
    }

    template <typename T>
    void ignore(T *default_result)
    {
        expectations_.ignore<T>(std::unique_ptr<Expectation>(default_result));
    }

    template <typename T>
    void allow() { expectations_.allow<T>(); }

    void done() const { expectations_.done(); }

    template <typename T, typename ... Args>
    auto check_next(Args ... args) -> decltype(std::declval<T>().check(args...))
    {
        return expectations_.check_and_advance<T, decltype(std::declval<T>().check(args...))>(args...);
    }

    template <typename T>
    const T &next(const char *caller)
    {
        return expectations_.next<T>(caller);
    }
};

class GetDeviceInformation: public Expectation
{
  private:
    const bool retval_;
    const std::string devlink_;
    const Devices::DeviceInfo *const devinfo_;

  public:
    explicit GetDeviceInformation(std::string &&devlink, const Devices::DeviceInfo *info):
        retval_(info != nullptr),
        devlink_(std::move(devlink)),
        devinfo_(info)
    {}

    bool check(const std::string &devlink, Devices::DeviceInfo &info) const
    {
        CHECK(devlink == devlink_);

        if(devinfo_ != nullptr)
            info = *devinfo_;

        return retval_;
    }
};

class GetVolumeInformation: public Expectation
{
  private:
    const bool retval_;
    const std::string devname_;
    const Devices::VolumeInfo *const volinfo_;

  public:
    explicit GetVolumeInformation(std::string &&devname, const Devices::VolumeInfo *info):
        retval_(info != nullptr),
        devname_(std::move(devname)),
        volinfo_(info)
    {}

    bool check(const std::string &devname, Devices::VolumeInfo &info) const
    {
        CHECK(devname == devname_);

        if(volinfo_ != nullptr)
            info = *volinfo_;

        return retval_;
    }
};

extern Mock *singleton;

}

#endif /* !MOCK_DEVICES_OS_HH */
