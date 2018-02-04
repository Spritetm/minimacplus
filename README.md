# Tiny Macintosh Plus stuff

This repository contains the firmware, PCB artwork and case designs for an 1/6th scale Macintosh Plus
model. The entire project, including the schematics (well, for what they're worth) is documented on
http://spritesmods.com/?art=minimacplus .

## Building the firmware

The firmware for this build runs on an ESP32-Wrover module. To compile this, you need an Xtensa-toolchain
and ESP-IDF, the SDK for the ESP32 in the ESP32-Wrover module. You can find instructions on how to install
these on the Espressif ESP-IDF github page: https://github.com/espressif/esp-idf

This firmware can also be compiled for an ESP32-Wrover-Kit development board, provided it contains the 
Wrover module with the 4MiB additional PSRAM. To do this, run `make menuconfig`
and under `component config`, `Tiny Mac Emulator options`, select the Wrover-Kit display. Depending on your
devboard, you may also need to lower the SPI flash/psram clock speed: set 
`make menuconfig` -> `serial flasher config` -> `Flash SPI speed` to 40MHz if you get crashes or if 
the memory test fails.

## Obtaining a ROM file

The emulator needs a Macintosh Plus ROM file in order to function. This ROM file is still copyrighted
by Apple, and as such is not distributed with the source code. If you obtain it, you can use the
firmware/flashrom.sh file to flash it, or manually flash it to address 0x100000.

## Obtaining a hard disk image

The emulator needs a hard disk image to boot from. For an un-modified partitions.csv on a 4MiB flash device,
this image needs to be 1433600 bytes, or 2800 512-byte-sized blocks. The hard disk image needs a version
of the Macintosh operating system installed. The easiest way to do this, in my experience, is using the Mac
Plus emulator in MESS:

* Create a HD image of the correct size: ``chdman createhd -o my_hd.chd -s 1433600``
* Install stuff on this HD image, Refer to e.g. https://mamedev.emulab.it/etabeta/2010/04/10/mess-how-to-episode-ii-apple-macintosh-plus/ on how to do this.
* Extract the raw HD image: ``chdman createraw -i my_hd.chd -o my_hd.img``
* Flash ``my_hd.img`` to the ESP32 using ``flashhd.sh`` or by manually flashing it to address 0x120000.

A good trick on Linux to move files from your host OS to these images is mounting them. As root:
* Enable partition support for loop-mounted block devices: ``rmmod loop; modprobe loop max_part=8``
* Setup the image as a blockdevice: ``losetup /dev/loop0 my_hd.img``
* Mount the partition: ``mount /dev/loop0p1 /mnt``
* Move stuff from/to /mnt
* Umount the partition: ``umount /mnt``
* Remove the loop: ``losetup -d /dev/loop0``
* Flash the image

Note that 'hard disks' created in e.g. MiniVMac or BasiliskII will not work, as they actually are
hacked floppy images and as such are missing the partition table and hard disk driver.
