# gp-packaging.m4 - help with packaging libgphoto2 for distros -*- Autoconf -*-
# serial 13
dnl | Increment the above serial number every time you edit this file.
dnl | When it finds multiple m4 files with the same name,
dnl | aclocal will use the one with the highest serial.
dnl
AC_DEFUN([GPKG_CHECK_LINUX],
[
	# effective_target has to be determined in advance
	AC_REQUIRE([AC_NEED_BYTEORDER_H])

	is_linux=false
	case "$effective_target" in 
		*linux*)
			is_linux=true
			;;
	esac
	AM_CONDITIONAL([HAVE_LINUX], ["$is_linux"])

	AC_ARG_WITH([hotplug-doc-dir],
	[AS_HELP_STRING([--with-hotplug-doc-dir=PATH],
	[Where to install hotplug scripts as docs [default=autodetect]])])

	if "$is_linux"; then
		AC_MSG_CHECKING([for hotplug doc dir])
		if test "x${with_hotplug_doc_dir}" != "x"
		then # given as parameter
		    hotplugdocdir="${with_hotplug_doc_dir}"
		    AC_MSG_RESULT([${hotplugdocdir} (from parameter)])
		else # start at docdir
		    hotplugdocdir="${docdir}/linux-hotplug"
		    AC_MSG_RESULT([${hotplugdocdir} (default)])
		fi
	else
		hotplugdocdir=""
	fi

	AC_ARG_WITH([hotplug-usermap-dir],
	[AS_HELP_STRING([--with-hotplug-usermap-dir=PATH],
	[Where to install hotplug scripts as docs [default=autodetect]])])

	if "$is_linux"; then
		AC_MSG_CHECKING([for hotplug usermap dir])
		if test "x${with_hotplug_usermap_dir}" != "x"
		then # given as parameter
		    hotplugusermapdir="${with_hotplug_usermap_dir}"
		    AC_MSG_RESULT([${hotplugusermapdir} (from parameter)])
		else # start at docdir
		    hotplugusermapdir="${docdir}/linux-hotplug"
		    AC_MSG_RESULT([${hotplugusermapdir} (default)])
		fi
	else
		hotplugusermapdir=""
	fi

	# Let us hope that automake does not create "" directories
	# on non-Linux systems now.
	AC_SUBST([hotplugdocdir])
	AC_SUBST([hotplugusermapdir])
])
dnl
dnl
dnl ####################################################################
dnl
dnl Local Variables:
dnl mode: autoconf
dnl End:
