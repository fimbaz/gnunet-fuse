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
 * getattr.c - FUSE getattr function
 *
 *  Created on: Mar 14, 2012
 *      Author: mg
 *
 *
 *	 Get file attributes.
 *
 * Similar to stat().  The 'st_dev' and 'st_blksize' fields are
 * ignored.	 The 'st_ino' field is ignored except if the 'use_ino'
 * mount option is given.
 */
/**
 * @file fuse/getattr.c
 * @brief 'stat' for fuse files
 * @author Christian Grothoff
 */

#include "gnunet-fuse.h"
#include "gfs_download.h"

int
gn_getattr (const char *path, struct stat *stbuf)
{
  struct GNUNET_FUSE_PathInfo *pi;
  int eno;

  pi = GNUNET_FUSE_path_info_get (path, &eno);
  if (NULL == pi)
    return - eno;
  *stbuf = pi->stbuf;
  GNUNET_FUSE_path_info_done (pi);
  return 0;
}

/* end of getattr.c */

