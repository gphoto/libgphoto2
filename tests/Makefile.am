
INCLUDES =						\
	-I$(top_srcdir)/libgphoto2			\
	-I$(top_srcdir)/libgphoto2_port

noinst_PROGRAMS =	\
	test-gphoto2	\
	test-filesys

test_gphoto2_SOURCE = test-gphoto2.c
test_gphoto2_LDADD = ../libgphoto2/libgphoto2.la

test_filesys_SOURCE = test-filesys.c
test_filesys_LDADD = ../libgphoto2/libgphoto2.la
