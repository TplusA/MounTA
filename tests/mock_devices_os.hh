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
