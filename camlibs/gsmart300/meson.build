shared_module(
  'gsmart300',
  'library.c',
  'gsmart300.c',
  'gsmart300.h',
  'gsmart300-jpeg-header.h',
  dependencies: [
    libgphoto2_dep,
    config_dep,
  ],
  name_prefix: '',
  install: true,
  install_dir: camlibs_dir,
)

install_data(
  'README.gsmart300',
  install_dir: camlibdoc_dir,
  install_tag: 'doc',
)