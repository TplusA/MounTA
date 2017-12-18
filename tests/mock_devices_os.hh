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

#ifndef MOCK_DEVICES_OS_HH
#define MOCK_DEVICES_OS_HH

#include "devices_os.hh"
#include "mock_expectation.hh"

class MockDevicesOs
{
  public:
    class Expectation;
    typedef MockExpectationsTemplate<Expectation> MockExpectations;
    MockExpectations *expectations_;

    MockDevicesOs(const MockDevicesOs &) = delete;
    MockDevicesOs &operator=(const MockDevicesOs &) = delete;

    explicit MockDevicesOs();
    ~MockDevicesOs();

    void init();
    void check() const;

    void expect_get_device_information(const std::string &devlink, const Devices::DeviceInfo *info);
    void expect_get_volume_information(const std::string &devname, const Devices::VolumeInfo *info);
};

extern MockDevicesOs *mock_devices_os_singleton;

#endif /* !MOCK_DEVICES_OS_HH */
