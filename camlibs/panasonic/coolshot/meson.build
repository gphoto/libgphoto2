shared_module(
  'panasonic_coolshot',
  'coolshot.c',
  'library.c',
  'library.h',
  dependencies: [
    libgphoto2_dep,
  ],
  name_prefix: '',
  install: true,
  install_dir: camlibs_dir,
)

install_data(
  'README.panasonic-coolshot',
  install_dir: camlibdoc_dir,
  install_tag: 'doc',
)