AC_INIT(gnunet-fuse, 0.8.0c)
AM_INIT_AUTOMAKE
AM_CONFIG_HEADER(config.h)

AC_PROG_CC
AM_PROG_CC_C_O
AC_C_CHAR_UNSIGNED

export PKG_CONFIG_PATH=/usr/local/lib/pkgconfig:$PKG_CONFIG_PATH

PKG_CHECK_MODULES(GLIB, [glib-2.0])
PKG_CHECK_MODULES(FUSE, [fuse >= 2.6])

# test for GNUnet core
gnunet=0
AC_MSG_CHECKING([for GNUnet core])
AC_ARG_WITH(gnunet,
   [  --with-gnunet=PFX       Base of GNUnet installation],
   [AC_MSG_RESULT([$with_gnunet])
    case $with_gnunet in
      no)
        ;;
      yes)
        AC_CHECK_HEADERS(GNUnet/gnunet_ecrs_lib.h,
          AC_CHECK_LIB([gnunetecrs], [GNUNET_ECRS_file_download_partial],
            gnunet=1))
        ;;
      *)
        LDFLAGS="-L$with_gnunet/lib $LDFLAGS"
        LIBPATH="$with_gnunet/lib $LIBPATH" 
        CPPFLAGS="-I$with_gnunet/include $CPPFLAGS"
        CFLAGS="-I$with_gnunet/include $CFLAGS"
        INCLUDEPATH="$with_gnunet/include $INCLUDEPATH"
        AC_CHECK_HEADERS(GNUnet/gnunet_ecrs_lib.h,
          AC_CHECK_LIB([gnunetecrs], [GNUNET_ECRS_file_download_partial],
            EXT_LIB_PATH="-L$with_gnunet/lib $EXT_LIB_PATH"
            gnunet=1))
        ;;
    esac
   ],
   [AC_MSG_RESULT([--with-gnunet not specified])
    AC_CHECK_HEADERS(GNUnet/gnunet_ecrs_lib.h,
     AC_CHECK_LIB([gnunetecrs], [GNUNET_ECRS_file_download_partial],
      gnunet=1))])
if test "$gnunet" != 1
then
 AC_MSG_ERROR([gnunet-fuse requires GNUnet])
fi
AC_CHECK_HEADERS([GNUnet/gnunet_util.h GNUnet/gnunet_getoption_lib.h],,
             AC_MSG_ERROR([compiling gnunet-fuse requires GNUnet core headers]
))

AC_CHECK_LIB(gnunetutil,GNUNET_init,,
              AC_MSG_ERROR([gnunet-fuse requires GNUnet-Util]))
AC_CHECK_LIB(gnunetecrs,GNUNET_ECRS_file_download_partial,,
             AC_MSG_ERROR([gnunet-fuse requires ECRS]))

CFLAGS="$CFLAGS -Wall -W $GLIB_CFLAGS $FUSE_CFLAGS"
LIBS="$LIBS $GLIB_LIBS $FUSE_LIBS"

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
