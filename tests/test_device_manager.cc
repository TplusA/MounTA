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
#include "mock_devices_os.hh"

namespace device_manager_tests
{

struct DevNames
{
    const char *const block_device_name;
    const char *const device_identifier;
    const char *const volume_label;
    const char *const volume_fstype;

    constexpr explicit DevNames(const char *bdn, const char *devid):
        DevNames(bdn, devid, nullptr, nullptr)
    {}

    constexpr explicit DevNames(const char *bdn, const char *devid,
                                const char *label, const char *fstype):
        block_device_name(bdn),
        device_identifier(devid),
        volume_label(label),
        volume_fstype(fstype)
    {}
};

static MockMessages *mock_messages;
static MockOs *mock_os;
static MockDevicesOs *mock_devices_os;

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

    mock_devices_os = new MockDevicesOs;
    cppcut_assert_not_null(mock_devices_os);
    mock_devices_os->init();
    mock_devices_os_singleton = mock_devices_os;

    devs = new Devices::AllDevices();
    cppcut_assert_not_null(devs);
}

void cut_teardown(void)
{
    delete devs;
    devs = nullptr;

    mock_messages->check();
    mock_os->check();
    mock_devices_os->check();

    mock_messages_singleton = nullptr;
    mock_os_singleton = nullptr;
    mock_devices_os_singleton = nullptr;

    delete mock_messages;
    delete mock_os;
    delete mock_devices_os;

    mock_messages = nullptr;
    mock_os = nullptr;
    mock_devices_os = nullptr;
}

static const Devices::Device *
new_device_with_expectations(const DevNames &device_names,
                             const Devices::Volume **ret_volume,
                             bool expecting_null_volume,
                             bool device_exists_already = false,
                             const struct osdev_volume_info *fake_info = nullptr)
{
    mock_os->expect_os_resolve_symlink(device_names.block_device_name, device_names.device_identifier);

    if(!device_exists_already)
    {
        mock_devices_os->expect_osdev_get_volume_information(device_names.block_device_name, fake_info);

        if(fake_info != nullptr)
            mock_devices_os->expect_osdev_free_volume_information(fake_info);
    }

    const Devices::Volume *volume;
    const Devices::Device *const dev = devs->new_entry(device_names.device_identifier, &volume);

    cppcut_assert_not_null(dev);
    cppcut_assert_equal(device_names.device_identifier, dev->get_name().c_str());

    if(expecting_null_volume)
        cppcut_assert_null(volume);
    else
        cppcut_assert_not_null(volume);

    if(ret_volume != nullptr)
        *ret_volume = volume;

    return dev;
}

static void remove_device_with_expectations(const char *devlink)
{
    cut_assert_true(devs->remove_entry(devlink));
}

static const Devices::Volume *
new_volume_with_expectations(int idx, const DevNames &volume_names,
                             const Devices::Device *expected_device)
{
    const struct osdev_volume_info fake_info =
    {
        .idx = idx,
        .label = volume_names.volume_label,
        .fstype = volume_names.volume_fstype,
    };

    mock_os->expect_os_resolve_symlink(volume_names.block_device_name, volume_names.device_identifier);
    mock_devices_os->expect_osdev_get_volume_information(volume_names.block_device_name, &fake_info);
    mock_devices_os->expect_osdev_free_volume_information(&fake_info);

    const Devices::Volume *vol;
    cppcut_assert_equal(expected_device, devs->new_entry(volume_names.device_identifier, &vol));
    cppcut_assert_not_null(vol);
    cppcut_assert_equal(expected_device, vol->get_device());
    cppcut_assert_equal(volume_names.volume_label, vol->get_label().c_str());

    cppcut_assert_equal(vol, expected_device->lookup_volume_by_devname(vol->get_device_name().c_str()));

    return vol;
}

static const Devices::Volume *
new_volume_with_expectations(int idx, const DevNames &volume_names,
                             const Devices::Device *&ret_device,
                             bool expecting_null_device)
{
    const struct osdev_volume_info fake_info =
    {
        .idx = idx,
        .label = volume_names.volume_label,
        .fstype = volume_names.volume_fstype,
    };

    mock_os->expect_os_resolve_symlink(volume_names.block_device_name, volume_names.device_identifier);
    mock_devices_os->expect_osdev_get_volume_information(volume_names.block_device_name, &fake_info);
    mock_devices_os->expect_osdev_free_volume_information(&fake_info);

    const Devices::Volume *vol;
    ret_device = devs->new_entry(volume_names.device_identifier, &vol);
    cppcut_assert_not_null(vol);
    cppcut_assert_equal(volume_names.volume_label, vol->get_label().c_str());

    if(expecting_null_device)
        cppcut_assert_null(ret_device);
    else
    {
        cppcut_assert_not_null(ret_device);
        cppcut_assert_equal(ret_device, vol->get_device());
        cppcut_assert_equal(vol, ret_device->lookup_volume_by_devname(vol->get_device_name().c_str()));
    }

    return vol;
}

/*!\test
 * Most straightforward use of the API: add full device, then its volumes.
 */
void test_new_device_with_volumes()
{
    /* device first */
    static constexpr DevNames device_names("/dev/sdt", "usb-Mass_Storage_Device_12345");

    static constexpr std::array<const DevNames, 3> volume_names =
    {
        DevNames("/dev/sdt1", "usb-Mass_Storage_Device_12345-part1", "P1", "ext4"),
        DevNames("/dev/sdt2", "usb-Mass_Storage_Device_12345-part2", "P2", "ext3"),
        DevNames("/dev/sdt5", "usb-Mass_Storage_Device_12345-part5", "P5", "btrfs"),
    };

    const Devices::Device *const dev = new_device_with_expectations(device_names, nullptr, true);

    new_volume_with_expectations(1, volume_names[0], dev);
    new_volume_with_expectations(2, volume_names[1], dev);
    new_volume_with_expectations(5, volume_names[2], dev);

    /* enumerate devices (only one) */
    size_t i = 0;
    for(const auto &it : *devs)
    {
        cppcut_assert_operator(size_t(1), >, i);
        cppcut_assert_equal(device_names.device_identifier, it.second->get_name().c_str());
        ++i;
    }

    cppcut_assert_equal(size_t(1), i);

    /* enumerate volumes on device */
    const auto &it = devs->begin();
    i = 0;
    for(const auto &p : *it->second)
    {
        cppcut_assert_operator(volume_names.size(), >, i);
        cppcut_assert_equal(volume_names[i].volume_label, p.second->get_label().c_str());
        ++i;
    }

    cppcut_assert_equal(volume_names.size(), i);
}

/*!\test
 * Devices may contain a volume without any partition table.
 */
void test_new_device_with_volume_on_whole_disk()
{
    static constexpr DevNames device_names("/dev/sdt", "usb-Device_ABC");
    static constexpr struct osdev_volume_info fake_info =
    {
        .idx = -1,
        .label = "My Volume",
        .fstype = "ext2",
    };

    const Devices::Volume *vol;
    const Devices::Device *dev =
        new_device_with_expectations(device_names, &vol, false, false, &fake_info);

    cppcut_assert_equal(dev, vol->get_device());
    auto it = dev->begin();
    cut_assert_true(it != dev->end());
    cppcut_assert_equal(vol, it->second);

    static constexpr DevNames expected_volume(device_names.block_device_name,
                                              device_names.device_identifier,
                                              fake_info.label,
                                              fake_info.fstype);

    cppcut_assert_equal(expected_volume.block_device_name, vol->get_device_name().c_str());
    cppcut_assert_equal(expected_volume.device_identifier, vol->get_device()->get_name().c_str());
    cppcut_assert_equal(expected_volume.volume_label, vol->get_label().c_str());
    cppcut_assert_equal(-1, vol->get_index());
}

/*!\test
 * New devices may be added without knowing their volumes.
 */
void test_new_devices_without_volumes()
{
    static constexpr std::array<const DevNames, 2> device_names =
    {
        DevNames("/dev/sdt", "usb-Some_USB_Mass_Storage_Device_12345"),
        DevNames("/dev/sdu", "usb-Another_Block_Device_98765"),
    };

    const Devices::Device *dev1 = new_device_with_expectations(device_names[0], nullptr, true);
    const Devices::Device *dev2 = new_device_with_expectations(device_names[1], nullptr, true);

    cppcut_assert_not_equal(dev1, dev2);

    /* enumerate devices (two of them) */
    size_t i = 0;
    for(const auto &dev : *devs)
    {
        cppcut_assert_operator(device_names.size(), >, i);
        cppcut_assert_equal(device_names[i].device_identifier, dev.second->get_name().c_str());
        cut_assert_true(dev.second->begin() == dev.second->end());
        ++i;
    }

    cppcut_assert_equal(device_names.size(), i);
}

/*!\test
 * New volumes may be added without prior introduction of the full device.
 */
void test_new_volumes_with_late_full_device()
{
    static constexpr DevNames device_names("/dev/sdt", "usb-Disk_864216");

    static constexpr std::array<const DevNames, 4> volume_names =
    {
        DevNames("/dev/sdt1",   "usb-Disk_864216-part1",   "First partition",         "vfat"),
        DevNames("/dev/sdt10",  "usb-Disk_864216-part10",  "Second",                  "ext2"),
        DevNames("/dev/sdt100", "usb-Disk_864216-part100", "Unreasonably high index", "ufs"),
        DevNames("/dev/sdt2",   "usb-Disk_864216-part2",   "Slow partition",          "hfs"),
    };

    /* three volumes on same device, but full device not seen yet */
    const Devices::Device *dev = nullptr;
    const Devices::Volume *vol1 = new_volume_with_expectations(1,   volume_names[0], dev, false);
    const Devices::Volume *vol2 = new_volume_with_expectations(10,  volume_names[1], dev);
    const Devices::Volume *vol3 = new_volume_with_expectations(100, volume_names[2], dev);

    /* name was already guessed */
    cppcut_assert_equal(device_names.device_identifier, dev->get_name().c_str());

    /* found full device: existing structure is used, no volume is returned */
    mock_messages->expect_msg_info_formatted("Device usb-Disk_864216 already registered");
    cppcut_assert_equal(dev, new_device_with_expectations(device_names, nullptr, true, true));

    cppcut_assert_equal(dev, vol1->get_device());
    cppcut_assert_equal(device_names.device_identifier, dev->get_name().c_str());

    /* found yet another partition on that strange device */
    const Devices::Volume *vol4 = new_volume_with_expectations(2, volume_names[3], dev);

    cppcut_assert_equal(dev, vol1->get_device());
    cppcut_assert_equal(dev, vol2->get_device());
    cppcut_assert_equal(dev, vol3->get_device());
    cppcut_assert_equal(dev, vol4->get_device());
}

static void check_device_iterator(const DevNames *const device_names,
                                  const size_t number_of_device_names)
{
    if(device_names == nullptr || number_of_device_names == 0)
    {
        for(const auto &it : *devs)
        {
            cut_fail("Number of devices is 0, but iterator returned element");

            /* avoid compiler warning about unused variable */
            cppcut_assert_null(it.second);
        }
    }
    else
    {
        size_t i = 0;

        for(const auto &it : *devs)
        {
            cppcut_assert_operator(number_of_device_names, >, i);
            cppcut_assert_equal(device_names[i].device_identifier, it.second->get_name().c_str());
            ++i;
        }

        cppcut_assert_equal(number_of_device_names, i);
    }
}

/*!\test
 * Disks without any volumes can be removed.
 */
void test_remove_devices_without_volumes()
{
    static constexpr std::array<const DevNames, 3> device_names =
    {
        DevNames("/dev/sdt", "usb-Device_A_12345"),
        DevNames("/dev/sdu", "usb-Device_B_98765"),
        DevNames("/dev/sdv", "usb-Device_C_1337"),
    };

    new_device_with_expectations(device_names[0], nullptr, true);
    new_device_with_expectations(device_names[1], nullptr, true);
    new_device_with_expectations(device_names[2], nullptr, true);

    cppcut_assert_equal(device_names.size(), devs->get_number_of_devices());
    check_device_iterator(device_names.data(), device_names.size());

    remove_device_with_expectations(device_names[2].device_identifier);

    cppcut_assert_equal(device_names.size() - 1U, devs->get_number_of_devices());
    check_device_iterator(device_names.data(), device_names.size() - 1U);

    remove_device_with_expectations(device_names[1].device_identifier);

    cppcut_assert_equal(device_names.size() - 2U, devs->get_number_of_devices());
    check_device_iterator(device_names.data(), device_names.size() - 2U);

    remove_device_with_expectations(device_names[0].device_identifier);

    cppcut_assert_equal(size_t(0), devs->get_number_of_devices());
    check_device_iterator(nullptr, 0);
}

/*!\test
 * Disks with volumes can be removed.
 */
void test_remove_devices()
{
    static constexpr std::array<const DevNames, 2> device_names =
    {
        DevNames("/dev/sdm", "usb-Device_D_0BC7"),
        DevNames("/dev/sdn", "usb-Device_E_0815"),
    };

    static constexpr std::array<const DevNames, 2> volume_names_sdm =
    {
        DevNames("/dev/sdn1", "usb-Device_D_0BC7-part1", "Pn1", "fsn1"),
        DevNames("/dev/sdn2", "usb-Device_D_0BC7-part2", "Pn2", "fsn2"),
    };

    static constexpr std::array<const DevNames, 4> volume_names_sdn =
    {
        DevNames("/dev/sdn1", "usb-Device_E_0815-part1", "Pm1", "fsn1"),
        DevNames("/dev/sdn2", "usb-Device_E_0815-part2", "Pm2", "fsn2"),
        DevNames("/dev/sdn3", "usb-Device_E_0815-part3", "Pm3", "fsn3"),
        DevNames("/dev/sdn4", "usb-Device_E_0815-part4", "Pm4", "fsn4"),
    };

    const auto *dev_sdm = new_device_with_expectations(device_names[0], nullptr, true);
    const auto *dev_sdn = new_device_with_expectations(device_names[1], nullptr, true);

    for(const auto &vol : volume_names_sdm)
        new_volume_with_expectations((&vol - &volume_names_sdm[0]) + 1U,
                                     vol, dev_sdm);

    for(const auto &vol : volume_names_sdn)
        new_volume_with_expectations((&vol - &volume_names_sdn[0]) + 1U,
                                     vol, dev_sdn);

    cppcut_assert_equal(device_names.size(), devs->get_number_of_devices());
    check_device_iterator(device_names.data(), device_names.size());

    remove_device_with_expectations(device_names[1].device_identifier);

    cppcut_assert_equal(device_names.size() - 1U, devs->get_number_of_devices());
    check_device_iterator(device_names.data(), device_names.size() - 1U);

    remove_device_with_expectations(device_names[0].device_identifier);

    cppcut_assert_equal(size_t(0), devs->get_number_of_devices());
    check_device_iterator(nullptr, 0);
}

}
