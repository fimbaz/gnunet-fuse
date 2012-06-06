/*
 * mknod.c - FUSE mknod function
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

#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fuse.h>
#include "gnfs.h"

int gn_mknod(const char *path, mode_t mode, dev_t rdev)
{
	struct dirent *de, *newde;
	struct GNUNET_ECRS_URI *uri;
	struct GNUNET_MetaData *meta;
	char *parent, *file;
	int ret;

	(void)rdev;
	GNUNET_GE_LOG(ectx, GNUNET_GE_BULK | GNUNET_GE_DEVELOPER | GNUNET_GE_DEBUG, "%s: for '%s'\n",
		__FUNCTION__, path);

	/* We only support regular files */
	if(!S_ISREG(mode))
		return -ENOTSUP;

	/* Check for special file */
	if(gn_exists_special_file(path))
		return -EEXIST;

	/* Check for existing file */
	de = gn_dirent_find(path);
	if(de != NULL)
	{
		gn_dirent_put(de);
		return -EEXIST;
	}

	/* Create new file */
	parent = gn_dirname(path, &file);
	de = gn_dirent_find(parent);
	if(de == NULL)
	{
		GNUNET_free(parent);
		return -ENOENT;
	}
	uri = GNUNET_ECRS_string_to_uri(ectx, GN_EMPTY_FILE_URI);
	meta = GNUNET_meta_data_create();
	GNUNET_meta_data_insert(meta, EXTRACTOR_FILENAME, file);
	GNUNET_free(parent);
	newde = gn_dirent_new(path, uri, meta, DE_FILE);
	GNUNET_meta_data_destroy(meta);
	GNUNET_ECRS_uri_destroy(uri);
	ret = gn_directory_insert(de, newde);
	gn_dirent_put(de);
	gn_dirent_put(newde);
	if(ret == -1)
		return -EIO;
	return 0;
}
