shared_module(
  'clicksmart310',
  'library.c',
  'clicksmart.c',
  'clicksmart.h',
  'spca50x-jpeg-header.h',
  dependencies: [
    libgphoto2_dep,
    config_dep,
  ],
  name_prefix: '',
  install: true,
  install_dir: camlibs_dir,
)

install_data(
  'README.clicksmart310',
  install_dir: camlibdoc_dir,
  install_tag: 'doc',
)