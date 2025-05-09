libgphoto2_port_sources = files(
  'gphoto2-port-info-list.c',
  'gphoto2-port-locking.c',
  'gphoto2-port-log.c',
  'gphoto2-port-portability.c',
  'gphoto2-port-result.c',
  'gphoto2-port-version.c',
  'gphoto2-port.c',
)

libgphoto2_port_headers = files(
  '../gphoto2/gphoto2-port-info-list.h',
  '../gphoto2/gphoto2-port-locking.h',
  '../gphoto2/gphoto2-port-log.h',
  '../gphoto2/gphoto2-port-portability.h',
  '../gphoto2/gphoto2-port-result.h',
  '../gphoto2/gphoto2-port-version.h',
  '../gphoto2/gphoto2-port.h',
)

libgphoto2_port_private_headers = files(
  'gphoto2-port-info.h',
  'i18n.h',
  'compiletime-assert.h',
)

libgphoto2_port_lib = library(
  'gphoto2_port',
  libgphoto2_port_sources,
  libgphoto2_port_headers,
  libgphoto2_port_private_headers,
  include_directories: [libgphoto2_root_inc, libgphoto2_port_root_inc],
  version: '@0@.@1@.@2@'.format(libgphoto2_port_current_min, libgphoto2_port_age, libgphoto2_port_revision),
  soversion: libgphoto2_port_current_min,
  install: true,
  dependencies: [
    ltdl_dep,
    lockdev_dep,
    m_dep,
    intl_dep,
    config_dep,
  ],
)

install_headers(
  libgphoto2_port_headers,
  subdir: 'gphoto2',
)

libgphoto2_port_dep = declare_dependency(
  link_with: libgphoto2_port_lib,
  include_directories: libgphoto2_port_root_inc,
  dependencies: config_dep,
)

pkg.generate(
  libgphoto2_port_lib,
  filebase: 'libgphoto2_port',
  description: 'Device-independent access to serial, USB, and other ports',
  url: 'http://gphoto.org/proj/libgphoto2/',
  subdirs: ['.', 'gphoto2'],
  version: libgphoto2_port_version,
  variables: [
    'VERSION=@0@'.format(libgphoto2_port_version),
    'driverdir=${libdir}/@0@'.format(iolibs_dir_suffix),
  ],
)