CAMLIBS=$(BASEPATH)\CAMLIBS
IOLIBS=$(BASEPATH)\LIBGPHOTO2_PORT
OBJPATH=$(BASEPATH)\OBJ
CCFLAGS=-O2 -c -I$(BASEPATH)\include -IE:\gs\gs7.00\zlib -I$(BASEPATH)\libgphoto2_port\include -Zmtd -Zcrtdll -Zbin-files -D__ST_MT_ERRNO_ -I. -DOS2 -DHAVE_TERMIOS_H
CC=gcc
DLLLINKFLAGS=-Zdll -Zmtd -Zcrtdll
EXELINKFLAGS=-Zmtd -Zcrtdll
LINK=gcc
#LIBS=-L$(BASEPATH)\lib -Zbin-files -llibgphoto2 -llibgphoto2_port -ldllib -lst\check -lgcc
LIBS=-L$(BASEPATH)\lib -Zbin-files -llibgphoto2 -llibgphoto2_port -ldllib
MAKEDEF=copy $*.df $@ >NUL & emxexp $** >> $@
MKLIB=ar r

.SUFFIXES: .o

start:
  @echo ****
  @echo   Entering: $(MAKEDIR)
  @echo ****

end:
  @echo ****
  @echo   Done:     $(MAKEDIR)
  @echo ****

