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

#include "device_manager.hh"
#include "mock_messages.hh"
#include "mock_os.hh"

namespace device_manager_tests
{

static MockMessages *mock_messages;
static MockOs *mock_os;

static Devices::AllDevices *devs;

void cut_setup(void)
{
    mock_messages = new MockMessages;
    cppcut_assert_not_null(mock_messages);
    mock_messages->init();
    mock_messages_singleton = mock_messages;

    mock_os = new MockOs;
    cppcut_assert_not_null(mock_os);
    mock_os->init();
    mock_os_singleton = mock_os;

    devs = new Devices::AllDevices();
}

void cut_teardown(void)
{
    delete devs;
    devs = nullptr;

    mock_messages->check();
    mock_os->check();

    mock_messages_singleton = nullptr;
    mock_os_singleton = nullptr;

    delete mock_messages;
    delete mock_os;

    mock_messages = nullptr;
    mock_os = nullptr;
}

/*!\test
 * Most straightforward use of the API: add full device, then its volumes.
 */
void test_new_device_with_volumes()
{
    const Devices::Volume *no_vol;
    const Devices::Device *const dev = devs->new_entry("/dev/sdt", &no_vol);
    cppcut_assert_not_null(dev);
    cppcut_assert_equal("My Device", dev->get_name().c_str());
    cppcut_assert_null(no_vol);

    const Devices::Volume *vol1;
    cppcut_assert_equal(dev, devs->new_entry("/dev/sdt1", &vol1));
    cppcut_assert_not_null(vol1);

    const Devices::Volume *vol2;
    cppcut_assert_equal(dev, devs->new_entry("/dev/sdt2", &vol2));
    cppcut_assert_not_null(vol2);

    const Devices::Volume *vol3;
    cppcut_assert_equal(dev, devs->new_entry("/dev/sdt5", &vol3));
    cppcut_assert_not_null(vol3);

    cppcut_assert_equal("P1", vol1->get_name().c_str());
    cppcut_assert_equal("P2", vol2->get_name().c_str());
    cppcut_assert_equal("P5", vol3->get_name().c_str());

    cppcut_assert_equal(dev, vol1->get_device());
    cppcut_assert_equal(dev, vol2->get_device());
    cppcut_assert_equal(dev, vol3->get_device());
}

/*!\test
 * New devices may be added without knowing their volumes.
 */
void test_new_devices_without_volumes()
{
    static std::array<const char *, 2> names = { "Dev A", "Dev B", };

    const Devices::Volume *vol;
    const Devices::Device *dev1 = devs->new_entry("/dev/sdt", &vol);
    cppcut_assert_not_null(dev1);
    cppcut_assert_equal(names[0], dev1->get_name().c_str());
    cppcut_assert_null(vol);

    const Devices::Device *dev2 = devs->new_entry("/dev/sdu", &vol);
    cppcut_assert_not_null(dev2);
    cppcut_assert_equal(names[1], dev2->get_name().c_str());
    cppcut_assert_null(vol);

    cppcut_assert_not_equal(dev1, dev2);

    size_t i = 0;
    for(const auto &dev : *devs)
    {
        cppcut_assert_operator(names.size(), >, i);
        cppcut_assert_equal(names[i], dev.second->get_name().c_str());
        ++i;
    }
}

/*!\test
 * New volumes may be added without prior introduction of the full device.
 */
void test_new_volumes_with_late_full_device()
{
    /* three volumes on same device, but full device not seen yet */
    const Devices::Volume *vol1;
    const Devices::Device *const dev = devs->new_entry("/dev/sdt1", &vol1);
    cppcut_assert_not_null(dev);
    cppcut_assert_not_null(vol1);
    cppcut_assert_equal(dev, vol1->get_device());

    const Devices::Volume *vol2;
    cppcut_assert_equal(dev, devs->new_entry("/dev/sdt10", &vol2));
    cppcut_assert_not_null(vol2);
    cppcut_assert_equal(dev, vol2->get_device());

    const Devices::Volume *vol3;
    cppcut_assert_equal(dev, devs->new_entry("/dev/sdt100", &vol3));
    cppcut_assert_not_null(vol3);
    cppcut_assert_equal(dev, vol3->get_device());

    cppcut_assert_equal("First partition", vol1->get_name().c_str());
    cppcut_assert_equal("Second", vol2->get_name().c_str());
    cppcut_assert_equal("Partition with unreasonably high index", vol3->get_name().c_str());

    cut_assert_true(dev->get_name().empty());

    /* found full device: existing structure is used, no volume is returned */
    const Devices::Volume *no_vol;
    cppcut_assert_equal(dev, devs->new_entry("/dev/sdt", &no_vol));
    cppcut_assert_null(no_vol);

    cppcut_assert_equal(dev, vol1->get_device());
    cppcut_assert_equal("Device with three partitions", dev->get_name().c_str());

    /* found yet another partition on that strange device */
    const Devices::Volume *vol4;
    cppcut_assert_equal(dev, devs->new_entry("/dev/sdt2", &vol4));
    cppcut_assert_not_null(vol4);

    cppcut_assert_equal("Slow partition", vol4->get_name().c_str());

    cppcut_assert_equal(dev, vol1->get_device());
    cppcut_assert_equal(dev, vol2->get_device());
    cppcut_assert_equal(dev, vol3->get_device());
    cppcut_assert_equal(dev, vol4->get_device());
}

}
