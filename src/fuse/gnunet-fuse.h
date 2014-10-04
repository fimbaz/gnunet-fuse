/*
  This file is part of gnunet-fuse.
  (C) 2012 Christian Grothoff (and other contributing authors)

  gnunet-fuse is free software; you can redistribute it and/or
  modify if under the terms of version 2 of the GNU General Public License
  as published by the Free Software Foundation.

  gnunet-fuse is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

*/
/**
 * @file fuse/gnunet-fuse.h
 * @brief global definitions for gnunet-fuse
 * @author Christian Grothoff
 * @author Mauricio Günther
 */
#ifndef GNUNET_FUSE_H
#define GNUNET_FUSE_H

#include <gnunet/platform.h>
#include <gnunet/gnunet_util_lib.h>
#include <gnunet/gnunet_resolver_service.h>
#include <gnunet/gnunet_fs_service.h>

#define FUSE_USE_VERSION 26
#include <fuse.h>
#include "mutex.h"


/**
 * Anonymity level to use.
 */
extern unsigned int anonymity_level;

/**
 * Configuration to use.
 */
extern const struct GNUNET_CONFIGURATION_Handle *cfg;


/**
 * struct containing mapped Path, with URI and other Information like Attributes etc.
 */
struct GNUNET_FUSE_PathInfo
{

  /**
   * All files in a directory are kept in a DLL.
   */
  struct GNUNET_FUSE_PathInfo *next;

  /**
   * All files in a directory are kept in a DLL.
   */
  struct GNUNET_FUSE_PathInfo *prev;

  /**
   * Parent directory, NULL for the root.
   */
  struct GNUNET_FUSE_PathInfo *parent;

  /**
   * Head of linked list of entries in this directory
   * (NULL if this is a file).
   */
  struct GNUNET_FUSE_PathInfo *child_head;

  /**
   * Head of linked list of entries in this directory
   * (NULL if this is a file).
   */
  struct GNUNET_FUSE_PathInfo *child_tail;

  /**
   * URI of the file or directory.
   */
  struct GNUNET_FS_Uri *uri;

  /**
   * meta data to corresponding path (can be NULL)
   */
  struct GNUNET_CONTAINER_MetaData *meta;

  /**
   * Name of the file for this path (i.e. "home").  '/' for the root (all other
   * filenames must not contain '/')
   */
  char *filename;

  /**
   * Name of temporary file, NULL if we never accessed this file or directory.
   */
  char *tmpfile;

  /**
   * file attributes
   */
  struct stat stbuf;

  /**
   * Lock for exclusive access to this struct (i.e. for downloading blocks).
   * Lock order: always lock parents before children.
   */
  struct GNUNET_Mutex *lock;

  /**
   * Beginning of a contiguous range of blocks of the file what we
   * have downloaded already to 'tmpfile'.
   */
  uint64_t download_start;

  /**
   * End of a contiguous range of blocks of the file what we
   * have downloaded already to 'tmpfile'.
   */
  uint64_t download_end;

  /**
   * Reference counter (used if the file is deleted while being opened, etc.)
   */
  unsigned int rc;

  /**
   * Should the file be deleted after the RC hits zero?
   */
  int delete_later;
};


/**
 * Create a new path info entry in the global map.
 *
 * @param parent parent directory (can be NULL)
 * @param filename name of the file to create
 * @param uri URI to use for the path
 * @param is_directory GNUNET_YES if this entry is for a directory
 * @return existing path entry if one already exists, otherwise
 *         new path entry with the desired URI; in both cases
 *         the reference counter has been incremented by 1
 */
struct GNUNET_FUSE_PathInfo *
GNUNET_FUSE_path_info_create (struct GNUNET_FUSE_PathInfo *parent,
			      const char *filename,
			      const struct GNUNET_FS_Uri *uri,
			      int is_directory);


/**
 * Obtain an existing path info entry from the global map.
 *
 * @param path path the entry represents
 * @param eno where to store 'errno' on errors
 * @return NULL if no such path entry exists, otherwise
 *  an entry with incremented reference counter (!)
 */
struct GNUNET_FUSE_PathInfo *
GNUNET_FUSE_path_info_get (const char *path,
			   int *eno);


/**
 * Reduce the reference counter of a path info entry.
 *
 * @param pi entry to decrement the RC of
 */
void
GNUNET_FUSE_path_info_done (struct GNUNET_FUSE_PathInfo *pi);


/**
 * Delete a path info entry from the global map (does not actually
 * remove anything from the file system).  Also decrements the RC.
 *
 * @param pi entry to remove
 * @return - ENOENT if the file was already deleted, 0 on success
 */
int
GNUNET_FUSE_path_info_delete (struct GNUNET_FUSE_PathInfo *pi);


/**
 * Load and parse a directory.
 *
 * @param pi path to the directory
 * @param eno where to store 'errno' on errors
 * @return GNUNET_OK on success
 */
int
GNUNET_FUSE_load_directory (struct GNUNET_FUSE_PathInfo *pi,
			    int * eno);


/* FUSE function files */
int gn_getattr(const char *path, struct stat *stbuf);

int gn_open(const char *path, struct fuse_file_info *fi);

int gn_read(const char *path, char *buf, size_t size, off_t offset,
	struct fuse_file_info *fi);

int gn_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
	off_t offset, struct fuse_file_info *fi);


int gn_mknod(const char *path, mode_t mode, dev_t rdev);

int gn_mkdir(const char *path, mode_t mode);

int gn_unlink(const char *path);

int gn_rmdir(const char *path);

int gn_rename(const char *from, const char *to);

int gn_truncate(const char *path, off_t size);

int gn_write(const char *path, const char *buf, size_t size, off_t offset,
	struct fuse_file_info *fi);

int gn_release(const char *path, struct fuse_file_info *fi);

int gn_utimens(const char *path, const struct timespec ts[2]);


#endif
/* GNUNET_FUSE_H */
