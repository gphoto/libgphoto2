gp_port_test_env = [
  'IOLIBS=@0@'.format(':'.join(iolib_paths)),
]

test_gp_port_exe = executable(
  'test-gp-port',
  'test-gp-port.c',
  dependencies: libgphoto2_port_dep
)

test(
  'test-gp-port',
  test_gp_port_exe,
  env: gp_port_test_env,
  suite: 'no-ci',
)

test_port_list_exe = executable(
  'test-port-list',
  'test-port-list.c',
  dependencies: libgphoto2_port_dep
)

test(
  'test-port-list',
  test_port_list_exe,
  env: gp_port_test_env,
)