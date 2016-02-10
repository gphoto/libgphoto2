/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* gphoto2-port-usb.c
 *
 * Copyright (c) 2001 Lutz Mueller <lutz@users.sf.net>
 * Copyright (c) 1999-2000 Johannes Erdfelt <johannes@erdfelt.com>
 * Copyright (c) 2005, 2007 Hubert Figuiere <hub@figuiere.net>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, 
 * but WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details. 
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

#include "config.h"
#include <gphoto2/gphoto2-port-library.h>

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#ifdef HAVE_MNTENT_H
# include <mntent.h>
#endif
#ifdef HAVE_SYS_MNTENT_H
# include <sys/mntent.h>
#endif
#ifdef HAVE_SYS_MNTTAB_H
# include <sys/mnttab.h>
#endif

/* on Solaris */
#ifndef HAVE_SETMNTENT
#define setmntent(f,m) fopen(f,m)
#endif
#ifndef HAVE_ENDMNTENT
#define endmntent(f) fclose(f)
#endif

#include <gphoto2/gphoto2-port.h>
#include <gphoto2/gphoto2-port-result.h>
#include <gphoto2/gphoto2-port-log.h>

#ifdef ENABLE_NLS
#  include <libintl.h>
#  undef _
#  define _(String) dgettext (GETTEXT_PACKAGE, String)
#  ifdef gettext_noop
#    define N_(String) gettext_noop (String)
#  else
#    define N_(String) (String)
#  endif
#else
#  define textdomain(String) (String)
#  define gettext(String) (String)
#  define dgettext(Domain,Message) (Message)
#  define dcgettext(Domain,Message,Type) (Message)
#  define bindtextdomain(Domain,Directory) (Domain)
#  define _(String) (String)
#  define N_(String) (String)
#endif

#define CHECK(result) {int r=(result); if (r<0) return (r);}

struct _GPPortPrivateLibrary {
	char *mount_point;
};

GPPortType
gp_port_library_type (void)
{
        return GP_PORT_DISK;
}

int
gp_port_library_list (GPPortInfoList *list)
{
	GPPortInfo info;
# ifdef HAVE_MNTENT_H
	FILE *mnt;
	struct mntent *mntent;
	char	path[1024];
	char	*s;
	struct stat stbuf;

	mnt = setmntent ("/etc/fstab", "r");
	if (mnt) {
		while ((mntent = getmntent (mnt))) {
			/* detect floppies so we don't access them with the stat() below */
			GP_LOG_D ("found fstab fsname %s", mntent->mnt_fsname);

			if ((NULL != strstr(mntent->mnt_fsname,"fd"))	||
			    (NULL != strstr(mntent->mnt_fsname,"floppy")) ||
			    (NULL != strstr(mntent->mnt_fsname,"fuse"))	||
			    (NULL != strstr(mntent->mnt_fsname,"nfs"))	||
			    (NULL != strstr(mntent->mnt_fsname,"cifs"))	||
			    (NULL != strstr(mntent->mnt_fsname,"smbfs"))||
			    (NULL != strstr(mntent->mnt_fsname,"afs"))	||
			    (NULL != strstr(mntent->mnt_fsname,"autofs"))||
                            (NULL != strstr(mntent->mnt_fsname,"cgroup"))||
                            (NULL != strstr(mntent->mnt_fsname,"systemd"))||
                            (NULL != strstr(mntent->mnt_fsname,"mqueue"))||
                            (NULL != strstr(mntent->mnt_fsname,"securityfs"))||
                            (NULL != strstr(mntent->mnt_fsname,"proc"))||
                            (NULL != strstr(mntent->mnt_fsname,"devtmpfs"))||
                            (NULL != strstr(mntent->mnt_fsname,"devpts"))||
                            (NULL != strstr(mntent->mnt_fsname,"sysfs"))||
			    (NULL != strstr(mntent->mnt_fsname,"gphotofs"))||
			/* fstype based */
			    (NULL != strstr(mntent->mnt_type,"autofs"))	||
			    (NULL != strstr(mntent->mnt_type,"nfs"))	||
			    (NULL != strstr(mntent->mnt_type,"smbfs"))||
			    (NULL != strstr(mntent->mnt_type,"proc"))||
			    (NULL != strstr(mntent->mnt_type,"sysfs"))||
			    (NULL != strstr(mntent->mnt_type,"cifs"))||
			    (NULL != strstr(mntent->mnt_type,"afs")) ||
			/* mount options */
				/* x-systemd.automount or similar */
			    (NULL != strstr(mntent->mnt_opts,"automount"))
			) {
				continue;
			}

			/* Whitelist some fuse based filesystems, e.g. to help exfat mounts */
			/* In general, if we are backed by a device, it is probably good(tm) */
			if (NULL != strstr(mntent->mnt_type,"fuse")) {
				if (!strstr(mntent->mnt_fsname,"/dev/"))
					continue;
			}

			snprintf (path, sizeof(path), "%s/DCIM", mntent->mnt_dir);
			if (-1 == stat(path, &stbuf)) {
				snprintf (path, sizeof(path), "%s/dcim", mntent->mnt_dir);
				if (-1 == stat(path, &stbuf))
					continue;
			}
			s = malloc (strlen(_("Media '%s'"))+strlen(mntent->mnt_fsname)+1);
			sprintf (s, _("Media '%s'"), mntent->mnt_fsname);
			gp_port_info_new (&info);
			gp_port_info_set_type (info, GP_PORT_DISK);
			gp_port_info_set_name (info, s);
			free (s);
			
			s = malloc (strlen("disk:")+strlen(mntent->mnt_dir)+1);
			sprintf (s, "disk:%s", mntent->mnt_dir);
			gp_port_info_set_path (info, s);
			if (gp_port_info_list_lookup_path (list, s) >= GP_OK) {
				free (s);
				continue;
			}
			free(s);
			CHECK (gp_port_info_list_append (list, info));
		}
		endmntent(mnt);
	}
	mnt = setmntent ("/etc/mtab", "r");
	if (mnt) {
		while ((mntent = getmntent (mnt))) {
			/* detect floppies so we don't access them with the stat() below */
			GP_LOG_D ("found mtab fsname %s", mntent->mnt_fsname);

			if ((NULL != strstr(mntent->mnt_fsname,"fd"))	||
			    (NULL != strstr(mntent->mnt_fsname,"floppy")) ||
			    (NULL != strstr(mntent->mnt_fsname,"fuse"))	||
			    (NULL != strstr(mntent->mnt_fsname,"nfs"))	||
			    (NULL != strstr(mntent->mnt_fsname,"cifs"))	||
			    (NULL != strstr(mntent->mnt_fsname,"smbfs"))||
			    (NULL != strstr(mntent->mnt_fsname,"afs"))	||
			    (NULL != strstr(mntent->mnt_fsname,"autofs"))||
                            (NULL != strstr(mntent->mnt_fsname,"cgroup"))||
                            (NULL != strstr(mntent->mnt_fsname,"systemd"))||
                            (NULL != strstr(mntent->mnt_fsname,"mqueue"))||
                            (NULL != strstr(mntent->mnt_fsname,"securityfs"))||
                            (NULL != strstr(mntent->mnt_fsname,"proc"))||
                            (NULL != strstr(mntent->mnt_fsname,"devtmpfs"))||
                            (NULL != strstr(mntent->mnt_fsname,"devpts"))||
                            (NULL != strstr(mntent->mnt_fsname,"sysfs"))||
			    (NULL != strstr(mntent->mnt_fsname,"gphotofs"))||
			/* fstype based */
			    (NULL != strstr(mntent->mnt_type,"autofs"))	||
			    (NULL != strstr(mntent->mnt_type,"nfs"))	||
			    (NULL != strstr(mntent->mnt_type,"smbfs"))||
			    (NULL != strstr(mntent->mnt_type,"proc"))||
			    (NULL != strstr(mntent->mnt_type,"sysfs"))||
			    (NULL != strstr(mntent->mnt_type,"cifs"))||
			    (NULL != strstr(mntent->mnt_type,"afs")) ||
			/* options */
			    (NULL != strstr(mntent->mnt_opts,"automount"))
			) {
				continue;
			}
			/* Whitelist some fuse based filesystems, e.g. to help exfat mounts */
			/* In general, if we are backed by a device, it is probably good(tm) */
			if (NULL != strstr(mntent->mnt_type,"fuse")) {
				if (!strstr(mntent->mnt_fsname,"/dev/"))
					continue;
			}

			snprintf (path, sizeof(path), "%s/DCIM", mntent->mnt_dir);
			if (-1 == stat(path, &stbuf)) {
				snprintf (path, sizeof(path), "%s/dcim", mntent->mnt_dir);
				if (-1 == stat(path, &stbuf))
					continue;
			}
			/* automount should be blacklist here, but we still need
			 * to look it up first otherwise the automounted camera
			 * won't appear.
			 */
			if (NULL != strstr(mntent->mnt_fsname, "automount")) {
				continue;
			}
			gp_port_info_new (&info);
			gp_port_info_set_type (info, GP_PORT_DISK);
			s = malloc (strlen(_("Media '%s'"))+strlen(mntent->mnt_fsname)+1);
			sprintf (s, _("Media '%s'"),  mntent->mnt_fsname);
			gp_port_info_set_name (info, s);
			free (s);
			
			s = malloc (strlen("disk:")+strlen(mntent->mnt_dir)+1);
			sprintf (s, "disk:%s", mntent->mnt_dir);
			gp_port_info_set_path (info, s);
			if (gp_port_info_list_lookup_path (list, s) >= GP_OK) {
				free (s);
				continue;
			}
			free (s);
			CHECK (gp_port_info_list_append (list, info));
		}
		endmntent(mnt);
	}
# else
#  ifdef HAVE_MNTTAB
	FILE *mnt;
	struct mnttab mnttab;
	char	path[1024];
	struct stat stbuf;

	info.type = GP_PORT_DISK;

	mnt = fopen ("/etc/fstab", "r");
	if (mnt) {
		while (! getmntent (mnt, &mntent)) {
			/* detect floppies so we don't access them with the stat() below */
			if (	(NULL != strstr(mnttab.mnt_special,"fd")) ||
				(NULL != strstr(mnttab.mnt_special,"floppy"))
			)
				continue;

			snprintf (path, sizeof(path), "%s/DCIM", mnttab.mnt_mountp);
			if (-1 == stat(path, &stbuf)) {
				snprintf (path, sizeof(path), "%s/dcim", mnttab.mnt_mountp);
				if (-1 == stat(path, &stbuf))
					continue;
			}
			snprintf (info.name, sizeof(info.name), _("Media '%s'"), mntent.mnt_special),
			snprintf (info.path, sizeof(info.path), "disk:%s", mntent.mnt_mountp);
			if (gp_port_info_list_lookup_path (list, info.path) >= GP_OK)
				continue;
			CHECK (gp_port_info_list_append (list, info));
		}
		fclose(mnt);
	}
	mnt = fopen ("/etc/mtab", "r");
	if (mnt) {
		while (! getmntent (mnt, &mntent)) {
			/* detect floppies so we don't access them with the stat() below */
			if (	(NULL != strstr(mnttab.mnt_special,"fd")) ||
				(NULL != strstr(mnttab.mnt_special,"floppy"))
			)
				continue;

			snprintf (path, sizeof(path), "%s/DCIM", mnttab.mnt_mountp);
			if (-1 == stat(path, &stbuf)) {
				snprintf (path, sizeof(path), "%s/dcim", mnttab.mnt_mountp);
				if (-1 == stat(path, &stbuf))
					continue;
			}
			snprintf (info.name, sizeof(info.name), _("Media '%s'"), mntent.mnt_special),
			snprintf (info.path, sizeof(info.path), "disk:%s", mntent.mnt_mountp);
			if (gp_port_info_list_lookup_path (list, info.path) >= GP_OK)
				continue;
			CHECK (gp_port_info_list_append (list, info));
		}
		fclose(mnt);
	}
#  endif
# endif
	/* generic disk:/xxx/ matcher */
	gp_port_info_new (&info);
	gp_port_info_set_type (info, GP_PORT_DISK);
	gp_port_info_set_name (info, "");
	gp_port_info_set_path (info, "^disk:");
	gp_port_info_list_append (list, info); /* do not check return */
	return GP_OK;
}

static int gp_port_disk_init (GPPort *dev)
{
	C_MEM (dev->pl = calloc (1, sizeof (GPPortPrivateLibrary)));

	return GP_OK;
}

static int
gp_port_disk_exit (GPPort *port)
{
	free (port->pl);
	port->pl = NULL;

	return GP_OK;
}

static int
gp_port_disk_open (GPPort *port)
{
	return GP_OK;
}

static int
gp_port_disk_close (GPPort *port)
{
	return GP_OK;
}

static int
gp_port_disk_write (GPPort *port, const char *bytes, int size)
{
        return GP_OK;
}

static int
gp_port_disk_read(GPPort *port, char *bytes, int size)
{
        return GP_OK;
}

GPPortOperations *
gp_port_library_operations (void)
{
	GPPortOperations *ops;

	ops = calloc (1, sizeof (GPPortOperations));
	if (!ops)
		return NULL;

	ops->init   = gp_port_disk_init;
	ops->exit   = gp_port_disk_exit;
	ops->open   = gp_port_disk_open;
	ops->close  = gp_port_disk_close;
	ops->read   = gp_port_disk_read;
	ops->write  = gp_port_disk_write;

	return ops;
}
