/*
 * truncate.c - FUSE truncate function
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

#include <errno.h>
#include <unistd.h>
#include <fuse.h>
#include "gnfs.h"

int gn_truncate(const char *path, off_t size)
{
	struct dirent *de;
	int ret = 0, dirty = GN_UNLOCK_CLEAN;

	GNUNET_GE_LOG(ectx, GNUNET_GE_BULK | GNUNET_GE_DEVELOPER | GNUNET_GE_DEBUG,
		"%s: called for '%s' %lld bytes\n", __FUNCTION__, path, size);

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
		ret = -EISDIR;
		goto out;
	}

	/* Lock our path */
	if(gn_lock_path(de) == -1)
	{
		ret = -EIO;
		goto out;
	}
	if(!de->de_cached)
	{
		if(gn_file_download_locked(de) == -1)
		{
			ret = -EIO;
			goto out_unlock;
		}
	}

	/* Perform truncate */
	ret = ftruncate(de->de_fd, size);
	if(ret == -1)
	{
		ret = -errno;
		goto out_unlock;
	}

	/* Mark us dirty */
	de->de_dirty = 1;

	/* Then un-mark us dirty */
	if(gn_file_upload_locked(de) == 0)
	{
		dirty = GN_UNLOCK_ANCESTORS_DIRTY;
	}
out_unlock:
	gn_unlock_path(de, GN_UNLOCK_ANCESTORS_DIRTY);
out:
	gn_dirent_put(de);
	return ret;
}
