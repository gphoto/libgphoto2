libgphoto2_sources = files(
  'gphoto2-abilities-list.c',
  'ahd_bayer.c',
  'bayer.c',
  'gphoto2-camera.c',
  'gphoto2-context.c',
  'exif.c',
  'gphoto2-file.c',
  'gphoto2-filesys.c',
  'gamma.c',
  'jpeg.c',
  'gphoto2-list.c',
  'gphoto2-result.c',
  'gphoto2-version.c',
  'gphoto2-setting.c',
  'gphoto2-widget.c',
)

libgphoto2_headers = files(
  '../gphoto2/gphoto2-abilities-list.h',
  '../gphoto2/gphoto2-camera.h',
  '../gphoto2/gphoto2-context.h',
  '../gphoto2/gphoto2-file.h',
  '../gphoto2/gphoto2-filesys.h',
  '../gphoto2/gphoto2-library.h',
  '../gphoto2/gphoto2-list.h',
  '../gphoto2/gphoto2-result.h',
  '../gphoto2/gphoto2-setting.h',
  '../gphoto2/gphoto2-version.h',
  '../gphoto2/gphoto2-widget.h',
  '../gphoto2/gphoto2.h',
)

libgphoto2_private_headers = files(
  'bayer.h',
  'bayer-types.h',
  'exif.h',
  'gamma.h',
  'jpeg.h',
)

endian_dic = configuration_data()

# Look for NetBSD-style extended byte swapping macros
if cc.compiles(
  '''
  #include <machine/endian.h>
  int main() { le32toh(0); return 0; }
  '''
)
  endian_dic.set10('HAS_LE32TOH', true)
elif cc.compiles(
  '''
  #include <arpa/inet.h>
  int main() { ntohl(0); return 0; }
  '''
)
  endian_dic.set10('HAS_NTOHL_ARPA_INET', true)
elif cc.compiles(
  '''
  #include <netinet/in.h>
  int main() { ntohl(0); return 0; }
  '''
)
  endian_dic.set10('HAS_NTOHL_NETINET_IN', true)
endif

if cc.compiles(
  '''
  #include <byteswap.h>
  int main() { bswap_32(0); return 0; }
  '''
)
  endian_dic.set10('HAS_BSWAP_32', true)
elif cc.compiles(
  '''
  #include <machine/endian.h>
  int main() { swap32(0); return 0; }
  '''
)
  endian_dic.set10('HAS_SWAP32', true)
elif cc.compiles(
  '''
  #include <machine/endian.h>
  int main() { bswap32(0); return 0; }
  '''
)
  endian_dic.set10('HAS_BSWAP32', true)
elif cc.compiles(
  '''
  #include <sys/types.h>
  int main() { __byte_swap_long(0); return 0; }
  '''
)
  endian_dic.set10('HAS_BYTE_SWAP_LONG', true)
elif cc.compiles(
  '''
  #include <machine/byte_order.h>
  int main() { NXSwapLong(0); return 0; }
  '''
)
  endian_dic.set10('HAS_NXSWAPLONG', true)
endif

# The i386 and compatibles can handle unaligned memory access,
# so use the optimized macros above to do this job
endian_dic.set('CAN_CAST_BYTES_TO_MEM', target_machine.cpu_family() in ['x86', 'x86_64'])


gphoto2_endian = configure_file(
  input: 'gphoto2-endian.h.in',
  output: '@BASENAME@',
  configuration: endian_dic
)

libgphoto2_private_headers += gphoto2_endian

libgphoto2_lib = library(
  'gphoto2',
  libgphoto2_sources,
  libgphoto2_headers,
  libgphoto2_private_headers,
  include_directories: libgphoto2_root_inc,
  version: '@0@.@1@.@2@'.format(libgphoto2_current_min, libgphoto2_age, libgphoto2_revision),
  soversion: libgphoto2_current_min,
  install: true,
  dependencies: [
    ltdl_dep,
    libexif_dep,
    m_dep,
    intl_dep,
    libgphoto2_port_dep,
    config_dep,
  ],
  c_args: [
    '-D_GPHOTO2_INTERNAL_CODE',
  ]
)

install_headers(
  libgphoto2_headers,
  subdir: 'gphoto2',
)

libgphoto2_dep = declare_dependency(
  link_with: libgphoto2_lib,
  include_directories: [libgphoto2_root_inc, include_directories('.'),],
  dependencies: [ libgphoto2_port_dep, config_dep ],
  sources: gphoto2_endian,
)

pkg.generate(
  libgphoto2_lib,
  filebase: 'libgphoto2',
  description: 'Device-independent access to serial, USB, and other ports',
  url: 'http://gphoto.org/proj/libgphoto2/',
  subdirs: ['.', 'gphoto2'],
  version: meson.project_version(),
  requires: [ libgphoto2_port_lib ],
  variables: [
    'VERSION=@0@'.format(meson.project_version()),
    'driverdir=${libdir}/@0@'.format(iolibs_dir_suffix),
  ],
)
