shared_module(
  'sony_dscf1',
  'command.c',
  'command.h',
  'sonydscf1.c',
  'pmp.h',
  dependencies: [
    libgphoto2_dep,
  ],
  name_prefix: '',
  install: true,
  install_dir: camlibs_dir,
)

install_data(
  'README.sonydscf1',
  install_dir: camlibdoc_dir,
  install_tag: 'doc',
)