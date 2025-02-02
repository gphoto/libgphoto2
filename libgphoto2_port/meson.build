libusb_req         = '>= 0.1.5'
libusb1_req        = '>= 1.0.0'

ltdl_dep = cc.find_library(
  'ltdl',
  has_headers: 'ltdl.h',
  required: false
)
if not ltdl_dep.found()
  ltdl_dep = cc.find_library('libltdl', has_headers: 'ltdl.h', required: true)
endif
add_project_arguments('-DHAVE_LTDL=1', language: 'c')

# This will be libusb-1.0 by default and fallback on the older libusb.
libusb_dep = dependency('libusb-1.0', version: libusb1_req, required: 'libusb1' in get_option('iolibs'))

usb0 = false
usb1 = false

if libusb_dep.found()
  usb1 = true
  add_project_arguments('-DHAVE_LIBUSB1=1', language: 'c')
  if cc.has_function('libusb_strerror', dependencies: libusb_dep, prefix: '#include<libusb.h>')
    add_project_arguments('-DHAVE_LIBUSB_STRERROR=1', language: 'c')
  endif
  if cc.has_function('libusb_wrap_sys_device', dependencies: libusb_dep, prefix: '#include<libusb.h>')
    add_project_arguments('-DHAVE_LIBUSB_WRAP_SYS_DEVICE=1', language: 'c')
  endif
  if cc.compiles('''
#include<libusb.h>
enum libusb_option opt = LIBUSB_OPTION_NO_DEVICE_DISCOVERY;
''', dependencies: libusb_dep, name: 'Has LIBUSB_OPTION_NO_DEVICE_DISCOVERY')
    add_project_arguments('-DHAVE_LIBUSB_OPTION_NO_DEVICE_DISCOVERY1', language: 'c')
  endif

else

  libusb_dep = dependency('libusb', version: libusb_req, required: 'usb' in get_option('iolibs'))
  if libusb_dep.found()
    usb0 = true
    add_project_arguments('-DHAVE_LIBUSB=1', language: 'c')
  endif
endif

lockdev_dep = cc.find_library('lockdev', required : false)

if lockdev_dep.found()
  add_project_arguments('-DHAVE_LOCKDEV=1', language: 'c')
endif

if cc.has_header('ttylock.h')
  add_project_arguments('-DHAVE_TTYLOCK=1', language: 'c')
endif

if cc.has_header('regex.h')
  add_project_arguments('-DHAVE_REGEX=1', language: 'c')
  if cc.has_function('re_compile_pattern', prefix: '#include <regex.h>')
    add_project_arguments('-DHAVE_GNU_REGEX=1', language: 'c')
  endif
endif

if 'serial' in  get_option('iolibs')
  add_project_arguments('-DHAVE_SERIAL=1', language: 'c')
endif

# The . stands for the current dir, i.e. the iolibs to build.
# Make sure the iolibs are built before running tests on them.
#SUBDIRS = po libgphoto2_port . tests doc
libgphoto2_port_root_inc = include_directories('.')

headers = ['stdlib.h', 'unistd.h', 'stdio.h', 'errno.h', 'sys/time.h',
  'sys/param.h', 'sys/select.h', 'termios.h', 'sgetty.h', 'ttold.h', 'ioctl-types.h',
  'fcntl.h', 'sgtty.h', 'sys/ioctl.h', 'sys/time.h', 'termio.h', 'unistd.h',
  'endian.h', 'byteswap.h', 'asm/io.h', 'mntent.h', 'sys/mntent.h', 'sys/mnttab.h',
  'scsi/sg.h', 'limits.h', 'sys/file.h', 'lockdev.h', 'ttylock.h',
]
foreach header : headers
  if cc.has_header(header)
    add_project_arguments('-DHAVE_@0@=1'.format(header.replace('.', '_').replace('/', '_').to_upper()), language: 'c')
  endif
endforeach

add_project_arguments('-DURL_USB_MASSSTORAGE="http://www.linux-usb.org/USB-guide/x498.html"', language: 'c')

subdir('libgphoto2_port')

iolib_paths = []
foreach iolib : get_option('iolibs')
  if (iolib == 'libusb1' and not usb1) or (iolib == 'usb' and not usb0)
     continue
  endif
  subdir(iolib)
  iolib_paths += meson.current_build_dir() / iolib
endforeach

subdir('tests')
subdir('po')
