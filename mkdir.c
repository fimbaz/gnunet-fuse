/*
 * mkdir.c - FUSE mkdir function
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

int gn_mkdir(const char *path, mode_t mode)
{
	struct dirent *de, *newde;
	struct GNUNET_MetaData *meta;
	char *parent, *file;
	int ret;

	(void)mode;
	GNUNET_GE_LOG(ectx, GNUNET_GE_BULK | GNUNET_GE_DEVELOPER | GNUNET_GE_DEBUG, "%s: for '%s'\n",
		__FUNCTION__, path);

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

	/* Create new directory */
	parent = gn_dirname(path, &file);
	de = gn_dirent_find(parent);
	if(de == NULL)
	{
		GNUNET_free(parent);
		return -ENOENT;
	}
	meta = GNUNET_meta_data_create();
	GNUNET_meta_data_insert(meta, EXTRACTOR_FILENAME, file);
	GNUNET_meta_data_insert(meta, EXTRACTOR_MIMETYPE, GNUNET_DIRECTORY_MIME);
	newde = gn_dirent_new(path, NULL, meta, DE_DIR);
	GNUNET_meta_data_destroy(meta);
	ret = gn_directory_insert(de, newde);
	gn_dirent_put(de);
	gn_dirent_put(newde);
	GNUNET_free(parent);
	if(ret == -1)
		return -EIO;
	return 0;
}
