shared_module(
  'jamcam',
  'library.c',
  'library.h',
  'jamcam.c',
  dependencies: [
    libgphoto2_dep,
  ],
  name_prefix: '',
  install: true,
  install_dir: camlibs_dir,
)

install_data(
  'README.jamcam',
  install_dir: camlibdoc_dir,
  install_tag: 'doc',
)