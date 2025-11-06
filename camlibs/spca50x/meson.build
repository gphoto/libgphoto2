shared_module(
  'spca50x',
  'spca50x.c',
  'spca50x.h',
  'spca50x-flash.c',
  'spca50x-flash.h',
  'spca50x-sdram.c',
  'spca50x-sdram.h',
  'library.c',
  'spca50x-jpeg-header.h',
  'spca50x-registers.h',
  'spca50x-avi-header.h',
  dependencies: [
    libgphoto2_dep,
  ],
  name_prefix: '',
  install: true,
  install_dir: camlibs_dir,
)

install_data(
  'README.spca50x',
  install_dir: camlibdoc_dir,
  install_tag: 'doc',
)