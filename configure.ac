AC_INIT(fbshow.c)
PRODUCT=fbshow
VERSION=2.0
AC_DEFINE_UNQUOTED(PRODUCT, "$PRODUCT")
AC_DEFINE_UNQUOTED(VERSION, "$VERSION")
AC_SUBST(PRODUCT)
AC_SUBST(VERSION)
AC_CONFIG_HEADER(config.h)

dnl Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL

dnl Checks for libraries.

dnl Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS(limits.h unistd.h)

dnl Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST

dnl Checks for library functions.
AC_TYPE_SIGNAL

##AC_CHECK_FUNCS()

AC_OUTPUT(Makefile)
