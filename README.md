# MounTA, the T+A Mount Daemon for USB mass storage devices

## Copyright and contact

MounTA is released under the terms of the GNU General Public License version 3
(GPLv3). See file <tt>COPYING</tt> for licensing terms.

Contact:

    T+A elektroakustik GmbH & Co. KG
    Planckstrasse 11
    32052 Herford
    Germany

## Short description

_mounta_ is a daemon that watches changes in <tt>/dev/disk/by-id/</tt>. Any USB
devices showing up in that directory are mounted and made available to a USB
file browser via D-Bus.

## Details on functionality

How it should work:

- Set inotify watch on `/dev/disk/by-id/`.
- USB mass storage devices and their partitions appear as symlinks to devices.
  These symlinks have names starting with `usb-`.
- We should probably use a blacklist instead of a whitelist to filter symlinks,
  and we should read it from a configuration file.
- When a new USB device appears, then
  - If the device is already registered either as root device or partition, and
    is not marked as pending, then ignore it. This case may occur as the result
    of a data race during startup.
  - If the device is a root device, then
    - Find the corresponding symlink in `/sys/block/` (partitions do not show
      up there).
    - Check the symlink to find out the USB port it is connected to. This will
      involve parsing the path the symlink points to.
    - Find out if this device contains a usable volume by calling external
      program `blkid` (in this case there will be no partition table). If it
      contains a volume, then treat it as a virtual partition and continue as
      if a partition had been found.
    - Create a directory for the device. This will be populated with
      subdirectories which serve as mountpoints for the partitions on that
      device. The directory must include a serial number and should be short.
      The list broker is responsible for mapping the path to a properly
      displayed name using the information it gets on partition announcement.
    - Store the path of that directory in the internal root device structure
      (new one or existing pending one).
    - Announce a new USB device on D-Bus, complete with
      - readable device name from `/dev/disk/by-id/` (sans the `usb-` prefix
        and any numerical cruft following the hex ID)
      - path to the mountpoint container
      - USB root hub ID and port number the device is (transitively) connected
        to (such as `1-3` for port 3 on root hub 1).
    - Process any pending partition structures associated with the root
      partition structure (see below).
  - If the device is a partition, then
    - Associate the partition device with the root device. If the root device
      structure does not exist (yet), then create one and mark it pending.
    - Read out the volume label and file system type by calling external
      program `blkid`. If the volume label does not exist, then generate one
      from the file system type.
    - If the root device structure is available, then
      - Create a directory inside the root device's directory. The directory
        should be just the partition number, nothing else.
      - Try to mount the partition to that directory.
      - If mounting fails, remove the directory and mark the partition as known,
        but unusable.
      - If mounting succeeds, announce a new partition on D-Bus, including
        - the partition number (for sorting, displaying, or nothing),
        - the volume label (for displaying purposes), and
        - complete path to the mountpoint (for browsing purposes).
    - If the root device structure is not available, then mark the partition
      structure as pending. Perform missing steps listed above when the root
      device becomes available.
- When a USB partition disappears, then
  - Unmount the device. It will fail, most probably, but we must make sure that
    we don't leak any resources.
  - Remove the mountpoint directory.
  - Mark the partition as unavailable, do not announce removal to avoid useless
    communication (the list broker will see I/O errors that have to be handled
    anyway).
- When a USB root device disappears, then
  - Unmount all partitions still mounted (with inotify watches, there could be
    a data race that causes whole device removals being reported before
    partition removals---that's why). See previous point.
  - Remove device directory.
  - Announce on D-Bus that the device and thus all its partitions are not
    available anymore, complete with path to the device mountpoint container.
  - Forget the device and its partitions.
- In case of any funny things going on, keep the device structure around, but
  mark the device broken. Only remove when the root device itself disappears
  from `/dev/disk/by-id/`.
- At startup, scan the contents of `/dev/disk/by-id/` after setting the inotify
  watch. Enter devices and partitions directly from directory contents. Then
  start listening to events from the inotify watch.

## Permissions

The _mounta_ daemon requires privileges for mounting and unmounting block
devices. It uses the `mount` and `umount` programs for this. The `mount` and
`umount` programs are usually suid binaries, and it should be possible to
configure the system such that _mounta_ is allowed to mount and unmount USB
devices without having them mentioned in `/etc/fstab`.

The daemon also requires permission to read from block devices so that volume
labels (partition names) can be obtained using `blkid`. This can be
accomplished by `udev` rules that grant group read access rights to block
devices stored on USB devices. Unix group `plugdev` or `usb` could be used for
this. The commonly used `disk` group should not be used because this group is
also used for other devices. Often, this group also has write access to its
devices, meaning a bug or exploited security hole in _mounta_ could kill the
root partition.

Altogether, the system can be configured so that _mounta_ does not require to
be executed with superuser privileges.
