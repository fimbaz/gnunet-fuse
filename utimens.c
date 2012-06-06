/*
 * utimens.c - FUSE utimens function
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

#define _GNU_SOURCE
#include <sys/time.h>
#include <string.h>
#include <errno.h>
#include <fuse.h>
#include "gnfs.h"

int gn_utimens(const char *path, const struct timespec ts[2])
{
	struct dirent *de;
	struct timeval tv[2];
	int ret = 0;

	GNUNET_GE_LOG(ectx, GNUNET_GE_BULK | GNUNET_GE_DEVELOPER | GNUNET_GE_DEBUG, "%s: for '%s'\n",
		__FUNCTION__, path);

	/* Check to see if this is a special file */
	if(gn_exists_special_file(path))
		return -EACCES;

	/* Get file or dir */
	de = gn_dirent_find(path);
	if(de == NULL)
		return -ENOENT;

	/* If it's a cached file just call utime */
	if(GNUNET_semaphore_down(de->de_sema, GNUNET_YES) == GNUNET_SYSERR)
	{
		gn_dirent_put(de);
		return -EIO;
	}
	if(de->de_cached && de->de_type == DE_FILE)
	{
		TIMESPEC_TO_TIMEVAL(&tv[0], &ts[0]);
		TIMESPEC_TO_TIMEVAL(&tv[1], &ts[1]);
		ret = utimes(path, tv);
		if(ret == -1)
		{
			ret = -errno;
			GNUNET_GE_LOG_STRERROR(ectx, GNUNET_GE_BULK | GNUNET_GE_USER | GNUNET_GE_ERROR,
				"utimes");
			goto out;
		}
		goto out;
	}

	/* For now we do nothing otherwise */
out:
	GNUNET_semaphore_up(de->de_sema);
	gn_dirent_put(de);
	return ret;
}
