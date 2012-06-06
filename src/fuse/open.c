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
 * open.c - FUSE open function
 *
 *  Created on: Mar 14, 2012
 *      Author: mg
 *
 *  File open operation
 *
 * No creation (O_CREAT, O_EXCL) and by default also no
 * truncation (O_TRUNC) flags will be passed to open(). If an
 * application specifies O_TRUNC, fuse first calls truncate()
 * and then open(). Only if 'atomic_o_trunc' has been
 * specified and kernel version is 2.6.24 or later, O_TRUNC is
 * passed on to open.
 *
 * Unless the 'default_permissions' mount option is given,
 * open should check if the operation is permitted for the
 * given flags. Optionally open may also return an arbitrary
 * filehandle in the fuse_file_info structure, which will be
 * passed to all file operations.
 *
 * Changed in version 2.2
 */

/**
 * @file fuse/open.c
 * @brief opening files
 * @author Christian Grothoff
 */
#include "gnunet-fuse.h"
#include "gfs_download.h"


int
gn_open (const char *path, struct fuse_file_info *fi)
{
  struct GNUNET_FUSE_PathInfo *pi;
  int eno;

  pi = GNUNET_FUSE_path_info_get (path, &eno);
  if (NULL == pi)
    return - eno;
  /* NOTE: once we allow writes, we need to keep the RC
     incremented until close... */
  GNUNET_FUSE_path_info_done (pi);
  if (O_RDONLY != (fi->flags & 3))
    return - EACCES;
  return 0;
}

/* end of open.c */
