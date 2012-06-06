/*
 * open.c - FUSE open function
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

int gn_open(const char *path, struct fuse_file_info *fi)
{
	struct dirent *de;

	GNUNET_GE_LOG(ectx, GNUNET_GE_BULK | GNUNET_GE_DEVELOPER | GNUNET_GE_DEBUG, "%s: for '%s'\n",
		__FUNCTION__, path);
	
	/* Check for special file */
	if(gn_exists_special_file(path))
	{
		if(fi->flags & O_WRONLY)
			return -EACCES;
		if((fi->flags & (O_CREAT | O_EXCL)) == (O_CREAT | O_EXCL))
			return -EEXIST;
		return 0;
	}

	/* Check for existing file */
	de = gn_dirent_find(path);
	if(de == NULL)
		return -ENOENT;
	if(de->de_type != DE_FILE)
	{
		GNUNET_GE_LOG(ectx, GNUNET_GE_BULK | GNUNET_GE_DEVELOPER | GNUNET_GE_DEBUG,
			"%s: not a file\n", __FUNCTION__);
		gn_dirent_put(de);
		return -ENOENT;
	}
	gn_dirent_put(de);
	if((fi->flags & (O_CREAT | O_EXCL)) == (O_CREAT | O_EXCL))
		return -EEXIST;
	return 0;
}
