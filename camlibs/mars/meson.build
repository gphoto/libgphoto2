shared_module(
  'mars',
  'library.c',
  'mars.c',
  'mars.h',
  dependencies: [
    libgphoto2_dep,
    config_dep,
  ],
  name_prefix: '',
  install: true,
  install_dir: camlibs_dir,
)

install_data(
  'README.mars',
  install_dir: camlibdoc_dir,
  install_tag: 'doc',
)