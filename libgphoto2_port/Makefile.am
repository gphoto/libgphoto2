## Process this file with automake to produce Makefile.in

## Specify some includes and versioning
INCLUDES = -I@srcdir@ $(VERSION_FLAGS)
VERSION_FLAGS = -DLIBGPPORT_VERSION=\"@LIBGPPORT_VERSION@\" -DHAVE_TERMIOS_H

## Executable files
bin_SCRIPTS = gphoto2-port-config

SUBDIRS = po intl libgphoto2_port include test @IOLIB_SUBDIRS@ doc

## GNOME configuration script
confexecdir=$(libdir)
confexec_DATA = gphoto2portConf.sh

## Extra files 
CLEANFILES=gphoto2portConf.sh
EXTRA_DIST = 		\
	AUTHORS \
	ChangeLog \
	COPYING.LIB \
	NEWS \
	README \
	libgphoto2_port.spec \
	gphoto2-port-config.in \
	gphoto2portConf.sh.in 

gphoto2portConf.sh: gphoto2portConf.sh.in Makefile
## Use sed and then mv to avoid problems if the user interrupts.
	sed -e 's?\@GPPORT_LIBDIR\@?$(GPPORT_LIBDIR)?g' \
	    -e 's?\@GPPORT_INCLUDEDIR\@?$(GPPORT_INCLUDEDIR)?g' \
	    -e 's?\@GPPORT_LIBS\@?$(GPPORT_LIBS)?g' \
	    -e 's?\@LIBGPPORT_VERSION\@?$(LIBGPPORT_VERSION)?g' \
	      < $(srcdir)/gphoto2portConf.sh.in > gphoto2portConf.tmp \
	  && mv gphoto2portConf.tmp gphoto2portConf.sh
