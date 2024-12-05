if target_machine.system() != 'linux'
  error('usbscsi is only available on linux.')
endif

shared_module('usbdiskdirect',
  'linux.c',
  name_prefix: '',
  install_dir: iolibs_dir,
  install: true,
  dependencies: [
    libgphoto2_port_dep,
    lockdev_dep,
  ],
)