print_camera_list_exe = executable(
  'print-camera-list',
  'print-camera-list.c',
  dependencies: libgphoto2_dep,
)

if target_machine.system() in ['linux']
  udev_script_dir = get_option('udevscriptdir')
  if udev_script_dir == ''
    udev_script_dir = get_option('prefix') / get_option('libdir') / 'udev'
  endif

  install_data(
    'check-ptp-camera',
    install_dir: udev_script_dir
  )
endif