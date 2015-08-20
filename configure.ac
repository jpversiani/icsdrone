#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.61)
AC_INIT([icsdroneng], [0.28], [michel.vandenbergh@uhasselt.be])
AM_INIT_AUTOMAKE
AC_CONFIG_SRCDIR([fics.c])
AC_CONFIG_HEADER([config.h])

# Checks for programs.
AC_PROG_CC

# Checks for libraries.
AC_CHECK_LIB([m],[main])

# Check if we have a git repository
if [test -e .git] 
then
	AC_DEFINE([GIT], [], [We have a git repository])
fi

AM_CONDITIONAL([GIT], [test -e .git])

# Checks for readline
AC_ARG_WITH([readline],
	[AS_HELP_STRING([--with-readline],
		[console with readline support @<:@default=check@:>@])],
		[],
		[with_readline=check])
          
LIBREADLINE=
AS_IF([test "x$with_readline" != xno],
	    [AC_CHECK_HEADER([readline/readline.h],
                       [
		       AC_CHECK_LIB([ncurses],[main])
                       AC_CHECK_LIB([readline],[main],,[ncurses])
		       ],
               [if test "x$with_readline" != xcheck; then
                 AC_MSG_FAILURE(
                   [--with-readline was given, but test for readline failed])
                  fi
               ], [])
            ])

# Checks for libz
AC_ARG_WITH([zlib],
	[AS_HELP_STRING([--with-zlib],
		[compressed logfile support @<:@default=check@:>@])],
		[],
		[with_zlib=check])
          
LIBZ=
AS_IF([test "x$with_zlib" != xno],
	    [AC_CHECK_HEADER([zlib.h],AC_CHECK_LIB([z],[main]),
               [if test "x$with_zlib" != xcheck; then
                 AC_MSG_FAILURE(
                   [--with-zlib was given, but test for readline failed])
                  fi
               ], [])
            ])


# Checks for header files.
AC_HEADER_STDC
AC_HEADER_SYS_WAIT
AC_CHECK_HEADERS([arpa/inet.h arpa/telnet.h ncurses.h fcntl.h netdb.h netinet/in.h readline/readline.h readline/history.h stdlib.h string.h sys/ioctl.h sys/socket.h sys/time.h sys/wait.h term.h ncurses/term.h termios.h unistd.h zlib.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_C_CONST
AC_TYPE_PID_T
AC_HEADER_TIME
AC_STRUCT_TM

# Checks for library functions.
AC_FUNC_FORK
AC_FUNC_MALLOC
AC_FUNC_SELECT_ARGTYPES
AC_TYPE_SIGNAL
AC_FUNC_VPRINTF
AC_CHECK_FUNCS([alarm dup2 floor getcwd gethostbyname gettimeofday inet_ntoa memmove memset select socket strcasecmp strchr strdup strerror strncasecmp strpbrk strstr tzset])
AC_CONFIG_FILES([Makefile icsdroneng.spec])
AC_OUTPUT
