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
 * @file fuse/gfs_download.h
 * @brief download files using FS
 * @author Christian Grothoff
 */
#include "gfs_download.h"


/**
 * Context for a download operation.
 */
struct Context
{

  /**
   * Information about the file we are downloading.
   */
  struct GNUNET_FUSE_PathInfo *path_info;

  /**
   * Download handle.
   */
  struct GNUNET_FS_DownloadContext *dc;

  /**
   * FS handle.
   */
  struct GNUNET_FS_Handle *fs;

  /**
   * Start offset.
   */
  off_t start_offset;

  /**
   * Number of bytes to download.
   */
  uint64_t length;

  /**
   * Return value for the operation, 0 on success.
   */
  int ret;

};


/**
 * Task run when we shut down.
 *
 * @param cls our 'struct Context'
 */
static void
shutdown_task (void *cls)
{
  struct Context *ctx = cls;

  if (NULL != ctx->dc)
  {
    GNUNET_FS_download_stop (ctx->dc, GNUNET_YES);
    ctx->dc = NULL;
  }
  if (NULL != ctx->fs)
  {
    GNUNET_FS_stop (ctx->fs);
    ctx->fs = NULL;
  }
}


/**
 * Function called from FS with progress information.
 *
 * @param cls our 'struct Context'
 * @param info progress information
 * @return NULL
 */
static void *
progress_cb (void *cls, const struct GNUNET_FS_ProgressInfo *info)
{
  struct Context *ctx = cls;
  char *s;

  switch (info->status)
    {
    case GNUNET_FS_STATUS_DOWNLOAD_START:
      GNUNET_break (info->value.download.dc == ctx->dc);
      GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
		  "Started download `%s'.\n",
		  info->value.download.filename);
      break;
    case GNUNET_FS_STATUS_DOWNLOAD_PROGRESS:
      GNUNET_break (info->value.download.dc == ctx->dc);
      GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
		  "Downloading `%s' at %llu/%llu\n",
		  info->value.download.filename,
		  (unsigned long long) info->value.download.completed,
		  (unsigned long long) info->value.download.size);
      break;
    case GNUNET_FS_STATUS_DOWNLOAD_ERROR:
      GNUNET_break (info->value.download.dc == ctx->dc);
      GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
		  "Error downloading: %s.\n",
		  info->value.download.specifics.error.message);
      GNUNET_SCHEDULER_shutdown ();
      break;
    case GNUNET_FS_STATUS_DOWNLOAD_COMPLETED:
      GNUNET_break (info->value.download.dc == ctx->dc);
      s =
	GNUNET_STRINGS_byte_size_fancy (info->value.download.completed *
					1000000LL /
					(info->value.download.
					 duration.rel_value_us + 1));
      GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
		  "Downloading `%s' done (%s/s).\n",
		  info->value.download.filename, s);
      GNUNET_free (s);
      ctx->ret = 0;
      GNUNET_SCHEDULER_shutdown ();
      break;
    case GNUNET_FS_STATUS_DOWNLOAD_STOPPED:
      GNUNET_SCHEDULER_shutdown ();
      break;
    case GNUNET_FS_STATUS_DOWNLOAD_ACTIVE:
    case GNUNET_FS_STATUS_DOWNLOAD_INACTIVE:
      break;
    default:
      GNUNET_log (GNUNET_ERROR_TYPE_ERROR,
		  _("Unexpected status: %d\n"), info->status);
      break;
    }
  return NULL;
}


/**
 * Main task run by the helper process which downloads the file.
 *
 * @param cls 'struct Context' with information about the download
 */
static void
download_task (void *cls)
{
  struct Context *ctx = cls;

  ctx->fs = GNUNET_FS_start (cfg, "gnunet-fuse", &progress_cb, ctx,
			     GNUNET_FS_FLAGS_NONE,
			     GNUNET_FS_OPTIONS_DOWNLOAD_PARALLELISM, 1,
			     GNUNET_FS_OPTIONS_REQUEST_PARALLELISM, 1,
			     GNUNET_FS_OPTIONS_END);
  if (NULL == ctx->fs)
  {
    GNUNET_log (GNUNET_ERROR_TYPE_ERROR, _("Could not initialize `%s' subsystem.\n"), "FS");
    return;
  }
  ctx->dc = GNUNET_FS_download_start (ctx->fs,
				      ctx->path_info->uri, ctx->path_info->meta,
				      ctx->path_info->tmpfile, NULL,
				      (uint64_t) ctx->start_offset,
				      ctx->length,
				      anonymity_level,
				      GNUNET_FS_DOWNLOAD_OPTION_NONE,
				      NULL, NULL);
  if (NULL == ctx->dc)
  {
    GNUNET_FS_stop (ctx->fs);
    ctx->fs = NULL;
    return;
  }
  GNUNET_SCHEDULER_add_shutdown (&shutdown_task, ctx);
}


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
			   uint64_t length)
{
  struct Context ctx;
  pid_t pid;
  int status;
  int ret;

  /* lock to prevent two processes from downloading / manipulating the
     same file at the same time */
  GNUNET_mutex_lock (path_info->lock);
  pid = fork ();
  if (-1 == pid)
  {
    GNUNET_log_strerror (GNUNET_ERROR_TYPE_ERROR, "fork");
    GNUNET_mutex_unlock (path_info->lock);
    return GNUNET_SYSERR;
  }
  if (0 != pid)
  {
    while ( (-1 == (ret = waitpid (pid, &status, 0))) &&
	    (EINTR == errno) ) ;
    if (pid != ret)
    {
      GNUNET_log_strerror (GNUNET_ERROR_TYPE_ERROR, "waitpid");
      (void) kill (pid, SIGKILL);
      (void) waitpid (pid, &status, 0);
      GNUNET_mutex_unlock (path_info->lock);
      return GNUNET_SYSERR;
    }
    GNUNET_mutex_unlock (path_info->lock);
    if ( (WIFEXITED (status)) &&
	 (0 == WEXITSTATUS (status)) )
      return GNUNET_OK;
    return GNUNET_SYSERR;
  }
  memset (&ctx, 0, sizeof (ctx));
  ctx.ret = 1;
  ctx.path_info = path_info;
  ctx.start_offset = start_offset;
  ctx.length = length;
  GNUNET_SCHEDULER_run (&download_task, &ctx);
  _exit (ctx.ret);
}

/* end of gfs_download.c */
