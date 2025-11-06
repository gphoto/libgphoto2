shared_module(
  'lumix',
  'lumix.c',
  dependencies: [
    libgphoto2_dep,
    libxml_dep,
    libcurl_dep,
  ],
  name_prefix: '',
  install: true,
  install_dir: camlibs_dir,
)

install_data(
  'README.lumix',
  install_dir: camlibdoc_dir,
  install_tag: 'doc',
)