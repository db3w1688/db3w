#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([DB3W], [0.9], [cchou@db3w.com])
AC_CONFIG_SRCDIR([client/hbapi/breaks.c])
AC_CONFIG_HEADERS([config.h])

AM_INIT_AUTOMAKE

# Checks for programs.
AC_PROG_CC

AC_PROG_RANLIB

AC_CONFIG_FILES([
	Makefile
	server/Makefile
	server/hb/Makefile
	server/dbe/Makefile
	client/Makefile
	client/hbapi/Makefile
	client/hbcmd/Makefile
	client/cgi/Makefile
	misc/Makefile
	misc/tiger32/Makefile
	misc/libcrc-2.0/Makefile
	misc/libb64-1.2/Makefile
	doc/Makefile
	man/Makefile
	examples/Makefile
])

# Checks for libraries.

# Checks for header files.
AC_CHECK_HEADERS([arpa/inet.h fcntl.h fenv.h float.h limits.h malloc.h memory.h netdb.h netinet/in.h shadow.h stdint.h stdlib.h string.h sys/file.h sys/ioctl.h sys/param.h sys/socket.h sys/statvfs.h sys/time.h syslog.h termios.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_INT32_T
AC_TYPE_INT64_T
AC_TYPE_OFF_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_STRUCT_TIMEZONE
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_TYPE_UINT8_T

# Checks for library functions.
AC_FUNC_CHOWN
AC_FUNC_FORK
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_FUNC_STRTOD
AC_CHECK_FUNCS([atexit euidaccess floor ftruncate getcwd getspnam inet_ntoa memmove memset mkdir pow regcomp rmdir select socket sqrt strcasecmp strchr strerror strncasecmp strrchr strtol strtoul])

AC_OUTPUT
