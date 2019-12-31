# This Makefile.am is in the public domain
## Process this file with automake to produce Makefile.in

SUBDIRS = src doc po

EXTRA_DIST = config.rpath install-sh

ACLOCAL_AMFLAGS = -I m4

if HAVE_UNCRUSTIFY_BINARY
MCRUSTIFY = find $(top_srcdir) -type f -name '*.c' -or -name '*.h' -print0 | xargs -0 uncrustify -c $(top_srcdir)/uncrustify.cfg -q --replace --no-backup 2>&1 || true
else
MCRUSTIFY = echo "crustify: skipped"
endif

pretty:
	$(MCRUSTIFY)
