/*
 * main.c - program start
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

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <glib.h>
#include <fuse.h>
#include <GNUnet/gnunet_directories.h>
#include <GNUnet/gnunet_util.h>
#include <GNUnet/gnunet_ecrs_lib.h>
#include "gnfs.h"
#include "gettext.h"

struct GNUNET_GC_Configuration *cfg;
struct GNUNET_GE_Context *ectx;
static char *cfgFilename = GNUNET_DEFAULT_CLIENT_CONFIG_FILE;
static char *cfgLogfile = "/tmp/gnunet_fuse.log";

/* Flag to indicate that we are shutting down */
int closing = 0;

/* Level of anonymity for downloading and uploading files */
unsigned int anonymity = 1;

/* Priority for uploaded files */
unsigned int priority = 1000;

/* Flag for including .uri files in readdir() */
int uri_files = 0;

/* argv and argc to pass to fuse, filled in by main and getopt_configure_argv */
char **fuse_argv;
int fuse_argc;

/* Root directory entry, currently used by the dirent cache when asked for / */
int root_fd;
struct dirent *root_de;

int getopt_configure_argv(GNUNET_CommandLineProcessorContext *ctx, void *scls,
	const char *cmdLineOption, const char *value)
{
	(void)ctx;
	(void)scls;
	(void)cmdLineOption;

	fuse_argv[fuse_argc] = (char *)value;
	fuse_argc++;
	fuse_argv[fuse_argc] = NULL;
	return GNUNET_OK;
}

static struct fuse_operations fops =
{
	.getattr = gn_getattr,
	.mknod = gn_mknod,
	.mkdir = gn_mkdir,
	.unlink = gn_unlink,
	.rmdir = gn_rmdir,
	.rename = gn_rename,
	.truncate = gn_truncate,
	.open = gn_open,
	.read = gn_read,
	.write = gn_write,
	.release = gn_release,
	.readdir = gn_readdir,
	.utimens = gn_utimens,
};

static struct GNUNET_CommandLineOption gn_options[] =
{
        GNUNET_COMMAND_LINE_OPTION_HELP("GNUnet filesystem"),
	GNUNET_COMMAND_LINE_OPTION_CFG_FILE(&cfgFilename), /* -c */
	GNUNET_COMMAND_LINE_OPTION_LOGGING, /* -L */
	{ 'l', "logfile", "FILE", "set logfile name", 1,
		&GNUNET_getopt_configure_set_string, &cfgLogfile },
	{ 'a', "anonymity", "LEVEL",
		"set the desired LEVEL of sender-anonymity", 1,
		&GNUNET_getopt_configure_set_uint, &anonymity },
	{ 'p', "priority", "LEVEL",
		"set the desired LEVEL of priority", 1,
		&GNUNET_getopt_configure_set_uint, &priority },
	{ 'u', "uri-files", NULL, "Make .uri files visible", 0,
		&GNUNET_getopt_configure_set_one, &uri_files },
	{ 'x', "Xfuse", NULL, "Escape fuse option", 1,
		&getopt_configure_argv, NULL },
	GNUNET_COMMAND_LINE_OPTION_END,
};

int main(int argc, char **argv)
{
	int i, ret;
	struct GNUNET_ECRS_URI *uri;
	char *buf;

	/* Initialize fuse options */
	fuse_argc = 1;
	fuse_argv = GNUNET_malloc(sizeof(char *) * argc);
	fuse_argv[0] = argv[0];
	fuse_argv[1] = NULL;

	/* Parse gnunet options */
	i = GNUNET_init(argc, argv,
		"gnunet-fuse [OPTIONS] <URI FILE> <MOUNT-POINT>",
		&cfgFilename, gn_options, &ectx, &cfg);
	if(i == -1)
	{
		ret = -1;
		goto quit;
	}

	/* Set up log file */
	GNUNET_disk_directory_create_for_file(ectx, cfgLogfile);
	ectx = GNUNET_GE_create_context_logfile(ectx, GNUNET_GE_ALL, cfgLogfile, NULL, GNUNET_YES, 0);
	GNUNET_GE_setDefaultContext(ectx);

	/* There should be exactly two extra arguments */
	if(i + 2 != argc)
	{
		printf("You must specify a URI to mount and mountpoint\n");
		ret = -1;
		goto quit;
	}

	/* Set URI as our root directory entry */
	gn_dirent_cache_init();
	if(GNUNET_disk_file_test(ectx, argv[i]) == GNUNET_YES)
	{
	        unsigned long long len;
		char *uribuf;

		root_fd = GNUNET_disk_file_open(ectx, argv[i], O_RDWR | O_SYNC);
		if(root_fd == -1)
		{
			printf("Unable to open URI file: %s\n", argv[i]);
			ret = -1;
			goto quit;
		}
		if(GNUNET_disk_file_size(ectx, argv[i], &len, GNUNET_YES) == GNUNET_SYSERR)
		{
			printf("Unable to determine URI file size\n");
			ret = -1;
			goto out_close_root;
		}
		uribuf = GNUNET_malloc(len + 1);
		read(root_fd, uribuf, len);
		uribuf[len] = '\0';
		uri = GNUNET_ECRS_string_to_uri(ectx, uribuf);
		GNUNET_free(uribuf);
		if(uri == NULL)
		{
			printf("URI cannot be parsed\n");
			ret = -1;
			goto out_close_root;
		}
		if(!GNUNET_ECRS_uri_test_chk(uri))
		{
			struct GNUNET_ECRS_URI *new_uri;

			new_uri = GNUNET_ECRS_uri_get_content_uri_from_loc(uri);
			if(new_uri == NULL)
			{
				printf("URI cannot be mounted\n");
				ret = -1;
				goto out_close_root;
			}
			GNUNET_ECRS_uri_destroy(uri);
			uri = new_uri;
		}
		root_de = gn_dirent_new(G_DIR_SEPARATOR_S, uri, NULL, DE_DIR);
		GNUNET_ECRS_uri_destroy(uri);
	}
	else
	{
		/* In the case where the file does not exist, let's mount an
		 * empty directory and create the file to store its URI */
	        root_fd = GNUNET_disk_file_open(ectx, argv[i], O_RDWR | O_SYNC
			| O_CREAT, 0666);
		if(root_fd == -1)
		{
			printf("Unable to create URI file: %s\n", argv[i]);
			ret = -1;
			goto quit;
		}
		root_de = gn_dirent_new(G_DIR_SEPARATOR_S, NULL, NULL, DE_DIR);
	}

	/* Add mount point as the last fuse option */
	fuse_argv = GNUNET_realloc(fuse_argv, sizeof(char *) * (fuse_argc + 2));
	fuse_argv[fuse_argc] = argv[i + 1];
	fuse_argc++;
	fuse_argv[fuse_argc] = NULL;

	GNUNET_GE_LOG(ectx, GNUNET_GE_BULK | GNUNET_GE_USER | GNUNET_GE_DEBUG, "calling fuse_main\n");
	ret = fuse_main(fuse_argc, fuse_argv, &fops, NULL);
	GNUNET_GE_LOG(ectx, GNUNET_GE_BULK | GNUNET_GE_USER | GNUNET_GE_DEBUG, "fuse_main returned\n");

	/* Save root uri */
	closing = 1;
	buf = gn_get_special_file(G_DIR_SEPARATOR_S URI_FILE);
	if(buf != NULL)
	{
		ftruncate(root_fd, 0);
		lseek(root_fd, SEEK_SET, 0);
		write(root_fd, buf, strlen(buf));
		GNUNET_free(buf);
	}
out_close_root:
	GNUNET_disk_file_close(ectx, argv[i], root_fd);
quit:
	GNUNET_free(fuse_argv);
	GNUNET_fini(ectx, cfg);
	return ret;
}
