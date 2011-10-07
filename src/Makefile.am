lib_LTLIBRARIES = libarakoon-1.0.la
libarakoon_1_0_la_SOURCES = arakoon.c arakoon.h \
			    arakoon-networking.c arakoon-networking.h \
			    arakoon-nursery.c arakoon-nursery.h \
			    arakoon-utils.h arakoon-protocol.h
libarakoon_1_0_la_LDFLAGS = \
	-version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE) \
	-export-dynamic $(no_undefined) $(export_symbols)

libarakoon_1_0_includedir=$(includedir)/arakoon-1.0/arakoon
libarakoon_1_0_include_HEADERS = arakoon.h arakoon-nursery.h
libarakoon_1_0_la_LIBADD = -lrt

AM_CPPFLAGS = $(GLIB_DEBUG_FLAGS)

export_symbols = $(LIBTOOL_EXPORT_OPTIONS)
