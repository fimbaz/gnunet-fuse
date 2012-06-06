/*
 * dirent.c - stuff for directory entries
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

#include <unistd.h>
#include <glib.h>
#include <string.h>
#include <errno.h>
#include "gnfs.h"

GHashTable *path_hash;
struct GNUNET_Semaphore *path_sema;

/*
 * Reference a dirent, call gn_dirent_put when finished
 */
void gn_dirent_ref(struct dirent *de)
{
	GNUNET_mutex_lock(de->de_refs_mutex);
	de->de_refs++;
	GNUNET_mutex_unlock(de->de_refs_mutex);
}

/*
 * Reference a dirent from the cache, call gn_dirent_put when finished with it
 */
struct dirent *gn_dirent_get(const gchar *path)
{
	struct dirent *de;

	if(GNUNET_semaphore_down(path_sema, GNUNET_YES) == GNUNET_SYSERR)
		return NULL;
	de = g_hash_table_lookup(path_hash, path);
	if(de != NULL)
		gn_dirent_ref(de);
	GNUNET_semaphore_up(path_sema);
	return de;
}

/*
 * Release a reference to a dirent
 */
void gn_dirent_put(struct dirent *de)
{
	GNUNET_mutex_lock(de->de_refs_mutex);
	de->de_refs--;
	if(de->de_refs >= 1)
	{
		GNUNET_mutex_unlock(de->de_refs_mutex);
		return;
	}
	GNUNET_mutex_unlock(de->de_refs_mutex);
	GNUNET_mutex_destroy(de->de_path_mutex);
	GNUNET_free(de->de_path);
	GNUNET_mutex_destroy(de->de_refs_mutex);
	GNUNET_semaphore_destroy(de->de_sema);
	if(de->de_fi.uri != NULL)
		GNUNET_ECRS_uri_destroy(de->de_fi.uri);
	if(de->de_fi.meta != NULL)
		GNUNET_meta_data_destroy(de->de_fi.meta);
	if(de->de_type == DE_DIR)
	{
		if(de->de_cached)
		{
			g_hash_table_destroy(de->de_dir_hash);
		}
	}
	else
	{
		if(de->de_cached)
		{
			close(de->de_fd);
			unlink(de->de_filename);
			GNUNET_free(de->de_filename);
		}
	}
	GNUNET_free(de);
}

char *gn_dirent_path_get(struct dirent *de)
{
	char *ret;

	GNUNET_mutex_lock(de->de_path_mutex);
	ret = GNUNET_strdup(de->de_path);
	GNUNET_mutex_unlock(de->de_path_mutex);
	return ret;
}

/*
 * DON'T call this if the dirent is ref'd by a hash
 */
void gn_dirent_path_set(struct dirent *de, const char *path)
{
	GNUNET_mutex_lock(de->de_path_mutex);
	GNUNET_free(de->de_path);
	de->de_path = GNUNET_strdup(path);
	de->de_basename = strrchr(de->de_path, G_DIR_SEPARATOR) + 1;
	GNUNET_mutex_unlock(de->de_path_mutex);
}

void gn_dirent_cache_init(void)
{
	path_hash = g_hash_table_new_full(g_str_hash, g_str_equal, NULL,
		(GDestroyNotify)gn_dirent_put);
	path_sema = GNUNET_semaphore_create(1);
}

/*
 * Create a new dirent with a reference, path and uri are copied
 */
struct dirent *gn_dirent_new(const gchar *path, struct GNUNET_ECRS_URI *uri,
	struct GNUNET_MetaData *meta, gchar type)
{
	struct dirent *de;

	de = GNUNET_malloc(sizeof(*de));
	de->de_path_mutex = GNUNET_mutex_create(0);
	de->de_path = GNUNET_strdup(path);
	de->de_basename = strrchr(de->de_path, G_DIR_SEPARATOR) + 1;
	de->de_refs_mutex = GNUNET_mutex_create(0);
	de->de_refs = 1;
	de->de_type = type;
	de->de_sema = GNUNET_semaphore_create(1);
	if(uri != NULL)
	{
		de->de_dirty = 0;
		de->de_cached = 0;
		de->de_fi.uri = GNUNET_ECRS_uri_duplicate(uri);
	}
	else
	{
		de->de_dirty = 1;
		de->de_cached = 1;
		if(type == DE_FILE)
		{
			char filename[] = GN_MKSTEMP_FILE;

			de->de_fd = mkstemp(filename);
			de->de_filename = GNUNET_strdup(filename);
		}
		else
		{
			de->de_dir_hash = g_hash_table_new_full(g_str_hash,
				g_str_equal, NULL,
				(GDestroyNotify)gn_dirent_put);
		}
	}
	if(meta == NULL)
		de->de_fi.meta = GNUNET_meta_data_create();
	else
		de->de_fi.meta = GNUNET_meta_data_duplicate(meta);
	return de;
}

/*
 * Add a dirent to the cache
 */
void gn_dirent_cache_insert(struct dirent *de)
{
	/* TODO: Here we need to see if the cache has gotten too big and empty
	 * it.
	 * XXX: But what about diry entries?? */
	if(GNUNET_semaphore_down(path_sema, GNUNET_YES) == GNUNET_SYSERR)
		return;
	GNUNET_mutex_lock(de->de_path_mutex);
	GNUNET_GE_ASSERT(ectx, !g_hash_table_lookup(path_hash, de->de_path));
	g_hash_table_replace(path_hash, de->de_path, de);
	GNUNET_mutex_unlock(de->de_path_mutex);
	gn_dirent_ref(de);
	GNUNET_semaphore_up(path_sema);
}

/*
 * Remove a dirent from the cache
 */
void gn_dirent_cache_remove(struct dirent *de)
{
	if(GNUNET_semaphore_down(path_sema, GNUNET_YES) == GNUNET_SYSERR)
		return;
	/* This is safe because we still hold a ref */
	GNUNET_mutex_lock(de->de_path_mutex);
	g_hash_table_remove(path_hash, de->de_path);
	GNUNET_mutex_unlock(de->de_path_mutex);
	GNUNET_semaphore_up(path_sema);
}

/*
 * Call 'cb' for each element in 'path', treats the empty string as "/"
 */
int gn_path_foreach(const gchar *path, gn_dir_foreach_callback cb, void *data)
{
	struct dirent *de, *next_de;
	size_t len, plen;
	gchar *ppath, *filename;

	/* Start de off at the root */
	de = root_de;
	gn_dirent_ref(de);

	/* Allocate partial path buffer */
	len = strlen(path);
	ppath = GNUNET_malloc(len + 1);
	plen = 0;

	/* Loop through each path element */
	for( ; ; )
	{
		/* Do callback for current element */
		if(cb(de, data))
			break;

		/* Do we have any more work to do? */
		if(plen >= len || path[plen + 1] == '\0'
			|| path[plen + 1] == G_DIR_SEPARATOR)
		{
			break;
		}

		/* Save pointer to ppath end */
		filename = &ppath[plen + 1];

		/* Cat next path component */
		ppath[plen] = G_DIR_SEPARATOR;
		for(plen++; path[plen] != '\0' && path[plen] != G_DIR_SEPARATOR;
			plen++)
		{
			ppath[plen] = path[plen];
		}
		ppath[plen] = '\0';

		/* Look it up in the cache first */
		next_de = gn_dirent_get(ppath);

		/* If we found it then continue */
		if(next_de != NULL)
		{
			gn_dirent_put(de);
			de = next_de;
			continue;
		}

		/* We need to find it by listing its parent directory, de */
		next_de = gn_directory_find(de, filename);

		/* Not found? */
		if(next_de == NULL)
		{
			gn_dirent_put(de);
			de = NULL;
			break;
		}

		/* Continue to the next path element */
		gn_dirent_put(de);
		de = next_de;
	}

	/* Done */
	GNUNET_free(ppath);
	if(de == NULL)
		return -1;
	gn_dirent_put(de);
	return 0;
}

static gboolean dirent_find_callback(struct dirent *de, void *data)
{
	struct dirent **d = data;

	if(*d != NULL)
		gn_dirent_put(*d);
	*d = de;
	gn_dirent_ref(*d);
	return 0;
}

/*
 * Retrieve a dirent with a reference given it's (normalized) path.
 */
struct dirent *gn_dirent_find(const gchar *path)
{
	struct dirent *de = NULL;

	GNUNET_GE_LOG(ectx, GNUNET_GE_BULK | GNUNET_GE_DEVELOPER | GNUNET_GE_DEBUG,
		"%s: called for '%s'\n", __FUNCTION__, path);
	if(gn_path_foreach(path, dirent_find_callback, &de) == -1)
	{
		if(de != NULL)
			gn_dirent_put(de);
		return NULL;
	}
	return de;
}

static gboolean lock_path_callback(struct dirent *de, void *data)
{
	struct dirent **detmp = data;

	if(GNUNET_semaphore_down(de->de_sema, GNUNET_YES) == -1)
		return 1;
	gn_dirent_ref(de);
	*detmp = de;
	return 0;
}

/*
 * Locks each element in a path.
 */
int gn_lock_path(struct dirent *de)
{
	struct dirent *detmp = NULL;
	char *path;

	path = gn_dirent_path_get(de);
	if(gn_path_foreach(path, lock_path_callback, &detmp) == -1)
	{
		GNUNET_free(path);
		GNUNET_GE_LOG(ectx, GNUNET_GE_BULK | GNUNET_GE_DEVELOPER | GNUNET_GE_ERROR,
			"%s: failed!\n", __FUNCTION__);
		/* Back out all the locks we aquired */
		if(detmp != NULL)
			gn_unlock_path(detmp, GN_UNLOCK_CLEAN);
		return -1;
	}
	GNUNET_free(path);
	return 0;
}

struct unlock_path_data
{
	int dirty;
	struct dirent *de;
};

static gboolean unlock_path_callback(struct dirent *de, void *data)
{
	struct unlock_path_data *d = data;

	if(d->dirty == GN_UNLOCK_ALL_DIRTY)
		de->de_dirty = 1;
	else if(d->dirty == GN_UNLOCK_ANCESTORS_DIRTY && de != d->de)
		de->de_dirty = 1;
	GNUNET_semaphore_up(de->de_sema);
	gn_dirent_put(de);
	return 0;
}

/*
 * Un-lock each element in a path and set the dirty state
 */
int gn_unlock_path(struct dirent *de, int dirty)
{
	struct unlock_path_data d;
	char *path;

	d.dirty = dirty;
	d.de = de;
	path = gn_dirent_path_get(de);
	if(gn_path_foreach(path, unlock_path_callback, &d) == -1)
	{
		GNUNET_free(path);
		GNUNET_GE_LOG(ectx, GNUNET_GE_BULK | GNUNET_GE_DEVELOPER | GNUNET_GE_ERROR,
			"%s: failed!\n", __FUNCTION__);
		return -1;
	}
	GNUNET_free(path);
	return 0;
}
