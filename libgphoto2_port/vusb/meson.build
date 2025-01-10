vcamera_dir = get_option('prefix') / get_option('datadir') / meson.project_name() / meson.project_version() / 'vcamera'
shared_module('vusb',
  'vusb.c',
  'vcamera.c',
  'vcamera.h',
  name_prefix: '',
  install_dir: iolibs_dir,
  install: true,
  dependencies: [
    libgphoto2_port_dep,
    libexif_dep,
  ],
  c_args: [
      '-DVCAMERADIR="@0@"'.format(vcamera_dir),
  ],
)
