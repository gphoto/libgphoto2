shared_module(
  'iclick',
  'library.c',
  'iclick.c',
  'iclick.h',
  dependencies: [
    libgphoto2_dep,
  ],
  name_prefix: '',
  install: true,
  install_dir: camlibs_dir,
)

install_data(
  'README.iclick',
  install_dir: camlibdoc_dir,
  install_tag: 'doc',
)