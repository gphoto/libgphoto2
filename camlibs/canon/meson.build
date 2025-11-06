shared_module(
  'canon',
  'canon.c',
  'canon.h',
  'library.c',
  'library.h',
  'serial.c',
  'serial.h',
  'usb.c',
  'usb.h',
  'crc.c',
  'crc.h',
  'util.c',
  'util.h',
  dependencies: [
    libgphoto2_dep,
  ],
  name_prefix: '',
  install: true,
  install_dir: camlibs_dir,
)

install_data(
  'README.canon',
  install_dir: camlibdoc_dir,
  install_tag: 'doc',
)