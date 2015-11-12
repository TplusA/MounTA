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

#ifndef DEVICES_HH
#define DEVICES_HH

#include <string>
#include <map>

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
    static constexpr value_type max_id_ = 1000;
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

/*!
 * Micro class for improved type-safety and documentation.
 *
 * Avoids mixing up hub ID (passed through this class) and hub port (passed as
 * regular \c int).
 */
class USBHubID
{
  private:
    int id_;

  public:
    constexpr explicit USBHubID(unsigned int hub_id) noexcept: id_(hub_id) {}
    constexpr int get() const noexcept { return id_; }
};

class Volume;

/*!
 * Representation of a block device that may contain mountable volumes.
 */
class Device
{
  private:
    /*!
     * Internal unique ID of this device.
     */
    const ID id_;

    /*!
     * Human-readable name of the device as extracted from the device.
     *
     * Note that this is not the name of the block device as maintained by the
     * kernel, but a string that is queried from the device itself. This could
     * be some string from a USB descriptor or from ATA information.
     */
    std::string name_;

    /*!
     * Where the mountpoints for this device will be created.
     */
    std::string mountpoint_container_path_;

    /*!
     * All volumes on this device, indexed by volume index (partition number).
     */
    std::map<int, Volume *> volumes_;

    /*!
     * Whether or not this structure was created because a volume was found.
     *
     * False if this device was created because the kernel has added the device
     * to the system; true if this device was created because a volume for this
     * device was found, but the device itself wasn't (yet).
     *
     * In case this flag is true, all volumes in #Devices::Device::volumes_
     * need to be processed (i.e., mounted) when the kernel adds the device.
     * This way we defer mounting of volumes until information about their
     * containing device are available. We need these information for
     * client-friendly D-Bus announcements (any volume is announced after its
     * containing device).
     */
    bool is_pending_;

    /*!
     * ID of the USB root hub ID as provided by the kernel.
     */
    USBHubID root_hub_id_;

    /*!
     * USB port number as provided by the kernel.
     */
    unsigned int hub_port_;

  public:
    Device(const Device &) = delete;
    Device &operator=(const Device &) = delete;

    explicit Device(ID device_id, const std::string &name):
        id_(device_id),
        name_(name),
        is_pending_(true),
        root_hub_id_(0),
        hub_port_(0)
    {}

    explicit Device(ID device_id, const std::string &name,
                    const std::string &mountpoint_container_path,
                    const USBHubID &root_hub_id, unsigned int hub_port):
        id_(device_id),
        name_(name),
        mountpoint_container_path_(mountpoint_container_path),
        is_pending_(false),
        root_hub_id_(root_hub_id),
        hub_port_(hub_port)
    {}

    ~Device();

    const ID::value_type get_id() const { return id_.value_; }
    const std::string &get_name() const { return name_; }

    Volume *lookup_volume_by_devname(const char *devname) const;
    bool add_volume(Volume &volume);

    decltype(volumes_)::const_iterator begin() const { return volumes_.begin(); };
    decltype(volumes_)::const_iterator end() const   { return volumes_.end(); };
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
        REMOVED,  /*!< Volume is not mounted anymore (shutting down). */
    };

  private:
    /*!
     * Which device this volume is stored on.
     */
    Device &containing_device_;

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
     * Name of the block device.
     */
    std::string devname_;

    /*!
     * Full path to this volume's mountpoint.
     */
    std::string mountpoint_path_;

  public:
    Volume(const Volume &) = delete;
    Volume &operator=(const Volume &) = delete;

    explicit Volume(Device &containing_device, int idx, const char *label,
                    const char *devname):
        containing_device_(containing_device),
        index_(idx),
        state_(PENDING),
        label_(label),
        devname_(devname)
    {}

    const Device *get_device() const { return &containing_device_; }
    int get_index() const { return index_; }
    const std::string &get_label() const { return label_; }
    const std::string &get_device_name() const { return devname_; }
};

}

#endif /* !DEVICES_HH */
