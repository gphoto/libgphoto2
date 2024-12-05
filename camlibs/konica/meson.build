
konica_localization_dir = camlibs_dir_suffix / 'konica'
install_data(
  files(
    'localization/english',
    'localization/french',
    'localization/german',
    'localization/japanese',
    'localization/korean',
    'localization/spanish',
  ),
  install_dir: konica_localization_dir,
)

shared_module(
  'konica',
  'lowlevel.c',
  'lowlevel.h',
  'konica.c',
  'konica.h',
  'library.c',
  dependencies: [
    libgphoto2_dep,
  ],
  c_args: [
    '-DLOCALIZATION="@0@"'.format(konica_localization_dir),
  ],
  name_prefix: '',
  install: true,
  install_dir: camlibs_dir,
)

shared_module(
  'konica_qm150',
  'qm150.c',
  dependencies: [
    libgphoto2_dep,
  ],
  c_args: [
    '-DLOCALIZATION="@0@"'.format(konica_localization_dir),
  ],
  name_prefix: '',
  install: true,
  install_dir: camlibs_dir,
)