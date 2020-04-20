/*
 * Copyright (C) 2015, 2017, 2019, 2020  T+A elektroakustik GmbH & Co. KG
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

#include <doctest.h>

#include "device_manager.hh"
#include "external_tools.hh"
#include "automounter.hh"

#include "mock_messages.hh"
#include "mock_os.hh"
#include "mock_devices_os.hh"

/* Stuff the linker wants, but we don't need */
bool os_rmdir(const char *path, bool must_exist)
{
    FAIL("Unexpected call");
    return false;
}

int os_foreach_in_path(const char *path,
                       int (*callback)(const char *path, unsigned char dtype,
                                       void *user_data),
                       void *user_data)
{
    FAIL("Unexpected call");
    return -1;
}

int os_stat(const char *path, struct stat *buf)
{
    FAIL("Unexpected call");
    return -1;
}

bool os_mkdir_hierarchy(const char *path, bool must_not_exist)
{
    FAIL("Unexpected call");
    return false;
}

void os_nanosleep(const struct timespec *tp)
{
    FAIL("Unexpected call");
}

int os_system_formatted(bool is_verbose, const char *format_string, ...)
{
    FAIL("Unexpected call");
    return -1;
}

const char *Automounter::FSMountOptions::get_options(const std::string &fstype) const
{
    FAIL("Unexpected call");
    return nullptr;
}

/* The actual unit tests */
TEST_SUITE_BEGIN("Device manager");

class DevNames
{
  public:
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

static Automounter::ExternalTools tools(
            Automounter::ExternalTools::Command("/bin/mount",          nullptr),
            Automounter::ExternalTools::Command("/bin/umount",         nullptr),
            Automounter::ExternalTools::Command("/usr/bin/mountpoint", "-q"),
            Automounter::ExternalTools::Command("/sbin/blkid",         nullptr),
            Automounter::ExternalTools::Command("/bin/udevadm",        nullptr));

class Fixture
{
  protected:
    std::unique_ptr<MockMessages::Mock> mock_messages;
    std::unique_ptr<MockOS::Mock> mock_os;
    std::unique_ptr<MockDevicesOs::Mock> mock_devices_os;

    std::unique_ptr<Devices::AllDevices> devs;

    explicit Fixture():
        mock_messages(std::make_unique<MockMessages::Mock>()),
        mock_os(std::make_unique<MockOS::Mock>()),
        mock_devices_os(std::make_unique<MockDevicesOs::Mock>()),
        devs(std::make_unique<Devices::AllDevices>(tools, std::string()))
    {
        MockMessages::singleton = mock_messages.get();
        MockOS::singleton = mock_os.get();
        MockDevicesOs::singleton = mock_devices_os.get();
    }

    ~Fixture()
    {
        devs = nullptr;

        try
        {
            mock_messages->done();
            mock_os->done();
            mock_devices_os->done();
        }
        catch(...)
        {
            /* no throwing from dtors */
        }

        MockMessages::singleton = nullptr;
        MockOS::singleton = nullptr;
        MockDevicesOs::singleton = nullptr;
    }

  protected:
    void add_device_probe_expectations(const char *name,
                                       const Devices::DeviceInfo &info)
    {
        expect<MockDevicesOs::GetDeviceInformation>(mock_devices_os, name, &info);
    }

    std::shared_ptr<Devices::Device>
    new_device_with_expectations(const DevNames &device_names,
                                 const Devices::Volume **ret_volume,
                                 bool expecting_null_volume,
                                 bool device_exists_already = false,
                                 const Devices::VolumeInfo *fake_info = nullptr,
                                 bool expecting_device_probe = true)
    {
        static const Devices::DeviceInfo
            fake_device_info("/sys/devices/platform/bcm2708_usb/usb1/1-1/1-1.5/1-1.5:1.0");

        expect<MockOS::ResolveSymlink>(mock_os, device_names.block_device_name, 0, device_names.device_identifier);

        if(!device_exists_already)
        {
            expect<MockDevicesOs::GetVolumeInformation>(mock_devices_os, device_names.block_device_name, fake_info);

            if(expecting_device_probe)
                add_device_probe_expectations(device_names.device_identifier, fake_device_info);
        }
        else if(expecting_device_probe)
            add_device_probe_expectations(device_names.device_identifier, fake_device_info);

        Devices::Volume *volume;
        bool have_probed_dev;
        const std::shared_ptr<Devices::Device> dev =
            devs->new_entry(device_names.device_identifier, volume, have_probed_dev);

        REQUIRE(dev.get() != nullptr);
        CHECK(dev->get_devlink_name() == device_names.device_identifier);
        CHECK(int(dev->get_state()) == int(Devices::Device::PROBED));
        CHECK(have_probed_dev == expecting_device_probe);

        if(expecting_null_volume)
            CHECK(volume == nullptr);
        else
            CHECK(volume != nullptr);

        if(ret_volume != nullptr)
            *ret_volume = volume;

        return dev;
    }

    void remove_device_with_expectations(const char *devlink, const DevNames *expected_volumes)
    {
        bool removed = false;

        const bool ret =
            devs->remove_entry(devlink,
                               [&removed] (const Devices::Device &dev) { removed = true; });

        CHECK(ret);
        CHECK(removed);
    }

    const Devices::Volume *
    new_volume_with_expectations(int idx, const DevNames &volume_names,
                                std::shared_ptr<Devices::Device> expected_device,
                                Devices::Device::State expected_device_state = Devices::Device::PROBED)
    {
        const Devices::VolumeInfo fake_info(idx, volume_names.volume_label, volume_names.volume_fstype);

        expect<MockOS::ResolveSymlink>(mock_os, volume_names.block_device_name, 0, volume_names.device_identifier);
        expect<MockDevicesOs::GetVolumeInformation>(mock_devices_os, volume_names.block_device_name, &fake_info);

        const Devices::Volume *vol;
        bool have_probed_dev;
        CHECK(devs->new_entry(volume_names.device_identifier, vol, have_probed_dev).get() == expected_device.get());
        REQUIRE(vol != nullptr);
        CHECK(vol->get_device().get() == expected_device.get());
        CHECK(vol->get_label() == volume_names.volume_label);
        CHECK(vol->get_fstype() == volume_names.volume_fstype);
        CHECK(int(vol->get_device()->get_state()) == int(expected_device_state));
        CHECK_FALSE(have_probed_dev);

        CHECK(expected_device->lookup_volume_by_devname(vol->get_device_name().c_str()) == vol);

        return vol;
    }

    const Devices::Volume *
    new_volume_with_expectations(int idx, const DevNames &volume_names,
                                 std::shared_ptr<Devices::Device> &ret_device,
                                 bool expecting_null_device)
    {
        const Devices::VolumeInfo fake_info(idx, volume_names.volume_label, volume_names.volume_fstype);

        expect<MockOS::ResolveSymlink>(mock_os, volume_names.block_device_name, 0, volume_names.device_identifier);
        expect<MockDevicesOs::GetVolumeInformation>(mock_devices_os, volume_names.block_device_name, &fake_info);

        Devices::Volume *vol;
        bool have_probed_dev;
        ret_device = devs->new_entry(volume_names.device_identifier, vol, have_probed_dev);
        REQUIRE(vol != nullptr);
        CHECK(vol->get_label() == volume_names.volume_label);
        CHECK(vol->get_fstype() == volume_names.volume_fstype);
        CHECK_FALSE(have_probed_dev);

        if(expecting_null_device)
            CHECK(ret_device.get() == nullptr);
        else
        {
            REQUIRE(ret_device.get() != nullptr);
            CHECK(int(ret_device->get_state()) == int(Devices::Device::SYNTHETIC));
            CHECK(vol->get_device().get() == ret_device.get());
            CHECK(ret_device->lookup_volume_by_devname(vol->get_device_name().c_str()) == vol);
        }

        return vol;
    }
};

/*!\test
 * Most straightforward use of the API: add full device, then its volumes.
 */
TEST_CASE_FIXTURE(Fixture, "Add new device with several volumes")
{
    /* device first */
    static constexpr DevNames device_names("/dev/sdt", "usb-Mass_Storage_Device_12345");

    static constexpr std::array<const DevNames, 3> volume_names =
    {
        DevNames("/dev/sdt1", "usb-Mass_Storage_Device_12345-part1", "P1", "ext4"),
        DevNames("/dev/sdt2", "usb-Mass_Storage_Device_12345-part2", "P2", "ext3"),
        DevNames("/dev/sdt5", "usb-Mass_Storage_Device_12345-part5", "P5", "btrfs"),
    };

    const auto dev = new_device_with_expectations(device_names, nullptr, true);

    new_volume_with_expectations(1, volume_names[0], dev);
    new_volume_with_expectations(2, volume_names[1], dev);
    new_volume_with_expectations(5, volume_names[2], dev);

    /* enumerate devices (only one) */
    size_t i = 0;
    for(const auto &it : *devs)
    {
        CHECK(i < 1);
        CHECK(it.second->get_devlink_name() == device_names.device_identifier);
        ++i;
    }

    CHECK(i == 1);

    /* enumerate volumes on device */
    const auto &it = devs->begin();
    i = 0;
    for(const auto &p : *it->second)
    {
        REQUIRE(i < volume_names.size());
        CHECK(p.second->get_label() == volume_names[i].volume_label);
        CHECK(p.second->get_fstype() == volume_names[i].volume_fstype);
        ++i;
    }

    CHECK(i == volume_names.size());

    dev->drop_volumes();
}

/*!\test
 * Devices may contain a volume without any partition table.
 */
TEST_CASE_FIXTURE(Fixture, "Add new device with single volume without partition table")
{
    static constexpr DevNames device_names("/dev/sdt", "usb-Device_ABC");
    static const Devices::VolumeInfo fake_info(-1, "My Volume", "ext2");

    const Devices::Volume *vol;
    const auto dev =
        new_device_with_expectations(device_names, &vol, false, false, &fake_info);

    CHECK(vol->get_device().get() == dev.get());
    REQUIRE(dev.get() != nullptr);
    auto it = dev->begin();
    CHECK(it != dev->end());
    CHECK(it->second.get() == vol);

    static const DevNames expected_volume(device_names.block_device_name,
                                          device_names.device_identifier,
                                          fake_info.label.c_str(),
                                          fake_info.fstype.c_str());

    CHECK(vol->get_device_name() == expected_volume.block_device_name);
    CHECK(vol->get_device()->get_devlink_name() == expected_volume.device_identifier);
    CHECK(vol->get_label() == expected_volume.volume_label);
    CHECK(vol->get_fstype() == expected_volume.volume_fstype);
    CHECK(vol->get_index() == -1);

    dev->drop_volumes();
}

/*!\test
 * New devices may be added without knowing their volumes.
 */
TEST_CASE_FIXTURE(Fixture, "Add new devices before their volumes are known")
{
    static constexpr std::array<const DevNames, 2> device_names =
    {
        DevNames("/dev/sdt", "usb-Some_USB_Mass_Storage_Device_12345"),
        DevNames("/dev/sdu", "usb-Another_Block_Device_98765"),
    };

    const auto dev1 = new_device_with_expectations(device_names[0], nullptr, true);
    const auto dev2 = new_device_with_expectations(device_names[1], nullptr, true);

    CHECK(dev1.get() != dev2.get());

    /* enumerate devices (two of them) */
    size_t i = 0;
    for(const auto &dev : *devs)
    {
        REQUIRE(i < device_names.size());
        CHECK(dev.second->get_devlink_name() == device_names[i].device_identifier);
        CHECK(dev.second->begin() == dev.second->end());
        ++i;
    }

    CHECK(i == device_names.size());
}

/*!\test
 * New volumes may be added without prior introduction of the full device.
 */
TEST_CASE_FIXTURE(Fixture, "Add new volumes before their respective devices are known")
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
    std::shared_ptr<Devices::Device> dev;
    const Devices::Volume *vol1 = new_volume_with_expectations(1,   volume_names[0], dev, false);
    const Devices::Volume *vol2 = new_volume_with_expectations(10,  volume_names[1], dev, Devices::Device::SYNTHETIC);
    const Devices::Volume *vol3 = new_volume_with_expectations(100, volume_names[2], dev, Devices::Device::SYNTHETIC);

    /* name was already guessed */
    CHECK(dev->get_devlink_name() == device_names.device_identifier);

    /* found full device: existing structure is used, no volume is returned */
    expect<MockMessages::MsgInfo>(mock_messages, "Device usb-Disk_864216 already registered", false);
    CHECK(new_device_with_expectations(device_names, nullptr, true, true).get() == dev.get());

    CHECK(dev.get() == vol1->get_device().get());
    CHECK(dev->get_devlink_name() == device_names.device_identifier);

    /* found yet another partition on that strange device */
    const Devices::Volume *vol4 = new_volume_with_expectations(2, volume_names[3], dev);

    CHECK(dev.get() == vol1->get_device().get());
    CHECK(dev.get() == vol2->get_device().get());
    CHECK(dev.get() == vol3->get_device().get());
    CHECK(dev.get() == vol4->get_device().get());

    dev->drop_volumes();
}

static void check_device_iterator(const Devices::AllDevices &devs,
                                  const DevNames *const device_names,
                                  const size_t number_of_device_names)
{
    if(device_names == nullptr || number_of_device_names == 0)
    {
        for(const auto &it : devs)
        {
            FAIL("Number of devices is 0, but iterator returned element");

            /* avoid compiler warning about unused variable */
            CHECK(it.second.get() == nullptr);
        }
    }
    else
    {
        size_t i = 0;

        for(const auto &it : devs)
        {
            REQUIRE(i < number_of_device_names);
            CHECK(it.second->get_devlink_name() == device_names[i].device_identifier);
            ++i;
        }

        CHECK(i == number_of_device_names);
    }
}

/*!\test
 * Disks without any volumes can be removed.
 */
TEST_CASE_FIXTURE(Fixture, "Disks without any volumes can be removed")
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

    REQUIRE(devs->get_number_of_devices() == device_names.size());
    check_device_iterator(*devs, device_names.data(), device_names.size());

    remove_device_with_expectations(device_names[2].device_identifier, nullptr);

    REQUIRE(devs->get_number_of_devices() == device_names.size() - 1U);
    check_device_iterator(*devs, device_names.data(), device_names.size() - 1U);

    remove_device_with_expectations(device_names[1].device_identifier, nullptr);

    REQUIRE(devs->get_number_of_devices() == device_names.size() - 2U);
    check_device_iterator(*devs, device_names.data(), device_names.size() - 2U);

    remove_device_with_expectations(device_names[0].device_identifier, nullptr);

    REQUIRE(devs->get_number_of_devices() == 0);
    check_device_iterator(*devs, nullptr, 0);
}

/*!\test
 * Disks with volumes can be removed.
 */
TEST_CASE_FIXTURE(Fixture, "Disks with volumes can be removed")
{
    static constexpr std::array<const DevNames, 2> device_names =
    {
        DevNames("/dev/sdm", "usb-Device_D_0BC7"),
        DevNames("/dev/sdn", "usb-Device_E_0815"),
    };

    static constexpr std::array<const DevNames, 2> volume_names_sdm =
    {
        DevNames("/dev/sdm1", "usb-Device_D_0BC7-part1", "Pm1", "fsm1"),
        DevNames("/dev/sdm2", "usb-Device_D_0BC7-part2", "Pm2", "fsm2"),
    };

    static constexpr std::array<const DevNames, 4> volume_names_sdn =
    {
        DevNames("/dev/sdn1", "usb-Device_E_0815-part1", "Pn1", "fsn1"),
        DevNames("/dev/sdn2", "usb-Device_E_0815-part2", "Pn2", "fsn2"),
        DevNames("/dev/sdn3", "usb-Device_E_0815-part3", "Pn3", "fsn3"),
        DevNames("/dev/sdn4", "usb-Device_E_0815-part4", "Pn4", "fsn4"),
    };

    const auto dev_sdm = new_device_with_expectations(device_names[0], nullptr, true);
    const auto dev_sdn = new_device_with_expectations(device_names[1], nullptr, true);

    for(const auto &vol : volume_names_sdm)
        new_volume_with_expectations((&vol - &volume_names_sdm[0]) + 1U,
                                     vol, dev_sdm);

    for(const auto &vol : volume_names_sdn)
        new_volume_with_expectations((&vol - &volume_names_sdn[0]) + 1U,
                                     vol, dev_sdn);

    REQUIRE(devs->get_number_of_devices() == device_names.size());
    check_device_iterator(*devs, device_names.data(), device_names.size());

    remove_device_with_expectations(device_names[1].device_identifier, volume_names_sdn.data());

    REQUIRE(devs->get_number_of_devices() == device_names.size() - 1U);
    check_device_iterator(*devs, device_names.data(), device_names.size() - 1U);

    remove_device_with_expectations(device_names[0].device_identifier, volume_names_sdm.data());

    REQUIRE(devs->get_number_of_devices() == 0);
    check_device_iterator(*devs, nullptr, 0);

    dev_sdm->drop_volumes();
    dev_sdn->drop_volumes();
}

/*!\test
 * In case a device is added twice, a diagnostic message is emitted, but no
 * further resources are allocated.
 */
TEST_CASE_FIXTURE(Fixture, "Devices cannot be added twice")
{
    static constexpr DevNames device_names("/dev/sdd", "usb-Duplicate_Disk_9310");

    const auto dev = new_device_with_expectations(device_names, nullptr, true);

    expect<MockMessages::MsgInfo>(mock_messages, "Device usb-Duplicate_Disk_9310 already registered", false);
    const auto again = new_device_with_expectations(device_names, nullptr, true, true, nullptr, false);
    CHECK(again.get() == dev.get());

    CHECK(devs->get_number_of_devices() == 1);
}

/*!\test
 * In case a device containing a volume without a partition table is added
 * twice, a diagnostic message is emitted, but no further resources are
 * allocated.
 */
TEST_CASE_FIXTURE(Fixture, "Devices with volume on whole device cannot be added twice")
{
    static constexpr DevNames device_names("/dev/sdd", "usb-Duplicate_Disk_9310");
    static const Devices::VolumeInfo fake_info(-1, "Awesome Storage Device", "ext4");

    const Devices::Volume *vol;
    const auto dev = new_device_with_expectations(device_names, &vol, false, false, &fake_info);

    expect<MockMessages::MsgInfo>(mock_messages, "Device usb-Duplicate_Disk_9310 already registered", false);
    const Devices::Volume *vol_again;
    const auto dev_again = new_device_with_expectations(device_names, &vol_again, false, true, nullptr, false);
    CHECK(dev_again.get() == dev.get());
    CHECK(vol_again == vol);

    REQUIRE(devs->get_number_of_devices() == 1);

    auto dev_it(devs->begin());
    CHECK(dev.get() == dev_it->second.get());
    ++dev_it;
    CHECK(dev_it == devs->end());

    auto vol_it(dev->begin());
    CHECK(vol == vol_it->second.get());
    ++vol_it;
    CHECK(vol_it == dev->end());

    dev->drop_volumes();
}

/*!\test
 * In case a volume is added twice, a diagnostic message is emitted, but no
 * further resources are allocated.
 */
TEST_CASE_FIXTURE(Fixture, "Volumes cannot be added twice")
{
    static constexpr DevNames volume_names("/dev/sdd1", "usb-Duplicate_9310-part1", "One", "btrfs");
    std::shared_ptr<Devices::Device> dev;
    const Devices::Volume *vol = new_volume_with_expectations(1, volume_names, dev, false);

    expect<MockMessages::MsgInfo>(mock_messages, "Volume usb-Duplicate_9310-part1 already registered on device usb-Duplicate_9310", false);
    const Devices::Volume *again = new_volume_with_expectations(1, volume_names, dev, false);
    CHECK(again == vol);

    dev->drop_volumes();
}

TEST_SUITE_END();
