# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.62])

AC_CONFIG_MACRO_DIR([m4])

m4_define(crakoon_configure_ac)

# Making releases:
#   crakoon_micro_version += 1;
#   crakoon_interface_age += 1;
#   crakoon_binary_interface += 1;
# If any functions have been added, set crakoon_interface_age to 0.
# If backwards compatibility has been broken, set
# crakoon_binary_interface *and* crakoon_interface_age to 0.
#
# In other words:
#
# On the stable branches, crakoon_interface_age == crakoon_micro_version
# On the unstable branches, crakoon_interface_age = 0

m4_define([crakoon_major_version], [0])
m4_define([crakoon_minor_version], [1])
m4_define([crakoon_micro_version], [0])
m4_define([crakoon_interface_age], [0])
m4_define([crakoon_binary_age],
          [m4_eval(100 * crakoon_minor_version + crakoon_micro_version)])
m4_define([crakoon_version],
          [crakoon_major_version.crakoon_minor_version.crakoon_micro_version])

# Libtool stuff
m4_define([crakoon_lt_release], [crakoon_major_version.crakoon_minor_version])
m4_define([crakoon_lt_current],
          [m4_eval(100 * crakoon_minor_version + crakoon_micro_version - crakoon_interface_age)])
m4_define([crakoon_lt_revision], [crakoon_interface_age])
m4_define([crakoon_lt_age],
          [m4_eval(crakoon_binary_age - crakoon_interface_age)])
m4_define([crakoon_lt_current_minus_age],
          [m4_eval(crakoon_lt_current - crakoon_lt_age)])

# It the minor version number is odd, we want debugging
m4_define([crakoon_debug_default],
          [m4_if(m4_eval(crakoon_minor_version % 2), [1], [yes], [no])])dnl

AC_INIT([crakoon], [crakoon_version])

AC_CONFIG_HEADER([config.h])
AC_CONFIG_SRCDIR([src/arakoon.h])

cflags_set=${CFLAGS+set}

AM_INIT_AUTOMAKE([-Wall -Werror foreign 1.10])

m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

CRAKOON_MAJOR_VERSION=crakoon_major_version
CRAKOON_MINOR_VERSION=crakoon_minor_version
CRAKOON_MICRO_VERSION=crakoon_micro_version
CRAKOON_INTERFACE_AGE=crakoon_interface_age
CRAKOON_BINARY_AGE=crakoon_binary_age
CRAKOON_VERSION=crakoon_version

AC_SUBST(CRAKOON_MAJOR_VERSION)
AC_SUBST(CRAKOON_MINOR_VERSION)
AC_SUBST(CRAKOON_MICRO_VERSION)
AC_SUBST(CRAKOON_VERSION)
AC_SUBST(CRAKOON_INTERFACE_AGE)
AC_SUBST(CRAKOON_BINARY_AGE)

AC_DEFINE(CRAKOON_MAJOR_VERSION, [crakoon_major_version],
	  [Define to the CRAKOON major version])
AC_DEFINE(CRAKOON_MINOR_VERSION, [crakoon_minor_version],
	  [Define to the CRAKOON minor version])
AC_DEFINE(CRAKOON_MICRO_VERSION, [crakoon_micro_version],
	  [Define to the CRAKOON micro version])
AC_DEFINE(CRAKOON_INTERFACE_AGE, [crakoon_interface_age],
	  [Define to the CRAKOON interface age])
AC_DEFINE(CRAKOON_BINARY_AGE, [crakoon_binary_age],
	  [Define to the CRAKOON binary age])

if (git --version > /dev/null 2>&1 && test -d .git);
then
  CRAKOON_GIT_VERSION=`git describe --all --dirty --always --long`
else
  CRAKOON_GIT_VERSION="unknown"
fi

AC_SUBST(CRAKOON_GIT_VERSION)

# libtool versioning
LT_RELEASE=crakoon_lt_release
LT_CURRENT=crakoon_lt_current
LT_REVISION=crakoon_lt_revision
LT_AGE=crakoon_lt_age
LT_CURRENT_MINUS_AGE=crakoon_lt_current_minus_age
AC_SUBST(LT_RELEASE)
AC_SUBST(LT_CURRENT)
AC_SUBST(LT_REVISION)
AC_SUBST(LT_AGE)
AC_SUBST(LT_CURRENT_MINUS_AGE)

dnl Initialize maintainer mode
AM_MAINTAINER_MODE

AC_PROG_CC
AC_PROG_CXX

# We want to check for compiler flag support, but there is no way to make
# clang's "argument unused" warning fatal.  So we invoke the compiler through a
# wrapper script that greps for this message.
saved_CC="$CC"
saved_CXX="$CXX"
saved_LD="$LD"
flag_wrap="$srcdir/scripts/wrap-compiler-for-flag-check"
CC="$flag_wrap $CC"
CXX="$flag_wrap $CXX"
LD="$flag_wrap $LD"

AC_DEFUN([check_cc_flag],
 [AC_LANG_PUSH(C)
  AX_CHECK_COMPILE_FLAG([$1], [$2], [$3], [-Werror $4])
  AC_LANG_POP(C)])
AC_DEFUN([check_link_flag],
 [AX_CHECK_LINK_FLAG([$1], [$2], [$3], [-Werror $4])])

AC_ARG_ENABLE([hardening],
  [AS_HELP_STRING([--enable-hardening],
    [Enable compiler and linker options to frustrate memory corruption exploits @<:@yes@:>@])],
  [hardening="$enableval"],
  [hardening="yes"])

HARDEN_CFLAGS=""
HARDEN_LDFLAGS=""
AS_IF([test x"$hardening" != x"no"], [
  check_cc_flag([-fno-strict-overflow], [HARDEN_CFLAGS="$HARDEN_CFLAGS -fno-strict-overflow"])

  # This one will likely succeed, even on platforms where it does nothing.
  check_cc_flag([-D_FORTIFY_SOURCE=2], [HARDEN_CFLAGS="$HARDEN_CFLAGS -D_FORTIFY_SOURCE=2"])

  check_cc_flag([-fstack-protector-all],
   [check_link_flag([-fstack-protector-all],
     [HARDEN_CFLAGS="$HARDEN_CFLAGS -fstack-protector-all"
      check_cc_flag([-Wstack-protector], [HARDEN_CFLAGS="$HARDEN_CFLAGS -Wstack-protector"],
        [], [-fstack-protector-all])
      check_cc_flag([--param ssp-buffer-size=1], [HARDEN_CFLAGS="$HARDEN_CFLAGS --param ssp-buffer-size=1"],
        [], [-fstack-protector-all])])])

  # At the link step, we might want -pie (GCC) or -Wl,-pie (Clang on OS X)
  #
  # The linker checks also compile code, so we need to include -fPIE as well.
  check_cc_flag([-fPIE],
   [check_link_flag([-fPIE -pie],
     [HARDEN_CFLAGS="$HARDEN_CFLAGS -fPIE"
      HARDEN_LDFLAGS="$HARDEN_LDFLAGS -pie"],
     [check_link_flag([-fPIE -Wl,-pie],
       [HARDEN_CFLAGS="$HARDEN_CFLAGS -fPIE"
        HARDEN_LDFLAGS="$HARDEN_LDFLAGS -Wl,-pie"])])])

  check_link_flag([-Wl,-z,relro],
   [HARDEN_LDFLAGS="$HARDEN_LDFLAGS -Wl,-z,relro"
    check_link_flag([-Wl,-z,now], [HARDEN_LDFLAGS="$HARDEN_LDFLAGS -Wl,-z,now"])])])
AC_SUBST([HARDEN_CFLAGS])
AC_SUBST([HARDEN_LDFLAGS])

# End of flag tests.
CC="$saved_CC"
CXX="$saved_CXX"
LD="$saved_LD"


AM_PROG_CC_STDC
AM_PROG_CC_C_O
AC_PROG_CPP
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
AC_PROG_LIBTOOL
PKG_PROG_PKG_CONFIG(0.16)

LT_INIT

PKG_CHECK_MODULES([CHECK], [check >= 0.9.4])

LIBTOOL_EXPORT_OPTIONS='-export-symbols-regex "^arakoon_.*"'
AC_SUBST(LIBTOOL_EXPORT_OPTIONS)

AC_MSG_CHECKING([whether to enable debug mode])
AC_ARG_ENABLE(debug,
              AC_HELP_STRING([--enable-debug=@<:@no/yes@:>@],
                             [turn on debugging @<:@default=crakoon_debug_default@:>@]),,
              enable_debug=crakoon_debug_default)

if test "x$enable_debug" = "xyes"; then
  if test x$cflags_set != xset ; then
      case " $CFLAGS " in
      *[[\ \	]]-ggdb3[[\ \	]]*) ;;
      *) CFLAGS="$CFLAGS -ggdb3" ;;
      esac
  fi

  CRAKOON_DEBUG_FLAGS="-DARAKOON_ENABLE_DEBUG"
else
  CRAKOON_DEBUG_FLAGS=""
fi
AC_SUBST(CRAKOON_DEBUG_FLAGS)
AC_MSG_RESULT([$enable_debug])

if test "x$enable_debug" = "xyes"; then
  crakoon_assert_default=abort
else
  crakoon_assert_default=log
fi

AC_MSG_CHECKING([which asserts to enable])
AC_ARG_ENABLE(asserts,
              AC_HELP_STRING([--enable-asserts=@<:@no/yes/log/abort@:>@],
                             [enable asserts @<:@default=$crakoon_assert_default@:>@]),,
              enable_asserts=$crakoon_assert_default)

case "x$enable_asserts" in
  xno) AC_SUBST(CRAKOON_ASSERT_FLAGS, []) ;;
  xyes) AC_SUBST(CRAKOON_ASSERT_FLAGS, [-DARAKOON_ASSERT=0]) ;;
  xlog) AC_SUBST(CRAKOON_ASSERT_FLAGS, [-DARAKOON_ASSERT=1]) ;;
  xabort) AC_SUBST(CRAKOON_ASSERT_FLAGS, [-DARAKOON_ASSERT=2]) ;;
  *) AC_MSG_ERROR([Invalid setting for --enable-asserts. Use "no", "yes", "log" or "abort"]) ;;
esac
AC_MSG_RESULT([$enable_asserts])

LT_PREREQ([2.2])
LT_INIT([disable-static])
m4_ifdef([LT_OUTPUT], [LT_OUTPUT])

if test "x$GCC" = "xyes"; then
  case " $CFLAGS " in
  *[[\ \	]]-Wall[[\ \	]]*) ;;
  *) CFLAGS="$CFLAGS -Wall" ;;
  esac
  case " $CFLAGS " in
  *[[\ \	]]-Wextra[[\ \	]]*) ;;
  *) CFLAGS="$CFLAGS -Wextra" ;;
  esac
  case " $CFLAGS " in
  *[[\ \	]]-Werror[[\ \	]]*) ;;
  *) CFLAGS="$CFLAGS -Werror" ;;
  esac
  case " $CFLAGS " in
  *[[\ \	]]-Wswitch-enum[[\ \	]]*) ;;
  *) CFLAGS="$CFLAGS -Wswitch-enum" ;;
  esac

fi

AC_C_CONST

SANE_MALLOC_PROTOS=no
AC_MSG_CHECKING([if malloc() and friends prototypes are arakoon_memory compatible])
crakoon_save_CFLAGS=$CFLAGS
if test "x$GCC" = "xyes"; then
  CFLAGS="$CFLAGS -Werror"
  AC_TRY_COMPILE([#include <stdlib.h>], [
    void* (*my_calloc_p)  (size_t, size_t) = calloc;
    void* (*my_malloc_p)  (size_t)         = malloc;
    void  (*my_free_p)    (void*)          = free;
    void* (*my_realloc_p) (void*, size_t)  = realloc;
    my_calloc_p = 0;
    my_malloc_p = 0;
    my_free_p = 0;
    my_realloc_p = 0;
  ],
    AC_DEFINE(SANE_MALLOC_PROTOS, 1,
      [Define if you have correct malloc prototypes])
    SANE_MALLOC_PROTOS=yes)
fi
AC_MSG_RESULT($SANE_MALLOC_PROTOS)
CFLAGS=$crakoon_save_CFLAGS

AC_CACHE_CHECK([for working do while(0) macros],
        crakoon_cv_support_dowhile_macros, [
	AC_TRY_COMPILE([],[
	#define STMT_START do
	#define STMT_END while(0)
	#define STMT_TEST STMT_START { i = 0; } STMT_END
	int main(void) { int i = 1; STMT_TEST; return i; }],
	[crakoon_cv_support_dowhile_macros=yes],
	[crakoon_cv_support_dowhile_macros=no],
	[crakoon_cv_support_dowhile_macros=yes])
])
if test x$crakoon_cv_support_dowhile_macros = xyes; then
  AC_DEFINE(HAVE_DOWHILE_MACROS, 1, [define for working do while(0) macros])
fi

AC_C_BIGENDIAN
if test x$ac_cv_c_bigendian = xuniversal ; then
AC_TRY_COMPILE([#include <endian.h>], [#if __BYTE_ORDER == __BIG_ENDIAN
#else
#error Not a big endian.
#endif],
    ac_cv_c_bigendian=yes
    ,AC_TRY_COMPILE([#include <endian.h>], [#if __BYTE_ORDER == __LITTLE_ENDIAN
#else
#error Not a little endian.
#endif],
    ac_cv_c_bigendian=no
    ,AC_MSG_WARN([Could not determine endianness.])))
fi


crakoon_save_CFLAGS=$CFLAGS
CFLAGS=`echo $CFLAGS | sed 's/-Werror//g'`

AC_HEADER_STDC
AC_CHECK_HEADERS([stdlib.h netdb.h sys/socket.h])
AC_CHECK_HEADERS([stdint.h string.h unistd.h])
AC_CHECK_HEADERS([errno.h])

AC_CHECK_LIB([rt], [clock_gettime])

AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_CHECK_FUNCS([strerror vsnprintf])
AC_CHECK_FUNCS([memset])
AC_CHECK_FUNCS([socket])
AC_CHECK_FUNCS([clock_gettime])

AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_INT32_T
AC_TYPE_UINT32_T

CFLAGS=$crakoon_save_CFLAGS

AC_CONFIG_FILES([
         Makefile
         src/Makefile
         tests/Makefile
         arakoon-1.0.pc
])

AC_OUTPUT
