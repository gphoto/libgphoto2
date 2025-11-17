shared_module(
  'largan',
  'largan.c',
  'lmini.c',
  'lmini.h',
  'lmini_ccd.c',
  'lmini_ccd.h',
  dependencies: [
    libgphoto2_dep,
  ],
  name_prefix: '',
  install: true,
  install_dir: camlibs_dir,
)

install_data(
  'README.largan-lmini',
  install_dir: camlibdoc_dir,
  install_tag: 'doc',
)