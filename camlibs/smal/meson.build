shared_module(
  'smal',
  'smal.c',
  'smal.h',
  'ultrapocket.c',
  'ultrapocket.h',
  dependencies: [
    libgphoto2_dep,
    config_dep,
  ],
  name_prefix: '',
  install: true,
  install_dir: camlibs_dir,
)

install_data(
  'README.smal',
  install_dir: camlibdoc_dir,
  install_tag: 'doc',
)