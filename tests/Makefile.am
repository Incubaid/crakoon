TESTS = check_memory check_arakoon

check_PROGRAMS = check_memory check_arakoon
noinst_PROGRAMS = test_arakoon_client

check_arakoon_SOURCES = check_arakoon.c memory.c $(top_srcdir)/src/arakoon.h
check_arakoon_CFLAGS = @CHECK_CFLAGS@ -I$(top_srcdir)/src
check_arakoon_LDADD = $(top_builddir)/src/.libs/libarakoon-1.0.la @CHECK_LIBS@

check_memory_SOURCES = check_memory.c memory.c $(top_srcdir)/src/arakoon.h
check_memory_CFLAGS = @CHECK_CFLAGS@ -I$(top_srcdir)/src
check_memory_LDADD = @CHECK_LIBS@

test_arakoon_client_SOURCES = test_arakoon_client.c memory.c $(top_srcdir)/src/arakoon.h
test_arakoon_client_CFLAGS = -I$(top_srcdir)/src
test_arakoon_client_LDADD = $(top_builddir)/src/.libs/libarakoon-1.0.la
