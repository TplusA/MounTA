/*
 * Copyright (C) 2015, 2017  T+A elektroakustik GmbH & Co. KG
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

#ifndef DEVICES_HH
#define DEVICES_HH

#include <memory>
#include <string>
#include <map>

#include "autodir.hh"
#include "messages.h"

namespace Automounter { class FSMountOptions; }

namespace Devices
{

/*!
 * \internal
 * \brief Internal class for generating IDs for devices.
 */
class ID
{
  public:
    using value_type = unsigned short;

    const value_type value_;

  private:
    static constexpr value_type max_id_ = 999;
    static value_type next_free_id_;

  public:
    explicit ID(): value_(::Devices::ID::get_next_id()) {}

  private:
    static value_type get_next_id()
    {
        if(++next_free_id_ > max_id_)
            next_free_id_ = 1;

        return next_free_id_;
    }
};

class Volume;

/*!
 * Representation of a block device that may contain mountable volumes.
 */
class Device
{
  public:
    enum State
    {
        /*!
         * Device was created because a volume for it was found.
         *
         * The device itself wasn't found yet, but could be found at some later
         * point.
         */
        SYNTHETIC,

        /*!
         * Device was created because the kernel has added it to the system.
         *
         * Hardware connection has been probed, but device was not yet accepted
         * as OK-to-use by application.
         */
        PROBED,

        /*!
         * Device is usable.
         */
        OK,

        /*!
         * Device is known and usable, but rejected by system policies.
         *
         * If the device is rejected, then so are all its volumes.
         */
        REJECTED,

        /*!
         * Device is known, but not usable at all.
         *
         * Kept around for filtering.
         */
        BROKEN,
    };

  private:
    /*!
     * Internal unique ID of this device.
     */
    const ID id_;

    /*!
     * Original name of the symlink pointing to the block device.
     */
    std::string devlink_name_;

    /*!
     * Human-readable name of the device as extracted from the device.
     *
     * Basically, this string contains the name of the symlink from
     * Devices::Device::devlink_name_, without the full path to it. It is
     * filled when the device is probed, i.e., it will not be available for
     * synthesized objects.
     *
     * Note that this is not the name of the block device as maintained by the
     * kernel, but a string that is queried from the device itself. This could
     * be some string from a USB descriptor or from ATA information.
     *
     * This string may require some post-processing before being useful for
     * displaying purposes.
     */
    std::string device_name_;

    /*!
     * Where the mountpoints for this device will be created.
     */
    Automounter::Directory mountpoint_container_path_;

    /*!
     * All volumes on this device, indexed by volume index (partition number).
     */
    std::map<int, std::unique_ptr<Volume>> volumes_;

    /*!
     * Whether or not this structure was created because a volume was found.
     *
     * In case the state is #Devices::Device::SYNTHETIC, volumes belong to this
     * device are not mounted because the device structure was generated from
     * the volume information alone. When the device itself is found and added
     * to the device manager, its state transitions to
     * #Devices::Device::PROBED or #Devices::Device::BROKEN.
     *
     * When handling the device in state #Devices::Device::PROBED in the
     * automounter, it will call #Devices::Device::accept() or
     * #Devices::Device::reject(), depending on filter policies. The device is
     * then set to state #Devices::Device::OK or #Devices::Device::REJECTED,
     * respectively.
     *
     * In case the state is #Devices::Device::OK, all volumes in
     * #Devices::Device::volumes_ and new volumes found for this device need to
     * be processed (i.e., mounted) while the kernel adds new devices.
     *
     * In case the state is #Devices::Device::REJECTED, no volume in
     * #Devices::Device::volumes_ will be mounted (note that it is also
     * possible to reject individual volumes).
     *
     * This way we defer mounting of volumes until information about their
     * containing device are available. We need these information for
     * client-friendly D-Bus announcements (any volume is announced after its
     * containing device).
     */
    State state_;

    /*!
     * Name of the USB port in sysfs.
     */
    std::string usb_port_;

  public:
    Device(const Device &) = delete;
    Device &operator=(const Device &) = delete;

    explicit Device(ID device_id, const std::string &devlink, bool is_real):
        id_(device_id),
        devlink_name_(devlink),
        state_(SYNTHETIC)
    {
        if(is_real)
            do_probe();
    }

    ~Device();

    const ID::value_type get_id() const { return id_.value_; }
    const std::string &get_devlink_name() const { return devlink_name_; }
    const std::string &get_display_name() const { return device_name_; }
    const std::string &get_usb_port() const { return usb_port_; }

    State get_state() const { return state_; }

    void accept() { state_ = OK; }
    void reject() { state_ = REJECTED; }
    bool probe();

    Volume *lookup_volume_by_devname(const std::string &devname) const;
    bool add_volume(std::unique_ptr<Devices::Volume> &&volume);
    void drop_volumes();

    const Automounter::Directory &get_working_directory() const { return mountpoint_container_path_; }
    bool mk_working_directory(std::string &&path);

    bool empty() const { return volumes_.empty(); }
    decltype(volumes_)::const_iterator begin() const { return volumes_.begin(); };
    decltype(volumes_)::const_iterator end() const   { return volumes_.end(); };

  private:
    bool do_probe();
    void cleanup_fs(bool not_expecting_failure);
};

/*!
 * Representation of a mountable volume.
 *
 * Volumes are always owned by #Devices::Device objects.
 */
class Volume
{
  public:
    enum State
    {
        PENDING,  /*!< No attempt has yet been made to mount the volume. */
        MOUNTED,  /*!< Volume is currently mounted. */
        UNUSABLE, /*!< Attempted to mount the volume, but failed. */
        REJECTED, /*! <Volume is rejected by system policies. */
        REMOVED,  /*!< Volume is not mounted anymore (shutting down). */
    };

  private:
    /*!
     * Which device this volume is stored on.
     */
    std::shared_ptr<Device> containing_device_;

    /*!
     * Number of the volume on its containing device.
     *
     * These numbers are usually positive partition numbers. A value of -1
     * means that there is no number, which usually means that the volume spans
     * the whole containing device. Values smaller than -1 are invalid.
     *
     * The low-level code is generic enough to avoid making these assumptions,
     * though. It allows having volumes with index -1, index 0, and positive
     * numbers on the same device at the same time as long as they are unique.
     */
    int index_;

    /*!
     * State of this volume.
     */
    State state_;

    /*!
     * Volume label as stored on the volume.
     *
     * This is either a human-readable name read from the volume itself, or the
     * file system type. The label will be empty for volumes in state
     * #Devices::Volume::PENDING or #Devices::Volume::UNUSABLE.
     */
    std::string label_;

    /*!
     * Volume file system type.
     */
    std::string fstype_;

    /*!
     * Name of the block device.
     */
    std::string devname_;

    /*!
     * Full path to this volume's mountpoint.
     */
    Automounter::Mountpoint mountpoint_;

  public:
    Volume(const Volume &) = delete;
    Volume &operator=(const Volume &) = delete;

    explicit Volume(std::shared_ptr<Device> containing_device,
                    int idx, const std::string &label,
                    const std::string &fstype, const std::string &devname,
                    const Automounter::ExternalTools &tools):
        containing_device_(containing_device),
        index_(idx),
        state_(PENDING),
        label_(label),
        fstype_(fstype),
        devname_(devname),
        mountpoint_(tools)
    {}

    std::shared_ptr<const Device> get_device() const { return containing_device_; }
    int get_index() const { return index_; }
    State get_state() const { return state_; }
    const std::string &get_label() const { return label_; }
    const std::string &get_fstype() const { return fstype_; }
    const std::string &get_device_name() const { return devname_; }

    void reject() { state_ = REJECTED; }

    bool mk_mountpoint_directory();
    const std::string &get_mountpoint_name() const { return mountpoint_.str(); }

    bool mount(const Automounter::FSMountOptions &mount_options);

    void set_mounted();
    void set_removed();
    void set_unusable();

  private:
    void set_eol_state_and_cleanup(State state, bool not_expecting_failure);
};

}

#endif /* !DEVICES_HH */
