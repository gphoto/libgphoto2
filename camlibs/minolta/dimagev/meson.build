shared_module(
  'dimagev',
  'capture.c',
  'data.c',
  'delete.c',
  'dimagev.c',
  'dimagev.h',
  'download.c',
  'info.c',
  'packet.c',
  'status.c',
  'upload.c',
  'util.c',
  dependencies: [
    libgphoto2_dep,
  ],
  name_prefix: '',
  install: true,
  install_dir: camlibs_dir,
)

install_data(
  'README.minolta-dimagev',
  install_dir: camlibdoc_dir,
  install_tag: 'doc',
)