#!/bin/sh

# Create directory structure
echo Create directory structure
rpmdev-setuptree

# Create package info
cat <<EOF >> ~/rpmbuild/SPECS/sdrpp.spec
Name:       sdr_receiver_dvb_t2
Version:    0.1
Release:    $BUILD_NO
Summary:    sdr_receiver_dvb_t2
License:    GPLv3+

%description
Software DVB-T2 receiver

%install
cd /root/git_repo/build
%make_install

%files
/usr/bin/sdr_receiver_dvb_t2
/usr/share/applications/sdr_receiver_dvb_t2.desktop
EOF

# Create package
echo Create package
rpmbuild -ba ~/rpmbuild/SPECS/sdrpp.spec
