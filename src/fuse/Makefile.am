# This Makefile.am is in the public domain
SUBDIRS = .

AM_CPPFLAGS = \
  -I$(top_srcdir)/src/include \
  -I$(top_srcdir) \
  @GNUNET_CFLAGS@


bin_PROGRAMS = gnunet-fuse

gnunet_fuse_SOURCES = \
  gnunet-fuse.c gnunet-fuse.h \
  gfs_download.c gfs_download.h \
  mutex.c mutex.h \
  readdir.c \
  read.c \
  open.c \
  getattr.c
#
#	mkdir.c \
#	mknod.c \
#	release.c \
#	rename.c \
#	rmdir.c \
#	truncate.c \
#	unlink.c \
#	utimens.c \
#	write.c

gnunet_fuse_LDADD = \
  -lgnunetutil \
  -lfuse \
  -lgnunetfs \
  $(INTLLIBS) $(GNUNET_LIBS) -lpthread
gnunet_fuse_CPPFLAGS = \
  $(AM_CPPFLAGS) \
  -D_FILE_OFFSET_BITS=64 \
  -DFUSE_USE_VERSION=26
