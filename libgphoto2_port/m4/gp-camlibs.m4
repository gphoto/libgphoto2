dnl GP_CAMLIB & Co.
dnl
dnl Redundancy free definition of libgphoto2 camlibs.
dnl Removes the redundany from the camlib lists, and executes
dnl additional consistency checks, e.g. to ensure that subdirectories
dnl actually exist.
dnl
dnl You can mark camlibs as obsolete, i.e. they won't be listed
dnl explicitly but still be recognized.
dnl
dnl Example usage:
dnl
dnl   GP_CAMLIB([canon])
dnl   GP_CAMLIB([casio])
dnl   [...]
dnl   GP_CAMLIB([ptp],[obsolete])
dnl   GP_CAMLIB([ptp2])
dnl   [...]
dnl   GP_CAMLIB([toshiba])
dnl   GP_CAMLIBS_DEFINE([camlibs])
dnl
dnl The camlibs basedir parameter of GP_CAMLIBS_DEFINE is optional.
dnl
dnl ####################################################################
dnl
AC_DEFUN([GP_CAMLIBS_INIT],[dnl
AC_BEFORE([$0],[GP_CAMLIB])dnl
m4_define_default([gp_camlib_srcdir], [camlibs])dnl
m4_define_default([gp_camlibs], [])dnl
m4_define_default([gp_camlibs_obsolete], [])dnl
])dnl
dnl
dnl ####################################################################
dnl
AC_DEFUN([GP_CAMLIB],[dnl
AC_REQUIRE([GP_CAMLIBS_INIT])dnl
AC_BEFORE([$0],[GP_CAMLIBS_DEFINE])dnl
m4_if([$2],[obsolete],[dnl
# $0($1,$2)
m4_append([gp_camlibs_obsolete], [$1], [ ])dnl
],[$#],[1],[dnl
# $0($1)
m4_append([gp_camlibs], [$1], [ ])dnl
],[dnl
m4_errprint(__file__:__line__:[ Error:
*** Illegal parameter 2 to $0: `$2'
*** Valid values are: undefined or [obsolete]
])dnl
m4_exit(1)dnl
])dnl
])dnl
dnl
dnl ####################################################################
dnl
AC_DEFUN([GP_CAMLIBS_DEFINE],[dnl
AC_REQUIRE([GP_CAMLIBS_INIT])dnl
AC_BEFORE([$0], [GP_CAMLIBS_CHECK_SUBDIRS])dnl
m4_pattern_allow([m4_strip])dnl
m4_ifval([$1],[m4_define([gp_camlib_srcdir],[$1])])dnl
for camlib in m4_strip(gp_camlibs) m4_strip(gp_camlibs_obsolete)
do
	if test -d "$srcdir/m4_strip(gp_camlib_srcdir)/$camlib"; then :; else
		AC_MSG_ERROR([
* Fatal:
* Source subdirectory for camlib \`$camlib' not found in
* directory \`$srcdir/m4_strip(gp_camlib_srcdir)/'
])
	fi
done
AC_MSG_CHECKING([which drivers to compile])
dnl Yes, that help output won't be all that pretty, but we at least
dnl do not have to edit it by hand.
AC_ARG_WITH([drivers],[AS_HELP_STRING(
	[--with-drivers=<list>],
	[compile drivers in <list>.]
		[Drivers may be separated with commas,]
		['all' is the default and compiles all drivers.]
		[Possible drivers are: ]
		m4_strip(gp_camlibs)dnl
		[.]
		m4_ifval([gp_camlibs_obsolete],
			 [ Obsolete drivers not included in default: ]
			 m4_strip(gp_camlibs_obsolete))
	)],
	[drivers="$withval"],
	[drivers="all"])dnl
dnl
if test "$drivers" = "all"; then
	SUBDIRS_CAMLIBS="m4_strip(gp_camlibs)"
	AC_MSG_RESULT([all])
else
	# drivers=$(echo $drivers | sed 's/,/ /g')
	IFS_save="$IFS"
	IFS=",$IFS"
	for driver in $drivers; do
		IFS="$IFS_save"
		found=false
		for camlib in m4_strip(gp_camlibs) m4_strip(gp_camlibs_obsolete); do
			if test "$driver" = "$camlib"; then
				SUBDIRS_CAMLIBS="$SUBDIRS_CAMLIBS $driver"
				found=:
				break
			fi
		done
		if $found; then :; else
			AC_MSG_ERROR([Unknown driver $driver!])		
		fi
	done
	IFS="$IFS_save"
	AC_MSG_RESULT([$drivers])
fi
AC_SUBST([SUBDIRS_CAMLIBS])
])dnl
dnl
dnl ####################################################################
dnl
AC_DEFUN([GP_CAMLIBS_CHECK_SUBDIRS],[
AC_REQUIRE([GP_CAMLIBS_DEFINE])dnl
# check that for each camlib a Makefile is generated
# m4_if([X],[X],AC_LIST_FILES)
for camlib in m4_strip(gp_camlibs) m4_strip(gp_camlibs_obsolete)
do
	camake="m4_strip(gp_camlib_srcdir)/$camlib/Makefile"
	found=false
	for confile in $ac_config_files
	do
		if test "$camake" = "$confile"; then
			found=:
			break
		fi
	done
	if "$found"; then :; else
		AC_MSG_ERROR([
* Fatal: Makefile for camlib $camlib not generated
*        $camake
])
	fi
done
])dnl
dnl
dnl
dnl Local Variables:
dnl mode: autoconf
dnl End:
