ACLOCAL_AMFLAGS = -I m4
EXTRA_DIST = scripts/wrap-compiler-for-flag-check

SUBDIRS = src . tests

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = arakoon-1.0.pc

EXTRA_DIST +=
	arakoon-1.0.pc.in

DISTCHECK_CONFIGURE_FLAGS = --enable-debug --enable-asserts=abort
