AC_DEFUN(GPKG_CHECK_RPM,
[
AC_ARG_WITH(rpmbuild, [  --with-rpmbuild=PATH    program to use for building RPMs])

AC_MSG_CHECKING([for rpmbuild or rpm])
if test -x "${with_doc_dir}"
then
    RPMBUILD="${with_doc_dir}"
    AC_MSG_RESULT([${RPMBUILD} (from parameter)])
    AC_SUBST(RPMBUILD)
else
    AC_MSG_RESULT([using autodectection])
    AC_CHECK_PROGS(RPMBUILD, [rpmbuild rpm], /bin/false)
    AC_MSG_RESULT([${RPMBUILD} (autodetect)])
fi
AM_CONDITIONAL(ENABLE_RPM, test "$RPMBUILD" != "/bin/false")
])
