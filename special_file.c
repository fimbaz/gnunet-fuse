/*
 * special_file.c - special file support (like .uri)
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

#include <string.h>
#include <GNUnet/gnunet_util_string.h>
#include <GNUnet/gnunet_ecrs_lib.h>
#include "gnfs.h"

char *gn_dirname(const char *path, char **file)
{
	char *parent, *slash;

	parent = GNUNET_strdup(path);
	slash = strrchr(parent, G_DIR_SEPARATOR);
	if(slash != NULL)
	{
		slash[0] = '\0';
		slash++;
	}
	if(file != NULL)
		*file = slash;
	return parent;
}

/* Checks to see if path is the path to a special file */
int gn_exists_special_file(const char *path)
{
	struct dirent *de;
	char *file, *parent;
	int ret = 0;

	parent = gn_dirname(path, &file);

	/* Check for special file name */
	if(strcmp(file, URI_FILE) == 0)
	{
		ret = 1;
	}
	else if(strncmp(file, URI_FILE ".", URI_LEN + 1) == 0)
	{
		char *actual_file = GNUNET_malloc(strlen(path));

		/* Return URI of the file named after the .uri. */
		sprintf(actual_file, "%s" G_DIR_SEPARATOR_S "%s", parent,
			&file[URI_LEN + 1]);
		de = gn_dirent_find(actual_file);
		GNUNET_free(actual_file);
		if(de == NULL)
			goto out;
		gn_dirent_put(de);
		ret = 1;
	}
out:
	GNUNET_free(parent);
	return ret;
}

/*
 * Returns a malloc'd string for a special file, and in the case of .uri files
 * will sync it if it's dirty
 */
char *gn_get_special_file(const char *path)
{
	struct dirent *de;
	char *buf = NULL, *file, *parent;

	parent = gn_dirname(path, &file);

	/* Check for special file name */
	if(strcmp(file, URI_FILE) == 0)
	{
		/* Return URI of the 'current' directory */
		de = gn_dirent_find(parent);
		if(de == NULL)
			goto out;
		if(GNUNET_semaphore_down(de->de_sema, GNUNET_YES) == GNUNET_SYSERR)
		{
			gn_dirent_put(de);
			goto out;
		}
		if(de->de_dirty)
		{
			if(gn_directory_upload_locked(de) == -1)
			{
				GNUNET_semaphore_up(de->de_sema);
				gn_dirent_put(de);
				goto out;
			}
		}
		buf = GNUNET_ECRS_uri_to_string(de->de_fi.uri);
		GNUNET_semaphore_up(de->de_sema);
		gn_dirent_put(de);
		buf = GNUNET_realloc(buf, strlen(buf) + 2);
		strcat(buf, "\n");
	}
	else if(strncmp(file, URI_FILE ".", URI_LEN + 1) == 0)
	{
		char *actual_file = GNUNET_malloc(strlen(path));

		/* Return URI of the file named after the .uri. */
		sprintf(actual_file, "%s" G_DIR_SEPARATOR_S "%s", parent,
			&file[URI_LEN + 1]);
		de = gn_dirent_find(actual_file);
		GNUNET_free(actual_file);
		if(de == NULL)
			goto out;
		if(GNUNET_semaphore_down(de->de_sema, GNUNET_YES) == GNUNET_SYSERR)
		{
			gn_dirent_put(de);
			goto out;
		}
		if(de->de_dirty)
		{
			if(de->de_type == DE_DIR)
			{
				if(gn_directory_upload_locked(de) == -1)
				{
					GNUNET_semaphore_up(de->de_sema);
					gn_dirent_put(de);
					goto out;
				}
			}
			else
			{
				if(de->de_fi.uri == NULL)
				{
					GNUNET_semaphore_up(de->de_sema);
					gn_dirent_put(de);
					goto out;
				}
			}
		}
		buf = GNUNET_ECRS_uri_to_string(de->de_fi.uri);
		GNUNET_semaphore_up(de->de_sema);
		gn_dirent_put(de);
		buf = GNUNET_realloc(buf, strlen(buf) + 2);
		strcat(buf, "\n");
	}
out:
	GNUNET_free(parent);
	return buf;
}
