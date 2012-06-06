/*
 * write.c - FUSE write function
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

#define _XOPEN_SOURCE 500
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fuse.h>
#include "gnfs.h"

int gn_write(const char *path, const char *buf, size_t size, off_t offset,
	struct fuse_file_info *fi)
{
	struct dirent *de;
	ssize_t slen;

	(void)fi;

	GNUNET_GE_LOG(ectx, GNUNET_GE_BULK | GNUNET_GE_DEVELOPER | GNUNET_GE_DEBUG,
		"%s: called for '%s' %d bytes\n", __FUNCTION__, path, size);

	/* Check for special file */
	if(gn_exists_special_file(path))
		return -EACCES;

	/* Lookup dirent for path */
	de = gn_dirent_find(path);
	if(de == NULL)
	{
		GNUNET_GE_LOG(ectx, GNUNET_GE_BULK | GNUNET_GE_DEVELOPER | GNUNET_GE_DEBUG,
			"%s: file not found\n", __FUNCTION__);
		return -ENOENT;
	}
	if(de->de_type != DE_FILE)
	{
		GNUNET_GE_LOG(ectx, GNUNET_GE_BULK | GNUNET_GE_DEVELOPER | GNUNET_GE_DEBUG,
			"%s: not a file\n", __FUNCTION__);
		size = -ENOENT;
		goto out;
	}

	/* We must be cached */
	if(GNUNET_semaphore_down(de->de_sema, GNUNET_YES) == GNUNET_SYSERR)
	{
		size = -EIO;
		goto out;
	}
	if(!de->de_cached)
	{
		if(gn_file_download_locked(de) == -1)
		{
			size = -EIO;
			goto out_unlock;
		}
	}

	/* Perform write on temp file */
	slen = pwrite(de->de_fd, buf, size, offset);
	if(slen == -1)
		size = -errno;
	else
		size = slen;

	/* Mark us dirty */
	de->de_dirty = 1;
out_unlock:
	GNUNET_semaphore_up(de->de_sema);
out:
	gn_dirent_put(de);
	return size;
}
