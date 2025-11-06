shared_module(
  'jl2005c',
  'library.c',
  'jl2005c.c',
  'jpeg_memsrcdest.c',
  'jpeg_memsrcdest.h',
  'jl2005bcd_decompress.c',
  'jl2005bcd_decompress.h',
  'jl2005c.h',
  'img_enhance.c',
  'img_enhance.h',
  dependencies: [
    libgphoto2_dep,
    libjpeg_dep,
  ],
  name_prefix: '',
  install: true,
  install_dir: camlibs_dir,
)

install_data(
  'README.jl2005c',
  'README.jl2005bcd-compression',
  install_dir: camlibdoc_dir,
  install_tag: 'doc',
)