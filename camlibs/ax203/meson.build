shared_module(
  'ax203',
  'library.c',
  'ax203.c',
  'ax203.h',
  'ax203_decode_yuv.c',
  'ax203_decode_yuv_delta.c',
  'ax203_compress_jpeg.c',
  'jpeg_memsrcdest.h',
  'jpeg_memsrcdest.c',
  'tinyjpeg.c',
  'tinyjpeg.h',
  'tinyjpeg-internal.h',
  'jidctflt.c',
  dependencies: [
    libgphoto2_dep,
    libjpeg_dep,
    libgd_dep,
  ],
  name_prefix: '',
  install: true,
  install_dir: camlibs_dir,
)

install_data(
  'README.ax203',
  'README.ax203-compression',
  install_dir: camlibdoc_dir,
  install_tag: 'doc',
)