AC_INIT([touchndn-helper], [0.0.1], [peter@remap.ucla.edu], [touchndn-helper], [http://github.com/remap/touchndn])
AM_INIT_AUTOMAKE([1.10 -Wall -Werror no-define foreign subdir-objects])

AC_CONFIG_MACRO_DIR([m4])
AC_PREREQ([2.69])

AM_PROG_AR
AM_MAINTAINER_MODE
AM_SILENT_RULES([yes])

LT_INIT()

AC_PREFIX_DEFAULT([/usr/local])

AC_CONFIG_FILES([Makefile])

AC_CONFIG_SRCDIR([src/helper.cpp])
AC_CONFIG_HEADERS([src/config.hpp])

AC_PROG_CXX
AC_PROG_CC
AC_LANG([C++])

AX_CXX_COMPILE_STDCXX_11(, mandatory)
AC_CHECK_PROGS([TAR], [tar])

AC_OUTPUT
