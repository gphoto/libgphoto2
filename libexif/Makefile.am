INCLUDES =              \
        -I$(top_srcdir)

noinst_LTLIBRARIES = libexif.la

libexif_la_LDFLAGS = -version-info @LIBEXIF_VERSION_INFO@
libexif_la_SOURCES =			\
	exif-content.c exif-content.h	\
	exif-data.c    exif-data.h	\
	exif-entry.c   exif-entry.h	\
	exif-format.c  exif-format.h	\
	exif-i18n.h    exif-i18n.h	\
	exif-tag.c     exif-tag.h	\
	exif-utils.c   exif-utils.h
libexif_la_LIBADD = -lm
