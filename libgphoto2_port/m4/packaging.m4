AC_DEFUN(GPKG_CHECK_RPM,
[
AC_ARG_WITH(rpmbuild, [  --with-rpmbuild=PATH      Program to use for building RPMs])

AC_MSG_CHECKING([for rpmbuild or rpm])
if test -x "${with_rpmbuild}"
then
    RPMBUILD="${with_rpmbuild}"
    AC_MSG_RESULT([${RPMBUILD} (from parameter)])
    AC_SUBST(RPMBUILD)
else
    AC_MSG_RESULT([using autodetection])
    AC_CHECK_PROGS(RPMBUILD, [rpmbuild rpm], /bin/false)
    AC_MSG_RESULT([${RPMBUILD} (autodetect)])
fi
AM_CONDITIONAL(ENABLE_RPM, test "$RPMBUILD" != "/bin/false")

# whether libusb-devel is installed or not defines whether the RPM
# packages we're going to build will depend on libusb and libusb-devel
# RPM packages or not.
AM_CONDITIONAL(RPM_LIBUSB_DEVEL, rpm -q libusb-devel > /dev/null 2>&1)
])
