#!/bin/bash
set -e
cd /root

# Install dependencies and tools
apt-get update
apt-get install -y build-essential cmake wget p7zip-full qt6-base-dev libqcustomplot-dev libfftw3-dev libairspy-dev libuhd-dev libhackrf-dev

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
cmake ..
make VERBOSE=1 -j`nproc`

cd ..
sh make_debian_package.sh ./build $BUILD_ARCH 'libc6, libgcc-s1, libstdc++6, libqt6core6, libqt6widgets6, libqt6gui6, libqt6network6, libqcustomplot2.1-qt6, libfftw3-single3, libairspy0, libuhd4.8.0, libhackrf0' ''