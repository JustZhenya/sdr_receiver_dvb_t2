#!/bin/bash
set -e
cd /root

# Install dependencies and tools
# TODO missing: libairspyhf-dev libairspy-dev libad9361-dev libbladerf-dev liblimesuite-dev
dnf install -y cmake gcc g++ git p7zip p7zip-plugins wget xxd libtool autoconf rpmdevtools \
    fftw-devel glfw-devel volk-devel libzstd-devel libiio-devel libcorrect-devel \
    rtaudio-devel hackrf-devel rtl-sdr-devel portaudio-devel codec2-devel spdlog-devel

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