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
/**
 * @file fuse/gfs_download.h
 * @brief download files using FS
 * @author Christian Grothoff
 */
#ifndef GFS_DOWNLOAD_H
#define GFS_DOWNLOAD_H

#include "gnunet-fuse.h"

/**
 * Download a file.  Blocks until we're done.
 *
 * @param path_info information about the file to download
 * @param start_offset offset of the first byte to download
 * @param length number of bytes to download from 'start_offset'
 * @return GNUNET_OK on success
 */
int
GNUNET_FUSE_download_file (struct GNUNET_FUSE_PathInfo *path_info,
			   off_t start_offset,
			   uint64_t length);

#endif
