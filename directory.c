/*
 * directory.c - stuff you want to do with directories
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

static void dpcb(unsigned long long totalBytes,
	unsigned long long completedBytes, GNUNET_CronTime eta,
	unsigned long long lastBlockOffset, const char *lastBlock,
	unsigned int lastBlockSize, void *cls)
{
	(void)totalBytes;
	(void)completedBytes;
	(void)eta;
	memcpy((char *)cls + lastBlockOffset, lastBlock, lastBlockSize);
}

static int tt(void *cls)
{
	(void)cls;
	if(closing)
		return GNUNET_OK;
	if(fuse_interrupted())
		return GNUNET_SYSERR;
	return GNUNET_OK;
}

static int dir_cache_cb(const GNUNET_ECRS_FileInfo *fi, const GNUNET_HashCode *key,
	int isRoot, void *data)
{
	struct dirent *de, *deparent = data;
	gchar *filename, *path, *newpath, type;
	size_t len, rlen;

	(void)key;

	if(isRoot == GNUNET_YES)
		return GNUNET_OK;

	/* Figure out the filename and type from metadata */
	filename = GNUNET_meta_data_get_by_type(fi->meta, EXTRACTOR_FILENAME);
	if(filename == NULL)
	{
		GNUNET_GE_LOG(ectx, GNUNET_GE_BULK | GNUNET_GE_USER | GNUNET_GE_WARNING,
			"%s: dirent has no filename\n", __FUNCTION__);
		return GNUNET_OK;
	}
	len = strlen(filename);
	if(GNUNET_meta_data_test_for_directory(fi->meta) == GNUNET_YES)
	{
		if(filename[len - 1] == '/' || filename[len - 1] == '\\')
			filename[len - 1] = '\0';
		type = DE_DIR;
	}
	else
		type = DE_FILE;

	/* Create newpath, the path to this entry */
	path = gn_dirent_path_get(deparent);
	rlen = strlen(path);
	newpath = GNUNET_malloc(rlen + len + 1);
	strcpy(newpath, path);
	if(path[rlen - 1] != G_DIR_SEPARATOR)
		strcat(newpath, G_DIR_SEPARATOR_S);
	GNUNET_free(path);
	strcat(newpath, filename);

	/* Create a new dirent for this entry only if one doesn't already exist
	 * and the only place that can be is in the cache */
	de = gn_dirent_get(newpath);
	if(de == NULL)
	{
		de = gn_dirent_new(newpath, fi->uri, fi->meta, type);

		/* Add it to the cache (creates its own ref)*/
		/* NB: the lock on deparent is enough to guarantee that another
		 * thread hasn't added this dirent to the cache in the time
		 * between the above check and this insert */
		gn_dirent_cache_insert(de);
	}

	/* Add it to the directory's list (steals our ref)*/
	GNUNET_mutex_lock(de->de_path_mutex);
	GNUNET_GE_ASSERT(ectx,
		!g_hash_table_lookup(deparent->de_dir_hash, de->de_basename));
	g_hash_table_replace(deparent->de_dir_hash, de->de_basename, de);
	GNUNET_mutex_unlock(de->de_path_mutex);

	/* Clean up */
	GNUNET_free(filename);
	GNUNET_free(newpath);
	return GNUNET_OK;
}

static int directory_cache_locked(struct dirent *de)
{
	struct GNUNET_MetaData *md;
	void *mem;
	int ret;
	guint64 len;

	len = GNUNET_ECRS_uri_get_file_size(de->de_fi.uri);
	mem = GNUNET_malloc(len);
	ret = GNUNET_ECRS_file_download_partial(ectx, cfg, de->de_fi.uri,
		"/dev/null", 0, len, anonymity, GNUNET_YES, dpcb, mem, tt,
		NULL);
	if(ret != GNUNET_OK)
	{
		GNUNET_GE_LOG(ectx, GNUNET_GE_BULK | GNUNET_GE_USER | GNUNET_GE_ERROR,
			"%s: failed to download directory\n",
			__FUNCTION__);
		GNUNET_free(mem);
		return -1;
	}
	de->de_dir_hash = g_hash_table_new_full(g_str_hash, g_str_equal, NULL,
		(GDestroyNotify)gn_dirent_put);
	GNUNET_ECRS_directory_list_contents(ectx, mem, len, NULL, &md, dir_cache_cb, de);
	GNUNET_free(mem);
	GNUNET_meta_data_destroy(md);
	de->de_cached = 1;
	GNUNET_GE_LOG(ectx, GNUNET_GE_BULK | GNUNET_GE_DEVELOPER | GNUNET_GE_DEBUG,
		"%s: cached %d entries\n", __FUNCTION__,
		g_hash_table_size(de->de_dir_hash));
	return 0;
}

struct dir_foreach_data
{
	gn_dir_foreach_callback cb;
	void *data;
};

static gboolean dir_foreach_callback(gpointer key, gpointer value,
	gpointer data)
{
	struct dirent *de = value;
	struct dir_foreach_data *d = data;

	(void)key;
	return d->cb(de, d->data) == -1;
}

/*
 * Call cb for each element in a directory
 */
int gn_directory_foreach(struct dirent *de, gn_dir_foreach_callback cb,
	void *data)
{
	struct dir_foreach_data d;
	int ret = 0;

	if(de->de_type != DE_DIR)
		return -1;
	if(GNUNET_semaphore_down(de->de_sema, GNUNET_YES) == GNUNET_SYSERR)
		return -1;
	if(!de->de_cached)
	{
		ret = directory_cache_locked(de);
		if(ret == -1)
			goto out;
	}
	d.cb = cb;
	d.data = data;
	g_hash_table_find(de->de_dir_hash, dir_foreach_callback, &d);
out:
	GNUNET_semaphore_up(de->de_sema);
	return ret;
}

/*
 * Finds 'filename' in directory 'de' and returns a reference or NULL
 */
struct dirent *gn_directory_find(struct dirent *de, const gchar *filename)
{
	struct dirent *ret = NULL;

	if(de->de_type != DE_DIR)
		return NULL;
	if(GNUNET_semaphore_down(de->de_sema, GNUNET_YES) == GNUNET_SYSERR)
		return NULL;
	if(!de->de_cached)
	{
		if(directory_cache_locked(de) == -1)
			goto out;
	}
	ret = g_hash_table_lookup(de->de_dir_hash, filename);
	if(ret != NULL)
		gn_dirent_ref(ret);
out:
	GNUNET_semaphore_up(de->de_sema);
	return ret;
}

int gn_directory_insert(struct dirent *de, struct dirent *dechild)
{
	/* Lock our path */
	if(gn_lock_path(de) == -1)
		return -1;

	/* Cache ourselfs (because we're going to become dirty) */
	if(!de->de_cached)
	{
		if(directory_cache_locked(de) == -1)
		{
			GNUNET_GE_LOG(ectx, GNUNET_GE_BULK | GNUNET_GE_DEVELOPER | GNUNET_GE_DEBUG,
				"%s: failed to cache parent dir\n",
				__FUNCTION__);
			gn_unlock_path(de, GN_UNLOCK_CLEAN);
			return -1;
		}
	}

	/* If we're already in there, bail out */
	GNUNET_mutex_lock(dechild->de_path_mutex);
	if(g_hash_table_lookup(de->de_dir_hash, dechild->de_basename))
	{
		GNUNET_mutex_unlock(dechild->de_path_mutex);
		gn_unlock_path(de, GN_UNLOCK_CLEAN);
		return -1;
	}

	/* Insert the child in our de_dir_hash */
	gn_dirent_ref(dechild);
	g_hash_table_replace(de->de_dir_hash, dechild->de_basename, dechild);
	GNUNET_mutex_unlock(dechild->de_path_mutex);

	/* Cache the dirent */
	gn_dirent_cache_insert(dechild);

	/* Mark our path dirty */
	gn_unlock_path(de, GN_UNLOCK_ALL_DIRTY);
	return 0;
}

int gn_directory_remove(struct dirent *de, struct dirent *dechild)
{
	/* Lock our path */
	if(gn_lock_path(de) == -1)
		return -1;

	/* Cache ourselfs (because we're going to become dirty) */
	if(!de->de_cached)
	{
		if(directory_cache_locked(de) == -1)
		{
			GNUNET_GE_LOG(ectx, GNUNET_GE_BULK | GNUNET_GE_DEVELOPER | GNUNET_GE_ERROR,
				"%s: failed to cache parent dir\n",
				__FUNCTION__);
			goto out_err;
		}
	}

	/* Remove from dir_hash */
	GNUNET_mutex_lock(dechild->de_path_mutex);
	if(!g_hash_table_remove(de->de_dir_hash, dechild->de_basename))
	{
		GNUNET_mutex_unlock(dechild->de_path_mutex);
		GNUNET_GE_LOG(ectx, GNUNET_GE_BULK | GNUNET_GE_DEVELOPER | GNUNET_GE_ERROR,
			"%s: not found in dir_hash\n",
			__FUNCTION__);
		goto out_err;
	}
	GNUNET_mutex_unlock(dechild->de_path_mutex);

	/* Remove from dirent cache */
	gn_dirent_cache_remove(dechild);

	/* Mark our path dirty */
	gn_unlock_path(de, GN_UNLOCK_ALL_DIRTY);
	return 0;
out_err:
	gn_unlock_path(de, GN_UNLOCK_CLEAN);
	return -1;
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

struct dir_upload_data
{
	GNUNET_ECRS_FileInfo *fis;
	int count;
	int failed;
};

static gboolean dir_upload_callback(gpointer key, gpointer value, gpointer data)
{
	struct dirent *de = value;
	struct dir_upload_data *d = data;
	int ret = 0;

	(void)key;
	if(GNUNET_semaphore_down(de->de_sema, GNUNET_YES) == GNUNET_SYSERR)
	{
		d->failed = 1;
		return 1;
	}
	if(de->de_dirty)
	{
		if(de->de_type == DE_FILE)
		{
			if(de->de_fi.uri == NULL)
			{
				goto out;
			}
		}
		else
		{
			if(gn_directory_upload_locked(de) == -1)
			{
				d->failed = 1;
				ret = 1;
				goto out;
			}
		}
	}
	d->fis[d->count].uri = GNUNET_ECRS_uri_duplicate(de->de_fi.uri);
	d->fis[d->count].meta = GNUNET_meta_data_duplicate(de->de_fi.meta);
	d->count++;
out:
	GNUNET_semaphore_up(de->de_sema);
	return ret;
}

/*
 * Make a directory clean, de_sema must be locked
 */
int gn_directory_upload_locked(struct dirent *de)
{
	int i, ret, fd;
	char *buf, filename[] = GN_MKSTEMP_FILE;
	unsigned long long len;
	struct GNUNET_ECRS_URI *uri;
	struct dir_upload_data d;

	/* We may be already clean */
	if(!de->de_dirty)
		return 0;

	/* Collect FileInfo from hash table and make a GNUnet directory */
	d.fis = GNUNET_malloc(g_hash_table_size(de->de_dir_hash) * sizeof(*d.fis));
	d.count = 0;
	d.failed = 0;
	g_hash_table_find(de->de_dir_hash, dir_upload_callback, &d);
	if(d.failed)
	{
		GNUNET_GE_LOG(ectx, GNUNET_GE_BULK | GNUNET_GE_DEVELOPER | GNUNET_GE_ERROR,
			"%s: failed\n", __FUNCTION__);
		return -1;
	}
	GNUNET_GE_LOG(ectx, GNUNET_GE_BULK | GNUNET_GE_DEVELOPER | GNUNET_GE_DEBUG,
		"%s: creating dir of %d elements\n", __FUNCTION__, d.count);
	ret = GNUNET_ECRS_directory_create(ectx, &buf, &len, d.count, d.fis,
		de->de_fi.meta);
	for(i = 0; i < d.count; i++)
	{
		GNUNET_ECRS_uri_destroy(d.fis[i].uri);
		GNUNET_meta_data_destroy(d.fis[i].meta);
	}
	GNUNET_free(d.fis);
	if(ret == GNUNET_SYSERR)
	{
		GNUNET_GE_LOG(ectx, GNUNET_GE_BULK | GNUNET_GE_DEVELOPER | GNUNET_GE_ERROR,
			"%s: GNUNET_ECRS_directory_create failed\n",
			__FUNCTION__);
		return -1;
	}

	/* Write the GNUnet directory out to a file and upload it */
	fd = mkstemp(filename);
	if(fd == -1)
	{
		GNUNET_GE_LOG_STRERROR_FILE(ectx, GNUNET_GE_BULK | GNUNET_GE_DEVELOPER
			| GNUNET_GE_ERROR, "mkstemp", filename);
		return -1;
	}
	write(fd, buf, len);
	GNUNET_GE_LOG(ectx, GNUNET_GE_BULK | GNUNET_GE_DEVELOPER | GNUNET_GE_DEBUG,
		"%s: wrote to %lld bytes to '%s'\n", __FUNCTION__, len,
		filename);
	ret = GNUNET_ECRS_file_upload(ectx, cfg, filename, GNUNET_NO, anonymity, priority,
		-1, upcb, NULL, tt, NULL, &uri);
	close(fd);
	unlink(filename);
	if(ret == GNUNET_SYSERR)
	{
		GNUNET_GE_LOG(ectx, GNUNET_GE_BULK | GNUNET_GE_DEVELOPER | GNUNET_GE_ERROR,
			"%s: GNUNET_ECRS_file_upload failed\n", __FUNCTION__);
		return -1;
	}

	GNUNET_GE_LOG(ectx, GNUNET_GE_BULK | GNUNET_GE_DEVELOPER | GNUNET_GE_DEBUG,
		"%s: done\n", __FUNCTION__);
	/* Update the dirent info with our new URI and mark it clean */
	if(de->de_fi.uri != NULL)
		GNUNET_ECRS_uri_destroy(de->de_fi.uri);
	de->de_fi.uri = uri;
	de->de_dirty = 0;
	return 0;
}
