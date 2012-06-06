/*
  This file is part of gnunet-fuse.
  (C) 2012 Christian Grothoff (and other contributing authors)
  
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
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

*/
/*
 * read.c - FUSE read function
 *
 *  Created on: Mar 14, 2012
 *      Author: mg
 *
 *		 Read data from an open file
 *
 * Read should return exactly the number of bytes requested except
 * on EOF or error, otherwise the rest of the data will be
 * substituted with zeroes.	 An exception to this is when the
 * 'direct_io' mount option is specified, in which case the return
 * value of the read system call will reflect the return value of
 * this operation.
 *
 * Changed in version 2.2
 */

/**
 * @file fuse/read.c
 * @brief reading files
 * @author Christian Grothoff
 */
#include "gnunet-fuse.h"
#include "gfs_download.h"



int
gn_read (const char *path, char *buf, size_t size, off_t offset,
	 struct fuse_file_info *fi)
{
  struct GNUNET_FUSE_PathInfo *path_info;
  uint64_t fsize;
  struct GNUNET_DISK_FileHandle *fh;
  int eno;

  path_info = GNUNET_FUSE_path_info_get (path, &eno);
  if (NULL == path_info)
    return - eno;
  fsize = GNUNET_FS_uri_chk_get_file_size (path_info->uri);
  if (offset > fsize)
  {
    GNUNET_log (GNUNET_ERROR_TYPE_DEBUG, 
		"No data available at offset %llu of file `%s'\n",
		(unsigned long long) offset,
		path);
    return 0; 
  }
  if (offset + size > fsize)
    size = fsize - offset;
  if (NULL == path_info->tmpfile)
  {
    /* store to temporary file */
    path_info->tmpfile = GNUNET_DISK_mktemp ("gnunet-fuse-tempfile");
    if (GNUNET_OK != GNUNET_FUSE_download_file (path_info, 
						offset,
						size))
    {
      UNLINK (path_info->tmpfile);
      GNUNET_free (path_info->tmpfile);
      path_info->tmpfile = NULL;
      GNUNET_FUSE_path_info_done (path_info);
      return - EIO; /* low level IO error */
    }
  } 
  else
  {
    if ( (offset < path_info->download_start) ||
	 (size + offset > path_info->download_end) )
    {
      /* need to download some more... */
      if (GNUNET_OK != GNUNET_FUSE_download_file (path_info, 
						  offset,
						  size))
      {
	UNLINK (path_info->tmpfile);
	GNUNET_free (path_info->tmpfile);
	path_info->tmpfile = NULL;
	GNUNET_FUSE_path_info_done (path_info);
	return - EIO; /* low level IO error */
      }
    }
  }
  /* combine ranges */
  if (path_info->download_start == path_info->download_end)
  {
    /* first range */
    path_info->download_start = offset;
    path_info->download_end = offset + size;
  }
  else
  {
    /* only combine ranges if the resulting range would
       be contiguous... */
    if ( (offset >= path_info->download_start) &&
	 (offset <= path_info->download_end) &&
	 (offset + size > path_info->download_end) )
      path_info->download_end = offset + size;
    if ( (offset + size >= path_info->download_start) &&
	 (offset + size <= path_info->download_end) &&
	 (offset < path_info->download_start) )
      path_info->download_start = offset;
  }


  fh = GNUNET_DISK_file_open (path_info->tmpfile,
			      GNUNET_DISK_OPEN_READ,
			      GNUNET_DISK_PERM_NONE);			      
  if (NULL == fh)
  {
    GNUNET_FUSE_path_info_done (path_info);
    return - EBADF;
  }
  GNUNET_log (GNUNET_ERROR_TYPE_DEBUG, 
	      "Trying to read bytes %llu-%llu/%llu of file `%s'\n",
	      (unsigned long long) offset,
	      (unsigned long long) offset + size,
	      (unsigned long long) fsize,
	      path);	      
  if (offset != GNUNET_DISK_file_seek (fh, offset, GNUNET_DISK_SEEK_SET))
  {
    GNUNET_log (GNUNET_ERROR_TYPE_DEBUG, 
		"No data available at offset %llu of file `%s'\n",
		(unsigned long long) offset,
		path);
    GNUNET_DISK_file_close (fh);
    GNUNET_FUSE_path_info_done (path_info);
    return 0; 
  }
  size = GNUNET_MIN (size, fsize - offset);
  if (GNUNET_SYSERR == (size = GNUNET_DISK_file_read (fh, buf, size)))
  {
    int eno = errno;
    GNUNET_log (GNUNET_ERROR_TYPE_DEBUG, 
		"Error reading from file `%s': %s\n",
		path,
		STRERROR (errno));
    GNUNET_DISK_file_close (fh);
    GNUNET_FUSE_path_info_done (path_info);
    return - eno; 
  }
  GNUNET_DISK_file_close (fh);
  GNUNET_FUSE_path_info_done (path_info);
  return size;
}

/* end of read.c */

