AC_INIT([HPCBench], [0.1a], [Jonathan Hall - <Jon@JonathanDavidHall.com>])
AM_INIT_AUTOMAKE([-Wall -Werror foreign])
AC_PROG_CC
#AC_CONFIG_HEADERS([config.h])
AC_CONFIG_SUBDIRS = src/plugins
AC_CONFIG_FILES([
    Makefile
    src/Makefile
])

AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_CHECK_FUNCS([memset strstr])

AC_CHECK_HEADERS([pthread.h], [ LIBS="-lpthread ${LIBS}"], [
    echo "pthread.h required, failing"
    exit -1
])
AC_CHECK_HEADERS([math.h], [ LIBS="-lm ${LIBS}"], [
    echo "math.h required, failing"
    exit -1
])
AC_CHECK_HEADERS([dlfcn.h], [ LIBS="-ldl -Wl,-dynamic-list,plugins.list ${LIBS}"], [
    echo "Dynamic linking libraries required, failing"
    exit -1
])
#AC_CONFIG_FILES([Makefile src/Makefile])
AC_OUTPUT
