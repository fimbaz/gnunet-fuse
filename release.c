/*
 * release.c - FUSE release function
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

#include <fuse.h>
#include "gnfs.h"

int gn_release(const char *path, struct fuse_file_info *fi)
{
	struct dirent *de;
	int dirty = GN_UNLOCK_CLEAN;

	(void)fi;
	GNUNET_GE_LOG(ectx, GNUNET_GE_BULK | GNUNET_GE_DEVELOPER | GNUNET_GE_DEBUG, "%s: for '%s'\n",
		__FUNCTION__, path);

	/* Don't do anything for special files */
	if(gn_exists_special_file(path))
		return 0;

	/* If it doesn't exist we don't care */
	de = gn_dirent_find(path);
	if(de == NULL)
		return 0;
	if(de->de_type != DE_FILE)
	{
		GNUNET_GE_LOG(ectx, GNUNET_GE_BULK | GNUNET_GE_DEVELOPER | GNUNET_GE_DEBUG,
			"%s: not a file\n", __FUNCTION__);
		gn_dirent_put(de);
		return 0;
	}

	/* Lock our path */
	if(gn_lock_path(de) == -1)
		return 0;

	/* Un-dirty ourselfs */
	if(gn_file_upload_locked(de) == 0)
	{
		/* Now we must mark every containing directory dirty */
		dirty = GN_UNLOCK_ANCESTORS_DIRTY;
	}

	gn_unlock_path(de, dirty);
	gn_dirent_put(de);
	return 0;
}
