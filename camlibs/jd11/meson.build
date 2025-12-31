jd11_sources = files(
  'jd11.c',
  'decomp.c',
  'decomp.h',
  'serial.c',
  'serial.h',
)

if libtiff_dep.found()
  jd11_sources += files('dng.c', 'dng.h')
endif

shared_module(
  'jd11',
  jd11_sources,
  dependencies: [
    libgphoto2_dep,
    libtiff_dep,
  ],
  name_prefix: '',
  install: true,
  install_dir: camlibs_dir,
)