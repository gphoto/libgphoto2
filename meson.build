project('libgphoto2', 'c',
  version : '2.5.31.1',
  meson_version : '>= 1.4.0',
  default_options : [
    'buildtype=debugoptimized',
    'warning_level=3',
    'c_std=gnu99'
  ],
  license: 'LGPL-2.1-or-later'
)

i18n = import('i18n')
pkg = import('pkgconfig')

libxml_req         = '>= 2.0'
libcurl_req        = '>= 7.1'
libgd_req          = '>= 2.0'
libexif_req        = '>= 0.6.13'

# Versioning:
#  - AGE (Micro):      Increment if any interfaces have been added; set to 0
#                      if any interfaces have been removed. Removal has
#                      precedence over adding, so set to 0 if both happened.
#                      It denotes upward compatibility.
#  - REVISION (Minor): Increment any time the source changes; set to
#                      0 if you incremented CURRENT.
#  - CURRENT (Major):  Increment if the interface has changes. AGE is always
#                      *changed* at the same time.
#
#  To summarize. Any interface *change* increment CURRENT. If that interface
#  change does not break upward compatibility (ie it is an addition),
#  increment AGE, Otherwise AGE is reset to 0. If CURRENT has changed,
#  REVISION is set to 0, otherwise REVISION is incremented.
libgphoto2_age         = 4
libgphoto2_revision    = 0
libgphoto2_current     = 10
libgphoto2_current_min = libgphoto2_current - libgphoto2_age

libgphoto2_port_name    = 'libgphoto2_port'
libgphoto2_port_version = '0.12.2'

libgphoto2_port_age      = 2
libgphoto2_port_revision = 0
libgphoto2_port_current  = 14
libgphoto2_port_current_min = libgphoto2_port_current - libgphoto2_port_age

cc = meson.get_compiler('c')
add_project_arguments('-DHAVE_CC="@0@"'.format(cc.get_id()), language: 'c')

test_cflags = [
  '-Werror=unknown-warning-option',
  '-Werror=incompatible-pointer-types',
  '-Werror=implicit',
  '-Werror=int-conversion',
  '-Wno-error=reserved-identifier',
  '-Wno-error=documentation-deprecated-sync',
  '-Wno-unused-parameter',
  '-Wno-unused',
]

common_cflags = cc.get_supported_arguments(test_cflags)

m_dep       = cc.find_library('m', required: false)
libxml_dep  = dependency('libxml-2.0', version: libxml_req, required: 'lumix' in get_option('camlibs'))
libcurl_dep = dependency('libcurl', version: libcurl_req, required: ['lumix', 'pentax'] in get_option('camlibs'))
libgd_dep   = dependency('gdlib', version: libgd_req, required: 'docupen' in get_option('camlibs'))
libexif_dep = dependency('libexif', version: libexif_req, required: false)
libjpeg_dep = dependency('libjpeg', required: false)
intl_dep    = dependency('intl', required: false)

add_project_arguments('-DMAIL_GPHOTO_DEVEL="<gphoto-devel@lists.sourceforge.net>"', language: 'c')

if cc.has_function('va_copy', prefix: '#include <stdarg.h>')
  add_project_arguments('-DHAVE_VA_COPY=1', language: 'c')
elif cc.has_function('__va_copy', prefix: '#include <stdarg.h>')
  add_project_arguments('-Dva_copy=__va_copy', language: 'c')
  add_project_arguments('-DHAVE_VA_COPY=1', language: 'c')
endif

if cc.has_function('strncpy', prefix: '#include <string.h>')
  add_project_arguments('-DHAVE_STRNCPY=1', language: 'c')
endif

if (libcurl_dep.found())
  add_project_arguments('-DHAVE_LIBCURL=1', language: 'c')
endif

if (libgd_dep.found())
  add_project_arguments('-DHAVE_LIBGD=1', language: 'c')
endif

if (libexif_dep.found())
  add_project_arguments('-DHAVE_LIBEXIF=1', language: 'c')
  if (cc.has_member('ExifData', 'ifd', dependencies: libexif_dep, prefix: '#include <libexif/exif-data.h>'))
    add_project_arguments('-DHAVE_LIBEXIF_IFD=1', language: 'c')
  endif
endif

if (libxml_dep.found())
  add_project_arguments('-DHAVE_LIBXML2=1', language: 'c')
elif ('ptp2' in get_option('camlibs'))
  message('ptp2 is built without libxml2 support.')
endif

if (libjpeg_dep.found())
  jpeg_test = '''#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
/* jpeglib.h fails to include all required system headers, so jpeglib.h
 * must be included after stddef.h (size_t) and stdio.h (FILE).
 */
#include <jpeglib.h>

int main(int argc, char **argv)
{
  j_decompress_ptr cinfo = NULL;
  (void) argc;
  (void) argv;
  /* Running this will give a segfault */
  if (jpeg_start_decompress(cinfo)) {
    printf("true\n");
  } else {
    printf("false\n");
  }
  return 0;
}
'''
  if not cc.links(jpeg_test, dependencies: libjpeg_dep, name: 'Make sure we can actually compile and link against libjpeg.')
    warning('Unable to use libjpeg')
    libjpeg_dep = disabler()
  else
    add_project_arguments('-DHAVE_LIBJPEG=1', language: 'c')
  endif
endif

headers = [
  'sys/param.h', 'sys/mman.h', 'sys/select.h', 'locale.h', 'memory.h',
  'getopt.h', 'unistd.h', 'mcheck.h', 'limits.h', 'sys/time.h', 'langinfo.h',
  'stdio.h', 'stdlib.h', 'string.h',
]

foreach header : headers
  if cc.has_header(header)
    add_project_arguments('-DHAVE_@0@=1'.format(header.replace('.', '_').replace('/', '_').to_upper()), language: 'c')
  endif
endforeach

add_project_arguments(common_cflags, language: 'c')
add_project_arguments('-D_GPHOTO2_INTERNAL_CODE', language: 'c')
add_project_arguments('-DLOCALEDIR="@0@"'.format (get_option('prefix') / get_option('localedir')), language: 'c')
if intl_dep.found()
  add_project_arguments('-DENABLE_NLS=1', language: 'c')
endif

libgphoto2_root_inc = include_directories('.')

iolibs_dir_suffix = libgphoto2_port_name / libgphoto2_port_version
iolibs_dir = get_option('prefix') / get_option('libdir') / iolibs_dir_suffix

camlibs_dir_suffix = meson.project_name() / meson.project_version()
camlibs_dir = get_option('prefix') / get_option('libdir') / camlibs_dir_suffix

config_data = configuration_data()
config_data.set_quoted('PACKAGE_VERSION', meson.project_version())
config_data.set_quoted('IOLIB_LIST', ' '.join(get_option('iolibs')))
config_data.set_quoted('IOLIBS', iolibs_dir)
config_data.set_quoted('GP_CAMLIB_SET', ' '.join(get_option('camlibs')))
config_data.set_quoted('CAMLIBS', camlibs_dir)
config_data.set_quoted('GETTEXT_PACKAGE_LIBGPHOTO2_PORT', 'libgphoto2_port-@0@'.format(libgphoto2_port_current_min))
config_data.set_quoted('GETTEXT_PACKAGE_LIBGPHOTO2', 'libgphoto2-@0@'.format(libgphoto2_current_min))
config_data.set10('WORDS_BIGENDIAN', target_machine.endian() == 'big')

config_h = configure_file(
  output: 'config.h',
  configuration: config_data,
)

config_dep = declare_dependency(
  include_directories: include_directories('.'),
  sources: config_h
)

configure_file(
  input: 'gphoto2-config.in',
  output: '@BASENAME@',
  install: true,
  install_dir: get_option('prefix') / get_option('bindir'),
  configuration: {
    'PACKAGE': meson.project_name(),
    'VERSION': meson.project_version(),
    'prefix': get_option('prefix'),
    'exec_prefix': '${prefix}',
    'includedir': '${prefix}/@0@'.format(get_option('includedir')),
    'libdir': '${prefix}/@0@'.format(get_option('libdir')),
  }
)

subdir('libgphoto2_port')
subdir('libgphoto2')
subdir('camlibs')
subdir('doc')
subdir('po')
subdir('tests')
subdir('examples')
subdir('packaging/generic')