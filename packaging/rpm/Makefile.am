# $Id$
#
# create an RPM package out of an automake based source distribution
# (C) 2001 Hans Ulrich Niedermann <gp@n-dimensional.de>
#

EXTRA_DIST = package.spec.in

clean-generic:
	rm -rf BUILD RPMS SPECS SOURCES SRPMS

rpm: Makefile package.spec
	$(MAKE) clean
	mkdir -p BUILD SPECS SOURCES SRPMS
	for arch in i386 i486 i586 i686 alpha noarch; do \
		mkdir -p "RPMS/$${arch}"; \
	done
	ln -f "$(top_srcdir)/@PACKAGE@-@VERSION@.tar.gz" SOURCES/
	ln -f package.spec SPECS/@PACKAGE@-@VERSION@.spec
	rpm --define "_topdir $$(pwd)" -ba SPECS/@PACKAGE@-@VERSION@.spec
