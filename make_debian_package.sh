#!/bin/sh

# Create directory structure
echo Create directory structure
mkdir package
mkdir package/DEBIAN

# Create package info
echo Create package info
echo Package: sdr_receiver_dvb_t2 >> package/DEBIAN/control
echo Version: 0.1-$BUILD_NO >> package/DEBIAN/control
echo Maintainer: Oleg Malyutin, vladisslav2011, just_zhenya >> package/DEBIAN/control
echo Architecture: $2 >> package/DEBIAN/control
echo Description: Software DVB-T2 receiver >> package/DEBIAN/control
echo Depends: $3 >> package/DEBIAN/control

if [ ! -z "$4" ]
then
    echo Recommends: $4 >> package/DEBIAN/control
fi

# Copying files
echo Copy files
ORIG_DIR="$PWD"
cd $1
make install DESTDIR=$ORIG_DIR/package
cd $ORIG_DIR

# Create package
echo Create package
dpkg-deb --build package
