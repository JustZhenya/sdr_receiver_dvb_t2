cmake_minimum_required(VERSION 3.9)

# set the project name
project(sdr_receiver_dvb_t2 C CXX)

# require c++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# select the release build type by default to get optimization flags
if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE "Release")
endif()
message(STATUS "Build type: " ${CMAKE_BUILD_TYPE})

# add the executable
set(SRC
	DVB_T2/LDPC/tables_handler.cc
	DVB_T2/address_freq_deinterleaver.cpp
	DVB_T2/bb_de_header.cpp
	DVB_T2/bch_decoder.cpp
	DVB_T2/data_symbol.cpp
	DVB_T2/dvbt2_definition.cpp
	DVB_T2/dvbt2_frame.cpp
	DVB_T2/fc_symbol.cpp
	DVB_T2/ldpc_decoder.cpp
	DVB_T2/llr_demapper.cpp
	DVB_T2/p1_symbol.cpp
	DVB_T2/p2_symbol.cpp
	DVB_T2/pilot_generator.cpp
	DVB_T2/time_deinterleaver.cpp
	libairspy/src/airspy.c
	libairspy/src/iqconverter_float.c
	libairspy/src/iqconverter_int16.c
	libairspy/vc/getopt/getopt.c
	libairspy/vc/getopt/getopt1.c
	libplutosdr/plutosdr_hi_speed_rx.c
	main.cpp
	main_window.cpp
	qcustomplot.cpp
	plot.cpp
	rx_airspy.cpp
	rx_plutosdr.cpp
	rx_sdrplay.cpp
)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

set(UIS
	main_window.ui
)

set(RCS
	libplutosdr/pluto_kernel_module.qrc
)

find_package(Qt5 COMPONENTS Core Gui Network PrintSupport REQUIRED)

add_executable(sdr_receiver_dvb_t2 ${SRC} ${UIS} ${RCS})

# add the includes
target_include_directories(sdr_receiver_dvb_t2 PRIVATE .)

# Strip binary for release builds
if(CMAKE_BUILD_TYPE STREQUAL Release)
	add_custom_command(TARGET sdr_receiver_dvb_t2 POST_BUILD COMMAND ${CMAKE_STRIP} sdr_receiver_dvb_t2)
	message(STATUS "Will strip the binary")
endif()

# threads
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
target_link_libraries(sdr_receiver_dvb_t2 Threads::Threads)

# find libraries
#find_package(libssh REQUIRED)

#find_package(PkgConfig REQUIRED)
#pkg_check_modules(pkgconfig REQUIRED fftw3 libusb-1.0)

target_include_directories(sdr_receiver_dvb_t2 PRIVATE Qt5::Core Qt5::Gui Qt5::Network Qt5::PrintSupport)
target_link_libraries(sdr_receiver_dvb_t2 -lsdrplay_api Qt5::Core Qt5::Gui Qt5::Network Qt5::PrintSupport -lfftw3f -lusb-1.0 -lssh)

# flags
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mavx2")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mavx2")
