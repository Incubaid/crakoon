TESTS = check-memory check-arakoon

check_PROGRAMS = check-memory check-arakoon
noinst_PROGRAMS = test-arakoon-client

check_arakoon_SOURCES = check-arakoon.c memory.c memory.h $(top_srcdir)/src/arakoon.h
check_arakoon_CFLAGS = @CHECK_CFLAGS@ -I$(top_srcdir)/src
check_arakoon_LDADD = $(top_builddir)/src/.libs/libarakoon-1.0.la @CHECK_LIBS@

check_memory_SOURCES = check-memory.c memory.c memory.h $(top_srcdir)/src/arakoon.h
check_memory_CFLAGS = @CHECK_CFLAGS@ -I$(top_srcdir)/src
check_memory_LDADD = @CHECK_LIBS@

test_arakoon_client_SOURCES = test-arakoon-client.c memory.c memory.h $(top_srcdir)/src/arakoon.h
test_arakoon_client_CFLAGS = -I$(top_srcdir)/src
test_arakoon_client_LDADD = $(top_builddir)/src/.libs/libarakoon-1.0.la
