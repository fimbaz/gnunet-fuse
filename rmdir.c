/*
 * rmdir.c - FUSE rmdir function
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

static gboolean rmdir_callback(struct dirent *de, void *data)
{
	int *empty = data;

	(void)de;
	*empty = 0;
	return 1;
}

int gn_rmdir(const char *path)
{
	struct dirent *de, *dechild;
	char *parent, *file;
	int ret, empty = 1;

	GNUNET_GE_LOG(ectx, GNUNET_GE_BULK | GNUNET_GE_DEVELOPER | GNUNET_GE_DEBUG, "%s: for '%s'\n",
		__FUNCTION__, path);

	/* Check for special file */
	if(gn_exists_special_file(path))
		return -ENOTDIR;

	/* Check for existing file */
	dechild = gn_dirent_find(path);
	if(dechild == NULL)
		return -ENOENT;

	/* Can't rmdir a non-empty directory */
	gn_directory_foreach(dechild, rmdir_callback, &empty);
	if(!empty)
	{
		gn_dirent_put(dechild);
		return -ENOTEMPTY;
	}

	/* Remove directory */
	parent = gn_dirname(path, &file);
	de = gn_dirent_find(parent);
	GNUNET_free(parent);
	if(de == NULL)
	{
		gn_dirent_put(dechild);
		return -ENOENT;
	}
	ret = gn_directory_remove(de, dechild);
	gn_dirent_put(dechild);
	gn_dirent_put(de);
	if(ret == -1)
		return -EIO;
	return 0;
}
