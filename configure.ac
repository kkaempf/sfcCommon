#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_INIT(sblim-sfcCommon, 1.0.0, sblim-devel@lists.sourceforge.net, sblim-sfcCommon)
AC_CONFIG_HEADER([config.h])

AM_INIT_AUTOMAKE

# Configure options
#AC_ARG_ENABLE(library,
#	[AC_HELP_STRING([--disable-library],
#		[disables installation of C++ library.])],
#	[enable_library=$enableval],
#	[enable_library="yes"]
#	)
#AM_CONDITIONAL(INSTALL_LIBRARY,[test "$enable_library" == "yes"]) 

AC_CANONICAL_HOST
case ${host_os} in
   linux*) SBLIM_CMPI_PLATFORM="-D CMPI_PLATFORM_LINUX_GENERIC_GNU"
   ;;
esac

AC_SUBST(SBLIM_CMPI_PLATFORM)

# Checks for programs.
AC_PROG_CXX
AC_PROG_CC
AC_PROG_LIBTOOL

# Checks for libraries.
# FIXME: Replace `main' with a function in `-ldl':
AC_CHECK_LIB([dl], [main])
# FIXME: Replace `main' with a function in `-lpthread':
AC_CHECK_LIB([pthread], [main])

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([string.h strings.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_HEADER_STDBOOL
AC_C_INLINE

# Checks for library functions.
AC_CHECK_FUNCS([strcasecmp])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
