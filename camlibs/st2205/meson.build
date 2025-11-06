shared_module(
  'st2205',
  'library.c',
  'st2205.c',
  'st2205.h',
  'st2205_decode.c',
  'st2205_tables.c',
  dependencies: [
    libgphoto2_dep,
    libgd_dep,
  ],
  name_prefix: '',
  install: true,
  install_dir: camlibs_dir,
)

install_data(
  'README.st2205',
  'README.st2205-compression',
  install_dir: camlibdoc_dir,
  install_tag: 'doc',
)