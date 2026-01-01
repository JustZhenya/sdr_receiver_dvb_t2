cmake_minimum_required(VERSION 3.10.0)
project(sdr_receiver_dvb_t2 C CXX)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mavx")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mavx")
endif()

find_package(Qt5 REQUIRED Core Widgets Gui Network)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

option(USE_HACKRF "Build with HackRF support (requires libhackrf)" ON)
option(USE_USRP "Build with USRP/UHD support (requires libuhd)" ON)
option(USE_MIRISDR "Build with SDRPlay support (requires sdrplay v2.x)" ON)
option(USE_PLUTOSDR "Build with PlutoSDR support (requires libusb, libssh)" ON)
option(USE_AIRSPY "Build with AirSpy support (requires libairspy)" ON)

set(SRCFILES
    src/DVB_T2/LDPC/tables_handler.cc
    src/DVB_T2/address_freq_deinterleaver.cpp
    src/DVB_T2/bb_de_header.cpp
    src/DVB_T2/bch_decoder.cpp
    src/DVB_T2/data_symbol.cpp
    src/DVB_T2/dvbt2_definition.cpp
    src/DVB_T2/dvbt2_demodulator.cpp
    src/DVB_T2/fc_symbol.cpp
    src/DVB_T2/ldpc_decoder.cpp
    src/DVB_T2/llr_demapper.cpp
    src/DVB_T2/p1_symbol.cpp
    src/DVB_T2/p2_symbol.cpp
    src/DVB_T2/pilot_generator.cpp
    src/DVB_T2/time_deinterleaver.cpp
    src/main.cpp
    src/main_window.cpp
    src/plot.cpp
    src/rx_raw.cpp
    #src/rx_sdrplay.cpp
    #src/rx_miri.cpp
    #src/rx_plutosdr.cpp

    src/rx_interface.h

    src/main_window.ui
)

add_executable(sdr_receiver_dvb_t2 ${SRCFILES})

target_include_directories(sdr_receiver_dvb_t2 PRIVATE src/ Qt5::Core Qt5::Widgets Qt5::Gui Qt5::Network)
target_link_libraries(sdr_receiver_dvb_t2 PRIVATE Qt5::Core Qt5::Widgets Qt5::Gui Qt5::Network)
target_compile_features(sdr_receiver_dvb_t2 PRIVATE cxx_std_17)

find_package(PkgConfig QUIET REQUIRED)

pkg_check_modules(FFTW3F REQUIRED IMPORTED_TARGET fftw3f)
target_include_directories(sdr_receiver_dvb_t2 PRIVATE PkgConfig::FFTW3F)
target_link_libraries(sdr_receiver_dvb_t2 PRIVATE PkgConfig::FFTW3F)

pkg_check_modules(QCUSTOMPLOT-QT5 REQUIRED IMPORTED_TARGET qcustomplot-qt5)
target_include_directories(sdr_receiver_dvb_t2 PRIVATE PkgConfig::QCUSTOMPLOT-QT5)
target_link_libraries(sdr_receiver_dvb_t2 PRIVATE PkgConfig::QCUSTOMPLOT-QT5)

if(USE_HACKRF)
    pkg_check_modules(LIBHACKRF REQUIRED IMPORTED_TARGET libhackrf)
    target_include_directories(sdr_receiver_dvb_t2 PRIVATE PkgConfig::LIBHACKRF)
    target_link_libraries(sdr_receiver_dvb_t2 PRIVATE PkgConfig::LIBHACKRF)
    target_compile_definitions(sdr_receiver_dvb_t2 PRIVATE USE_HACKRF=1)
    target_sources(sdr_receiver_dvb_t2 PRIVATE src/rx_hackrf.cpp)
endif()

if(USE_USRP)
    pkg_check_modules(UHD REQUIRED IMPORTED_TARGET uhd)
    target_include_directories(sdr_receiver_dvb_t2 PRIVATE PkgConfig::UHD)
    target_link_libraries(sdr_receiver_dvb_t2 PRIVATE PkgConfig::UHD)
    target_compile_definitions(sdr_receiver_dvb_t2 PRIVATE USE_USRP=1)
    target_sources(sdr_receiver_dvb_t2 PRIVATE src/rx_usrp.cpp)
endif()

# TODO: USE_MIRISDR (I don't have Fedora packages)
# TODO: USE_PLUTO (I don't have Fedora packages)

if(USE_AIRSPY)
    pkg_check_modules(LIBAIRSPY REQUIRED IMPORTED_TARGET libairspy)
    target_include_directories(sdr_receiver_dvb_t2 PRIVATE PkgConfig::LIBAIRSPY)
    target_link_libraries(sdr_receiver_dvb_t2 PRIVATE PkgConfig::LIBAIRSPY)
    target_compile_definitions(sdr_receiver_dvb_t2 PRIVATE USE_AIRSPY=1)
    target_sources(sdr_receiver_dvb_t2 PRIVATE src/rx_airspy.cpp)
endif()