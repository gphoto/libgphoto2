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
# On systems such as mingw/Windows, mmap(2) is not part of the
# standard library and needs an explicit library to link against.
    mman_dep,
  ],
  name_prefix: '',
  install: true,
  install_dir: camlibs_dir,
)