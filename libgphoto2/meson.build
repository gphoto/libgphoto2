libgphoto2_sources = files(
  'gphoto2-abilities-list.c',
  'ahd_bayer.c',
  'bayer.c',
  'gphoto2-camera.c',
  'gphoto2-context.c',
  'exif.c',
  'gphoto2-file.c',
  'gphoto2-filesys.c',
  'gamma.c',
  'jpeg.c',
  'gphoto2-list.c',
  'gphoto2-result.c',
  'gphoto2-version.c',
  'gphoto2-setting.c',
  'gphoto2-widget.c',
)

libgphoto2_headers = files(
  '../gphoto2/gphoto2-abilities-list.h',
  '../gphoto2/gphoto2-camera.h',
  '../gphoto2/gphoto2-context.h',
  '../gphoto2/gphoto2-file.h',
  '../gphoto2/gphoto2-filesys.h',
  '../gphoto2/gphoto2-library.h',
  '../gphoto2/gphoto2-list.h',
  '../gphoto2/gphoto2-result.h',
  '../gphoto2/gphoto2-setting.h',
  '../gphoto2/gphoto2-version.h',
  '../gphoto2/gphoto2-widget.h',
  '../gphoto2/gphoto2.h',
)

libgphoto2_private_headers = files(
  'bayer.h',
  'bayer-types.h',
  'exif.h',
  'gamma.h',
  'jpeg.h',
)

libgphoto2_lib = library(
  'gphoto2',
  libgphoto2_sources,
  libgphoto2_headers,
  libgphoto2_private_headers,
  include_directories: libgphoto2_root_inc,
  version: '@0@.@1@.@2@'.format(libgphoto2_current_min, libgphoto2_age, libgphoto2_revision),
  soversion: libgphoto2_current_min,
  install: true,
  dependencies: [
    ltdl_dep,
    libexif_dep,
    m_dep,
    libgphoto2_port_dep,
    config_dep,
  ],
  c_args: [
    '-D_GPHOTO2_INTERNAL_CODE',
  ]
)

install_headers(
  libgphoto2_headers,
  subdir: 'gphoto2',
)

libgphoto2_dep = declare_dependency(
  link_with: libgphoto2_lib,
  include_directories: [libgphoto2_root_inc, include_directories('.'),],
  dependencies: [ libgphoto2_port_dep, config_dep ],
)

pkg.generate(
  libgphoto2_lib,
  filebase: 'libgphoto2',
  description: 'Device-independent access to serial, USB, and other ports',
  url: 'http://gphoto.org/proj/libgphoto2/',
  subdirs: 'gphoto2',
  version: meson.project_version(),
  requires: [ libgphoto2_port_lib ],
  variables: [
    'VERSION=@0@'.format(meson.project_version()),
    'driverdir=${libdir}/@0@'.format(iolibs_dir_suffix),
  ],
)