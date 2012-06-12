lib_LTLIBRARIES = libarakoon-1.0.la
libarakoon_1_0_la_SOURCES = arakoon.c arakoon.h \
			    arakoon-networking.c arakoon-networking.h \
			    arakoon-nursery.c arakoon-nursery.h \
			    arakoon-utils.c arakoon-utils.h \
			    arakoon-cluster-node.c arakoon-cluster-node.h \
			    arakoon-cluster.c arakoon-cluster.h \
			    arakoon-client-call-options.c arakoon-client-call-options.h \
			    arakoon-value-list.c arakoon-value-list.h \
			    arakoon-key-value-list.c arakoon-key-value-list.h \
			    arakoon-nursery-routing.c arakoon-nursery-routing.h \
			    arakoon-protocol.h \
			    arakoon-assert.c arakoon-assert.h \
			    arakoon-library-version.c
libarakoon_1_0_la_LDFLAGS = \
	-version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE) \
	-export-dynamic $(no_undefined) $(export_symbols) \
	$(HARDEN_LDFLAGS)

libarakoon_1_0_includedir=$(includedir)/arakoon-1.0/arakoon
libarakoon_1_0_include_HEADERS = arakoon.h arakoon-nursery.h
libarakoon_1_0_la_LIBADD = -lrt

AM_CPPFLAGS = $(CRAKOON_DEBUG_FLAGS) $(CRAKOON_ASSERT_FLAGS) \
	      -DCRAKOON_MAJOR_VERSION=$(CRAKOON_MAJOR_VERSION) \
	      -DCRAKOON_MINOR_VERSION=$(CRAKOON_MINOR_VERSION) \
	      -DCRAKOON_MICRO_VERSION=$(CRAKOON_MICRO_VERSION) \
	      -DCRAKOON_VERSION=$(CRAKOON_VERSION) \
	      -DCRAKOON_GIT_VERSION=$(CRAKOON_GIT_VERSION)

export_symbols = $(LIBTOOL_EXPORT_OPTIONS)

AM_CFLAGS = $(HARDEN_CFLAGS)
