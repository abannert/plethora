#AC_PREREQ(2.59)
#AC_COPYRIGHT([Copyright (C) 2004])
AC_REVISION($Revision: 1.1 $)
AC_INIT([plethora], [0.1], [aaron-plethora@codemass.com])

AC_CONFIG_SRCDIR([plethora.c])
AC_CONFIG_AUX_DIR([build])
AC_CONFIG_HEADER([config.h])

# Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL

# Checks for libraries.
AC_CHECK_LIB([event], [event_init],,
    [AC_MSG_ERROR([libevent not found, see config.log (Hint: use LDFLAGS)])])

# Checks for header files.
AC_FUNC_ALLOCA
AC_HEADER_STDC
AC_HEADER_SYS_WAIT
#AC_CHECK_HEADERS([arpa/inet.h fcntl.h limits.h malloc.h netinet/in.h stdlib.h string.h sys/socket.h unistd.h])
AC_CHECK_HEADERS([event.h],,
    [AC_MSG_ERROR([libevent header event.h not found, use CFLAGS])])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_PID_T
AC_TYPE_SIZE_T

# Checks for library functions.
AC_FUNC_FORK
AC_FUNC_MALLOC
AC_FUNC_MEMCMP
AC_FUNC_REALLOC
AC_FUNC_VPRINTF
AC_CHECK_FUNCS([socketpair memset socket strdup strerror])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT