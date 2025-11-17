install_man(
  'libgphoto2.3',
  'libgphoto2_port.3',
)

if get_option('docs')
  doxygen = find_program('doxygen', required : false)
  if doxygen.found()
    cdata = configuration_data()
    cdata.set('top_srcdir', meson.project_source_root())
    cdata.set('PACKAGE_NAME', 'libgphoto2 photo camera library')
    cdata.set('PACKAGE_TARNAME', meson.project_name())
    cdata.set('PACKAGE_VERSION', meson.project_version())
    cdata.set('DOXYGEN_OUTPUT_DIR', meson.current_build_dir())
    cdata.set('HTML_APIDOC_DIR', 'html')
    if find_program('dot', required : false).found()
      cdata.set('HAVE_DOT', 'YES')
    else
      cdata.set('HAVE_DOT', 'NO')
    endif
    cdata_internals = cdata
    cdata_internals.set('HTML_APIDOC_INTERNALS_DIR', 'html-internals')

    doxyfile = configure_file(
      input: 'Doxyfile.in',
      output: 'Doxyfile',
      configuration: cdata,
      install: false
    )

    html_target = custom_target(
      'docs',
      input: doxyfile,
      output: 'html',
      command: [doxygen, '@INPUT@'],
      install: true,
      install_dir: doc_dir
    )

    if get_option('docs-internal')
      doxyfile_internals = configure_file(
        input: 'Doxyfile-internals.in',
        output: 'Doxyfile-internals',
        configuration: cdata_internals,
        install: false
      )

      html_internals_target = custom_target(
        'docs-internals',
        input: doxyfile_internals,
        output: 'html-internals',
        command: [doxygen, '@INPUT@'],
        install: true,
        install_dir: doc_dir
      )
    endif
  endif
endif