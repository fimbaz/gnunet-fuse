/*
 * readdir.c - FUSE readdir function
 *
 * This file is part of gnunet-fuse.
 * Copyright (C) 2007 David Barksdale
 *
 * gnunet-fuse is free software; you can redistribute it and/or
 * modify if under the terms of version 2 of the GNU General Public License
 * as published by the Free Software Foundation.
 *
 * gnunet-fuse is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#define _LARGEFILE64_SOURCE
#include <sys/types.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <glib.h>
#include <fuse.h>
#include <GNUnet/gnunet_ecrs_lib.h>
#include "gnfs.h"

struct readdir_callback_data
{
	fuse_fill_dir_t filler;
	void *buf;
	const char *prefix;
};

static int readdir_callback(struct dirent *de, void *data)
{
	struct readdir_callback_data *d = data;

	(void)de;

	if(d->prefix != NULL)
	{
		char *buf = GNUNET_malloc(strlen(d->prefix) + strlen(de->de_basename)
			+ 1);

		sprintf(buf, "%s%s", d->prefix, de->de_basename);
		d->filler(d->buf, buf, NULL, 0);
		GNUNET_free(buf);
	}
	else
	{
		d->filler(d->buf, de->de_basename, NULL, 0);
	}
	return GNUNET_OK;
}

int gn_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
	off_t offset, struct fuse_file_info *fi)
{
	struct dirent *de;
	int ret = 0;
	struct readdir_callback_data d;

	(void)offset;
	(void)fi;

	GNUNET_GE_LOG(ectx, GNUNET_GE_BULK | GNUNET_GE_DEVELOPER | GNUNET_GE_DEBUG, "readdir for '%s'\n",
		path);
	de = gn_dirent_find(path);
	if(de == NULL)
	{
		GNUNET_GE_LOG(ectx, GNUNET_GE_BULK | GNUNET_GE_DEVELOPER | GNUNET_GE_DEBUG,
			"readdir: file not found\n");
		return -ENOENT;
	}
	if(de->de_type != DE_DIR)
	{
		GNUNET_GE_LOG(ectx, GNUNET_GE_BULK | GNUNET_GE_DEVELOPER | GNUNET_GE_DEBUG,
			"readdir: not a directory\n");
		gn_dirent_put(de);
		ret = -ENOENT;
		goto out;
	}
	filler(buf, ".", NULL, 0);
	filler(buf, "..", NULL, 0);
	if(uri_files)
	{
		filler(buf, URI_FILE, NULL, 0);
		d.filler = filler;
		d.buf = buf;
		d.prefix = ".uri.";
		ret = gn_directory_foreach(de, readdir_callback, &d);
		if(ret == -1)
		{
			ret = -ENOENT;
			goto out;
		}
	}
	d.filler = filler;
	d.buf = buf;
	d.prefix = NULL;
	ret = gn_directory_foreach(de, readdir_callback, &d);
	if(ret == -1)
		ret = -ENOENT;
out:
	gn_dirent_put(de);
	return ret;
}
