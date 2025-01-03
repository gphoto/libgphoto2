camlib_paths = []
foreach camlib : get_option('camlibs')
  subdir(camlib)
  camlib_paths += meson.current_build_dir() / camlib
endforeach