# $Id$
#
# create an RPM package out of an automake based source distribution
# © 2001 Hans Ulrich Niedermann <gp@n-dimensional.de>
#

EXTRA_DIST = package.spec.in @PACKAGE@.spec

DISTCLEANFILES = @PACKAGE@.spec

clean-generic:
	rm -rf BUILD RPMS SPECS SOURCES SRPMS

@PACKAGE@.spec: package.spec
	cp $< $@

rpm: Makefile package.spec
if RPM_LIBUSB_DEVEL
	@echo "Building gphoto2 RPM with USB support and dependency on the libusb RPM package."
else
	@echo "#############################################################################"
	@echo "# Building gphoto2 RPM packages without USB support.                        #"
	@echo "# Install both the libusb and the libusb-devel RPM package for USB support. #"
	@echo "#############################################################################"
	@sleep 3
endif
	$(MAKE) clean
	mkdir -p BUILD SPECS SOURCES SRPMS
	for arch in i386 i486 i586 i686 alpha noarch; do \
		mkdir -p "RPMS/$${arch}"; \
	done
	ln -f "$(top_srcdir)/@PACKAGE@-@VERSION@.tar.gz" SOURCES/
	ln -f package.spec SPECS/@PACKAGE@-@VERSION@.spec
	@RPMBUILD@ --define "_topdir $$(pwd)" -ba SPECS/@PACKAGE@-@VERSION@.spec
