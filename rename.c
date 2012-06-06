/*
 * rename.c - FUSE rename function
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

static gboolean rename_callback(struct dirent *de, void *data)
{
	int *empty = data;

	(void)de;
	*empty = 0;
	return 1;
}

int gn_rename(const char *from, const char *to)
{
	struct dirent *from_de, *to_de, *from_parent_de, *to_parent_de;
	char *from_parent, *from_file, *to_parent, *to_file;
	int ret = 0, empty = 1;

	GNUNET_GE_LOG(ectx, GNUNET_GE_BULK | GNUNET_GE_DEVELOPER | GNUNET_GE_DEBUG, "%s: '%s' to '%s'\n",
		__FUNCTION__, from, to);

	/* Check for special file */
	if(gn_exists_special_file(from) || gn_exists_special_file(to))
		return -EACCES;

	/* Make sure 'from' exists */
	from_de = gn_dirent_find(from);
	if(from_de == NULL)
		return -ENOENT;

	/* We need to check some things before we remove 'from' */
	to_de = gn_dirent_find(to);
	if(to_de != NULL)
	{
		if(from_de->de_type == DE_FILE && to_de->de_type == DE_DIR)
		{
			ret = -EISDIR;
			goto out;
		}
		if(from_de->de_type == DE_DIR && to_de->de_type == DE_FILE)
		{
			ret = -ENOTDIR;
			goto out;
		}
		if(to_de->de_type == DE_DIR)
		{
			gn_directory_foreach(to_de, rename_callback, &empty);
			if(!empty)
			{
				ret = -ENOTEMPTY;
				goto out;
			}
		}
	}

	/* Now we can remove the 'from' */
	from_parent = gn_dirname(from, &from_file);
	from_parent_de = gn_dirent_find(from_parent);
	GNUNET_free(from_parent);
	if(from_parent_de == NULL)
	{
		ret = -ENOENT;
		goto out;
	}
	gn_directory_remove(from_parent_de, from_de);
	gn_dirent_put(from_parent_de);
	GNUNET_GE_LOG(ectx, GNUNET_GE_BULK | GNUNET_GE_DEVELOPER | GNUNET_GE_DEBUG, "%s: removed '%s'\n",
		__FUNCTION__, from);

	/* Modify our path */
	gn_dirent_path_set(from_de, to);

	/* Replace the 'to' */
	to_parent = gn_dirname(to, &to_file);
	to_parent_de = gn_dirent_find(to_parent);
	GNUNET_free(to_parent);
	if(to_parent_de == NULL)
	{
		ret = -EIO;
		goto out;
	}

	/* We should have some kind of directory_remove_insert for atomicity */
	if(to_de != NULL)
	{
		if(gn_directory_remove(to_parent_de, to_de) == -1)
		{
			gn_dirent_put(to_parent_de);
			ret = -EIO;
			goto out;
		}
		GNUNET_GE_LOG(ectx, GNUNET_GE_BULK | GNUNET_GE_DEVELOPER | GNUNET_GE_DEBUG,
			"%s: removed '%s'\n", __FUNCTION__, to);
	}
	if(gn_directory_insert(to_parent_de, from_de) == -1)
	{
		gn_dirent_put(to_parent_de);
		ret = -EIO;
		goto out;
	}
	gn_dirent_put(to_parent_de);
	GNUNET_GE_LOG(ectx, GNUNET_GE_BULK | GNUNET_GE_DEVELOPER | GNUNET_GE_DEBUG,
		"%s: inserted '%s'\n", __FUNCTION__, to);

out:
	if(to_de != NULL)
		gn_dirent_put(to_de);
	if(from_de != NULL)
		gn_dirent_put(from_de);
	return ret;
}
