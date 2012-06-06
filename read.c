/*
 * read.c - FUSE read function
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

struct read_data
{
	char *buf;
	guint size;
	guint64 offset;
};

static void dpcb(unsigned long long totalBytes,
	unsigned long long completedBytes, GNUNET_CronTime eta,
	unsigned long long lastBlockOffset, const char *lastBlock,
	unsigned int lastBlockSize, void *cls)
{
	struct read_data *d = cls;
	guint64 block_end = lastBlockOffset + lastBlockSize;
	guint64 buf_end = d->offset + d->size;

	(void)totalBytes;
	(void)completedBytes;
	(void)eta;

	/* Check if this block is entirely before the buffer */
	if(block_end < d->offset)
		return;

	/* Check if this block is entirely after the buffer */
	if(lastBlockOffset > buf_end)
		return;

	/* Chop off residue at beginning of block */
	if(lastBlockOffset < d->offset)
	{
		lastBlock += d->offset - lastBlockOffset;
		lastBlockSize -= d->offset - lastBlockOffset;
		lastBlockOffset = d->offset;
	}
	/* Chop off residue at end of block */
	if(block_end > buf_end)
	{
		lastBlockSize -= block_end - buf_end;
	}
	memcpy(d->buf + (lastBlockOffset - d->offset), lastBlock,
		lastBlockSize);
}

static int tt(void *cls)
{
	(void)cls;
	return fuse_interrupted() ? GNUNET_SYSERR : GNUNET_OK;
}

int gn_read(const char *path, char *buf, size_t size, off_t offset,
	struct fuse_file_info *fi)
{
	struct dirent *de;
	struct read_data d;
	char *special;
	int ret;
	ssize_t slen;
	guint64 len;

	(void)fi;

	GNUNET_GE_LOG(ectx, GNUNET_GE_BULK | GNUNET_GE_DEVELOPER | GNUNET_GE_DEBUG,
		"%s: called for '%s' %u bytes %lld offset\n", __FUNCTION__,
		path, size, offset);

	/* Check for special file */
	special = gn_get_special_file(path);
	if(special != NULL)
	{
		slen = strlen(special);
		if(offset >= slen)
		{
			GNUNET_free(special);
			return 0;
		}
		if( ((ssize_t) (offset + size)) > slen)
		{
			size = slen - offset;
		}
		memcpy(buf, special + offset, size);
		GNUNET_free(special);
		return size;
	}

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
	if(GNUNET_semaphore_down(de->de_sema, GNUNET_YES) == GNUNET_SYSERR)
	{
		size = -EIO;
		goto out;
	}
	if(de->de_cached)
	{
		slen = pread(de->de_fd, buf, size, offset);
		if(slen == -1)
			size = -errno;
		else
			size = slen;
		goto out_sema_up;
	}
	len = GNUNET_ECRS_uri_get_file_size(de->de_fi.uri);
	if((guint64)offset >= len)
	{
		size = 0;
		goto out_sema_up;
	}
	if((guint64)offset + size > len)
	{
		size = len - offset;
	}
	d.buf = buf;
	d.size = size;
	d.offset = offset;
	ret = GNUNET_ECRS_file_download_partial(ectx, cfg, de->de_fi.uri, "/dev/null",
		offset, size, anonymity, GNUNET_YES, dpcb, &d, tt, NULL);
	if(ret != GNUNET_OK)
	{
		GNUNET_GE_LOG(ectx, GNUNET_GE_BULK | GNUNET_GE_USER | GNUNET_GE_ERROR,
			"%s: failed to download file\n", __FUNCTION__);
		size = -ENODATA;
	}
out_sema_up:
	GNUNET_semaphore_up(de->de_sema);
out:
	gn_dirent_put(de);
	return size;
}
