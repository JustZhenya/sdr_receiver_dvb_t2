#!/bin/bash
set -e
cd /root

# Install dependencies and tools
apt-get update
apt-get install -y build-essential cmake wget p7zip-full qt6-base-dev libqcustomplot-dev libfftw3-dev libairspy-dev libuhd-dev libhackrf-dev libusb-1.0-0-dev

# Install SDRPlay libraries
BUILD_ARCH=$(dpkg --print-architecture)
wget https://www.sdrplay.com/software/SDRplay_RSP_API-Linux-3.15.2.run
7z x ./SDRplay_RSP_API-Linux-3.15.2.run
7z x ./SDRplay_RSP_API-Linux-3.15.2
cp $BUILD_ARCH/libsdrplay_api.so.3.15 /usr/lib/libsdrplay_api.so
cp inc/* /usr/include/

cd git_repo
mkdir build
cd build
cmake .. -DCMAKE_INSTALL_PREFIX:PATH=/usr
make VERBOSE=1 -j`nproc`

cd ..
sh make_debian_package.sh ./build $BUILD_ARCH 'libc6, libgcc-s1, libstdc++6, libqt6core6t64, libqt6widgets6t64, libqt6gui6t64, libqt6network6t64, libqcustomplot2.1-qt6, libfftw3-single3, libairspy0, libuhd4.6.0t64, libhackrf0' ''