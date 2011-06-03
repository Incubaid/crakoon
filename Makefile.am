SUBDIRS = src . tests
EXTRA_DIST =

AM_CPPFLAGS = @CRAKOON_DEBUG_FLAGS@

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = arakoon-1.0.pc

EXTRA_DIST +=
	arakoon-1.0.pc.in

DISTCHECK_CONFIGURE_FLAGS = --enable-debug
