EXTRA_DIST = usbcam.console usbcam.group usbcam.user usbcam.x11-app

# no need to build or install the following on non-linux systems
if HAVE_LINUX

hotplugdoc_DATA = usbcam.console usbcam.group usbcam.user usbcam.x11-app

INCLUDES =	\
	-I$(top_srcdir)/libgphoto2	\
	-I$(top_srcdir)/libgphoto2_port/libgphoto2_port

noinst_PROGRAMS = print-usb-usermap

# FIXME: This should only be built if target system is linux
print_usb_usermap_SOURCE = print-usb-usermap.c
print_usb_usermap_LDADD = ../../libgphoto2/libgphoto2.la

USERMAP = gphoto2-usb.usermap

install-data-local: print-usb-usermap
	./print-usb-usermap usbcam > $(USERMAP)
	mkdir -p $(DESTDIR)$(hotplugusermapdir)/
	$(INSTALL_DATA) $(USERMAP) $(DESTDIR)$(hotplugusermapdir)/

uninstall-local:
	rm -f $(DESTDIR)$(docdir)/$(USERMAP)

else

install-data-local:
	@echo "Dummy $@ target"

uninstall-local:
	@echo "Dummy $@ target"

endif
