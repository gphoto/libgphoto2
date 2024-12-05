test_i18n_exe = executable(
  'test-i18n',
  'test-i18n.c',
  dependencies: libgphoto2_dep,
)

test(
  'test-i18n',
  test_i18n_exe,
)

test_endian_exe = executable(
  'test-endian',
  'test-endian.c',
  dependencies: libgphoto2_dep,
)

test(
  'test-endian',
  test_endian_exe,
)

print_libgphoto2_version_exe = executable(
  'print-libgphoto2-version',
  'print-libgphoto2-version.c',
  dependencies: libgphoto2_dep,
)

test(
  'print-libgphoto2-version',
  print_libgphoto2_version_exe,
)

test_gphoto2_exe = executable(
  'test-gphoto2',
  'test-gphoto2.c',
  dependencies: libgphoto2_dep,
)

test(
  'test-gphoto2',
  test_gphoto2_exe,
)

test_filesys_exe = executable(
  'test-filesys',
  'test-filesys.c',
  dependencies: libgphoto2_dep,
)

test(
  'test-filesys',
  test_filesys_exe,
  env: [
    'LTDL_LIBRARY_PATH=@0@'.format(':'.join(iolib_paths + camlib_paths)),
  ]
)

test_camera_list_exe = executable(
  'test-camera-list',
  'test-camera-list.c',
  dependencies: libgphoto2_dep,
)

test(
  'test-camera-list',
  test_camera_list_exe,
  env: [
    'LTDL_LIBRARY_PATH=@0@'.format(':'.join(iolib_paths + camlib_paths)),
  ]
)

test_init_localedir_exe = executable(
  'test-init-localedir',
  'test-init-localedir.c',
  dependencies: libgphoto2_dep,
)

test(
  'test-init-localedir',
  test_init_localedir_exe,
)