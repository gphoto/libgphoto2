/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* ptpip.c
 *
 * Copyright (c) 2006 Marcus Meissner <marcus@jet.franken.de>
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
#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif
#include <errno.h>
#ifdef HAVE_SYS_TIME_H
# include <sys/time.h>
#endif
#ifdef HAVE_SYS_PARAM_H
# include <sys/param.h>
#endif
#include <string.h>

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
	int dummy;	/* unused yet */
};

GPPortType
gp_port_library_type (void)
{
        return GP_PORT_PTPIP;
}

/* thanks to Avahi this makes more trouble than its worth */
#undef HAVE_MDNS_BONJOUR
#ifdef HAVE_MDNS_BONJOUR

#include <netinet/in.h>
#include <dns_sd.h>
#include <netdb.h>
#include <arpa/inet.h>

struct mdnsinfo
{
	GPPortInfoList* list;
	const char*	name;
};

static void
_ptpip_resolved (
	DNSServiceRef sdRef, DNSServiceFlags flags, uint32_t interfaceIndex,
	DNSServiceErrorType errorCode,
	const char *fullname, const char *hosttarget, uint16_t port,
	uint16_t txtLen, const char *txtRecord, void *context
) {
	struct hostent*	hent;
	struct in_addr	inaddr;
	GPPortInfo	*info;
	int		i, cnt;
	struct mdnsinfo	*mdnsi = context;
	char		path[200];

	if (errorCode != kDNSServiceErr_NoError) {
		GP_LOG_E ("Error on 2nd level query.");
		return;
	}
	GP_LOG_D ("fullname %s, hosttarget %s, port %d", fullname, hosttarget, htons(port));
	cnt = TXTRecordGetCount (txtLen, txtRecord);
	for (i=0;i<cnt;i++) {
		char	key[256];
		uint8_t	valuelen;
		const void	*value;

		valuelen = 0;
		if (kDNSServiceErr_NoError == TXTRecordGetItemAtIndex (txtLen, txtRecord, i, sizeof(key), key, &valuelen, &value))
			GP_LOG_D ("%d: %s:%s", i, key, (char*)value);
	}
	hent = gethostbyname (hosttarget);
	if (!hent) {
		GP_LOG_E ("Could not resolve the returned host: %s", hosttarget);
		return;
	}
	memcpy(&inaddr.s_addr,hent->h_addr_list[0],hent->h_length);
	gp_port_info_new (&info);
	gp_port_info_set_type (info, GP_PORT_PTPIP);
	gp_port_info_set_name (info, mdnsi->name);
	snprintf (path, sizeof(path), "ptpip:%s:%d", inet_ntoa(inaddr), htons(port));
	gp_port_info_set_path (info, path);
	gp_port_info_list_append (mdnsi->list, info);
}

static void
_ptpip_enumerate (
	DNSServiceRef sdRef, DNSServiceFlags flags,
	uint32_t interfaceIndex, DNSServiceErrorType errorCode,
	const char *serviceName, const char *regtype, const char *replyDomain,
	void *context
) {
	struct mdnsinfo mdnsi;
	DNSServiceRef		sd;

	if (errorCode != kDNSServiceErr_NoError) {
		GP_LOG_E ("Error on _ptp._tcp query.");
		return;
	}
	GP_LOG_D ("found %s, %s, %s", serviceName, regtype, replyDomain);
	mdnsi.list = context;
	mdnsi.name = serviceName;
	DNSServiceResolve (&sd,
		0,
		interfaceIndex,
		serviceName,
		regtype,
		replyDomain,
		_ptpip_resolved,
		&mdnsi
	);
	DNSServiceProcessResult (sd);
	DNSServiceRefDeallocate (sd);
}
#endif

int
gp_port_library_list (GPPortInfoList *list)
{
	GPPortInfo info;
#ifdef HAVE_MDNS_BONJOUR
	DNSServiceRef		sd;
	DNSServiceErrorType	ret;
	int			fd;
	fd_set			infds;
	struct timeval		tv;
#endif

	gp_port_info_new (&info);
	gp_port_info_set_type (info, GP_PORT_PTPIP);
	gp_port_info_set_name (info, _("PTP/IP Connection"));
	gp_port_info_set_path (info, "ptpip:");
	CHECK (gp_port_info_list_append (list, info));

	/* Generic matcher so you can pass any IP address */
	gp_port_info_new (&info);
	gp_port_info_set_type (info, GP_PORT_PTPIP);
	gp_port_info_set_name (info, "");
	gp_port_info_set_path (info, "^ptpip:");
	gp_port_info_list_append (list, info); /* do not check return */

#ifdef HAVE_MDNS_BONJOUR
	ret = DNSServiceBrowse (
		&sd,
		0,	/* unused flags */
		0,	/* all ifaces */
		"_ptp._tcp",
		NULL,
		_ptpip_enumerate,
		list
	);
	/* We need to make it a non-blocking query */
	fd = DNSServiceRefSockFD(sd);
	if (fd != -1) {
		FD_ZERO (&infds); FD_SET (fd, &infds); 
		tv.tv_sec = 0; tv.tv_usec = 1;
		/* If we have input, we can try to process a result */
		if (1 == select (fd+1, &infds, NULL, NULL, &tv))
			DNSServiceProcessResult (sd);
	}
	DNSServiceRefDeallocate (sd);
#endif
	return GP_OK;
}

static int gp_port_ptpip_init (GPPort *dev)
{
	C_MEM (dev->pl = calloc (1, sizeof (GPPortPrivateLibrary)));

	return GP_OK;
}

static int
gp_port_ptpip_exit (GPPort *port)
{
	free (port->pl);
	port->pl = NULL;

	return GP_OK;
}

static int
gp_port_ptpip_open (GPPort *port)
{
	return GP_OK;
}

static int
gp_port_ptpip_close (GPPort *port)
{
	return GP_OK;
}

static int
gp_port_ptpip_write (GPPort *port, const char *bytes, int size)
{
        return GP_OK;
}

static int
gp_port_ptpip_read(GPPort *port, char *bytes, int size)
{
        return GP_OK;
}

static int
gp_port_ptpip_update (GPPort *port)
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

	ops->init   = gp_port_ptpip_init;
	ops->exit   = gp_port_ptpip_exit;
	ops->open   = gp_port_ptpip_open;
	ops->close  = gp_port_ptpip_close;
	ops->read   = gp_port_ptpip_read;
	ops->write  = gp_port_ptpip_write;
	ops->update  = gp_port_ptpip_update;
	return ops;
}
