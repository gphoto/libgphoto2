gp_test_env = gp_port_test_env + [
  'CAMLIBS=@0@'.format(':'.join(camlib_paths)),
]

test_i18n_exe = executable(
  'test-i18n',
  'test-i18n.c',
  dependencies: [ libgphoto2_dep, intl_dep ],
)

test(
  'test-i18n',
  test_i18n_exe,
  env: gp_test_env,
)

test_endian_exe = executable(
  'test-endian',
  'test-endian.c',
  dependencies: libgphoto2_dep,
)

test(
  'test-endian',
  test_endian_exe,
  env: gp_test_env,
)

print_libgphoto2_version_exe = executable(
  'print-libgphoto2-version',
  'print-libgphoto2-version.c',
  dependencies: libgphoto2_dep,
)

test(
  'print-libgphoto2-version',
  print_libgphoto2_version_exe,
  env: gp_test_env,
)

test_gphoto2_exe = executable(
  'test-gphoto2',
  'test-gphoto2.c',
  dependencies: libgphoto2_dep,
)

test(
  'test-gphoto2',
  test_gphoto2_exe,
  env: gp_test_env,
)

test_filesys_exe = executable(
  'test-filesys',
  'test-filesys.c',
  dependencies: libgphoto2_dep,
)

test(
  'test-filesys',
  test_filesys_exe,
  env: gp_test_env,
  suite: 'no-ci',
)

test_camera_list_exe = executable(
  'test-camera-list',
  'test-camera-list.c',
  dependencies: libgphoto2_dep,
)

test(
  'test-camera-list',
  test_camera_list_exe,
  env: gp_test_env,
)

test_init_localedir_exe = executable(
  'test-init-localedir',
  'test-init-localedir.c',
  dependencies: libgphoto2_dep,
)

test(
  'test-init-localedir',
  test_init_localedir_exe,
  env: gp_test_env,
)