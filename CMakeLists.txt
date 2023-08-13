cmake_minimum_required(VERSION 3.13)
project(libgphoto)

option(FLAT_LAYOUT "Put camlibs/iolibs to same location " OFF)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/libgphoto2_port)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/libgphoto2)
include_directories(${CMAKE_CURRENT_BINARY_DIR})

#set(CMAKE_C_VISIBILITY_PRESET hidden)
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--no-undefined")

find_path(LIBUSB_HDR libusb-1.0/libusb.h)
find_library(LIBUSB_LIB usb-1.0)

find_path(LIBLTDL_HDR ltdl.h)
find_library(LIBLTDL_LIB ltdl)

include_directories(${LIBLTDL_HDR})

find_path(LIBJPEG_HDR jpeglib.h)
find_library(LIBJPEG_LIB jpeg)

if(LIBJPEG_HDR AND LIBJPEG_LIB)
    set(HAVE_LIBJPEG 1)
    include_directories(${LIBJPEG_HDR})
    set(EXTRA_LIB ${LIBJPEG_LIB})
else()
    set(EXTRA_LIB )
endif()

include_directories(${LIBUSB_HDR}/libusb-1.0)

if(FLAT_LAYOUT)
    set(CAMLIBS lib)
    set(IOLIBS lib)
    set(GPHOTO2_FLAT_LAYOUT 1)
    set(GPHOTO2_FLAT_LAYOUT_IO_PREFIX "libgphoto2_port_iolibs_")
    set(GPHOTO2_FLAT_LAYOUT_CAM_PREFIX "libgphoto2_camlibs_")
else()
    set(CAMLIBS lib/gphoto_cam)
    set(IOLIBS lib/gphoto_io)
    set(GPHOTO2_FLAT_LAYOUT)
endif()


set(LIBGPHOTO2_SRC 
        libgphoto2/gphoto2-abilities-list.c
        libgphoto2/ahd_bayer.c
        libgphoto2/bayer.c
        libgphoto2/gphoto2-camera.c
        libgphoto2/gphoto2-context.c
        libgphoto2/exif.c
        libgphoto2/gphoto2-file.c
        libgphoto2/gphoto2-filesys.c
        libgphoto2/gamma.c
        libgphoto2/jpeg.c
        libgphoto2/gphoto2-list.c
        libgphoto2/gphoto2-result.c
        libgphoto2/gphoto2-version.c
        libgphoto2/gphoto2-setting.c
        libgphoto2/gphoto2-widget.c)

set(LIBGPHOTO2_PORT_SRC
        libgphoto2_port/libgphoto2_port/gphoto2-port-info-list.c
        libgphoto2_port/libgphoto2_port/gphoto2-port-info.h
        libgphoto2_port/libgphoto2_port/gphoto2-port-locking.c
        libgphoto2_port/libgphoto2_port/gphoto2-port-log.c
        libgphoto2_port/libgphoto2_port/gphoto2-port-version.c
        libgphoto2_port/libgphoto2_port/gphoto2-port.c
        libgphoto2_port/libgphoto2_port/gphoto2-port-portability.c
        libgphoto2_port/libgphoto2_port/gphoto2-port-result.c)
add_definitions(-D_GPHOTO2_INTERNAL_CODE)
add_definitions(-DHAVE_CONFIG_H)
add_definitions("-DLOCALEDIR=\"${CMAKE_INSTALL_PREFIX}/locale\"")
add_definitions("-DCAMLIBS=\"${CMAKE_INSTALL_PREFIX}/${CAMLIBS}\"")
add_definitions("-DIOLIBS=\"${CMAKE_INSTALL_PREFIX}/${IOLIBS}\"")

configure_file(config.cmake.h config.h)
configure_file(gphoto2-endian.cmake.h libgphoto2/gphoto2-endian.h)



add_library(gphoto2_port SHARED ${LIBGPHOTO2_PORT_SRC})
add_library(gphoto2 SHARED ${LIBGPHOTO2_SRC})

target_link_libraries(gphoto2 gphoto2_port ${LIBLTDL_LIB} m)
target_link_libraries(gphoto2_port ${LIBLTDL_LIB})

add_library(gphoto2_port_iolibs_usb1 SHARED libgphoto2_port/libusb1/libusb1.c)
target_link_libraries(gphoto2_port_iolibs_usb1 gphoto2_port ${LIBUSB_LIB})

add_library(gphoto2_camlibs_canon SHARED 
    camlibs/canon/canon.c
    camlibs/canon/crc.c
    camlibs/canon/library.c
    camlibs/canon/serial.c
    camlibs/canon/usb.c
    camlibs/canon/util.c)

target_link_libraries(gphoto2_camlibs_canon gphoto2 gphoto2_port)

add_library(gphoto2_camlibs_ptp2 SHARED
    camlibs/ptp2/ptp.c
    camlibs/ptp2/library.c
    camlibs/ptp2/usb.c
    camlibs/ptp2/ptpip.c
    camlibs/ptp2/config.c
    camlibs/ptp2/olympus-wrap.c
    camlibs/ptp2/chdk.c
    camlibs/ptp2/fujiptpip.c
)

target_link_libraries(gphoto2_camlibs_ptp2 gphoto2 gphoto2_port ${EXTRA_LIB})

add_library(gphoto2_camlibs_ax203 SHARED 
    camlibs/ax203/library.c
    camlibs/ax203/ax203.c
    camlibs/ax203/ax203_decode_yuv.c
    camlibs/ax203/ax203_decode_yuv_delta.c
    camlibs/ax203/ax203_compress_jpeg.c
    camlibs/ax203/jpeg_memsrcdest.c
    camlibs/ax203/tinyjpeg.c
    camlibs/ax203/jidctflt.c)
target_link_libraries(gphoto2_camlibs_ax203 gphoto2 gphoto2_port ${EXTRA_LIB})



install(TARGETS gphoto2_port gphoto2
        LIBRARY DESTINATION lib)

install(TARGETS gphoto2_port_iolibs_usb1 
        LIBRARY DESTINATION ${IOLIBS})

install(TARGETS gphoto2_camlibs_canon gphoto2_camlibs_ptp2
        LIBRARY DESTINATION ${CAMLIBS})

install(DIRECTORY gphoto2 DESTINATION include)
install(DIRECTORY libgphoto2_port/gphoto2 DESTINATION include)

