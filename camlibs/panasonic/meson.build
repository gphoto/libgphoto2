shared_module(
  'panasonic_dc1000',
  'dc1000.c',
  'dc1000.h',
  'dc.c',
  'dc.h',
  dependencies: [
    libgphoto2_dep,
  ],
  name_prefix: '',
  install: true,
  install_dir: camlibs_dir,
)

shared_module(
  'panasonic_dc1580',
  'dc1580.c',
  'dc1580.h',
  'dc.c',
  'dc.h',
  'dc1000.h',
  dependencies: [
    libgphoto2_dep,
  ],
  name_prefix: '',
  install: true,
  install_dir: camlibs_dir,
)

subdir('coolshot')
subdir('l859')