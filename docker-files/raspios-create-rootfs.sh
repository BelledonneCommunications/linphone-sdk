#!/bin/sh

if [ ! -f "/home/bc/rpi_rootfs/build_rootfs.sh" ]; then
  rm -rf /home/bc/rpi_rootfs && \
  cd /home/bc && \
  git clone https://github.com/ghismary/rpi_rootfs.git
fi

if [ ! -f "/home/bc/rpi_rootfs/2023-02-21-raspios-bullseye-armhf-lite.img" ]; then
  cd /home/bc/rpi_rootfs && \
	wget https://downloads.raspberrypi.org/raspios_lite_armhf/images/raspios_lite_armhf-2023-02-22/2023-02-21-raspios-bullseye-armhf-lite.img.xz && \
	unxz 2023-02-21-raspios-bullseye-armhf-lite.img.xz
fi

if [ ! -d "/home/bc/rpi_rootfs/rootfs" ]; then
  cd /home/bc/rpi_rootfs && \
	sudo chmod +x ./build_rootfs.sh && \
  ./build_rootfs.sh create ./2023-02-21-raspios-bullseye-armhf-lite.img && \
  ./build_rootfs.sh run "/usr/bin/apt -y install libasound2-dev libbsd-dev libegl1-mesa-dev libglew-dev libpulse-dev libssl-dev libv4l-dev libvpx-dev libxv-dev xsdcxx" && \
  ./scripts/sysroot-relativelinks.py rootfs
fi
