dnl
dnl Define external references
dnl
dnl Define once, use many times. 
dnl No more URLs and Mail addresses in translated strings and stuff.
dnl

AC_DEFUN(GP_REFERENCES,
[
AC_DEFINE(URL_GPHOTO_HOME,[http://www.gphoto.org/],[gphoto project home page])
AC_DEFINE(URL_GPHOTO_PROJECT,[http://sourceforge.net/projects/gphoto],
	[gphoto sourceforge project page])
AC_DEFINE(URL_DIGICAM_LIST,
	[http://www.teaser.fr/~hfiguiere/linux/digicam.html],
	[camera list with support status])
AC_DEFINE(URL_JPHOTO_HOME,[http://jphoto.sourceforge.net/],[jphoto])
AC_DEFINE(URL_USB_MASSSTORAGE,[http://www.linux-usb.org/USB-guide/x498.html],
	[information about using USB mass storage])
])
