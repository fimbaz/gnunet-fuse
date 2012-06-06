/*
 * file.c - operations on files
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

#include <sys/mman.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <glib.h>
#include <GNUnet/gnunet_ecrs_lib.h>
#include "gnfs.h"

static int tt(void *cls)
{
	(void)cls;
	if(closing)
		return GNUNET_OK;
	if(fuse_interrupted())
		return GNUNET_SYSERR;
	return GNUNET_OK;
}

static void upcb(unsigned long long totalBytes, 
		 unsigned long long completedBytes, GNUNET_CronTime eta,
	void *closure)
{
	(void)totalBytes;
	(void)completedBytes;
	(void)eta;
	(void)closure;
}

static void dpcb(unsigned long long totalBytes,
        unsigned long long completedBytes, GNUNET_CronTime eta,
        unsigned long long lastBlockOffset, const char *lastBlock,
        unsigned int lastBlockSize, void *cls)
{
	(void)totalBytes;
	(void)completedBytes;
	(void)eta;
	(void)lastBlockOffset;
	(void)lastBlock;
	(void)lastBlockSize;
	(void)cls;
}

/*
 * Download a file for writing, de_sema must be held.
 */
int gn_file_download_locked(struct dirent *de)
{
	char filename[] = GN_MKSTEMP_FILE;

	/* We may already be cached */
	if(de->de_cached)
		return 0;

	/* Do the download */
	de->de_fd = mkstemp(filename);
	if(de->de_fd == -1)
	{
		GNUNET_GE_LOG_STRERROR_FILE(ectx, GNUNET_GE_BULK | GNUNET_GE_DEVELOPER
			| GNUNET_GE_ERROR, "mkstemp", filename);
		return -1;
	}
	de->de_filename = GNUNET_strdup(filename);

	GNUNET_GE_LOG(ectx, GNUNET_GE_BULK | GNUNET_GE_DEVELOPER | GNUNET_GE_DEBUG,
		"%s: downloading '%s'\n", __FUNCTION__, de->de_filename);
	if(GNUNET_ECRS_file_download(ectx, cfg, de->de_fi.uri, filename, anonymity,
		dpcb, NULL, tt, NULL) == GNUNET_SYSERR)
	{
		GNUNET_GE_LOG(ectx, GNUNET_GE_BULK | GNUNET_GE_DEVELOPER | GNUNET_GE_ERROR,
			"%s: download failed\n", __FUNCTION__);
		close(de->de_fd);
		unlink(de->de_filename);
		GNUNET_free(de->de_filename);
		return -1;
	}

	/* Mark ourselves cached */
	de->de_cached = 1;
	return 0;
}

int gn_file_upload_locked(struct dirent *de)
{
	struct GNUNET_ECRS_URI *uri;

	/* If we're not dirty then we're done */
	if(!de->de_dirty)
		return 0;

	if(GNUNET_ECRS_file_upload(ectx, cfg, de->de_filename, GNUNET_NO, anonymity, priority,
		-1, upcb, NULL, tt, NULL, &uri) == GNUNET_SYSERR)
	{
		GNUNET_GE_LOG(ectx, GNUNET_GE_BULK | GNUNET_GE_DEVELOPER | GNUNET_GE_ERROR,
			"%s: upload failed\n", __FUNCTION__);
		return -1;
	}
	if(de->de_fi.uri != NULL)
		GNUNET_ECRS_uri_destroy(de->de_fi.uri);
	de->de_fi.uri = uri;
	de->de_cached = 0;
	de->de_dirty = 0;
	close(de->de_fd);
	unlink(de->de_filename);
	GNUNET_free(de->de_filename);
	return 0;
}
