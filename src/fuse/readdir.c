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
/*
 * readdir.c - FUSE read directory function
 *
 *  Created on: Mar 14, 2012
 *      Author: MG, Christian Grothoff, Matthias Wachs,
 *      		Krista Bennett, James Blackwell, Igor Wronsky
 *
 * Read directory
 *
 * This supersedes the old getdir() interface.  New applications
 * should use this.
 *
 * The filesystem may choose between two modes of operation:
 *
 * 1) The readdir implementation ignores the offset parameter, and
 * passes zero to the filler function's offset.  The filler
 * function will not return '1' (unless an error happens), so the
 * whole directory is read in a single readdir operation.  This
 * works just like the old getdir() method.
 *
 * 2) The readdir implementation keeps track of the offsets of the
 * directory entries.  It uses the offset parameter and always
 * passes non-zero offset to the filler function.  When the buffer
 * is full (or an error happens) the filler function will return
 * '1'.
 *
 * Introduced in version 2.3
 */
/**
 * @file fuse/readdir.c
 * @brief readdir of fuse
 * @author Christian Grothoff
 */
#include "gnunet-fuse.h"
#include "gfs_download.h"


int
gn_readdir (const char *path, void *buf, fuse_fill_dir_t filler,
	    off_t offset, struct fuse_file_info *fi)
{
  struct GNUNET_FUSE_PathInfo *path_info;
  struct GNUNET_FUSE_PathInfo *pos;
  int eno;

  path_info = GNUNET_FUSE_path_info_get (path, &eno);
  if (NULL == path_info)
    return - eno;
  if ( (NULL == path_info->tmpfile) &&
       (GNUNET_OK != GNUNET_FUSE_load_directory (path_info, &eno)) )
    return - eno;
  filler (buf, ".", NULL, 0);
  filler (buf, "..", NULL, 0);
  for (pos = path_info->child_head; NULL != pos; pos = pos->next)
    filler (buf, pos->filename, 
	    &pos->stbuf,
	    0);
  GNUNET_FUSE_path_info_done (path_info);
  return 0;
}

/* end of readdir.c */
