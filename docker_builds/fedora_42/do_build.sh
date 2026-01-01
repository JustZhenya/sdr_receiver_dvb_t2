#!/bin/bash
set -e
cd /root

# Install dependencies and tools
dnf install -y cmake gcc g++ wget p7zip p7zip-plugins rpmdevtools \
    qt6-qtbase-devel qcustomplot-qt6-devel fftw-devel airspyone_host-devel hackrf-devel uhd-devel

# Install SDRPlay libraries
wget https://www.sdrplay.com/software/SDRplay_RSP_API-Linux-3.15.2.run
7z x ./SDRplay_RSP_API-Linux-3.15.2.run
7z x ./SDRplay_RSP_API-Linux-3.15.2
cp amd64/libsdrplay_api.so.3.15 /usr/lib/libsdrplay_api.so
cp inc/* /usr/include/

cd git_repo
mkdir build
cd build
cmake ..
make VERBOSE=1 -j`nproc`

sh ../make_rpm_package.sh