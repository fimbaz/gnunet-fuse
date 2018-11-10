/*
  This file is part of gnunet-fuse.
  Copyright (C) 2012 GNUnet e.V.

  gnunet-fuse is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published
  by the Free Software Foundation; either version 3, or (at your
  option) any later version.

  gnunet-fuse is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA

*/

/**
 * @file fuse/gnunet-fuse.c
 * @brief fuse tool
 * @author Christian Grothoff
 * @author Mauricio Günther
 */
#include "gnunet-fuse.h"
#include "gfs_download.h"

/**
 * Anonymity level to use.
 */
unsigned int anonymity_level;

/**
 * Configuration to use.
 */
const struct GNUNET_CONFIGURATION_Handle *cfg;

/**
 * Return code from 'main' (0 on success).
 */
static int ret;

/**
 * Flag to determine if we should run in single-threaded mode.
 */
static int single_threaded;

/**
 * Mounted URI (as string).
 */
static char *source;

/**
 * Mount point.
 */
static char *directory;

/**
 * Root of the file tree.
 */
static struct GNUNET_FUSE_PathInfo *root;


/**
 * Function used to process entries in a directory; adds the
 * respective entry to the parent directory.
 *
 * @param cls closure with the 'struct GNUNET_FUSE_PathInfo' of the parent
 * @param filename name of the file in the directory
 * @param uri URI of the file
 * @param metadata metadata for the file; metadata for
 *        the directory if everything else is NULL/zero
 * @param length length of the available data for the file
 *           (of type size_t since data must certainly fit
 *            into memory; if files are larger than size_t
 *            permits, then they will certainly not be
 *            embedded with the directory itself).
 * @param data data available for the file (length bytes)
 */
static void
process_directory_entry (void *cls,
			 const char *filename,
			 const struct GNUNET_FS_Uri *
			 uri,
			 const struct
			 GNUNET_CONTAINER_MetaData *
			 meta, size_t length,
			 const void *data)
{
  struct GNUNET_FUSE_PathInfo *parent = cls;
  struct GNUNET_FUSE_PathInfo *pi;
  int is_directory;

  if (NULL == filename)
    return; /* info about the directory itself */
  GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
	      "Adding file `%s' to directory `%s'\n",
	      filename,
	      parent->filename);
  is_directory = GNUNET_FS_meta_data_test_for_directory (meta);
  if (GNUNET_SYSERR == is_directory)
    is_directory = GNUNET_NO; /* if in doubt, say no */
  pi = GNUNET_FUSE_path_info_create (parent, filename, uri, is_directory);
  GNUNET_FUSE_path_info_done (pi);
}


/**
 * Load and parse a directory.
 *
 * @param pi path to the directory
 * @param eno where to store 'errno' on errors
 * @return GNUNET_OK on success
 */
int
GNUNET_FUSE_load_directory (struct GNUNET_FUSE_PathInfo *pi,
			    int * eno)
{
  size_t size;
  void *data;
  struct GNUNET_DISK_MapHandle *mh;
  struct GNUNET_DISK_FileHandle *fh;

  /* Need to download directory; store to temporary file */
  GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
	      "Downloading directory `%s'\n",
	      pi->filename);
  pi->tmpfile = GNUNET_DISK_mktemp ("gnunet-fuse-tempfile");
  if (GNUNET_OK != GNUNET_FUSE_download_file (pi,
					      0,
					      GNUNET_FS_uri_chk_get_file_size (pi->uri)))
  {
    UNLINK (pi->tmpfile);
    GNUNET_free (pi->tmpfile);
    pi->tmpfile = NULL;
    *eno = EIO; /* low level IO error */
    return GNUNET_SYSERR;
  }

  size = (size_t) GNUNET_FS_uri_chk_get_file_size (pi->uri);
  fh = GNUNET_DISK_file_open (pi->tmpfile,
			      GNUNET_DISK_OPEN_READ,
			      GNUNET_DISK_PERM_NONE);
  if (NULL == fh)
  {
    *eno = EIO;
    return GNUNET_SYSERR;
  }
  data = GNUNET_DISK_file_map (fh,
			       &mh,
			       GNUNET_DISK_MAP_TYPE_READ,
			       size);
  if (NULL == data)
  {
    GNUNET_assert (GNUNET_OK == GNUNET_DISK_file_close (fh));
    return - ENOMEM;
  }
  *eno = 0;
  if (GNUNET_OK !=
      GNUNET_FS_directory_list_contents (size,
					 data, 0LL,
					 &process_directory_entry,
					 pi))
    *eno = ENOTDIR;
  GNUNET_assert (GNUNET_OK == GNUNET_DISK_file_unmap (mh));
  GNUNET_DISK_file_close (fh);
  if (0 != *eno)
    return GNUNET_SYSERR;
  return GNUNET_OK;
}


/**
 * Obtain an existing path info entry from the global map.
 *
 * @param path path the entry represents
 * @param eno where to store 'errno' on errors
 * @return NULL if no such path entry exists
 */
struct GNUNET_FUSE_PathInfo *
GNUNET_FUSE_path_info_get (const char *path,
			   int *eno)
{
  size_t slen = strlen (path) + 1;
  char buf[slen];
  struct GNUNET_FUSE_PathInfo *pi;
  struct GNUNET_FUSE_PathInfo *pos;
  char *tok;

  memcpy (buf, path, slen);
  pi = root;
  GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
	      "Looking up path `%s'\n",
	      path);
  GNUNET_mutex_lock (pi->lock);
  for (tok = strtok (buf, "/"); NULL != tok; tok = strtok (NULL, "/"))
  {
    GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
		"Searching for token `%s'\n",
		tok);
    if (NULL == pi->tmpfile)
    {
      if (GNUNET_OK != GNUNET_FUSE_load_directory (pi, eno))
      {
	GNUNET_mutex_unlock (pi->lock);
	return NULL;
      }
    }

    pos = pi->child_head;
    while ( (NULL != pos) &&
	    (0 != strcmp (tok,
			  pos->filename)) )
      pos = pos->next;
    if (NULL == pos)
    {
      GNUNET_mutex_unlock (pi->lock);
      *eno = ENOENT;
      GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
		  "No file with name `%s' in directory `%s'\n",
		  tok,
		  pi->filename);
      return NULL;
    }
    GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
		"Descending into directory `%s'\n",
		tok);
    GNUNET_mutex_lock (pos->lock);
    GNUNET_mutex_unlock (pi->lock);
    pi = pos;
  }
  ++pi->rc;
  GNUNET_mutex_unlock (pi->lock);
  return pi;
}


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
			      int is_directory)
{
  struct GNUNET_FUSE_PathInfo *pi;
  size_t len;

  if (NULL != parent)
  {
    GNUNET_mutex_lock (parent->lock);
  }

  pi = GNUNET_new (struct GNUNET_FUSE_PathInfo);
  pi->parent = parent;
  pi->filename = GNUNET_strdup (filename);
  len = strlen (pi->filename);
  if ('/' == pi->filename[len - 1])
    pi->filename[len - 1] = '\0';
  pi->uri = GNUNET_FS_uri_dup (uri);
  pi->lock = GNUNET_mutex_create (GNUNET_YES);
  pi->rc = 1;
  pi->stbuf.st_mode = (S_IRUSR | S_IRGRP | S_IROTH); /* read-only */
  if (GNUNET_YES == is_directory)
  {
    pi->stbuf.st_mode |= S_IFDIR | (S_IXUSR | S_IXGRP | S_IXOTH); /* allow traversal */
  }
  else
  {
    pi->stbuf.st_mode |= S_IFREG; /* regular file */
    pi->stbuf.st_size = (off_t) GNUNET_FS_uri_chk_get_file_size (uri);
  }

  if (NULL != parent)
  {
    GNUNET_CONTAINER_DLL_insert_tail (parent->child_head,
				      parent->child_tail,
				      pi);
    GNUNET_mutex_unlock (parent->lock);
  }
  return pi;
}


/**
 * Reduce the reference counter of a path info entry.
 *
 * @param pi entry to decrement the RC of
 */
void
GNUNET_FUSE_path_info_done (struct GNUNET_FUSE_PathInfo *pi)
{
  if (GNUNET_YES == pi->delete_later)
  {
    (void) GNUNET_FUSE_path_info_delete (pi);
    return;
  }
  GNUNET_mutex_lock (pi->lock);
  --pi->rc;
  GNUNET_mutex_unlock (pi->lock);
}


/**
 * Delete a path info entry from the tree (does not actually
 * remove anything from the file system).  Also decrements the RC.
 *
 * @param pi entry to remove
 * @return - ENOENT if the file was already deleted, 0 on success
 */
int
GNUNET_FUSE_path_info_delete (struct GNUNET_FUSE_PathInfo *pi)
{
  struct GNUNET_FUSE_PathInfo *parent = pi->parent;
  int rc;
  int ret;

  if (NULL != parent)
  {
    ret = 0;
    GNUNET_mutex_lock (parent->lock);
    GNUNET_mutex_lock (pi->lock);
    GNUNET_CONTAINER_DLL_remove (parent->child_head,
				 parent->child_tail,
				 pi);
    pi->parent = NULL;
    GNUNET_mutex_unlock (parent->lock);
  }
  else
  {
    ret = - ENOENT;
    GNUNET_mutex_lock (pi->lock);
  }
  rc = --pi->rc;
  if (0 != rc)
  {
    pi->delete_later = GNUNET_YES;
    GNUNET_mutex_unlock (pi->lock);
  }
  else
  {
    if (NULL != pi->tmpfile)
    {
      GNUNET_break (0 == UNLINK (pi->tmpfile));
      GNUNET_free (pi->tmpfile);
    }
    GNUNET_free (pi->filename);
    GNUNET_FS_uri_destroy (pi->uri);
    GNUNET_mutex_unlock (pi->lock);
    GNUNET_mutex_destroy (pi->lock);
    GNUNET_free (pi);
  }
  return ret;
}


/**
 * Called on each node in the path info tree to clean it up.
 *
 * @param pi path info to clean up
 */
static void
cleanup_path_info (struct GNUNET_FUSE_PathInfo *pi)
{
  struct GNUNET_FUSE_PathInfo *pos;

  while (NULL != (pos = pi->child_head))
    cleanup_path_info (pos);
  ++pi->rc;
  (void) GNUNET_FUSE_path_info_delete (pi);
}


/**
 * Main function that will be run (without the scheduler!)
 *
 * @param cls closure
 * @param args remaining command-line arguments
 * @param cfgfile name of the configuration file used (for saving, can be NULL!)
 * @param c configuration
 */
static void
run (void *cls,
     char *const *args,
     const char *cfgfile, const struct GNUNET_CONFIGURATION_Handle *c)
{
  static struct fuse_operations fops = {
    //  .mkdir = gn_mkdir,
    //  .mknod = gn_mknod,
    //  .release = gn_release,
    //  .rename = gn_rename,
    //  .rmdir = gn_rmdir,
    //  .truncate = gn_truncate,
    //  .unlink = gn_unlink,
    //  .utimens = gn_utimens,
    //  .write = gn_write,
    .getattr = gn_getattr,
    .readdir = gn_readdir,
    .open = gn_open,
    .read = gn_read
  };

  int argc;
  struct GNUNET_FS_Uri *uri;
  char *emsg;
  int eno;

  cfg = c;
  ret = 0;
  if (NULL == source)
    {
      fprintf (stderr, _("`%s' option for URI missing\n"), "-s");
      ret = 1;
      return;
    }
  if (NULL == directory)
    {
      fprintf (stderr, _("`%s' option for mountpoint missing\n"), "-d");
      ret = 2;
      return;
    }

  /* parse source string to uri */
  if (NULL == (uri = GNUNET_FS_uri_parse (source, &emsg)))
    {
      fprintf (stderr, "%s\n", emsg);
      GNUNET_free (emsg);
      ret = 3;
      return;
    }
  if ( (GNUNET_YES != GNUNET_FS_uri_test_chk (uri)) &&
       (GNUNET_YES != GNUNET_FS_uri_test_loc (uri)) )
  {
    fprintf (stderr,
	     _("The given URI is not for a directory and can thus not be mounted\n"));
    ret = 4;
    GNUNET_FS_uri_destroy (uri);
    return;
  }

  root = GNUNET_FUSE_path_info_create (NULL, "/", uri, GNUNET_YES);
  if (GNUNET_OK !=
      GNUNET_FUSE_load_directory (root, &eno))
  {
    fprintf (stderr,
	     _("Failed to mount `%s': %s\n"),
	     source,
	     STRERROR (eno));
    ret = 5;
    cleanup_path_info (root);
    GNUNET_FS_uri_destroy (uri);
    return;
  }

  if (GNUNET_YES == single_threaded)
    argc = 5;
  else
    argc = 2;

  {
    char *a[argc + 1];
    a[0] = "gnunet-fuse";
    a[1] = directory;
    if (GNUNET_YES == single_threaded)
      {
	a[2] = "-s";
	a[3] = "-f";
	a[4] = "-d";
      }
    a[argc] = NULL;
    fuse_main (argc, a, &fops, NULL);
  }
  cleanup_path_info (root);
  GNUNET_FS_uri_destroy (uri);
}


/**
 * The main function for gnunet-fuse.
 *
 * @param argc number of arguments from the command line
 * @param argv command line arguments
 * @return 0 ok, 1 on error
 */
int
main (int argc, char *const *argv)
{
  struct GNUNET_GETOPT_CommandLineOption options[] = {
    GNUNET_GETOPT_option_string ('s',
                                 "source",
                                 "URI",
                                 gettext_noop ("Source you get the URI from"),
                                 &source),
    GNUNET_GETOPT_option_string ('d',
                                 "directory",
                                 "PATH",
                                 gettext_noop ("path to your mountpoint"),
                                 &directory),
    GNUNET_GETOPT_option_flag ('t',
                               "single-threaded",
                               gettext_noop ("run in single-threaded mode"),
                               &single_threaded),
    GNUNET_GETOPT_OPTION_END
  };

  GNUNET_log_setup ("gnunet-fuse",
		    "DEBUG",
		    NULL);
  return (GNUNET_OK ==
	  GNUNET_PROGRAM_run2 (argc,
			       argv,
			       "gnunet-fuse -s URI [-- FUSE-OPTIONS] DIRECTORYNAME",
			       gettext_noop
			       ("fuse"),
			       options,
			       &run,
			       NULL,
			       GNUNET_YES)) ? ret : 1;
}

/* end of gnunet-fuse.c */
