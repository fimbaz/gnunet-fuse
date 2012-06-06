/*
 * gnfs.h - types and stuff
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

#ifndef _GNFS_H_
#define _GNFS_H_

#include <sys/types.h>
#include <sys/stat.h>
#include <glib.h>
#include <fuse.h>
#include <GNUnet/gnunet_util_boot.h>
#include <GNUnet/gnunet_ecrs_lib.h>

/* Some gnunet macros need these */
#define _(x) x
#define STRERROR strerror

#define URI_FILE	".uri"
#define URI_LEN		4
#define GN_MKSTEMP_FILE	"/tmp/gnfs.XXXXXX"
#define GN_EMPTY_FILE_URI "gnunet://ecrs/chk/00000000000000000000000000000000" \
	"00000000000000000000000000000000000000000000000000000000000000000000" \
	"000.0000000000000000000000000000000000000000000000000000000000000000" \
	"000000000000000000000000000000000000000.0"

struct dirent
{
	/* de_path_mutex protects de_path and de_basename */
	struct GNUNET_Mutex *de_path_mutex;
	gchar *de_path;
	gchar *de_basename;
	/* de_refs_mutex protects de_refs */
	struct GNUNET_Mutex *de_refs_mutex;
	gint de_refs;
	gchar de_type;
#define DE_FILE	'f'
#define DE_DIR 'd'
	/* de_sema protects everything below */
	struct GNUNET_Semaphore *de_sema;
	/* Cached entries have their entire contents in memory or on disk */
	gboolean de_cached;
	/* Dirty entires have been changed and not published in GNUnet (implies
	 * cached) */
	gboolean de_dirty;
	GNUNET_ECRS_FileInfo de_fi;
	union
	{
		/* For cached directories */
		GHashTable *de_dir_hash;
		/* For cached files */
		struct
		{
			gint de_fd;
			gchar *de_filename;
		};
	};
};

typedef gboolean (*gn_dir_foreach_callback)(struct dirent *de, void *data);

extern struct GNUNET_GC_Configuration *cfg;
extern struct GNUNET_GE_Context *ectx;
extern int closing;
extern unsigned int anonymity;
extern unsigned int priority;
extern int uri_files;
extern struct dirent *root_de;

/* dirent.c */
struct dirent *gn_dirent_new(const gchar *path, struct GNUNET_ECRS_URI *uri,
	struct GNUNET_MetaData *meta, gchar type);
struct dirent *gn_dirent_get(const gchar *path);
void gn_dirent_ref(struct dirent *de);
void gn_dirent_put(struct dirent *de);
char *gn_dirent_path_get(struct dirent *de);
void gn_dirent_path_set(struct dirent *de, const char *path);
void gn_dirent_cache_init(void);
void gn_dirent_cache_insert(struct dirent *de);
void gn_dirent_cache_remove(struct dirent *de);
struct dirent *gn_dirent_find(const gchar *path);
int gn_lock_path(struct dirent *de);
int gn_unlock_path(struct dirent *de, int dirty);
#define GN_UNLOCK_CLEAN			0
#define GN_UNLOCK_ALL_DIRTY		1
#define GN_UNLOCK_ANCESTORS_DIRTY	2

/* directory.c */
int gn_directory_foreach(struct dirent *de, gn_dir_foreach_callback cb,
        void *data);
struct dirent *gn_directory_find(struct dirent *de, const gchar *filename);
int gn_directory_insert(struct dirent *de, struct dirent *dechild);
int gn_directory_remove(struct dirent *de, struct dirent *dechild);
int gn_directory_upload_locked(struct dirent *de);

/* file.c */
int gn_file_download_locked(struct dirent *de);
int gn_file_upload_locked(struct dirent *de);

/* FUSE function files */
int gn_getattr(const char *path, struct stat *stbuf);
int gn_mknod(const char *path, mode_t mode, dev_t rdev);
int gn_mkdir(const char *path, mode_t mode);
int gn_unlink(const char *path);
int gn_rmdir(const char *path);
int gn_rename(const char *from, const char *to);
int gn_truncate(const char *path, off_t size);
int gn_open(const char *path, struct fuse_file_info *fi);
int gn_read(const char *path, char *buf, size_t size, off_t offset,
	struct fuse_file_info *fi);
int gn_write(const char *path, const char *buf, size_t size, off_t offset,
	struct fuse_file_info *fi);
int gn_release(const char *path, struct fuse_file_info *fi);
int gn_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
	off_t offset, struct fuse_file_info *fi);
int gn_utimens(const char *path, const struct timespec ts[2]);

/* special_file.c */
char *gn_dirname(const char *path, char **file);
int gn_exists_special_file(const char *path);
char *gn_get_special_file(const char *path);

#endif /* _GNFS_H_ */
