## Process this file with automake to generate a GNU Makefile

if CONFIG_DRIVER_BARBIE
  SUBDIRS_DRIVERS=barbie
else
  SUBDIRS_DRIVERS=
endif

if CONFIG_DRIVER_DIRECTORY
  SUBDIRS_DRIVERS+=directory
endif

if CONFIG_DRIVER_FUJITSU
  SUBDIRS_DRIVERS+=fujitsu
endif

if CONFIG_DRIVER_KONICA
  SUBDIRS_DRIVERS+=konica
endif

if CONFIG_DRIVER_PANASONIC
  SUBDIRS_DRIVERS+=panasonic
endif

SUBDIRS = $(SUBDIRS_DRIVERS)
