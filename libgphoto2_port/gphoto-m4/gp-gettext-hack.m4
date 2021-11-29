dnl
dnl GP_GETTEXT_HACK
dnl
dnl gettext hack, originally designed for libexif, libgphoto2, and Co.
dnl This creates a po/Makevars file with adequate values if the
dnl po/Makevars.template is present.
dnl
dnl Example usage:
dnl    GP_GETTEXT_HACK([${PACKAGE_TARNAME}-${LIBFOO_CURRENT}],
dnl                    [Copyright Holder],
dnl                    [foo-translation@example.org])
dnl    ALL_LINGUAS="de es fr"
dnl    AM_GNU_GETTEXT_VERSION([0.14.1])
dnl    AM_GNU_GETTEXT([external])
dnl    AM_PO_SUBDIRS()
dnl    AM_ICONV()
dnl    GP_GETTEXT_FLAGS
dnl
dnl You can leave out the GP_GETTEXT_HACK parameters if you want to,
dnl GP_GETTEXT_HACK will try fall back to sensible values in that case:
dnl
dnl
AC_DEFUN([GP_GETTEXT_HACK], [dnl
AC_BEFORE([$0], [AM_GNU_GETTEXT])dnl
AC_BEFORE([$0], [AM_GNU_GETTEXT_VERSION])dnl
m4_if([$2],[],[$1="${PACKAGE_TARNAME}"],[$1="$2"])
AC_DEFINE_UNQUOTED([$1], ["${$1}"],
                   [The gettext domain we're using])
AC_SUBST([$1])
sed_cmds="s|^DOMAIN.*|DOMAIN = ${$1}|"
m4_if([$3],[],[],[sed_cmds="${sed_cmds};s|^COPYRIGHT_HOLDER.*|COPYRIGHT_HOLDER = $3|"])
m4_ifval([$4],[
if test -n "$PACKAGE_BUGREPORT"; then
   sed_mb="${PACKAGE_BUGREPORT}"
else
   m4_pattern_allow([GP_GETTEXT_HACK])dnl
   AC_MSG_ERROR([
*** Your configure.{ac,in} is wrong.
*** Either define PACKAGE_BUGREPORT (by using the 4-parameter AC INIT syntax)
*** or give [GP_GETTEXT_HACK] the third parameter.
***
])
fi
],[
sed_mb="$4"
])
sed_cmds="${sed_cmds};s|^MSGID_BUGS_ADDRESS.*|MSGID_BUGS_ADDRESS = ${sed_mb}|"
# Not so sure whether this hack is all *that* evil...
AC_MSG_CHECKING([for po/Makevars requiring hack])
if test -f "${srcdir}/po/Makevars.template"; then
   sed "$sed_cmds" < "${srcdir}/po/Makevars.template" > "${srcdir}/po/Makevars"
   AC_MSG_RESULT([yes, done.])
else
   AC_MSG_RESULT([no])
fi
])dnl
dnl
dnl
dnl
AC_DEFUN([GP_GETTEXT_FLAGS],
[
AC_REQUIRE([AM_GNU_GETTEXT])
AC_REQUIRE([GP_CONFIG_INIT])
if test "x${BUILD_INCLUDED_LIBINTL}" = "xyes"; then
   AM_CFLAGS="${AM_CFLAGS} -I\$(top_srcdir)/intl"
fi
GP_CONFIG_MSG([Use translations],[${USE_NLS}])
if test "x$USE_NLS" = "xyes" && test "${BUILD_INCLUDED_LIBINTL}"; then
   GP_CONFIG_MSG([Use included libintl],[${BUILD_INCLUDED_LIBINTL}])
fi
])dnl
dnl
dnl

dnl Please do not remove this:
dnl filetype: 71ff3941-a5ae-4677-a369-d7cb01f92c81
dnl I use this to find all the different instances of this file which 
dnl are supposed to be synchronized.

dnl Local Variables:
dnl mode: autoconf
dnl End:
