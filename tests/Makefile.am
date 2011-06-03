TESTS = check_arakoon

check_PROGRAMS = check_arakoon
check_arakoon_SOURCES = check_arakoon.c $(top_srcdir)/src/arakoon.h
check_arakoon_CFLAGS = @CHECK_CFLAGS@
check_arakoon_LDADD = $(top_builddir)/src/.libs/libarakoon-1.0.la @CHECK_LIBS@
