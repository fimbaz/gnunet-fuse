/*
 * getattr.c - FUSE getattr function
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

#include <sys/stat.h>
#include <string.h>
#include <errno.h>
#include <fuse.h>
#include "gnfs.h"

int gn_getattr(const char *path, struct stat *stbuf)
{
	struct dirent *de;
	int ret = 0;

	GNUNET_GE_LOG(ectx, GNUNET_GE_BULK | GNUNET_GE_DEVELOPER | GNUNET_GE_DEBUG, "%s: for '%s'\n",
		__FUNCTION__, path);

	/* Check to see if this is a special file */
	if(gn_exists_special_file(path))
	{
		memset(stbuf, 0, sizeof(*stbuf));
		stbuf->st_mode = 0555 | S_IFREG;
		stbuf->st_nlink = 1;
		/* sysfs uses 4096 for variable sized files */
		stbuf->st_size = 4096;
		return 0;
	}

	/* Fill in dirent stat info */
	de = gn_dirent_find(path);
	if(de == NULL)
	{
		GNUNET_GE_LOG(ectx, GNUNET_GE_BULK | GNUNET_GE_USER | GNUNET_GE_DEBUG,
			"%s: could not find path '%s'\n", __FUNCTION__, path);
		return -ENOENT;
	}

	/* If it's a cached file just call stat */
	if(GNUNET_semaphore_down(de->de_sema, GNUNET_YES) == GNUNET_SYSERR)
	{
		gn_dirent_put(de);
		return -EIO;
	}
	if(de->de_cached && de->de_type == DE_FILE)
	{
		ret = stat(de->de_filename, stbuf);
		if(ret == -1)
		{
			ret = -errno;
			GNUNET_GE_LOG_STRERROR(ectx, GNUNET_GE_BULK | GNUNET_GE_USER | GNUNET_GE_ERROR,
				"stat");
			goto out;
		}
		goto out;
	}

	memset(stbuf, 0, sizeof(*stbuf));
	stbuf->st_mode = 0777;
	stbuf->st_mode |= de->de_type == DE_DIR ? S_IFDIR : S_IFREG;
	stbuf->st_nlink = 1;
	if(de->de_fi.uri != NULL)
		stbuf->st_size = GNUNET_ECRS_uri_get_file_size(de->de_fi.uri);
	else
		stbuf->st_size = 0;
out:
	GNUNET_semaphore_up(de->de_sema);
	gn_dirent_put(de);
	return ret;
}
