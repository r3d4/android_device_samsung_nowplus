#!/bin/bash

echo "formating sd card"
mkfs.ext2 /dev/sde2

echo "mounting"
mount ~/mnt/sdcard2

echo "copy android to sdcard"
cp -Rdpf $ANDROID/device/samsung/nowplus/out/rootfs/* ~/mnt/sdcard2

echo "unmounting - please wait"
umount ~/mnt/sdcard2

echo "done - remove sdcard"
