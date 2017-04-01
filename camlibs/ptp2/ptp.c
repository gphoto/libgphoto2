/* ptp.c
 *
 * Copyright (C) 2001-2004 Mariusz Woloszyn <emsi@ipartners.pl>
 * Copyright (C) 2003-2017 Marcus Meissner <marcus@jet.franken.de>
 * Copyright (C) 2006-2008 Linus Walleij <triad@df.lth.se>
 * Copyright (C) 2007 Tero Saarni <tero.saarni@gmail.com>
 * Copyright (C) 2009 Axel Waggershauser <awagger@web.de>
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

#define _DEFAULT_SOURCE
#include "config.h"
#include "ptp.h"

#ifdef HAVE_LIBXML2
# include <libxml/parser.h>
#endif

#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#ifdef ENABLE_NLS
#  include <libintl.h>
#  undef _
#  define _(String) dgettext (PACKAGE, String)
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

#define CHECK_PTP_RC(RESULT) do { uint16_t r = (RESULT); if (r != PTP_RC_OK) return r; } while(0)

static inline void
ptp_init_container(PTPContainer* ptp, uint16_t code, int n_param, ...)
{
	va_list	args;
	int	i;

	memset(ptp, 0, sizeof(*ptp));
	ptp->Code = code;
	ptp->Nparam = n_param;

	va_start(args, n_param);
	for (i=0; i<n_param; ++i)
		(&ptp->Param1)[i] = va_arg(args, uint32_t);
	va_end(args);
}

#define NARGS_SEQ(_1,_2,_3,_4,_5,_6,_7,_8,N,...) N
#define NARGS(...) NARGS_SEQ(-1, ##__VA_ARGS__, 7, 6, 5, 4, 3, 2, 1, 0)

#define PTP_CNT_INIT(PTP, CODE, ...) \
	ptp_init_container(&PTP, CODE, NARGS(__VA_ARGS__), ##__VA_ARGS__)

static uint16_t ptp_exit_recv_memory_handler (PTPDataHandler*,unsigned char**,unsigned long*);
static uint16_t ptp_init_recv_memory_handler(PTPDataHandler*);
static uint16_t ptp_init_send_memory_handler(PTPDataHandler*,unsigned char*,unsigned long len);
static uint16_t ptp_exit_send_memory_handler (PTPDataHandler *handler);

void
ptp_debug (PTPParams *params, const char *format, ...)
{  
        va_list args;

        va_start (args, format);
        if (params->debug_func!=NULL)
                params->debug_func (params->data, format, args);
        else
	{
                vfprintf (stderr, format, args);
		fprintf (stderr,"\n");
		fflush (stderr);
	}
        va_end (args);
}  

void
ptp_error (PTPParams *params, const char *format, ...)
{  
        va_list args;

        va_start (args, format);
        if (params->error_func!=NULL)
                params->error_func (params->data, format, args);
        else
	{
                vfprintf (stderr, format, args);
		fprintf (stderr,"\n");
		fflush (stderr);
	}
        va_end (args);
}

/* Pack / unpack functions */

#include "ptp-pack.c"

/* major PTP functions */

/**
 * ptp_transaction:
 * params:	PTPParams*
 * 		PTPContainer* ptp	- general ptp container
 * 		uint16_t flags		- lower 8 bits - data phase description
 * 		unsigned int sendlen	- senddata phase data length
 * 		char** data		- send or receive data buffer pointer
 * 		int* recvlen		- receive data length
 *
 * Performs PTP transaction. ptp is a PTPContainer with appropriate fields
 * filled in (i.e. operation code and parameters). It's up to caller to do
 * so.
 * The flags decide thether the transaction has a data phase and what is its
 * direction (send or receive). 
 * If transaction is sending data the sendlen should contain its length in
 * bytes, otherwise it's ignored.
 * The data should contain an address of a pointer to data going to be sent
 * or is filled with such a pointer address if data are received depending
 * od dataphase direction (send or received) or is beeing ignored (no
 * dataphase).
 * The memory for a pointer should be preserved by the caller, if data are
 * beeing retreived the appropriate amount of memory is beeing allocated
 * (the caller should handle that!).
 *
 * Return values: Some PTP_RC_* code.
 * Upon success PTPContainer* ptp contains PTP Response Phase container with
 * all fields filled in.
 **/
uint16_t
ptp_transaction_new (PTPParams* params, PTPContainer* ptp, 
		     uint16_t flags, uint64_t sendlen,
		     PTPDataHandler *handler
) {
	int 		tries;
	uint16_t	cmd;

	if ((params==NULL) || (ptp==NULL)) 
		return PTP_ERROR_BADPARAM;

	cmd = ptp->Code;
	ptp->Transaction_ID=params->transaction_id++;
	ptp->SessionID=params->session_id;
	/* send request */
	CHECK_PTP_RC(params->sendreq_func (params, ptp, flags));
	/* is there a dataphase? */
	switch (flags&PTP_DP_DATA_MASK) {
	case PTP_DP_SENDDATA:
		{
			uint16_t ret = params->senddata_func(params, ptp, sendlen, handler);
			if (ret == PTP_ERROR_CANCEL)
				CHECK_PTP_RC(params->cancelreq_func(params, params->transaction_id-1));
			CHECK_PTP_RC(ret);
		}
		break;
	case PTP_DP_GETDATA:
		{
			uint16_t ret = params->getdata_func(params, ptp, handler);
			if (ret == PTP_ERROR_CANCEL)
				CHECK_PTP_RC(params->cancelreq_func(params, params->transaction_id-1));
			CHECK_PTP_RC(ret);
		}
		break;
	case PTP_DP_NODATA:
		break;
	default:
		return PTP_ERROR_BADPARAM;
	}
	tries = 3;
	while (tries--) {
		uint16_t ret;
		/* get response */
		ret = params->getresp_func(params, ptp);
		if (ret == PTP_ERROR_RESP_EXPECTED) {
			ptp_debug (params,"PTP: response expected but not got, retrying.");
			tries++;
			continue;
		}
		CHECK_PTP_RC(ret);

		if (ptp->Transaction_ID < params->transaction_id-1) {
			/* The Leica uses Transaction ID 0 on result from CloseSession. */
			if (cmd == PTP_OC_CloseSession)
				break;
			tries++;
			ptp_debug (params,
				"PTP: Sequence number mismatch %d vs expected %d, suspecting old reply.",
				ptp->Transaction_ID, params->transaction_id-1
			);
			continue;
		}
		if (ptp->Transaction_ID != params->transaction_id-1) {
			/* try to clean up potential left overs from previous session */
			if ((cmd == PTP_OC_OpenSession) && tries)
				continue;
			ptp_error (params,
				"PTP: Sequence number mismatch %d vs expected %d.",
				ptp->Transaction_ID, params->transaction_id-1
			);
#ifndef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
			return PTP_ERROR_BADPARAM;
#endif
		}
		break;
	}
	return ptp->Code;
}

/* memory data get/put handler */
typedef struct {
	unsigned char	*data;
	unsigned long	size, curoff;
} PTPMemHandlerPrivate;

static uint16_t
memory_getfunc(PTPParams* params, void* private,
	       unsigned long wantlen, unsigned char *data,
	       unsigned long *gotlen
) {
	PTPMemHandlerPrivate* priv = (PTPMemHandlerPrivate*)private;
	unsigned long tocopy = wantlen;

	if (priv->curoff + tocopy > priv->size)
		tocopy = priv->size - priv->curoff;
	memcpy (data, priv->data + priv->curoff, tocopy);
	priv->curoff += tocopy;
	*gotlen = tocopy;
	return PTP_RC_OK;
}

static uint16_t
memory_putfunc(PTPParams* params, void* private,
	       unsigned long sendlen, unsigned char *data
) {
	PTPMemHandlerPrivate* priv = (PTPMemHandlerPrivate*)private;

	if (priv->curoff + sendlen > priv->size) {
		priv->data = realloc (priv->data, priv->curoff+sendlen);
		if (!priv->data)
			return PTP_RC_GeneralError;
		priv->size = priv->curoff + sendlen;
	}
	memcpy (priv->data + priv->curoff, data, sendlen);
	priv->curoff += sendlen;
	return PTP_RC_OK;
}

/* init private struct for receiving data. */
static uint16_t
ptp_init_recv_memory_handler(PTPDataHandler *handler)
{
	PTPMemHandlerPrivate* priv;
	priv = malloc (sizeof(PTPMemHandlerPrivate));
	if (!priv)
		return PTP_RC_GeneralError;
	handler->priv = priv;
	handler->getfunc = memory_getfunc;
	handler->putfunc = memory_putfunc;
	priv->data = NULL;
	priv->size = 0;
	priv->curoff = 0;
	return PTP_RC_OK;
}

/* init private struct and put data in for sending data.
 * data is still owned by caller.
 */
static uint16_t
ptp_init_send_memory_handler(PTPDataHandler *handler,
	unsigned char *data, unsigned long len
) {
	PTPMemHandlerPrivate* priv;
	priv = malloc (sizeof(PTPMemHandlerPrivate));
	if (!priv)
		return PTP_RC_GeneralError;
	handler->priv = priv;
	handler->getfunc = memory_getfunc;
	handler->putfunc = memory_putfunc;
	priv->data = data;
	priv->size = len;
	priv->curoff = 0;
	return PTP_RC_OK;
}

/* free private struct + data */
static uint16_t
ptp_exit_send_memory_handler (PTPDataHandler *handler)
{
	PTPMemHandlerPrivate* priv = (PTPMemHandlerPrivate*)handler->priv;
	/* data is owned by caller */
	free (priv);
	return PTP_RC_OK;
}

/* hand over our internal data to caller */
static uint16_t
ptp_exit_recv_memory_handler (PTPDataHandler *handler,
	unsigned char **data, unsigned long *size
) {
	PTPMemHandlerPrivate* priv = (PTPMemHandlerPrivate*)handler->priv;
	*data = priv->data;
	*size = priv->size;
	free (priv);
	return PTP_RC_OK;
}

/* fd data get/put handler */
typedef struct {
	int fd;
} PTPFDHandlerPrivate;

static uint16_t
fd_getfunc(PTPParams* params, void* private,
	       unsigned long wantlen, unsigned char *data,
	       unsigned long *gotlen
) {
	PTPFDHandlerPrivate* priv = (PTPFDHandlerPrivate*)private;
	int		got;

	got = read (priv->fd, data, wantlen);
	if (got != -1)
		*gotlen = got;
	else
		return PTP_RC_GeneralError;
	return PTP_RC_OK;
}

static uint16_t
fd_putfunc(PTPParams* params, void* private,
	       unsigned long sendlen, unsigned char *data
) {
	ssize_t		written;
	PTPFDHandlerPrivate* priv = (PTPFDHandlerPrivate*)private;

	written = write (priv->fd, data, sendlen);
	if (written != sendlen)
		return PTP_ERROR_IO;
	return PTP_RC_OK;
}

static uint16_t
ptp_init_fd_handler(PTPDataHandler *handler, int fd)
{
	PTPFDHandlerPrivate* priv;
	priv = malloc (sizeof(PTPFDHandlerPrivate));
	if (!priv)
		return PTP_RC_GeneralError;
	handler->priv = priv;
	handler->getfunc = fd_getfunc;
	handler->putfunc = fd_putfunc;
	priv->fd = fd;
	return PTP_RC_OK;
}

static uint16_t
ptp_exit_fd_handler (PTPDataHandler *handler)
{
	PTPFDHandlerPrivate* priv = (PTPFDHandlerPrivate*)handler->priv;
	free (priv);
	return PTP_RC_OK;
}

/* Old style transaction, based on memory */
/* A note on memory management:
 * If called with the flag PTP_DP_GETDATA, this function will internally
 * allocate memory as much as necessary. The caller has to free the memory
 * returned in *data. If the function returns an error, it will free any
 * memory it might have allocated. The recvlen may be NULL. After the
 * function returns, *data will be initialized (valid memory pointer or NULL),
 * i.e. it is not necessary to initialize *data or *recvlen beforehand.
 */
uint16_t
ptp_transaction (PTPParams* params, PTPContainer* ptp, 
		uint16_t flags, uint64_t sendlen,
		unsigned char **data, unsigned int *recvlen
) {
	PTPDataHandler	handler;
	uint16_t	ret;

	switch (flags & PTP_DP_DATA_MASK) {
	case PTP_DP_SENDDATA:
		if (!data)
			return PTP_ERROR_BADPARAM;
		CHECK_PTP_RC(ptp_init_send_memory_handler (&handler, *data, sendlen));
		break;
	case PTP_DP_GETDATA:
		if (!data)
			return PTP_ERROR_BADPARAM;
		*data = NULL;
		if (recvlen)
			*recvlen = 0;
		CHECK_PTP_RC(ptp_init_recv_memory_handler (&handler));
		break;
	default:break;
	}
	ret = ptp_transaction_new (params, ptp, flags, sendlen, &handler);
	switch (flags & PTP_DP_DATA_MASK) {
	case PTP_DP_SENDDATA:
		ptp_exit_send_memory_handler (&handler);
		break;
	case PTP_DP_GETDATA: {
		unsigned long len;
		ptp_exit_recv_memory_handler (&handler, data, &len);
		if (ret != PTP_RC_OK) {
			len = 0;
			free(*data);
			*data = NULL;
		}
		if (recvlen)
			*recvlen = len;
		break;
	}
	default:break;
	}
	return ret;
}


/**
 * PTP operation functions
 *
 * all ptp_ functions should take integer parameters
 * in host byte order!
 **/


/**
 * ptp_getdeviceinfo:
 * params:	PTPParams*
 *
 * Gets device info dataset and fills deviceinfo structure.
 *
 * Return values: Some PTP_RC_* code.
 **/
uint16_t
ptp_getdeviceinfo (PTPParams* params, PTPDeviceInfo* deviceinfo)
{
	PTPContainer	ptp;
	unsigned char	*data;
	unsigned int	size;
	int		ret;

	PTP_CNT_INIT(ptp, PTP_OC_GetDeviceInfo);
	CHECK_PTP_RC(ptp_transaction(params, &ptp, PTP_DP_GETDATA, 0, &data, &size));
	ret = ptp_unpack_DI(params, data, deviceinfo, size);
	free(data);
	if (ret)
		return PTP_RC_OK;
	else
		return PTP_ERROR_IO;
}

uint16_t
ptp_canon_eos_getdeviceinfo (PTPParams* params, PTPCanonEOSDeviceInfo*di)
{
	PTPContainer	ptp;
	unsigned char	*data;
	unsigned int	size;
	int		ret;

	PTP_CNT_INIT(ptp, PTP_OC_CANON_EOS_GetDeviceInfoEx);
	CHECK_PTP_RC(ptp_transaction(params, &ptp, PTP_DP_GETDATA, 0, &data, &size));
	ret = ptp_unpack_EOS_DI(params, data, di, size);
	free (data);
	if (ret)
		return PTP_RC_OK;
	else
		return PTP_ERROR_IO;
}

#ifdef HAVE_LIBXML2
static int
traverse_tree (PTPParams *params, int depth, xmlNodePtr node)
{
	xmlNodePtr	next;
	xmlChar		*xchar;
	int		n;
	char 		*xx;


	if (!node) return 0;
	xx = malloc (depth * 4 + 1);
	memset (xx, ' ', depth*4);
	xx[depth*4] = 0;

	n = xmlChildElementCount (node);

	next = node;
	do {
		fprintf(stderr,"%snode %s\n", xx,next->name);
		fprintf(stderr,"%selements %d\n", xx,n);
		xchar = xmlNodeGetContent (next);
		fprintf(stderr,"%scontent %s\n", xx,xchar);
		traverse_tree (params, depth+1,xmlFirstElementChild (next));
	} while ((next = xmlNextElementSibling (next)));
	free (xx);
	return PTP_RC_OK;
}

static int
parse_9301_cmd_tree (PTPParams *params, xmlNodePtr node, PTPDeviceInfo *di)
{
	xmlNodePtr	next;
	int		cnt;

	cnt = 0;
	next = xmlFirstElementChild (node);
	while (next) {
		cnt++;
		next = xmlNextElementSibling (next);
	}
	di->OperationsSupported_len = cnt;
	di->OperationsSupported = malloc (cnt*sizeof(di->OperationsSupported[0]));
	cnt = 0;
	next = xmlFirstElementChild (node);
	while (next) {
		unsigned int p;

		sscanf((char*)next->name, "c%04x", &p);
		ptp_debug( params, "cmd %s / 0x%04x", next->name, p);
		di->OperationsSupported[cnt++] = p;
		next = xmlNextElementSibling (next);
	}
	return PTP_RC_OK;
}

static int
parse_9301_value (PTPParams *params, const char *str, uint16_t type, PTPPropertyValue *propval)
{
	switch (type) {
	case 6: { /*UINT32*/
		unsigned int x;
		if (!sscanf(str,"%08x", &x)) {
			ptp_debug( params, "could not parse uint32 %s", str);
			return PTP_RC_GeneralError;
		}
		ptp_debug( params, "\t%d", x);
		propval->u32 = x;
		break;
	}
	case 5: { /*INT32*/
		int x;
		if (!sscanf(str,"%08x", &x)) {
			ptp_debug( params, "could not parse int32 %s", str);
			return PTP_RC_GeneralError;
		}
		ptp_debug( params, "\t%d", x);
		propval->i32 = x;
		break;
	}
	case 4: { /*UINT16*/
		unsigned int x;
		if (!sscanf(str,"%04x", &x)) {
			ptp_debug( params, "could not parse uint16 %s", str);
			return PTP_RC_GeneralError;
		}
		ptp_debug( params, "\t%d", x);
		propval->u16 = x;
		break;
	}
	case 3: { /*INT16*/
		int x;
		if (!sscanf(str,"%04x", &x)) {
			ptp_debug( params, "could not parse int16 %s", str);
			return PTP_RC_GeneralError;
		}
		ptp_debug( params, "\t%d", x);
		propval->i16 = x;
		break;
	}
	case 2: { /*UINT8*/
		unsigned int x;
		if (!sscanf(str,"%02x", &x)) {
			ptp_debug( params, "could not parse uint8 %s", str);
			return PTP_RC_GeneralError;
		}
		ptp_debug( params, "\t%d", x);
		propval->u8 = x;
		break;
	}
	case 1: { /*INT8*/
		int x;
		if (!sscanf(str,"%02x", &x)) {
			ptp_debug( params, "could not parse int8 %s", str);
			return PTP_RC_GeneralError;
		} 
		ptp_debug( params, "\t%d", x);
		propval->i8 = x;
		break;
	}
	case 65535: { /* string */
		int len;

		/* ascii ptp string, 1 byte length, little endian 16 bit chars */
		if (sscanf(str,"%02x", &len)) {
			int i;
			char *xstr = malloc(len+1);
			for (i=0;i<len;i++) {
				int xc;
				if (sscanf(str+2+i*4,"%04x", &xc)) {
					int cx;

					cx = ((xc>>8) & 0xff) | ((xc & 0xff) << 8);
					xstr[i] = cx;
				}
				xstr[len] = 0;
			}
			ptp_debug( params, "\t%s", xstr);
			propval->str = xstr;
			break;
		}
		ptp_debug( params, "string %s not parseable!", str);
		return PTP_RC_GeneralError;
	}
	case 7: /*INT64*/
	case 8: /*UINT64*/
	case 9: /*INT128*/
	case 10: /*UINT128*/
	default:
		ptp_debug( params, "unhandled data type %d!", type);
		return PTP_RC_GeneralError;
	}
	return PTP_RC_OK;
}

static int
parse_9301_propdesc (PTPParams *params, xmlNodePtr next, PTPDevicePropDesc *dpd)
{
	int type = -1;

	if (!next)
		return PTP_RC_GeneralError;

	ptp_debug (params, "parse_9301_propdesc");
	dpd->FormFlag	= PTP_DPFF_None;
	dpd->GetSet	= PTP_DPGS_Get;
	do {
		if (!strcmp((char*)next->name,"type")) {	/* propdesc.DataType */
			if (!sscanf((char*)xmlNodeGetContent (next), "%04x", &type)) {
				ptp_debug( params, "\ttype %s not parseable?",xmlNodeGetContent (next));
				return 0;
			}
			ptp_debug( params, "type 0x%x", type);
			dpd->DataType = type;
			continue;
		}
		if (!strcmp((char*)next->name,"attribute")) {	/* propdesc.GetSet */
			int attr;

			if (!sscanf((char*)xmlNodeGetContent (next), "%02x", &attr)) {
				ptp_debug( params, "\tattr %s not parseable",xmlNodeGetContent (next));
				return 0;
			}
			ptp_debug( params, "attribute 0x%x", attr);
			dpd->GetSet = attr;
			continue;
		}
		if (!strcmp((char*)next->name,"default")) {	/* propdesc.FactoryDefaultValue */
			ptp_debug( params, "default value");
			parse_9301_value (params, (char*)xmlNodeGetContent (next), type, &dpd->FactoryDefaultValue);
			continue;
		}
		if (!strcmp((char*)next->name,"value")) {	/* propdesc.CurrentValue */
			ptp_debug( params, "current value");
			parse_9301_value (params, (char*)xmlNodeGetContent (next), type, &dpd->CurrentValue);
			continue;
		}
		if (!strcmp((char*)next->name,"enum")) {	/* propdesc.FORM.Enum */
			int n,i;
			char *s;

			ptp_debug( params, "enum");
			dpd->FormFlag = PTP_DPFF_Enumeration;
			s = (char*)xmlNodeGetContent (next);
			n = 0;
			do {
				s = strchr(s,' ');
				if (s) s++;
				n++;
			} while (s);
			dpd->FORM.Enum.NumberOfValues = n;
			dpd->FORM.Enum.SupportedValue = malloc (n * sizeof(PTPPropertyValue));
			s = (char*)xmlNodeGetContent (next);
			i = 0;
			do {
				parse_9301_value (params, s, type, &dpd->FORM.Enum.SupportedValue[i]); /* should turn ' ' into \0? */
				i++;
				s = strchr(s,' ');
				if (s) s++;
			} while (s && (i<n));
			continue;
		}
		if (!strcmp((char*)next->name,"range")) {	/* propdesc.FORM.Enum */
			char *s = (char*)xmlNodeGetContent (next);
			dpd->FormFlag = PTP_DPFF_Range;
			ptp_debug( params, "range");
			parse_9301_value (params, s, type, &dpd->FORM.Range.MinimumValue); /* should turn ' ' into \0? */
			s = strchr(s,' ');
			if (!s) continue;
			s++;
			parse_9301_value (params, s, type, &dpd->FORM.Range.MaximumValue); /* should turn ' ' into \0? */
			s = strchr(s,' ');
			if (!s) continue;
			s++;
			parse_9301_value (params, s, type, &dpd->FORM.Range.StepSize); /* should turn ' ' into \0? */

			continue;
		}
		ptp_debug (params, "\tpropdescvar: %s", next->name);
		traverse_tree (params, 3, next);
	} while ((next = xmlNextElementSibling (next)));
	return PTP_RC_OK;
}

static int
parse_9301_prop_tree (PTPParams *params, xmlNodePtr node, PTPDeviceInfo *di)
{
	xmlNodePtr	next;
	int		cnt;
	unsigned int	i;

	cnt = 0;
	next = xmlFirstElementChild (node);
	while (next) {
		cnt++;
		next = xmlNextElementSibling (next);
	}

	di->DevicePropertiesSupported_len = cnt;
	di->DevicePropertiesSupported = malloc (cnt*sizeof(di->DevicePropertiesSupported[0]));
	cnt = 0;
	next = xmlFirstElementChild (node);
	while (next) {
		unsigned int p;
		PTPDevicePropDesc	dpd;

		sscanf((char*)next->name, "p%04x", &p);
		ptp_debug( params, "prop %s / 0x%04x", next->name, p);
		parse_9301_propdesc (params, xmlFirstElementChild (next), &dpd);
		dpd.DevicePropertyCode = p;
		di->DevicePropertiesSupported[cnt++] = p;

		/* add to cache of device propdesc */
		for (i=0;i<params->nrofdeviceproperties;i++)
			if (params->deviceproperties[i].desc.DevicePropertyCode == p)
				break;
		if (i == params->nrofdeviceproperties) {
			params->deviceproperties = realloc(params->deviceproperties,(i+1)*sizeof(params->deviceproperties[0]));
			memset(&params->deviceproperties[i],0,sizeof(params->deviceproperties[0]));
			params->nrofdeviceproperties++;
		} else {
			ptp_free_devicepropdesc (&params->deviceproperties[i].desc);
		}
		/* FIXME: free old entry */
		/* we are not using dpd, so copy it directly to the cache */
		time( &params->deviceproperties[i].timestamp);
		params->deviceproperties[i].desc = dpd;

		next = xmlNextElementSibling (next);
	}
	return PTP_RC_OK;
}

static int
parse_9301_event_tree (PTPParams *params, xmlNodePtr node, PTPDeviceInfo *di)
{
	xmlNodePtr	next;
	int		cnt;

	cnt = 0;
	next = xmlFirstElementChild (node);
	while (next) {
		cnt++;
		next = xmlNextElementSibling (next);
	}
	di->EventsSupported_len = cnt;
	di->EventsSupported = malloc (cnt*sizeof(di->EventsSupported[0]));
	cnt = 0;
	next = xmlFirstElementChild (node);
	while (next) {
		unsigned int p;

		sscanf((char*)next->name, "e%04x", &p);
		ptp_debug( params, "event %s / 0x%04x", next->name, p);
		di->EventsSupported[cnt++] = p;
		next = xmlNextElementSibling (next);
	}
	return PTP_RC_OK;
}

static int
parse_9301_tree (PTPParams *params, xmlNodePtr node, PTPDeviceInfo *di)
{
	xmlNodePtr	next;

	next = xmlFirstElementChild (node);
	while (next) {
		if (!strcmp ((char*)next->name, "cmd")) {
			parse_9301_cmd_tree (params, next, di);
			next = xmlNextElementSibling (next);
			continue;
		}
		if (!strcmp ((char*)next->name, "prop")) {
			parse_9301_prop_tree (params, next, di);
			next = xmlNextElementSibling (next);
			continue;
		}
		if (!strcmp ((char*)next->name, "event")) {
			parse_9301_event_tree (params, next, di);
			next = xmlNextElementSibling (next);
			continue;
		}
		fprintf (stderr,"9301: unhandled type %s\n", next->name);
		next = xmlNextElementSibling (next);
	}
	/*traverse_tree (0, node);*/
	return PTP_RC_OK;
}

static uint16_t
ptp_olympus_parse_output_xml(PTPParams* params, char*data, int len, xmlNodePtr *code)
{
        xmlDocPtr       docin;
        xmlNodePtr      docroot, output, next;
	int 		result, xcode;

	*code = NULL;

        docin = xmlReadMemory ((char*)data, len, "http://gphoto.org/", "utf-8", 0);
        if (!docin) return PTP_RC_GeneralError;
        docroot = xmlDocGetRootElement (docin);
        if (!docroot) {
		xmlFreeDoc (docin);
		return PTP_RC_GeneralError;
	}

        if (strcmp((char*)docroot->name,"x3c")) {
                ptp_debug (params, "olympus: docroot is not x3c, but %s", docroot->name);
		xmlFreeDoc (docin);
                return PTP_RC_GeneralError;
        }
        if (xmlChildElementCount(docroot) != 1) {
                ptp_debug (params, "olympus: x3c: expected 1 child, got %ld", xmlChildElementCount(docroot));
		xmlFreeDoc (docin);
                return PTP_RC_GeneralError;
        }
        output = xmlFirstElementChild (docroot);
        if (strcmp((char*)output->name, "output") != 0) {
                ptp_debug (params, "olympus: x3c node: expected child 'output', but got %s", (char*)output->name);
		xmlFreeDoc (docin);
                return PTP_RC_GeneralError;
	}
        next = xmlFirstElementChild (output);

	result = PTP_RC_GeneralError;

	while (next) {
		if (!strcmp((char*)next->name,"result")) {
			xmlChar	 *xchar;

			xchar = xmlNodeGetContent (next);
			if (!sscanf((char*)xchar,"%04x",&result))
				ptp_debug (params, "failed scanning result from %s", xchar);
			ptp_debug (params,  "ptp result is 0x%04x", result);
			next = xmlNextElementSibling (next);
			continue;
		}
		if (sscanf((char*)next->name,"c%x", &xcode)) {
			ptp_debug (params,  "ptp code node found %s", (char*)next->name);
			*code = next;
			next = xmlNextElementSibling (next);
			continue;
		}
		ptp_debug (params, "unhandled node %s", (char*)next->name);
		next = xmlNextElementSibling (next);
	}

	if (result != PTP_RC_OK) {
		*code = NULL;
		xmlFreeDoc (docin);
	}
	return result;
}
#endif

uint16_t
ptp_olympus_getdeviceinfo (PTPParams* params, PTPDeviceInfo *di)
{
#ifdef HAVE_LIBXML2
	PTPContainer	ptp;
	uint16_t 	ret;
	unsigned char	*data;
	unsigned int	size;
	xmlNodePtr	code;

	memset (di, 0, sizeof(PTPDeviceInfo));

	PTP_CNT_INIT(ptp, PTP_OC_OLYMPUS_GetDeviceInfo);
	ret=ptp_transaction(params, &ptp, PTP_DP_GETDATA, 0, &data, &size);
	/* TODO: check for error, only parse_output_xml if ret == PTP_RC_OK?
	 * where is 'data' going to be deallocated? */
	ret = ptp_olympus_parse_output_xml(params,(char*)data,size,&code);
	if (ret != PTP_RC_OK)
		return ret;

	ret = parse_9301_tree (params, code, di);

	xmlFreeDoc(code->doc);
	return ret;
#else
	return PTP_RC_GeneralError;
#endif
}

uint16_t
ptp_olympus_opensession (PTPParams* params, unsigned char**data, unsigned int *len)
{
	PTPContainer	ptp;

	PTP_CNT_INIT(ptp, PTP_OC_OLYMPUS_OpenSession);
	return ptp_transaction(params, &ptp, PTP_DP_GETDATA, 0, data, len);
}

uint16_t
ptp_olympus_getcameraid (PTPParams* params, unsigned char**data, unsigned int *len)
{
	PTPContainer	ptp;

	PTP_CNT_INIT(ptp, PTP_OC_OLYMPUS_GetCameraID);
	return ptp_transaction(params, &ptp, PTP_DP_GETDATA, 0, data, len);
}

/**
 * ptp_generic_no_data:
 * params:	PTPParams*
 * 		code	PTP OP Code
 * 		n_param	count of parameters
 *		... variable argument list ...
 *
 * Emits a generic PTP command without any data transfer.
 *
 * Return values: Some PTP_RC_* code.
 **/
uint16_t
ptp_generic_no_data (PTPParams* params, uint16_t code, unsigned int n_param, ...)
{
	PTPContainer	ptp;
	va_list		args;
	unsigned int	i;

	if( n_param > 5 )
		return PTP_ERROR_BADPARAM;

	memset(&ptp, 0, sizeof(ptp));
	ptp.Code=code;
	ptp.Nparam=n_param;

	va_start(args, n_param);
	for( i=0; i<n_param; ++i )
		(&ptp.Param1)[i] = va_arg(args, uint32_t);
	va_end(args);

	return ptp_transaction(params, &ptp, PTP_DP_NODATA, 0, NULL, NULL);
}

/**
 * ptp_opensession:
 * params:	PTPParams*
 * 		session			- session number 
 *
 * Establishes a new session.
 *
 * Return values: Some PTP_RC_* code.
 **/
uint16_t
ptp_opensession (PTPParams* params, uint32_t session)
{
	PTPContainer	ptp;
	uint16_t	ret;

	ptp_debug(params,"PTP: Opening session");

	/* SessonID field of the operation dataset should always
	   be set to 0 for OpenSession request! */
	params->session_id=0x00000000;
	/* TransactionID should be set to 0 also! */
	params->transaction_id=0x0000000;
	/* zero out response packet buffer */
	params->response_packet = NULL;
	params->response_packet_size = 0;
	/* no split headers */
	params->split_header_data = 0;

	PTP_CNT_INIT(ptp, PTP_OC_OpenSession, session);
	ret=ptp_transaction_new(params, &ptp, PTP_DP_NODATA, 0, NULL);
	/* TODO: check for error */
	/* now set the global session id to current session number */
	params->session_id=session;
	return ret;
}

void
ptp_free_devicepropvalue(uint16_t dt, PTPPropertyValue* dpd)
{
	switch (dt) {
	case PTP_DTC_INT8:	case PTP_DTC_UINT8:
	case PTP_DTC_UINT16:	case PTP_DTC_INT16:
	case PTP_DTC_UINT32:	case PTP_DTC_INT32:
	case PTP_DTC_UINT64:	case PTP_DTC_INT64:
	case PTP_DTC_UINT128:	case PTP_DTC_INT128:
		/* Nothing to free */
		break;
	case PTP_DTC_AINT8:	case PTP_DTC_AUINT8:
	case PTP_DTC_AUINT16:	case PTP_DTC_AINT16:
	case PTP_DTC_AUINT32:	case PTP_DTC_AINT32:
	case PTP_DTC_AUINT64:	case PTP_DTC_AINT64:
	case PTP_DTC_AUINT128:	case PTP_DTC_AINT128:
		free(dpd->a.v);
		break;
	case PTP_DTC_STR:
		free(dpd->str);
		break;
	}
}

void
ptp_free_devicepropdesc(PTPDevicePropDesc* dpd)
{
	uint16_t i;

	ptp_free_devicepropvalue (dpd->DataType, &dpd->FactoryDefaultValue);
	ptp_free_devicepropvalue (dpd->DataType, &dpd->CurrentValue);
	switch (dpd->FormFlag) {
	case PTP_DPFF_Range:
		ptp_free_devicepropvalue (dpd->DataType, &dpd->FORM.Range.MinimumValue);
		ptp_free_devicepropvalue (dpd->DataType, &dpd->FORM.Range.MaximumValue);
		ptp_free_devicepropvalue (dpd->DataType, &dpd->FORM.Range.StepSize);
		break;
	case PTP_DPFF_Enumeration:
		if (dpd->FORM.Enum.SupportedValue) {
			for (i=0;i<dpd->FORM.Enum.NumberOfValues;i++)
				ptp_free_devicepropvalue (dpd->DataType, dpd->FORM.Enum.SupportedValue+i);
			free (dpd->FORM.Enum.SupportedValue);
		}
	}
	dpd->DataType = PTP_DTC_UNDEF;
	dpd->FormFlag = PTP_DPFF_None;
}


void
ptp_free_objectpropdesc(PTPObjectPropDesc* opd)
{
	uint16_t i;

	ptp_free_devicepropvalue (opd->DataType, &opd->FactoryDefaultValue);
	switch (opd->FormFlag) {
	case PTP_OPFF_None:
		break;
	case PTP_OPFF_Range:
		ptp_free_devicepropvalue (opd->DataType, &opd->FORM.Range.MinimumValue);
		ptp_free_devicepropvalue (opd->DataType, &opd->FORM.Range.MaximumValue);
		ptp_free_devicepropvalue (opd->DataType, &opd->FORM.Range.StepSize);
		break;
	case PTP_OPFF_Enumeration:
		if (opd->FORM.Enum.SupportedValue) {
			for (i=0;i<opd->FORM.Enum.NumberOfValues;i++)
				ptp_free_devicepropvalue (opd->DataType, opd->FORM.Enum.SupportedValue+i);
			free (opd->FORM.Enum.SupportedValue);
		}
		break;
	case PTP_OPFF_DateTime:
	case PTP_OPFF_FixedLengthArray:
	case PTP_OPFF_RegularExpression:
	case PTP_OPFF_ByteArray:
	case PTP_OPFF_LongString:
		/* Ignore these presently, we cannot unpack them, so there is nothing to be freed. */
		break;
	default:
		fprintf (stderr, "Unknown OPFF type %d\n", opd->FormFlag);
		break;
	}
}


/**
 * ptp_free_params:
 * params:	PTPParams*
 *
 * Frees all data within the PTPParams struct.
 *
 * Return values: Some PTP_RC_* code.
 **/
void
ptp_free_params (PTPParams *params)
{
	unsigned int i;

	free (params->cameraname);
	free (params->wifi_profiles);
	for (i=0;i<params->nrofobjects;i++)
		ptp_free_object (&params->objects[i]);
	free (params->objects);
	free (params->storageids.Storage);
	free (params->events);
	for (i=0;i<params->nrofcanon_props;i++) {
		free (params->canon_props[i].data);
		ptp_free_devicepropdesc (&params->canon_props[i].dpd);
	}
	free (params->canon_props);
	free (params->backlogentries);

	for (i=0;i<params->nrofdeviceproperties;i++)
		ptp_free_devicepropdesc (&params->deviceproperties[i].desc);
	free (params->deviceproperties);

	ptp_free_DI (&params->deviceinfo);
}

/**
 * ptp_getststorageids:
 * params:	PTPParams*
 *
 * Gets array of StorageIDs and fills the storageids structure.
 *
 * Return values: Some PTP_RC_* code.
 **/
uint16_t
ptp_getstorageids (PTPParams* params, PTPStorageIDs* storageids)
{
	PTPContainer	ptp;
	unsigned char	*data;
	unsigned int	size;

	PTP_CNT_INIT(ptp, PTP_OC_GetStorageIDs);
	CHECK_PTP_RC(ptp_transaction(params, &ptp, PTP_DP_GETDATA, 0, &data, &size));
	ptp_unpack_SIDs(params, data, storageids, size);
	free(data);
	return PTP_RC_OK;
}

/**
 * ptp_getststorageinfo:
 * params:	PTPParams*
 *		storageid		- StorageID
 *
 * Gets StorageInfo dataset of desired storage and fills storageinfo
 * structure.
 *
 * Return values: Some PTP_RC_* code.
 **/
uint16_t
ptp_getstorageinfo (PTPParams* params, uint32_t storageid,
			PTPStorageInfo* storageinfo)
{
	PTPContainer	ptp;
	unsigned char	*data;
	unsigned int	size;

	PTP_CNT_INIT(ptp, PTP_OC_GetStorageInfo, storageid);
	CHECK_PTP_RC(ptp_transaction(params, &ptp, PTP_DP_GETDATA, 0, &data, &size));
	if (!data || !size)
		return PTP_RC_GeneralError;
	memset(storageinfo, 0, sizeof(*storageinfo));
	if (!ptp_unpack_SI(params, data, storageinfo, size)) {
		free(data);
		return PTP_RC_GeneralError;
	}
	free(data);
	return PTP_RC_OK;
}

/**
 * ptp_getobjecthandles:
 * params:	PTPParams*
 *		storage			- StorageID
 *		objectformatcode	- ObjectFormatCode (optional)
 *		associationOH		- ObjectHandle of Association for
 *					  wich a list of children is desired
 *					  (optional)
 *		objecthandles		- pointer to structute
 *
 * Fills objecthandles with structure returned by device.
 *
 * Return values: Some PTP_RC_* code.
 **/
uint16_t
ptp_getobjecthandles (PTPParams* params, uint32_t storage,
			uint32_t objectformatcode, uint32_t associationOH,
			PTPObjectHandles* objecthandles)
{
	PTPContainer	ptp;
	uint16_t	ret;
	unsigned char	*data;
	unsigned int	size;

	objecthandles->Handler = NULL;
	objecthandles->n = 0;

	PTP_CNT_INIT(ptp, PTP_OC_GetObjectHandles, storage, objectformatcode, associationOH);
	ret=ptp_transaction(params, &ptp, PTP_DP_GETDATA, 0, &data, &size);
	if (ret == PTP_RC_OK) {
		ptp_unpack_OH(params, data, objecthandles, size);
	} else {
		if (	(storage == 0xffffffff) &&
			(objectformatcode == 0) &&
			(associationOH == 0)
		) {
			/* When we query all object handles on all stores and
			 * get an error -> just handle it as "0 handles".
			 */
			objecthandles->Handler = NULL;
			objecthandles->n = 0;
			ret = PTP_RC_OK;
		}
	}
	free(data);
	return ret;
}

uint16_t
ptp_getfilesystemmanifest (PTPParams* params, uint32_t storage,
			uint32_t objectformatcode, uint32_t associationOH,
			unsigned char** data)
{
	PTPContainer ptp;

	PTP_CNT_INIT(ptp, PTP_OC_GetFilesystemManifest, storage, objectformatcode, associationOH);
	return ptp_transaction(params, &ptp, PTP_DP_GETDATA, 0, data, NULL);
}

/**
 * ptp_getnumobjects:
 * params:	PTPParams*
 *		storage			- StorageID
 *		objectformatcode	- ObjectFormatCode (optional)
 *		associationOH		- ObjectHandle of Association for
 *					  wich a list of children is desired
 *					  (optional)
 *		numobs			- pointer to uint32_t that takes number of objects
 *
 * Fills numobs with number of objects on device.
 *
 * Return values: Some PTP_RC_* code.
 **/
uint16_t
ptp_getnumobjects (PTPParams* params, uint32_t storage,
			uint32_t objectformatcode, uint32_t associationOH,
			uint32_t* numobs)
{
	PTPContainer	ptp;

	PTP_CNT_INIT(ptp, PTP_OC_GetNumObjects, storage, objectformatcode, associationOH);
	CHECK_PTP_RC(ptp_transaction(params, &ptp, PTP_DP_NODATA, 0, NULL, NULL));
	if (ptp.Nparam >= 1)
		*numobs = ptp.Param1;
	else
		return PTP_RC_GeneralError;
	return PTP_RC_OK;
}

/**
 * ptp_eos_bulbstart:
 * params:	PTPParams*
 *
 * Starts EOS Bulb capture.
 *
 * Return values: Some PTP_RC_* code.
 **/
uint16_t
ptp_canon_eos_bulbstart (PTPParams* params)
{
	PTPContainer	ptp;

	PTP_CNT_INIT(ptp, PTP_OC_CANON_EOS_BulbStart);
	CHECK_PTP_RC(ptp_transaction(params, &ptp, PTP_DP_NODATA, 0, NULL, NULL));
	if ((ptp.Nparam >= 1) && ((ptp.Param1 & 0x7000) == 0x2000))
		return ptp.Param1;
	return PTP_RC_OK;
}

/**
 * ptp_eos_capture:
 * params:	PTPParams*
 *              uint32_t*	result
 *
 * This starts a EOS400D style capture. You have to use the
 * get_eos_events to find out what resulted.
 * The return value is "0" for all OK, and "1" for capture failed. (not fully confirmed)
 *
 * Return values: Some PTP_RC_* code.
 **/
uint16_t
ptp_canon_eos_capture (PTPParams* params, uint32_t *result)
{
	PTPContainer	ptp;

	PTP_CNT_INIT(ptp, PTP_OC_CANON_EOS_RemoteRelease);
	*result = 0;
	CHECK_PTP_RC(ptp_transaction(params, &ptp, PTP_DP_NODATA, 0, NULL, NULL));
	if (ptp.Nparam >= 1)
		*result = ptp.Param1;
	return PTP_RC_OK;
}

/**
 * ptp_canon_eos_bulbend:
 * params:	PTPParams*
 *
 * Starts EOS Bulb capture.
 *
 * Return values: Some PTP_RC_* code.
 **/
uint16_t
ptp_canon_eos_bulbend (PTPParams* params)
{
	PTPContainer	ptp;

	PTP_CNT_INIT(ptp, PTP_OC_CANON_EOS_BulbEnd);
	CHECK_PTP_RC(ptp_transaction(params, &ptp, PTP_DP_NODATA, 0, NULL, NULL));
	if ((ptp.Nparam >= 1) && ((ptp.Param1 & 0x7000) == 0x2000))
		return ptp.Param1;
	return PTP_RC_OK;
}

/**
 * ptp_getobjectinfo:
 * params:	PTPParams*
 *		handle			- Object handle
 *		objectinfo		- pointer to objectinfo that is returned
 *
 * Get objectinfo structure for handle from device.
 *
 * Return values: Some PTP_RC_* code.
 **/
uint16_t
ptp_getobjectinfo (PTPParams* params, uint32_t handle,
			PTPObjectInfo* objectinfo)
{
	PTPContainer	ptp;
	unsigned char	*data;
	unsigned int	size;

	PTP_CNT_INIT(ptp, PTP_OC_GetObjectInfo, handle);
	CHECK_PTP_RC(ptp_transaction(params, &ptp, PTP_DP_GETDATA, 0, &data, &size));
	ptp_unpack_OI(params, data, objectinfo, size);
	free(data);
	return PTP_RC_OK;
}

/**
 * ptp_getobject:
 * params:	PTPParams*
 *		handle			- Object handle
 *		object			- pointer to data area
 *
 * Get object 'handle' from device and store the data in newly
 * allocated 'object'.
 *
 * Return values: Some PTP_RC_* code.
 **/
uint16_t
ptp_getobject (PTPParams* params, uint32_t handle, unsigned char** object)
{
	PTPContainer ptp;

	PTP_CNT_INIT(ptp, PTP_OC_GetObject, handle);
	return ptp_transaction(params, &ptp, PTP_DP_GETDATA, 0, object, NULL);
}

/**
 * ptp_getobject_with_size:
 * params:	PTPParams*
 *		handle			- Object handle
 *		object			- pointer to data area
 *		size			- pointer to uint, returns size of object
 *
 * Get object 'handle' from device and store the data in newly
 * allocated 'object'.
 *
 * Return values: Some PTP_RC_* code.
 **/
uint16_t
ptp_getobject_with_size (PTPParams* params, uint32_t handle, unsigned char** object, unsigned int *size)
{
	PTPContainer ptp;

	PTP_CNT_INIT(ptp, PTP_OC_GetObject, handle);
	return ptp_transaction(params, &ptp, PTP_DP_GETDATA, 0, object, size);
}

/**
 * ptp_getobject_to_handler:
 * params:	PTPParams*
 *		handle			- Object handle
 *		PTPDataHandler*		- pointer datahandler
 *
 * Get object 'handle' from device and store the data in newly
 * allocated 'object'.
 *
 * Return values: Some PTP_RC_* code.
 **/
uint16_t
ptp_getobject_to_handler (PTPParams* params, uint32_t handle, PTPDataHandler *handler)
{
	PTPContainer ptp;

	PTP_CNT_INIT(ptp, PTP_OC_GetObject, handle);
	return ptp_transaction_new(params, &ptp, PTP_DP_GETDATA, 0, handler);
}

/**
 * ptp_getobject_tofd:
 * params:	PTPParams*
 *		handle			- Object handle
 *		fd                      - File descriptor to write() to
 *
 * Get object 'handle' from device and write the data to the 
 * given file descriptor.
 *
 * Return values: Some PTP_RC_* code.
 **/
uint16_t
ptp_getobject_tofd (PTPParams* params, uint32_t handle, int fd)
{
	PTPContainer	ptp;
	PTPDataHandler	handler;
	uint16_t	ret;

	PTP_CNT_INIT(ptp, PTP_OC_GetObject, handle);
	ptp_init_fd_handler (&handler, fd);
	ret = ptp_transaction_new(params, &ptp, PTP_DP_GETDATA, 0, &handler);
	ptp_exit_fd_handler (&handler);
	return ret;
}

/**
 * ptp_getpartialobject:
 * params:	PTPParams*
 *		handle			- Object handle
 *		offset			- Offset into object
 *		maxbytes		- Maximum of bytes to read
 *		object			- pointer to data area
 *		len			- pointer to returned length
 *
 * Get object 'handle' from device and store the data in newly
 * allocated 'object'. Start from offset and read at most maxbytes.
 *
 * Return values: Some PTP_RC_* code.
 **/
uint16_t
ptp_getpartialobject (PTPParams* params, uint32_t handle, uint32_t offset,
			uint32_t maxbytes, unsigned char** object,
			uint32_t *len)
{
	PTPContainer ptp;

	PTP_CNT_INIT(ptp, PTP_OC_GetPartialObject, handle, offset, maxbytes);
	return ptp_transaction(params, &ptp, PTP_DP_GETDATA, 0, object, len);
}

/**
 * ptp_getpartialobject_to_handler:
 * params:	PTPParams*
 *		handle			- Object handle
 *		offset			- Offset into object
 *		maxbytes		- Maximum of bytes to read
 *		handler			- a ptp data handler
 *
 * Get object 'handle' from device and send the data to the
 * data handler. Start from offset and read at most maxbytes.
 *
 * Return values: Some PTP_RC_* code.
 **/
uint16_t
ptp_getpartialobject_to_handler (PTPParams* params, uint32_t handle, uint32_t offset,
			uint32_t maxbytes, PTPDataHandler *handler)
{
	PTPContainer ptp;

	PTP_CNT_INIT(ptp, PTP_OC_GetPartialObject, handle, offset, maxbytes);
	return ptp_transaction_new(params, &ptp, PTP_DP_GETDATA, 0, handler);
}

/**
 * ptp_getthumb:
 * params:	PTPParams*
 *		handle			- Object handle
 *		object			- pointer to data area
 *
 * Get thumb for object 'handle' from device and store the data in newly
 * allocated 'object'.
 *
 * Return values: Some PTP_RC_* code.
 **/
uint16_t
ptp_getthumb (PTPParams* params, uint32_t handle, unsigned char** object, unsigned int *len)
{
	PTPContainer ptp;

	PTP_CNT_INIT(ptp, PTP_OC_GetThumb, handle);
	return ptp_transaction(params, &ptp, PTP_DP_GETDATA, 0, object, len);
}

/**
 * ptp_deleteobject:
 * params:	PTPParams*
 *		handle			- object handle
 *		ofc			- object format code (optional)
 * 
 * Deletes desired objects.
 *
 * Return values: Some PTP_RC_* code.
 **/
uint16_t
ptp_deleteobject (PTPParams* params, uint32_t handle, uint32_t ofc)
{
	PTPContainer ptp;

	PTP_CNT_INIT(ptp, PTP_OC_DeleteObject, handle, ofc);
	CHECK_PTP_RC(ptp_transaction(params, &ptp, PTP_DP_NODATA, 0, NULL, NULL));
	/* If the object is cached and could be removed, cleanse cache. */
	ptp_remove_object_from_cache(params, handle);
	return PTP_RC_OK;
}

/**
 * ptp_sendobjectinfo:
 * params:	PTPParams*
 *		uint32_t* store		- destination StorageID on Responder
 *		uint32_t* parenthandle 	- Parent ObjectHandle on responder
 * 		uint32_t* handle	- see Return values
 *		PTPObjectInfo* objectinfo- ObjectInfo that is to be sent
 * 
 * Sends ObjectInfo of file that is to be sent via SendFileObject.
 *
 * Return values: Some PTP_RC_* code.
 * Upon success : uint32_t* store	- Responder StorageID in which
 *					  object will be stored
 *		  uint32_t* parenthandle- Responder Parent ObjectHandle
 *					  in which the object will be stored
 *		  uint32_t* handle	- Responder's reserved ObjectHandle
 *					  for the incoming object
 **/
uint16_t
ptp_sendobjectinfo (PTPParams* params, uint32_t* store, 
			uint32_t* parenthandle, uint32_t* handle,
			PTPObjectInfo* objectinfo)
{
	PTPContainer	ptp;
	uint16_t	ret;
	unsigned char	*data = NULL;
	uint32_t	size;

	PTP_CNT_INIT(ptp, PTP_OC_SendObjectInfo, *store, *parenthandle);
	size = ptp_pack_OI(params, objectinfo, &data);
	ret = ptp_transaction(params, &ptp, PTP_DP_SENDDATA, size, &data, NULL);
	free(data);
	*store=ptp.Param1;
	*parenthandle=ptp.Param2;
	*handle=ptp.Param3; 
	return ret;
}

/**
 * ptp_sendobject:
 * params:	PTPParams*
 *		char*	object		- contains the object that is to be sent
 *		uint64_t size		- object size
 *		
 * Sends object to Responder.
 *
 * Return values: Some PTP_RC_* code.
 *
 */
uint16_t
ptp_sendobject (PTPParams* params, unsigned char* object, uint64_t size)
{
	PTPContainer ptp;

	PTP_CNT_INIT(ptp, PTP_OC_SendObject);
	return ptp_transaction(params, &ptp, PTP_DP_SENDDATA, size, &object, NULL);
}

/**
 * ptp_sendobject_from_handler:
 * params:	PTPParams*
 *		PTPDataHandler*         - File descriptor to read() object from
 *              uint64_t size           - File/object size
 *
 * Sends object from file descriptor by consecutive reads from this
 * descriptor.
 *
 * Return values: Some PTP_RC_* code.
 **/
uint16_t
ptp_sendobject_from_handler (PTPParams* params, PTPDataHandler *handler, uint64_t size)
{
	PTPContainer ptp;

	PTP_CNT_INIT(ptp, PTP_OC_SendObject);
	return ptp_transaction_new(params, &ptp, PTP_DP_SENDDATA, size, handler);
}


/**
 * ptp_sendobject_fromfd:
 * params:	PTPParams*
 *		fd                      - File descriptor to read() object from
 *              uint64_t size           - File/object size
 *
 * Sends object from file descriptor by consecutive reads from this
 * descriptor.
 *
 * Return values: Some PTP_RC_* code.
 **/
uint16_t
ptp_sendobject_fromfd (PTPParams* params, int fd, uint64_t size)
{
	PTPContainer	ptp;
	PTPDataHandler	handler;
	uint16_t	ret;

	PTP_CNT_INIT(ptp, PTP_OC_SendObject);
	ptp_init_fd_handler (&handler, fd);
	ret = ptp_transaction_new(params, &ptp, PTP_DP_SENDDATA, size, &handler);
	ptp_exit_fd_handler (&handler);
	return ret;
}

#define PROPCACHE_TIMEOUT 5	/* seconds */

uint16_t
ptp_getdevicepropdesc (PTPParams* params, uint16_t propcode, 
			PTPDevicePropDesc* devicepropertydesc)
{
	PTPContainer	ptp;
	uint16_t	ret = PTP_RC_OK;
	unsigned char	*data;
	unsigned int	size;

	PTP_CNT_INIT(ptp, PTP_OC_GetDevicePropDesc, propcode);
	CHECK_PTP_RC(ptp_transaction(params, &ptp, PTP_DP_GETDATA, 0, &data, &size));

	if (!data) {
		ptp_debug (params, "no data received for getdevicepropdesc");
		return PTP_RC_InvalidDevicePropFormat;
	}

	if (params->device_flags & DEVICE_FLAG_OLYMPUS_XML_WRAPPED) {
#ifdef HAVE_LIBXML2
		xmlNodePtr	code;

		ret = ptp_olympus_parse_output_xml (params,(char*)data,size,&code);
		if (ret == PTP_RC_OK) {
			int x;

			if (	(xmlChildElementCount(code) == 1) &&
					(!strcmp((char*)code->name,"c1014"))
					) {
				code = xmlFirstElementChild (code);

				if (	(sscanf((char*)code->name,"p%x", &x)) &&
						(x == propcode)
						) {
					ret = parse_9301_propdesc (params, xmlFirstElementChild (code), devicepropertydesc);
					xmlFreeDoc(code->doc);
				}
			}
		} else {
			ptp_debug(params,"failed to parse output xml, ret %x?", ret);
		}
#endif
	} else {
		ptp_unpack_DPD(params, data, devicepropertydesc, size);
	}
	free(data);
	return ret;
}


uint16_t
ptp_getdevicepropvalue (PTPParams* params, uint16_t propcode,
			PTPPropertyValue* value, uint16_t datatype)
{
	PTPContainer	ptp;
	unsigned char	*data;
	unsigned int	size, offset = 0;
	uint16_t	ret;

	PTP_CNT_INIT(ptp, PTP_OC_GetDevicePropValue, propcode);
	CHECK_PTP_RC(ptp_transaction(params, &ptp, PTP_DP_GETDATA, 0, &data, &size));
	ret = ptp_unpack_DPV(params, data, &offset, size, value, datatype) ? PTP_RC_OK : PTP_RC_GeneralError;
	if (ret != PTP_RC_OK)
		ptp_debug (params, "ptp_getdevicepropvalue: unpacking DPV failed");
	free(data);
	return ret;
}

uint16_t
ptp_setdevicepropvalue (PTPParams* params, uint16_t propcode,
			PTPPropertyValue *value, uint16_t datatype)
{
	PTPContainer	ptp;
	uint16_t	ret;
	unsigned char	*data = NULL;
	uint32_t	size;

	PTP_CNT_INIT(ptp, PTP_OC_SetDevicePropValue, propcode);
	size=ptp_pack_DPV(params, value, &data, datatype);
	ret=ptp_transaction(params, &ptp, PTP_DP_SENDDATA, size, &data, NULL);
	free(data);
	return ret;
}

/**
 * ptp_ek_sendfileobjectinfo:
 * params:	PTPParams*
 *		uint32_t* store		- destination StorageID on Responder
 *		uint32_t* parenthandle 	- Parent ObjectHandle on responder
 * 		uint32_t* handle	- see Return values
 *		PTPObjectInfo* objectinfo- ObjectInfo that is to be sent
 * 
 * Sends ObjectInfo of file that is to be sent via SendFileObject.
 *
 * Return values: Some PTP_RC_* code.
 * Upon success : uint32_t* store	- Responder StorageID in which
 *					  object will be stored
 *		  uint32_t* parenthandle- Responder Parent ObjectHandle
 *					  in which the object will be stored
 *		  uint32_t* handle	- Responder's reserved ObjectHandle
 *					  for the incoming object
 **/
uint16_t
ptp_ek_sendfileobjectinfo (PTPParams* params, uint32_t* store, 
			uint32_t* parenthandle, uint32_t* handle,
			PTPObjectInfo* objectinfo)
{
	PTPContainer	ptp;
	uint16_t	ret;
	unsigned char	*data = NULL;
	uint32_t	size;

	PTP_CNT_INIT(ptp, PTP_OC_EK_SendFileObjectInfo, *store, *parenthandle);
	size=ptp_pack_OI(params, objectinfo, &data);
	ret=ptp_transaction(params, &ptp, PTP_DP_SENDDATA, size, &data, NULL);
	free(data);
	*store=ptp.Param1;
	*parenthandle=ptp.Param2;
	*handle=ptp.Param3; 
	return ret;
}

/**
 * ptp_ek_getserial:
 * params:	PTPParams*
 *		char**	serial		- contains the serial number of the camera
 *		uint32_t* size		- contains the string length
 *		
 * Gets the serial number from the device. (ptp serial)
 *
 * Return values: Some PTP_RC_* code.
 *
 */
uint16_t
ptp_ek_getserial (PTPParams* params, unsigned char **data, unsigned int *size)
{
	PTPContainer ptp;

	PTP_CNT_INIT(ptp, PTP_OC_EK_GetSerial);
	return ptp_transaction(params, &ptp, PTP_DP_GETDATA, 0, data, size); 
}

/**
 * ptp_ek_setserial:
 * params:	PTPParams*
 *		char*	serial		- contains the new serial number
 *		uint32_t size		- string length
 *		
 * Sets the serial number of the device. (ptp serial)
 *
 * Return values: Some PTP_RC_* code.
 *
 */
uint16_t
ptp_ek_setserial (PTPParams* params, unsigned char *data, unsigned int size)
{
	PTPContainer ptp;

	PTP_CNT_INIT(ptp, PTP_OC_EK_SetSerial);
	return ptp_transaction(params, &ptp, PTP_DP_SENDDATA, size, &data, NULL); 
}

/* unclear what it does yet */
uint16_t
ptp_ek_9007 (PTPParams* params, unsigned char **data, unsigned int *size)
{
	PTPContainer ptp;

	PTP_CNT_INIT(ptp, 0x9007);
	return ptp_transaction(params, &ptp, PTP_DP_GETDATA, 0, data, size); 
}

/* unclear what it does yet */
uint16_t
ptp_ek_9009 (PTPParams* params, uint32_t *p1, uint32_t *p2)
{
	PTPContainer	ptp;

	PTP_CNT_INIT(ptp, 0x9009);
	*p1 = *p2 = 0;
	CHECK_PTP_RC(ptp_transaction(params, &ptp, PTP_DP_NODATA, 0, NULL, NULL));
	*p1 = ptp.Param1;
	*p2 = ptp.Param2;
	return PTP_RC_OK;
}

/* unclear yet, but I guess it returns the info from 9008 */
uint16_t
ptp_ek_900c (PTPParams* params, unsigned char **data, unsigned int *size)
{
	PTPContainer ptp;

	PTP_CNT_INIT(ptp, 0x900c);
	return ptp_transaction(params, &ptp, PTP_DP_GETDATA, 0, data, size); 
	/* returned data is 16bit,16bit,32bit,32bit */
}

/**
 * ptp_ek_settext:
 * params:	PTPParams*
 *		PTPEKTextParams*	- contains the texts to display.
 *		
 * Displays the specified texts on the TFT of the camera.
 *
 * Return values: Some PTP_RC_* code.
 *
 */
uint16_t
ptp_ek_settext (PTPParams* params, PTPEKTextParams *text)
{
	PTPContainer	ptp;
	uint16_t	ret;
	unsigned char	*data = 0;
	uint32_t	size;

	PTP_CNT_INIT(ptp, PTP_OC_EK_SetText);
	if (0 == (size = ptp_pack_EK_text(params, text, &data)))
		return PTP_ERROR_BADPARAM;
	ret = ptp_transaction(params, &ptp, PTP_DP_SENDDATA, size, &data, NULL);
	free(data);
	return ret;
}

/**
 * ptp_ek_sendfileobject:
 * params:	PTPParams*
 *		char*	object		- contains the object that is to be sent
 *		uint32_t size		- object size
 *		
 * Sends object to Responder.
 *
 * Return values: Some PTP_RC_* code.
 *
 */
uint16_t
ptp_ek_sendfileobject (PTPParams* params, unsigned char* object, uint32_t size)
{
	PTPContainer ptp;

	PTP_CNT_INIT(ptp, PTP_OC_EK_SendFileObject);
	return ptp_transaction(params, &ptp, PTP_DP_SENDDATA, size, &object, NULL);
}

/**
 * ptp_ek_sendfileobject_from_handler:
 * params:	PTPParams*
 *		PTPDataHandler*	handler	- contains the handler of the object that is to be sent
 *		uint32_t size		- object size
 *		
 * Sends object to Responder.
 *
 * Return values: Some PTP_RC_* code.
 *
 */
uint16_t
ptp_ek_sendfileobject_from_handler (PTPParams* params, PTPDataHandler*handler, uint32_t size)
{
	PTPContainer ptp;

	PTP_CNT_INIT(ptp, PTP_OC_EK_SendFileObject);
	return ptp_transaction_new(params, &ptp, PTP_DP_SENDDATA, size, handler);
}

/*************************************************************************
 *
 * Canon PTP extensions support
 *
 * (C) Nikolai Kopanygin 2003
 *
 *************************************************************************/


/**
 * ptp_canon_getpartialobjectinfo:
 * params:	PTPParams*
 *		uint32_t handle		- ObjectHandle
 *		uint32_t p2 		- Not fully understood parameter
 *					  0 - returns full size
 *					  1 - returns thumbnail size (or EXIF?)
 * 
 * Gets form the responder the size of the specified object.
 *
 * Return values: Some PTP_RC_* code.
 * Upon success : uint32_t* size	- The object size
 *		  uint32_t* rp2		- Still unknown return parameter
 *                                        (perhaps upper 32bit of size)
 *
 *
 **/
uint16_t
ptp_canon_getpartialobjectinfo (PTPParams* params, uint32_t handle, uint32_t p2, 
			uint32_t* size, uint32_t* rp2) 
{
	PTPContainer	ptp;
	
	PTP_CNT_INIT(ptp, PTP_OC_CANON_GetPartialObjectInfo, handle, p2);
	*size = *rp2 = 0;
	CHECK_PTP_RC(ptp_transaction(params, &ptp, PTP_DP_NODATA, 0, NULL, NULL));
	*size=ptp.Param1;
	*rp2=ptp.Param2;
	return PTP_RC_OK;
}

/**
 * ptp_canon_get_mac_address:
 * params:	PTPParams*
 *					  value 0 works.
 * Gets the MAC address of the wireless transmitter.
 *
 * Return values: Some PTP_RC_* code.
 * Upon success : unsigned char* mac	- The MAC address
 *
 **/
uint16_t
ptp_canon_get_mac_address (PTPParams* params, unsigned char **mac)
{
	PTPContainer ptp;

        PTP_CNT_INIT(ptp, PTP_OC_CANON_GetMACAddress);
	return ptp_transaction(params, &ptp, PTP_DP_GETDATA, 0, mac, NULL);
}

/**
 * ptp_canon_get_directory:
 * params:	PTPParams*

 * Gets the full directory of the camera.
 *
 * Return values: Some PTP_RC_* code.
 * Upon success : PTPObjectHandles        *handles	- filled out with handles
 * 		  PTPObjectInfo           **oinfos	- allocated array of PTP Object Infos
 * 		  uint32_t                **flags	- allocated array of CANON Flags
 *
 **/
uint16_t
ptp_canon_get_directory (PTPParams* params,
	PTPObjectHandles	*handles,
	PTPObjectInfo		**oinfos,	/* size(handles->n) */
	uint32_t		**flags		/* size(handles->n) */
) {
	PTPContainer	ptp;
	unsigned char	*data;
	uint16_t	ret;

	PTP_CNT_INIT(ptp, PTP_OC_CANON_GetDirectory);
	CHECK_PTP_RC(ptp_transaction(params, &ptp, PTP_DP_GETDATA, 0, &data, NULL));
	ret = ptp_unpack_canon_directory(params, data, ptp.Param1, handles, oinfos, flags);
	free (data);
	return ret;
}

/**
 * ptp_canon_gettreeinfo:
 * params:	PTPParams*
 *              uint32_t *out
 * 
 * Switches the camera display to on and lets the user
 * select what to transfer. Sends a 0xc011 event when started 
 * and 0xc013 if direct transfer aborted.
 *
 * Return values: Some PTP_RC_* code.
 *
 **/
uint16_t
ptp_canon_gettreeinfo (PTPParams* params, uint32_t *out)
{
	PTPContainer	ptp;

	PTP_CNT_INIT(ptp, PTP_OC_CANON_GetTreeInfo, 0xf);
	CHECK_PTP_RC(ptp_transaction(params, &ptp, PTP_DP_NODATA, 0, NULL, NULL));
	if (ptp.Nparam > 0)
		*out = ptp.Param1;
	return PTP_RC_OK;
}

/**
 * ptp_canon_getpairinginfo:
 * params:	PTPParams*
 *              int nr
 * 
 * Get the pairing information.
 *
 * Return values: Some PTP_RC_* code.
 *
 **/
uint16_t
ptp_canon_getpairinginfo (PTPParams* params, uint32_t nr, unsigned char **data, unsigned int *size)
{
	PTPContainer ptp;
	
	PTP_CNT_INIT(ptp, PTP_OC_CANON_GetPairingInfo, nr);
	return ptp_transaction(params, &ptp, PTP_DP_GETDATA, 0, data, size);
}

/**
 * ptp_canon_get_target_handles:
 * params:	PTPParams*
 *              PTPCanon_directtransfer_entry **out
 *              unsigned int *outsize
 * 
 * Retrieves direct transfer entries specifying the images to transfer
 * from the camera (to be retrieved after 0xc011 event).
 *
 * Return values: Some PTP_RC_* code.
 *
 **/
uint16_t
ptp_canon_gettreesize (PTPParams* params,
	PTPCanon_directtransfer_entry **entries, unsigned int *cnt)
{
	PTPContainer	ptp;
	uint16_t	ret = PTP_RC_OK;
	unsigned char	*data, *cur;
	unsigned int	size, i;

	PTP_CNT_INIT(ptp, PTP_OC_CANON_GetTreeSize);
	CHECK_PTP_RC(ptp_transaction(params, &ptp, PTP_DP_GETDATA, 0, &data, &size));
	*cnt = dtoh32a(data);
	*entries = malloc(sizeof(PTPCanon_directtransfer_entry)*(*cnt));
	if (!*entries) {
		ret = PTP_RC_GeneralError;
		goto exit;
	}
	cur = data+4;
	for (i=0;i<*cnt;i++) {
		unsigned char len;
		(*entries)[i].oid = dtoh32a(cur);
		(*entries)[i].str = ptp_unpack_string(params, cur, 4, size-(cur-data-4), &len);
		cur += 4+(cur[4]*2+1);
	}
exit:
	free (data);
	return ret;
}

/**
 * ptp_canon_checkevent:
 * params:	PTPParams*
 * 
 * The camera has a FIFO stack, in which it accumulates events.
 * Partially these events are communicated also via the USB interrupt pipe
 * according to the PTP USB specification, partially not.
 * This operation returns from the device a block of data, empty,
 * if the event stack is empty, or filled with an event's data otherwise.
 * The event is removed from the stack in the latter case.
 * The Remote Capture app sends this command to the camera all the time
 * of connection, filling with it the gaps between other operations. 
 *
 * Return values: Some PTP_RC_* code.
 * Upon success : PTPUSBEventContainer* event	- is filled with the event data
 *						  if any
 *                int *isevent			- returns 1 in case of event
 *						  or 0 otherwise
 **/
uint16_t
ptp_canon_checkevent (PTPParams* params, PTPContainer* event, int* isevent)
{
	PTPContainer	ptp;
	unsigned char	*data;
	unsigned int	size;
	
	PTP_CNT_INIT(ptp, PTP_OC_CANON_CheckEvent);
	*isevent=0;
	CHECK_PTP_RC(ptp_transaction(params, &ptp, PTP_DP_GETDATA, 0, &data, &size));
	if (data && size) { /* check if we had a successfull call with data */
		ptp_unpack_EC(params, data, event, size);
		*isevent=1;
		free(data);
	}
	return PTP_RC_OK;
}

uint16_t
ptp_add_event (PTPParams *params, PTPContainer *evt)
{
	params->events = realloc(params->events, sizeof(PTPContainer)*(params->nrofevents+1));
	memcpy (&params->events[params->nrofevents],evt,1*sizeof(PTPContainer));
	params->nrofevents += 1;
	return PTP_RC_OK;
}

static void
handle_event_internal (PTPParams *params, PTPContainer *event)
{
	/* handle some PTP stack internal events */
	switch (event->Code) {
	case PTP_EC_DevicePropChanged: {
		unsigned int i;

		/* mark the property for a forced refresh on the next query */
		for (i=0;i<params->nrofdeviceproperties;i++)
			if (params->deviceproperties[i].desc.DevicePropertyCode == event->Param1) {
				params->deviceproperties[i].timestamp = 0;
				break;
			}
		break;
	}
	case PTP_EC_StoreAdded:
	case PTP_EC_StoreRemoved: {
		int i;

		/* refetch storage IDs and also invalidate whole object tree */
		free (params->storageids.Storage);
		params->storageids.Storage	= NULL;
		params->storageids.n 		= 0;
		ptp_getstorageids (params, &params->storageids);

		/* free object storage as it might be associated with the storage ids */
		/* FIXME: enhance and just delete the ones from the storage */
		for (i=0;i<params->nrofobjects;i++)
			ptp_free_object (&params->objects[i]);
		free (params->objects);
		params->objects 		= NULL;
		params->nrofobjects 		= 0;

		params->storagechanged		= 1;
		break;
	}
	default: /* check if we should handle it internally too */
		break;
	}
}

uint16_t
ptp_check_event_queue (PTPParams *params)
{
	PTPContainer	event;
	uint16_t	ret;

	/* We try to do a event check without I/O */
	/* Basically this means just looking at the meanwhile queued events */

	ret = params->event_check_queue(params,&event);

	if (ret == PTP_RC_OK) {
		ptp_debug (params, "event: nparams=0x%X, code=0x%X, trans_id=0x%X, p1=0x%X, p2=0x%X, p3=0x%X", event.Nparam,event.Code,event.Transaction_ID, event.Param1, event.Param2, event.Param3);
		ptp_add_event (params, &event);
		handle_event_internal (params, &event);
	}
	if (ret == PTP_ERROR_TIMEOUT) /* ok, just new events */
		ret = PTP_RC_OK;
	return ret;
}

uint16_t
ptp_check_event (PTPParams *params)
{
	PTPContainer	event;
	uint16_t	ret;

	/* Method offered by Nikon DSLR, Nikon 1, and some older Nikon Coolpix P*
	 * The Nikon Coolpix P2 however does not return anything. So if we never get
	 * events from here, use the ptp "interrupt" method */
	if (	(params->deviceinfo.VendorExtensionID == PTP_VENDOR_NIKON) &&
		ptp_operation_issupported(params, PTP_OC_NIKON_CheckEvent)
	) {
		unsigned int evtcnt = 0, i;
		PTPContainer *xevent = NULL;

		ret = ptp_nikon_check_event(params, &xevent, &evtcnt);
		if (ret != PTP_RC_OperationNotSupported)
			CHECK_PTP_RC(ret);

		if (evtcnt) {
			for (i = 0; i < evtcnt; i++)
				handle_event_internal (params, &xevent[i]);
			params->events = realloc(params->events, sizeof(PTPContainer)*(evtcnt+params->nrofevents));
			memcpy (&params->events[params->nrofevents],xevent,evtcnt*sizeof(PTPContainer));
			params->nrofevents += evtcnt;
			free (xevent);
			params->event90c7works = 1;
		}
		if (params->event90c7works)
			return PTP_RC_OK;
		/* fall through to generic event handling */
	}
	/* should not get here ... EOS has no normal PTP events and another queue handling. */
	if (	(params->deviceinfo.VendorExtensionID == PTP_VENDOR_CANON) &&
		ptp_operation_issupported(params, PTP_OC_CANON_EOS_GetEvent)
	) {
		return PTP_RC_OK;
	}

	if (	(params->deviceinfo.VendorExtensionID == PTP_VENDOR_CANON) &&
		ptp_operation_issupported(params, PTP_OC_CANON_CheckEvent)
	) {
		int isevent;

		CHECK_PTP_RC(ptp_canon_checkevent (params,&event,&isevent));

		if (isevent) {
			ret = PTP_RC_OK;
			goto store_event;
		}
		/* Event Emulate Mode 0 (unset) and 1-5 get interrupt events. 6-7 does not. */
		if (params->canon_event_mode > 5)
			return PTP_RC_OK;

		/* FIXME: fallthrough or return? */
#ifdef __APPLE__
		/* the libusb 1 on darwin currently does not like polling
		 * for interrupts, they have no timeout for it. 2010/08/23
		 * Check back in 2011 or so. -Marcus
		 */
		return PTP_RC_OK;
#endif
	}
	ret = params->event_check(params,&event);

store_event:
	if (ret == PTP_RC_OK) {
		ptp_debug (params, "event: nparams=0x%X, code=0x%X, trans_id=0x%X, p1=0x%X, p2=0x%X, p3=0x%X", event.Nparam,event.Code,event.Transaction_ID, event.Param1, event.Param2, event.Param3);
		ptp_add_event (params, &event);

		handle_event_internal (params, &event);

	
	}
	if (ret == PTP_ERROR_TIMEOUT) /* ok, just new events */
		ret = PTP_RC_OK;
	return ret;
}

uint16_t
ptp_wait_event (PTPParams *params)
{
	PTPContainer	event;
	uint16_t	ret;

	ret = params->event_wait(params,&event);
	if (ret == PTP_RC_OK) {
		ptp_debug (params, "event: nparams=0x%X, code=0x%X, trans_id=0x%X, p1=0x%X, p2=0x%X, p3=0x%X", event.Nparam,event.Code,event.Transaction_ID, event.Param1, event.Param2, event.Param3);
		ptp_add_event (params, &event);

		handle_event_internal (params, &event);
	}
	if (ret == PTP_ERROR_TIMEOUT) /* ok, just new events */
		ret = PTP_RC_OK;
	return ret;
}


int
ptp_get_one_event(PTPParams *params, PTPContainer *event)
{
	if (!params->nrofevents)
		return 0;
	memcpy (event, params->events, sizeof(PTPContainer));
	memmove (params->events, params->events+1, sizeof(PTPContainer)*(params->nrofevents-1));
	/* do not realloc on shrink. */
	params->nrofevents--;
	if (!params->nrofevents) {
		free (params->events);
		params->events = NULL;
	}
	return 1;
}

/**
 * ptp_canon_eos_getevent:
 * 
 * This retrieves configuration status/updates/changes
 * on EOS cameras. It reads a datablock which has a list of variable
 * sized structures.
 *
 * params:	PTPParams*
 *
 * Return values: Some PTP_RC_* code.
 *
 **/
uint16_t
ptp_canon_eos_getevent (PTPParams* params, PTPCanon_changes_entry **entries, int *nrofentries)
{
	PTPContainer	ptp;
	unsigned char	*data;
	unsigned int 	size;

	PTP_CNT_INIT(ptp, PTP_OC_CANON_EOS_GetEvent);
	*nrofentries = 0;
	*entries = NULL;
	CHECK_PTP_RC(ptp_transaction(params, &ptp, PTP_DP_GETDATA, 0, &data, &size));
	*nrofentries = ptp_unpack_CANON_changes(params,data,size,entries);
	free (data);
	return PTP_RC_OK;
}

uint16_t
ptp_check_eos_events (PTPParams *params)
{
	PTPCanon_changes_entry	*entries = NULL, *nentries;
	int			nrofentries = 0;

	while (1) { /* call it repeatedly until the camera does not report any */
		CHECK_PTP_RC(ptp_canon_eos_getevent (params, &entries, &nrofentries));
		if (!nrofentries)
			return PTP_RC_OK;

		if (params->nrofbacklogentries) {
			nentries = realloc(params->backlogentries,sizeof(entries[0])*(params->nrofbacklogentries+nrofentries));
			if (!nentries)
				return PTP_RC_GeneralError;
			params->backlogentries = nentries;
			memcpy (nentries+params->nrofbacklogentries, entries, nrofentries*sizeof(entries[0]));
			params->nrofbacklogentries += nrofentries;
			free (entries);
		} else {
			params->backlogentries = entries;
			params->nrofbacklogentries = nrofentries;
		}
	}
	return PTP_RC_OK;
}

int
ptp_get_one_eos_event (PTPParams *params, PTPCanon_changes_entry *entry)
{
	if (!params->nrofbacklogentries)
		return 0;
	memcpy (entry, params->backlogentries, sizeof(*entry));
	if (params->nrofbacklogentries > 1) {
		memmove (params->backlogentries,params->backlogentries+1,sizeof(*entry)*(params->nrofbacklogentries-1));
		params->nrofbacklogentries--;
	} else {
		free (params->backlogentries);
		params->backlogentries = NULL;
		params->nrofbacklogentries = 0;
	}
	return 1;
}


uint16_t
ptp_canon_eos_getdevicepropdesc (PTPParams* params, uint16_t propcode,
	PTPDevicePropDesc *dpd)
{
	unsigned int i;

	for (i=0;i<params->nrofcanon_props;i++)
		if (params->canon_props[i].proptype == propcode)
			break;
	if (params->nrofcanon_props == i)
		return PTP_RC_Undefined;
	memcpy (dpd, &params->canon_props[i].dpd, sizeof (*dpd));
	if (dpd->FormFlag == PTP_DPFF_Enumeration) {
		/* need to duplicate the Enumeration alloc */
		dpd->FORM.Enum.SupportedValue = malloc (sizeof (PTPPropertyValue)*dpd->FORM.Enum.NumberOfValues);
		memcpy (dpd->FORM.Enum.SupportedValue,
			params->canon_props[i].dpd.FORM.Enum.SupportedValue,
			sizeof (PTPPropertyValue)*dpd->FORM.Enum.NumberOfValues
		);
	}
	if (dpd->DataType == PTP_DTC_STR) {
		dpd->FactoryDefaultValue.str = strdup( params->canon_props[i].dpd.FactoryDefaultValue.str );
		dpd->CurrentValue.str = strdup( params->canon_props[i].dpd.CurrentValue.str );
	}

	return PTP_RC_OK;
}


uint16_t
ptp_canon_eos_getstorageids (PTPParams* params, PTPStorageIDs* storageids)
{
	PTPContainer	ptp;
	unsigned char	*data;
	unsigned int	size;

	PTP_CNT_INIT(ptp, PTP_OC_CANON_EOS_GetStorageIDs);
	CHECK_PTP_RC(ptp_transaction(params, &ptp, PTP_DP_GETDATA, 0, &data, &size));
	ptp_unpack_SIDs(params, data, storageids, size);
	free(data);
	return PTP_RC_OK;
}

uint16_t
ptp_canon_eos_getstorageinfo (PTPParams* params, uint32_t p1, unsigned char **data, unsigned int *size)
{
	PTPContainer	ptp;
	
	PTP_CNT_INIT(ptp, PTP_OC_CANON_EOS_GetStorageInfo, p1);
	return ptp_transaction(params, &ptp, PTP_DP_GETDATA, 0, data, size);
	/* FIXME: do stuff with data */
}

uint16_t
ptp_canon_eos_getobjectinfoex (
	PTPParams* params, uint32_t storageid, uint32_t oid, uint32_t unk,
	PTPCANONFolderEntry **entries, unsigned int *nrofentries
) {
	PTPContainer	ptp;
	uint16_t	ret = PTP_RC_OK;
	unsigned char	*data, *xdata;
	unsigned int	size, i;

	PTP_CNT_INIT(ptp, PTP_OC_CANON_EOS_GetObjectInfoEx, storageid, oid, unk);
	CHECK_PTP_RC(ptp_transaction(params, &ptp, PTP_DP_GETDATA, 0, &data, &size));
	if (!data) {
		*nrofentries = 0;
		return PTP_RC_OK;
	}

	if (size < 4) {
		ret = PTP_RC_GeneralError;
		goto exit;
	}
	/* check for integer overflow */
	if (dtoh32a(data) >= INT_MAX/sizeof(PTPCANONFolderEntry))  {
		ret = PTP_RC_GeneralError;
		goto exit;
	}

	*nrofentries = dtoh32a(data);
	*entries = malloc(*nrofentries * sizeof(PTPCANONFolderEntry));
	if (!*entries) {
		ret = PTP_RC_GeneralError;
		goto exit;
	}

	xdata = data+sizeof(uint32_t);
	for (i=0;i<*nrofentries;i++) {
		if ((dtoh32a(xdata) + (xdata-data)) > size) {
			ptp_debug (params, "reading canon FEs run over read data size?\n");
			free (*entries);
			*entries = NULL;
			*nrofentries = 0;
			ret = PTP_RC_GeneralError;
			goto exit;
		}
		ptp_unpack_Canon_EOS_FE (params, &xdata[4], &((*entries)[i]));
		xdata += dtoh32a(xdata);
	}
exit:
	free (data);
	return ret;
}

/**
 * ptp_canon_eos_getpartialobject:
 * 
 * This retrieves a part of an PTP object which you specify as object id.
 * The id originates from 0x9116 call.
 * After finishing it, we seem to need to call ptp_canon_eos_enddirecttransfer.
 *
 * params:	PTPParams*
 * 		oid		Object ID
 * 		offset		The offset where to start the data transfer 
 *		xsize		Size in bytes of the transfer to do
 *		data		Pointer that receives the malloc()ed memory of the transfer.
 *
 * Return values: Some PTP_RC_* code.
 *
 */
uint16_t
ptp_canon_eos_getpartialobject (PTPParams* params, uint32_t oid, uint32_t offset, uint32_t xsize, unsigned char**data)
{
	PTPContainer	ptp;

	PTP_CNT_INIT(ptp, PTP_OC_CANON_EOS_GetPartialObject, oid, offset, xsize);
	return ptp_transaction(params, &ptp, PTP_DP_GETDATA, 0, data, NULL);
}

uint16_t
ptp_canon_eos_setdevicepropvalueex (PTPParams* params, unsigned char* data, unsigned int size)
{
	PTPContainer	ptp;

	PTP_CNT_INIT(ptp, PTP_OC_CANON_EOS_SetDevicePropValueEx);
	return ptp_transaction(params, &ptp, PTP_DP_SENDDATA, size, &data, NULL);
}

uint16_t
ptp_canon_eos_setdevicepropvalue (PTPParams* params,
	uint16_t propcode, PTPPropertyValue *value, uint16_t datatype
) {
	PTPContainer	ptp;
	uint16_t	ret;
	unsigned char	*data = NULL;
	unsigned int	i, size;

	PTP_CNT_INIT(ptp, PTP_OC_CANON_EOS_SetDevicePropValueEx);

	for (i=0;i<params->nrofcanon_props;i++)
		if (params->canon_props[i].proptype == propcode)
			break;
	if (params->nrofcanon_props == i)
		return PTP_RC_Undefined;

	switch (propcode) {
	case PTP_DPC_CANON_EOS_ImageFormat:
	case PTP_DPC_CANON_EOS_ImageFormatCF:
	case PTP_DPC_CANON_EOS_ImageFormatSD:
	case PTP_DPC_CANON_EOS_ImageFormatExtHD:
		/* special handling of ImageFormat properties */
		size = 8 + ptp_pack_EOS_ImageFormat( params, NULL, value->u16 );
		data = malloc( size );
		if (!data) return PTP_RC_GeneralError;
		params->canon_props[i].dpd.CurrentValue.u16 = value->u16;
		ptp_pack_EOS_ImageFormat( params, data + 8, value->u16 );
		break;
	case PTP_DPC_CANON_EOS_CustomFuncEx:
		/* special handling of CustomFuncEx properties */
		ptp_debug (params, "ptp2/ptp_canon_eos_setdevicepropvalue: setting EOS prop %x to %s",propcode,value->str);
		size = 8 + ptp_pack_EOS_CustomFuncEx( params, NULL, value->str );
		data = malloc( size );
		if (!data) return PTP_RC_GeneralError;
		params->canon_props[i].dpd.CurrentValue.str = strdup( value->str );
		ptp_pack_EOS_CustomFuncEx( params, data + 8, value->str );
		break;
	default:
		if (datatype != PTP_DTC_STR) {
			data = calloc(3,sizeof(uint32_t));
			if (!data) return PTP_RC_GeneralError;
			size = sizeof(uint32_t)*3;
		} else {
			size = strlen(value->str) + 1 + 8;
			data = calloc(size,sizeof(char));
			if (!data) return PTP_RC_GeneralError;
		}
		switch (datatype) {
		case PTP_DTC_INT8:
		case PTP_DTC_UINT8:
			/*fprintf (stderr, "%x -> %d\n", propcode, value->u8);*/
			htod8a(&data[8], value->u8);
			params->canon_props[i].dpd.CurrentValue.u8 = value->u8;
			break;
		case PTP_DTC_UINT16:
		case PTP_DTC_INT16:
			/*fprintf (stderr, "%x -> %d\n", propcode, value->u16);*/
			htod16a(&data[8], value->u16);
			params->canon_props[i].dpd.CurrentValue.u16 = value->u16;
			break;
		case PTP_DTC_INT32:
		case PTP_DTC_UINT32:
			/*fprintf (stderr, "%x -> %d\n", propcode, value->u32);*/
			htod32a(&data[8], value->u32);
			params->canon_props[i].dpd.CurrentValue.u32 = value->u32;
			break;
		case PTP_DTC_STR:
			strcpy((char*)data + 8, value->str);
			free (params->canon_props[i].dpd.CurrentValue.str);
			params->canon_props[i].dpd.CurrentValue.str = strdup(value->str);
			break;
		}
	}

	htod32a(&data[0], size);
	htod32a(&data[4], propcode);

	ret = ptp_transaction(params, &ptp, PTP_DP_SENDDATA, size, &data, NULL);
	free (data);
	return ret;
}

/**
 * ptp_canon_getpartialobject:
 *
 * This operation is used to read from the device a data 
 * block of an object from a specified offset.
 *
 * params:	PTPParams*
 *      uint32_t handle - the handle of the requested object
 *      uint32_t offset - the offset in bytes from the beginning of the object
 *      uint32_t size - the requested size of data block to read
 *      uint32_t pos - 1 for the first block, 2 - for a block in the middle,
 *                  3 - for the last block
 *
 * Return values: Some PTP_RC_* code.
 *      char **block - the pointer to the block of data read
 *      uint32_t* readnum - the number of bytes read
 *
 **/
uint16_t
ptp_canon_getpartialobject (PTPParams* params, uint32_t handle, 
				uint32_t offset, uint32_t size,
				uint32_t pos, unsigned char** block, 
				uint32_t* readnum)
{
	PTPContainer	ptp;
	uint16_t	ret;
	unsigned char	*data;
	
	PTP_CNT_INIT(ptp, PTP_OC_CANON_GetPartialObjectEx, handle, offset, size, pos);
	ret=ptp_transaction(params, &ptp, PTP_DP_GETDATA, 0, &data, NULL);
	if (ret==PTP_RC_OK) {
		*block=data;
		*readnum=ptp.Param1;
	}
	free (data);
	return ret;
}

/**
 * ptp_canon_getviewfinderimage:
 *
 * This operation can be used to read the image which is currently
 * in the camera's viewfinder. The image size is 320x240, format is JPEG.
 * Of course, prior to calling this operation, one must turn the viewfinder
 * on with the CANON_ViewfinderOn command.
 * Invoking this operation many times, one can get live video from the camera!
 * 
 * params:	PTPParams*
 * 
 * Return values: Some PTP_RC_* code.
 *      char **image - the pointer to the read image
 *      unit32_t *size - the size of the image in bytes
 *
 **/
uint16_t
ptp_canon_getviewfinderimage (PTPParams* params, unsigned char** image, uint32_t* size)
{
	PTPContainer	ptp;
	uint16_t	ret;
	
	PTP_CNT_INIT(ptp, PTP_OC_CANON_GetViewfinderImage);
	ret=ptp_transaction(params, &ptp, PTP_DP_GETDATA, 0, image, NULL);
	if (ret==PTP_RC_OK)
		*size=ptp.Param1;
	return ret;
}

/**
 * ptp_canon_getchanges:
 *
 * This is an interesting operation, about the effect of which I am not sure.
 * This command is called every time when a device property has been changed 
 * with the SetDevicePropValue operation, and after some other operations.
 * This operation reads the array of Device Properties which have been changed
 * by the previous operation.
 * Probably, this operation is even required to make those changes work.
 *
 * params:	PTPParams*
 * 
 * Return values: Some PTP_RC_* code.
 *      uint16_t** props - the pointer to the array of changed properties
 *      uint32_t* propnum - the number of elements in the *props array
 *
 **/
uint16_t
ptp_canon_getchanges (PTPParams* params, uint16_t** props, uint32_t* propnum)
{
	PTPContainer	ptp;
	unsigned char	*data;
	unsigned int	size;
	
	PTP_CNT_INIT(ptp, PTP_OC_CANON_GetChanges);
	CHECK_PTP_RC(ptp_transaction(params, &ptp, PTP_DP_GETDATA, 0, &data, &size));
	*propnum=ptp_unpack_uint16_t_array(params,data,0,size,props);
	free(data);
	return PTP_RC_OK;
}

/**
 * ptp_canon_getobjectinfo:
 *
 * This command reads a specified object's record in a device's filesystem,
 * or the records of all objects belonging to a specified folder (association).
 *  
 * params:	PTPParams*
 *      uint32_t store - StorageID,
 *      uint32_t p2 - Yet unknown (0 value works OK)
 *      uint32_t parent - Parent Object Handle
 *                      # If Parent Object Handle is 0xffffffff, 
 *                      # the Parent Object is the top level folder.
 *      uint32_t handle - Object Handle
 *                      # If Object Handle is 0, the records of all objects 
 *                      # belonging to the Parent Object are read.
 *                      # If Object Handle is not 0, only the record of this 
 *                      # Object is read.
 *
 * Return values: Some PTP_RC_* code.
 *      PTPCANONFolderEntry** entries - the pointer to the folder entry array
 *      uint32_t* entnum - the number of elements of the array
 *
 **/
uint16_t
ptp_canon_getobjectinfo (PTPParams* params, uint32_t store, uint32_t p2, 
			    uint32_t parent, uint32_t handle, 
			    PTPCANONFolderEntry** entries, uint32_t* entnum)
{
	PTPContainer	ptp;
	uint16_t	ret;
	unsigned char	*data;
	unsigned int	i, size;
	
	*entnum = 0;
	*entries = NULL;
	PTP_CNT_INIT(ptp, PTP_OC_CANON_GetObjectInfoEx, store, p2, parent, handle);
	data = NULL;
	size = 0;
	ret=ptp_transaction(params, &ptp, PTP_DP_GETDATA, 0, &data, NULL);
	if (ret != PTP_RC_OK)
		goto exit;
	if (!data)
		return ret;
	if (ptp.Param1 > size/PTP_CANON_FolderEntryLen) {
		ptp_debug (params, "param1 is %d, size is only %d", ptp.Param1, size);
		ret = PTP_RC_GeneralError;
		goto exit;
	}

	*entnum = ptp.Param1;
	*entries= calloc(*entnum, sizeof(PTPCANONFolderEntry));
	if (*entries == NULL) {
		ret = PTP_RC_GeneralError;
		goto exit;
	}
	for(i=0; i<(*entnum); i++) {
		if (size < i*PTP_CANON_FolderEntryLen) break;
		ptp_unpack_Canon_FE(params,
				    data+i*PTP_CANON_FolderEntryLen,
				    &((*entries)[i]) );
	}

exit:
	free (data);
	return ret;
}

/**
 * ptp_canon_get_objecthandle_by_name:
 *
 * This command looks up the specified object on the camera.
 *
 * Format is "A:\\PATH".
 *
 * The 'A' is the VolumeLabel from GetStorageInfo,
 * my IXUS has "A" for the card and "V" for internal memory.
 *  
 * params:	PTPParams*
 *      char* name - path name
 *
 * Return values: Some PTP_RC_* code.
 *      uint32_t *oid - PTP object id.
 *
 **/
uint16_t
ptp_canon_get_objecthandle_by_name (PTPParams* params, char* name, uint32_t* objectid)
{
	PTPContainer	ptp;
	uint16_t	ret;
	unsigned char	*data;
	uint8_t		len = 0;

	PTP_CNT_INIT(ptp, PTP_OC_CANON_GetObjectHandleByName);
	data = malloc (2*(strlen(name)+1)+2);
	if (!data) return PTP_RC_GeneralError;
	memset (data, 0, 2*(strlen(name)+1)+2);
	ptp_pack_string (params, name, data, 0, &len);
	ret=ptp_transaction (params, &ptp, PTP_DP_SENDDATA, (len+1)*2+1, &data, NULL);
	free (data);
	*objectid = ptp.Param1;
	return ret;
}

/**
 * ptp_canon_get_customize_data:
 *
 * This command downloads the specified theme slot, including jpegs
 * and wav files.
 *  
 * params:	PTPParams*
 *      uint32_t themenr - nr of theme
 *
 * Return values: Some PTP_RC_* code.
 *      unsigned char **data - pointer to data pointer
 *      unsigned int  *size - size of data returned
 *
 **/
uint16_t
ptp_canon_get_customize_data (PTPParams* params, uint32_t themenr,
		unsigned char **data, unsigned int *size)
{
	PTPContainer ptp;

	PTP_CNT_INIT(ptp, PTP_OC_CANON_GetCustomizeData, themenr);
	return ptp_transaction(params, &ptp, PTP_DP_GETDATA, 0, data, size); 
}


uint16_t
ptp_nikon_curve_download (PTPParams* params, unsigned char **data, unsigned int *size)
{
	PTPContainer ptp;

	PTP_CNT_INIT(ptp, PTP_OC_NIKON_CurveDownload);
	return ptp_transaction(params, &ptp, PTP_DP_GETDATA, 0, data, size); 
}

/**
 * ptp_sony_sdioconnect:
 *
 * This changes modes of the camera
 *  
 * params:	PTPParams*
 *
 * Return values: Some PTP_RC_* code.
 *
 **/
uint16_t
ptp_sony_sdioconnect (PTPParams* params, uint32_t p1, uint32_t p2, uint32_t p3)
{
	PTPContainer	ptp;
	unsigned char	*data;

	PTP_CNT_INIT(ptp, PTP_OC_SONY_SDIOConnect, p1, p2, p3);
	CHECK_PTP_RC(ptp_transaction(params, &ptp, PTP_DP_GETDATA, 0, &data, NULL));
	free (data);
	return PTP_RC_OK;
}
/**
 * ptp_sony_get_vendorpropcodes:
 *
 * This command downloads the vendor specific property codes.
 *  
 * params:	PTPParams*
 *
 * Return values: Some PTP_RC_* code.
 *      unsigned char **data - pointer to data pointer
 *      unsigned int  *size - size of data returned
 *
 **/
uint16_t
ptp_sony_get_vendorpropcodes (PTPParams* params, uint16_t **props, unsigned int *size)
{
	PTPContainer	ptp;
	unsigned char	*xdata = NULL;
	unsigned int 	xsize, psize1 = 0, psize2 = 0;
	uint16_t	*props1 = NULL,*props2 = NULL;

	*props = NULL;
	*size = 0;
	PTP_CNT_INIT(ptp, PTP_OC_SONY_GetSDIOGetExtDeviceInfo, 0xc8 /* unclear */);
	CHECK_PTP_RC(ptp_transaction(params, &ptp, PTP_DP_GETDATA, 0, &xdata, &xsize));
	if (xsize == 0) {
		ptp_debug (params, "No special operations sent?");
		return PTP_RC_OK;
	}

	psize1 = ptp_unpack_uint16_t_array (params, xdata+2, 0, xsize, &props1);
	ptp_debug (params, "xsize %d, got size %d\n", xsize, psize1*2 + 2 + 4);
	if (psize1*2 + 2 + 4 < xsize) {
		psize2 = ptp_unpack_uint16_t_array(params,xdata+2+psize1*2+4, 0, xsize, &props2);
	}
	*props = calloc(psize1+psize2, sizeof(uint16_t));
	if (!*props) {
		ptp_debug (params, "oom during malloc?");
		free (props1);
		free (props2);
		free (xdata);
		return PTP_RC_OK;
	}
	*size = psize1+psize2;
	memcpy (*props, props1, psize1*sizeof(uint16_t));
	memcpy ((*props)+psize1, props2, psize2*sizeof(uint16_t));
	free (props1);
	free (props2);
	free (xdata);
	return PTP_RC_OK;
}

uint16_t
ptp_sony_getdevicepropdesc (PTPParams* params, uint16_t propcode, PTPDevicePropDesc *dpd)
{
	PTPContainer	ptp;
	unsigned char	*data;
	unsigned int 	size, len = 0;
	uint16_t	ret;

	PTP_CNT_INIT(ptp, PTP_OC_SONY_GetDevicePropdesc, propcode);
	CHECK_PTP_RC(ptp_transaction(params, &ptp, PTP_DP_GETDATA, 0, &data, &size));
	if (!data) return PTP_RC_GeneralError;
	/* first 16 bit is 0xc8 0x00, then an array of 16 bit PTP ids */
	ret = ptp_unpack_Sony_DPD(params,data,dpd,size,&len) ? PTP_RC_OK : PTP_RC_GeneralError;
	free (data);
	return ret;
}

uint16_t
ptp_sony_getalldevicepropdesc (PTPParams* params)
{
	PTPContainer		ptp;
	unsigned char		*data, *dpddata;
	unsigned int		size, readlen;
	PTPDevicePropDesc	dpd;

	PTP_CNT_INIT(ptp, PTP_OC_SONY_GetAllDevicePropData);
	CHECK_PTP_RC(ptp_transaction(params, &ptp, PTP_DP_GETDATA, 0, &data, &size));
	if (!data)
		return PTP_RC_GeneralError;
	if (size <= 8) {
		free (data);
		return PTP_RC_GeneralError;
	}
	dpddata = data+8; /* nr of entries 32bit, 0 32bit */
	size -= 8;
	while (size>0) {
		unsigned int	i;
		uint16_t	propcode;

		if (!ptp_unpack_Sony_DPD (params, dpddata, &dpd, size, &readlen))
			break;

		propcode = dpd.DevicePropertyCode;

		for (i=0;i<params->nrofdeviceproperties;i++)
			if (params->deviceproperties[i].desc.DevicePropertyCode == propcode)
				break;

		/* debug output to see what changes */
		if (i != params->nrofdeviceproperties) {
			switch (dpd.DataType) {
			case PTP_DTC_INT8:
#define CHECK_CHANGED(type) \
				if (params->deviceproperties[i].desc.CurrentValue.type != dpd.CurrentValue.type) \
					ptp_debug (params, "ptp_sony_getalldevicepropdesc: %04x: value %d -> %d", propcode, params->deviceproperties[i].desc.CurrentValue.type, dpd.CurrentValue.type);
				CHECK_CHANGED(i8);
				break;
			case PTP_DTC_UINT8:
				CHECK_CHANGED(u8);
				break;
			case PTP_DTC_UINT16:
				CHECK_CHANGED(u16);
				break;
			case PTP_DTC_INT16:
				CHECK_CHANGED(i16);
				break;
			case PTP_DTC_INT32:
				CHECK_CHANGED(i32);
				break;
			case PTP_DTC_UINT32:
				CHECK_CHANGED(u32);
				break;
			default:
				break;
			}
		}

		if (i == params->nrofdeviceproperties) {
			params->deviceproperties = realloc(params->deviceproperties,(i+1)*sizeof(params->deviceproperties[0]));
			memset(&params->deviceproperties[i],0,sizeof(params->deviceproperties[0]));
			params->nrofdeviceproperties++;
		} else {
			ptp_free_devicepropdesc (&params->deviceproperties[i].desc);
		}
		params->deviceproperties[i].desc = dpd;
#if 0
		ptp_debug (params, "dpd.DevicePropertyCode %04x, readlen %d, getset %d", dpd.DevicePropertyCode, readlen, dpd.GetSet);
		switch (dpd.DataType) {
		case PTP_DTC_INT8:
			ptp_debug (params, "value %d/%x", dpd.CurrentValue.i8, dpd.CurrentValue.i8);
			break;
		case PTP_DTC_UINT8:
			ptp_debug (params, "value %d/%x", dpd.CurrentValue.u8, dpd.CurrentValue.u8);
			break;
		case PTP_DTC_UINT16:
			ptp_debug (params, "value %d/%x", dpd.CurrentValue.u16, dpd.CurrentValue.u16);
			break;
		case PTP_DTC_INT16:
			ptp_debug (params, "value %d/%x", dpd.CurrentValue.i16, dpd.CurrentValue.i16);
			break;
		case PTP_DTC_INT32:
			ptp_debug (params, "value %d/%x", dpd.CurrentValue.i32, dpd.CurrentValue.i32);
			break;
		case PTP_DTC_UINT32:
			ptp_debug (params, "value %d/%x", dpd.CurrentValue.u32, dpd.CurrentValue.u32);
			break;
		default:
			ptp_debug (params, "unknown type %x", dpd.DataType);
			break;
		}
#endif
		dpddata += readlen;
		size -= readlen;
	}
	free(data);
	return PTP_RC_OK;
}

uint16_t
ptp_sony_setdevicecontrolvaluea (PTPParams* params, uint16_t propcode,
			PTPPropertyValue *value, uint16_t datatype)
{
	PTPContainer	ptp;
	uint16_t	ret;
	unsigned char	*data;
	uint32_t	size;

	PTP_CNT_INIT(ptp, PTP_OC_SONY_SetControlDeviceA, propcode);
	size = ptp_pack_DPV(params, value, &data, datatype);
	ret = ptp_transaction(params, &ptp, PTP_DP_SENDDATA, size, &data, NULL);
	free(data);
	return ret;
}

uint16_t
ptp_sony_setdevicecontrolvalueb (PTPParams* params, uint16_t propcode,
			PTPPropertyValue *value, uint16_t datatype)
{
	PTPContainer	ptp;
	uint16_t	ret;
	unsigned char	*data;
	uint32_t	size;

	PTP_CNT_INIT(ptp, PTP_OC_SONY_SetControlDeviceB, propcode);
	size = ptp_pack_DPV(params, value, &data , datatype);
	ret = ptp_transaction(params, &ptp, PTP_DP_SENDDATA, size, &data, NULL);
	free(data);
	return ret;
}

uint16_t
ptp_sony_9280 (PTPParams* params, uint32_t param1,
	uint32_t additional, uint32_t data2, uint32_t data3, uint32_t data4, uint8_t x, uint8_t y)
{
	PTPContainer	ptp;
	unsigned char 	buf[18];
	unsigned char	*buffer;

	PTP_CNT_INIT(ptp, 0x9280, param1);

	if ((additional != 0) && (additional != 2))
		return PTP_RC_GeneralError;

	htod32a(&buf[0], additional);
	htod32a(&buf[4], data2);
	htod32a(&buf[8], data3);
	htod32a(&buf[12], data4);

	/* only sent in the case where additional is 2 */
	buf[16]= x; buf[17]= y;

	buffer=buf;
	return ptp_transaction(params, &ptp, PTP_DP_SENDDATA, 16+additional, &buffer, NULL);
}

uint16_t
ptp_sony_9281 (PTPParams* params, uint32_t param1) {
	PTPContainer	ptp;
	unsigned int	size = 0;
	unsigned char	*buffer = NULL;
	uint16_t	ret;

	PTP_CNT_INIT(ptp, 0x9281, param1);
	ret =  ptp_transaction(params, &ptp, PTP_DP_GETDATA, 0, &buffer, &size);
	free (buffer);
	return ret;
}

/**
 * ptp_generic_getdevicepropdesc:
 *
 * This command gets a propertydesc.
 * If a vendor specific property desc query is available, it uses that.
 * If not, it falls back to the generic PTP getdevicepropdesc.
 *  
 * params:	PTPParams*
 *      uint16_t propcode 
 *      PTPDevicePropDesc *dpd
 *
 * Return values: Some PTP_RC_* code.
 *
 **/
/* Cache time in seconds. Should perhaps be more granular... */

uint16_t
ptp_generic_getdevicepropdesc (PTPParams *params, uint16_t propcode, PTPDevicePropDesc *dpd)
{
	unsigned int	i;
	time_t		now;

	for (i=0;i<params->nrofdeviceproperties;i++)
		if (params->deviceproperties[i].desc.DevicePropertyCode == propcode)
			break;
	if (i == params->nrofdeviceproperties) {
		params->deviceproperties = realloc(params->deviceproperties,(i+1)*sizeof(params->deviceproperties[0]));
		memset(&params->deviceproperties[i],0,sizeof(params->deviceproperties[0]));
		params->nrofdeviceproperties++;
	}

	if (params->deviceproperties[i].desc.DataType != PTP_DTC_UNDEF) {
		time(&now);
		if (params->deviceproperties[i].timestamp + params->cachetime > now) {
			duplicate_DevicePropDesc(&params->deviceproperties[i].desc, dpd);
			return PTP_RC_OK;
		}
		/* free cached entry as we will refetch it. */
		ptp_free_devicepropdesc (&params->deviceproperties[i].desc);
	}

	if (	(params->deviceinfo.VendorExtensionID == PTP_VENDOR_SONY) &&
		ptp_operation_issupported(params, PTP_OC_SONY_GetAllDevicePropData)
	) {
		CHECK_PTP_RC(ptp_sony_getalldevicepropdesc (params));

		for (i=0;i<params->nrofdeviceproperties;i++)
			if (params->deviceproperties[i].desc.DevicePropertyCode == propcode)
				break;
		if (i == params->nrofdeviceproperties) {
			ptp_debug (params, "property 0x%04x not found?\n", propcode);
			return PTP_RC_GeneralError;
		}
		time(&now);
		params->deviceproperties[i].timestamp = now;
		duplicate_DevicePropDesc(&params->deviceproperties[i].desc, dpd);
		return PTP_RC_OK;
	}
	if (	(params->deviceinfo.VendorExtensionID == PTP_VENDOR_SONY) &&
		ptp_operation_issupported(params, PTP_OC_SONY_GetDevicePropdesc)
	) {
		CHECK_PTP_RC(ptp_sony_getdevicepropdesc (params, propcode, &params->deviceproperties[i].desc));

		time(&now);
		params->deviceproperties[i].timestamp = now;
		duplicate_DevicePropDesc(&params->deviceproperties[i].desc, dpd);
		return PTP_RC_OK;
	}


	if (ptp_operation_issupported(params, PTP_OC_GetDevicePropDesc)) {
		CHECK_PTP_RC(ptp_getdevicepropdesc (params, propcode, &params->deviceproperties[i].desc));

		time(&now);
		params->deviceproperties[i].timestamp = now;
		duplicate_DevicePropDesc(&params->deviceproperties[i].desc, dpd);
		return PTP_RC_OK;
	}

	return PTP_RC_OK;
}

/**
 * ptp_generic_setdevicepropvalue:
 *
 * This command sets a property value, device specific.
 *  
 * params:	PTPParams*
 *      uint16_t propcode 
 *      PTPDevicePropertyValue *value
 *      uint16_t datatype
 *
 * Return values: Some PTP_RC_* code.
 *
 **/
uint16_t
ptp_generic_setdevicepropvalue (PTPParams* params, uint16_t propcode,
	PTPPropertyValue *value, uint16_t datatype)
{
	unsigned int i;

	/* reset the cache entry */
	for (i=0;i<params->nrofdeviceproperties;i++)
		if (params->deviceproperties[i].desc.DevicePropertyCode == propcode)
			break;
	if (i != params->nrofdeviceproperties)
		params->deviceproperties[i].timestamp = 0;

	/* FIXME: change the cache? hmm */
	/* this works for some methods, but not for all */
	if (	(params->deviceinfo.VendorExtensionID == PTP_VENDOR_SONY) &&
		ptp_operation_issupported(params, PTP_OC_SONY_SetControlDeviceA)
	)
		return ptp_sony_setdevicecontrolvaluea (params, propcode, value, datatype);
	return ptp_setdevicepropvalue (params, propcode, value, datatype);
}

/**
 * ptp_nikon_get_vendorpropcodes:
 *
 * This command downloads the vendor specific property codes.
 *  
 * params:	PTPParams*
 *
 * Return values: Some PTP_RC_* code.
 *      unsigned char **data - pointer to data pointer
 *      unsigned int  *size - size of data returned
 *
 **/
uint16_t
ptp_nikon_get_vendorpropcodes (PTPParams* params, uint16_t **props, unsigned int *size)
{
	PTPContainer	ptp;
	unsigned char	*data = NULL;
	unsigned int	xsize = 0;

	*props = NULL;
	*size = 0;
	PTP_CNT_INIT(ptp, PTP_OC_NIKON_GetVendorPropCodes);
	CHECK_PTP_RC(ptp_transaction(params, &ptp, PTP_DP_GETDATA, 0, &data, &xsize));
	*size = ptp_unpack_uint16_t_array(params,data,0,xsize,props);
	free (data);
	return PTP_RC_OK;
}

uint16_t
ptp_nikon_getfileinfoinblock ( PTPParams* params,
	uint32_t p1, uint32_t p2, uint32_t p3,
	unsigned char **data, unsigned int *size
) {
	PTPContainer ptp;

	PTP_CNT_INIT(ptp, PTP_OC_NIKON_GetFileInfoInBlock, p1, p2, p3);
	return ptp_transaction(params, &ptp, PTP_DP_GETDATA, 0, data, size); 
}

/**
 * ptp_nikon_get_liveview_image:
 *
 * This command gets a LiveView image from newer Nikons DSLRs.
 *  
 * params:	PTPParams*
 *
 * Return values: Some PTP_RC_* code.
 *
 **/
uint16_t
ptp_nikon_get_liveview_image (PTPParams* params, unsigned char **data, unsigned int *size)
{
        PTPContainer ptp;
        
        PTP_CNT_INIT(ptp, PTP_OC_NIKON_GetLiveViewImg);
        return ptp_transaction(params, &ptp, PTP_DP_GETDATA, 0, data, size);
}

/**
 * ptp_nikon_get_preview_image:
 *
 * This command gets a Preview image from newer Nikons DSLRs.
 *  
 * params:	PTPParams*
 *
 * Return values: Some PTP_RC_* code.
 *
 **/
uint16_t
ptp_nikon_get_preview_image (PTPParams* params, unsigned char **xdata, unsigned int *xsize,
	uint32_t *handle)
{
	PTPContainer	ptp;

        PTP_CNT_INIT(ptp, PTP_OC_NIKON_GetPreviewImg);

	/* FIXME:
	 * pdslrdashboard passes 3 parameters:
	 * objectid, minimum size, maximum size
	 */
	CHECK_PTP_RC(ptp_transaction(params, &ptp, PTP_DP_GETDATA, 0, xdata, xsize));
	if (ptp.Nparam > 0)
		*handle = ptp.Param1;
	return PTP_RC_OK;
}

/**
 * ptp_canon_eos_get_viewfinder_image:
 *
 * This command gets a Viewfinder image from newer Nikons DSLRs.
 *  
 * params:	PTPParams*
 *
 * Return values: Some PTP_RC_* code.
 *
 **/
uint16_t
ptp_canon_eos_get_viewfinder_image (PTPParams* params, unsigned char **data, unsigned int *size)
{
        PTPContainer ptp;
        
        PTP_CNT_INIT(ptp, PTP_OC_CANON_EOS_GetViewFinderData, 0x00100000 /* from trace */);
        return ptp_transaction(params, &ptp, PTP_DP_GETDATA, 0, data, size);
}

uint16_t
ptp_canon_eos_get_viewfinder_image_handler (PTPParams* params, PTPDataHandler*handler)
{
        PTPContainer ptp;
        
        PTP_CNT_INIT(ptp, PTP_OC_CANON_EOS_GetViewFinderData, 0x00100000 /* from trace */);
        return ptp_transaction_new(params, &ptp, PTP_DP_GETDATA, 0, handler);
}

/**
 * ptp_nikon_check_event:
 *
 * This command checks the event queue on the Nikon.
 *  
 * params:	PTPParams*
 *      PTPUSBEventContainer **event - list of usb events.
 *	int *evtcnt - number of usb events in event structure.
 *
 * Return values: Some PTP_RC_* code.
 *
 **/
uint16_t
ptp_nikon_check_event (PTPParams* params, PTPContainer** event, unsigned int* evtcnt)
{
	PTPContainer	ptp;
	unsigned char	*data;
	unsigned int	size;

	PTP_CNT_INIT(ptp, PTP_OC_NIKON_CheckEvent);
	*evtcnt = 0;
	CHECK_PTP_RC(ptp_transaction (params, &ptp, PTP_DP_GETDATA, 0, &data, &size));
	ptp_unpack_Nikon_EC (params, data, size, event, evtcnt);
	free (data);
	return PTP_RC_OK;
}

/**
 * ptp_nikon_getptpipinfo:
 *
 * This command gets the ptpip info data.
 *  
 * params:	PTPParams*
 *	unsigned char *data	- data
 *	unsigned int size	- size of returned data
 *
 * Return values: Some PTP_RC_* code.
 *
 **/
uint16_t
ptp_nikon_getptpipinfo (PTPParams* params, unsigned char **data, unsigned int *size)
{
        PTPContainer ptp;
        
        PTP_CNT_INIT(ptp, PTP_OC_NIKON_GetDevicePTPIPInfo);
        return ptp_transaction(params, &ptp, PTP_DP_GETDATA, 0, data, size);
}

/**
 * ptp_nikon_getwifiprofilelist:
 *
 * This command gets the wifi profile list.
 *  
 * params:	PTPParams*
 *
 * Return values: Some PTP_RC_* code.
 *
 **/
uint16_t
ptp_nikon_getwifiprofilelist (PTPParams* params)
{
	PTPContainer	ptp;
	uint16_t	ret;
	unsigned char	*data;
	unsigned int	size, pos, profn, n;
	char		*buffer;
	uint8_t		len;

        PTP_CNT_INIT(ptp, PTP_OC_NIKON_GetProfileAllData);
	CHECK_PTP_RC(ptp_transaction(params, &ptp, PTP_DP_GETDATA, 0, &data, &size));

	ret = PTP_RC_Undefined; /* FIXME: Add more precise error code */

	if (size < 2)
		goto exit;

	params->wifi_profiles_version = data[0];
	params->wifi_profiles_number = data[1];
	free(params->wifi_profiles);
	
	params->wifi_profiles = malloc(params->wifi_profiles_number*sizeof(PTPNIKONWifiProfile));
	memset(params->wifi_profiles, 0, params->wifi_profiles_number*sizeof(PTPNIKONWifiProfile));

	pos = 2;
	profn = 0;
	while (profn < params->wifi_profiles_number && pos < size) {
		if (pos+6 >= size)
			goto exit;
		params->wifi_profiles[profn].id = data[pos++];
		params->wifi_profiles[profn].valid = data[pos++];

		n = dtoh32a(&data[pos]);
		pos += 4;
		if (pos+n+4 >= size)
			goto exit;
		strncpy(params->wifi_profiles[profn].profile_name, (char*)&data[pos], n);
		params->wifi_profiles[profn].profile_name[16] = '\0';
		pos += n;

		params->wifi_profiles[profn].display_order = data[pos++];
		params->wifi_profiles[profn].device_type = data[pos++];
		params->wifi_profiles[profn].icon_type = data[pos++];

		buffer = ptp_unpack_string(params, data, pos, size, &len);
		strncpy(params->wifi_profiles[profn].creation_date, buffer, sizeof(params->wifi_profiles[profn].creation_date));
		free (buffer);
		pos += (len*2+1);
		if (pos+1 >= size)
			goto exit;
		/* FIXME: check if it is really last usage date */
		buffer = ptp_unpack_string(params, data, pos, size, &len);
		strncpy(params->wifi_profiles[profn].lastusage_date, buffer, sizeof(params->wifi_profiles[profn].lastusage_date));
		free (buffer);
		pos += (len*2+1);
		if (pos+5 >= size)
			goto exit;
		
		n = dtoh32a(&data[pos]);
		pos += 4;
		if (pos+n >= size)
			goto exit;
		strncpy(params->wifi_profiles[profn].essid, (char*)&data[pos], n);
		params->wifi_profiles[profn].essid[32] = '\0';
		pos += n;
		pos += 1;
		profn++;
	}

#if 0
	PTPNIKONWifiProfile test;
	memset(&test, 0, sizeof(PTPNIKONWifiProfile));
	strcpy(test.profile_name, "MyTest");
	test.icon_type = 1;
	strcpy(test.essid, "nikon");
	test.ip_address = 10 + 11 << 16 + 11 << 24;
	test.subnet_mask = 24;
	test.access_mode = 1;
	test.wifi_channel = 1;
	test.key_nr = 1;

	ptp_nikon_writewifiprofile(params, &test);
#endif
	/* everything went Ok */
	ret = PTP_RC_OK;
exit:
	free (data);
	return ret;
}

/**
 * ptp_nikon_writewifiprofile:
 *
 * This command gets the ptpip info data.
 *  
 * params:	PTPParams*
 *	unsigned int profilenr	- profile number
 *	unsigned char *data	- data
 *	unsigned int size	- size of returned data
 *
 * Return values: Some PTP_RC_* code.
 *
 **/
uint16_t
ptp_nikon_writewifiprofile (PTPParams* params, PTPNIKONWifiProfile* profile)
{
	PTPContainer ptp;
	unsigned char buffer[1024];
	unsigned char* data = buffer;
	int size = 0;
	int i;
	uint8_t len;
	int profilenr = -1;
	unsigned char guid[16];

	ptp_nikon_getptpipguid(guid);

	if (!params->wifi_profiles)
		CHECK_PTP_RC(ptp_nikon_getwifiprofilelist(params));
	
	for (i = 0; i < params->wifi_profiles_number; i++) {
		if (!params->wifi_profiles[i].valid) {
			profilenr = params->wifi_profiles[i].id;
			break;
		}
	}
	
	if (profilenr == -1) {
		/* No free profile! */
		return PTP_RC_StoreFull;
	}
	
	memset(buffer, 0, 1024);
	
	buffer[0x00] = 0x64; /* Version */
	
	/* Profile name */
	htod32a(&buffer[0x01], 17);
	/* 16 as third parameter, so there will always be a null-byte in the end */
	strncpy((char*)&buffer[0x05], profile->profile_name, 16);
	
	buffer[0x16] = 0x00; /* Display order */
	buffer[0x17] = profile->device_type;
	buffer[0x18] = profile->icon_type;
	
	/* FIXME: Creation date: put a real date here */
	ptp_pack_string(params, "19990909T090909", data, 0x19, &len);
	
	/* IP parameters */
	memcpy(&buffer[0x3A],&profile->ip_address,sizeof(profile->ip_address));
	/**((unsigned int*)&buffer[0x3A]) = profile->ip_address; *//* Do not reverse bytes */
	buffer[0x3E] = profile->subnet_mask;
	memcpy(&buffer[0x3F],&profile->gateway_address,sizeof(profile->gateway_address));
	/**((unsigned int*)&buffer[0x3F]) = profile->gateway_address; */ /* Do not reverse bytes */
	buffer[0x43] = profile->address_mode;
	
	/* Wifi parameters */
	buffer[0x44] = profile->access_mode;
	buffer[0x45] = profile->wifi_channel;
	
	htod32a(&buffer[0x46], 33); /* essid */
	 /* 32 as third parameter, so there will always be a null-byte in the end */
	strncpy((char*)&buffer[0x4A], profile->essid, 32);
	
	buffer[0x6B] = profile->authentification;
	buffer[0x6C] = profile->encryption;
	htod32a(&buffer[0x6D], 64);
	for (i = 0; i < 64; i++) {
		buffer[0x71+i] = profile->key[i];
	}
	buffer[0xB1] = profile->key_nr;
	memcpy(&buffer[0xB2], guid, 16);
	
	switch(profile->encryption) {
	case 1: /* WEP 64bit */
		htod16a(&buffer[0xC2], 5); /* (64-24)/8 = 5 */
		break;
	case 2: /* WEP 128bit */
		htod16a(&buffer[0xC2], 13); /* (128-24)/8 = 13 */
		break;
	default:
		htod16a(&buffer[0xC2], 0);
	}
	size = 0xC4;
	       
	PTP_CNT_INIT(ptp, PTP_OC_NIKON_SendProfileData, profilenr);
	return ptp_transaction(params, &ptp, PTP_DP_SENDDATA, size, &data, NULL);
}

/**
 * ptp_mtp_getobjectpropssupported:
 *
 * This command gets the object properties possible from the device.
 *  
 * params:	PTPParams*
 *	uint ofc		- object format code
 *	unsigned int *propnum	- number of elements in returned array
 *	uint16_t *props		- array of supported properties
 *
 * Return values: Some PTP_RC_* code.
 *
 **/
uint16_t
ptp_mtp_getobjectpropssupported (PTPParams* params, uint16_t ofc,
		 uint32_t *propnum, uint16_t **props
) {
	PTPContainer	ptp;
	unsigned char	*data = NULL;
	unsigned int	xsize = 0;

        PTP_CNT_INIT(ptp, PTP_OC_MTP_GetObjectPropsSupported, ofc);
	CHECK_PTP_RC(ptp_transaction(params, &ptp, PTP_DP_GETDATA, 0, &data, &xsize));
	if (!data) return PTP_RC_GeneralError;
	*propnum=ptp_unpack_uint16_t_array (params, data, 0, xsize, props);
	free(data);
	return PTP_RC_OK;
}

/**
 * ptp_mtp_getobjectpropdesc:
 *
 * This command gets the object property description.
 *  
 * params:	PTPParams*
 *	uint16_t opc	- object property code
 *	uint16_t ofc	- object format code
 *
 * Return values: Some PTP_RC_* code.
 *
 **/
uint16_t
ptp_mtp_getobjectpropdesc (
	PTPParams* params, uint16_t opc, uint16_t ofc, PTPObjectPropDesc *opd
) {
	PTPContainer	ptp;
	unsigned char	*data;
	unsigned int	size;

        PTP_CNT_INIT(ptp, PTP_OC_MTP_GetObjectPropDesc, opc, ofc);
        CHECK_PTP_RC(ptp_transaction(params, &ptp, PTP_DP_GETDATA, 0, &data, &size));
	ptp_unpack_OPD (params, data, opd, size);
	free(data);
	return PTP_RC_OK;
}

/**
 * ptp_mtp_getobjectpropvalue:
 *
 * This command gets the object properties of an object handle.
 *  
 * params:	PTPParams*
 *	uint32_t objectid	- object format code
 *	uint16_t opc		- object prop code
 *
 * Return values: Some PTP_RC_* code.
 *
 **/
uint16_t
ptp_mtp_getobjectpropvalue (
	PTPParams* params, uint32_t oid, uint16_t opc,
	PTPPropertyValue *value, uint16_t datatype
) {
	PTPContainer	ptp;
	uint16_t	ret = PTP_RC_OK;
	unsigned char	*data;
	unsigned int	size, offset = 0;
        
        PTP_CNT_INIT(ptp, PTP_OC_MTP_GetObjectPropValue, oid, opc);
        CHECK_PTP_RC(ptp_transaction(params, &ptp, PTP_DP_GETDATA, 0, &data, &size));
        if (!ptp_unpack_DPV(params, data, &offset, size, value, datatype)) {
                ptp_debug (params, "ptp_mtp_getobjectpropvalue: unpacking DPV failed");
                ret = PTP_RC_GeneralError;
        }
	free(data);
	return ret;
}

/**
 * ptp_mtp_setobjectpropvalue:
 *
 * This command gets the object properties of an object handle.
 *  
 * params:	PTPParams*
 *	uint32_t objectid	- object format code
 *	uint16_t opc		- object prop code
 *
 * Return values: Some PTP_RC_* code.
 *
 **/
uint16_t
ptp_mtp_setobjectpropvalue (
	PTPParams* params, uint32_t oid, uint16_t opc,
	PTPPropertyValue *value, uint16_t datatype
) {
	PTPContainer	ptp;
	uint16_t	ret;
	unsigned char	*data = NULL;
	uint32_t	size;

        PTP_CNT_INIT(ptp, PTP_OC_MTP_SetObjectPropValue, oid, opc);
	size = ptp_pack_DPV(params, value, &data, datatype);
        ret = ptp_transaction(params, &ptp, PTP_DP_SENDDATA, size, &data, NULL);
	free(data);
	return ret;
}

uint16_t
ptp_mtp_getobjectreferences (PTPParams* params, uint32_t handle, uint32_t** ohArray, uint32_t* arraylen)
{
	PTPContainer	ptp;
	unsigned char	*data;
	unsigned int	size;

	PTP_CNT_INIT(ptp, PTP_OC_MTP_GetObjectReferences, handle);
	CHECK_PTP_RC(ptp_transaction(params, &ptp, PTP_DP_GETDATA, 0, &data , &size));
	/* Sandisk Sansa skips the DATA phase, but returns OK as response.
		 * this will gives us a NULL here. Handle it. -Marcus */
	if ((data == NULL) || (size == 0)) {
		*arraylen = 0;
		*ohArray = NULL;
	} else {
		*arraylen = ptp_unpack_uint32_t_array(params, data , 0, size, ohArray);
	}
	free(data);
	return PTP_RC_OK;
}

uint16_t
ptp_mtp_setobjectreferences (PTPParams* params, uint32_t handle, uint32_t* ohArray, uint32_t arraylen)
{
	PTPContainer	ptp;
	uint16_t	ret;
	unsigned char	*data = NULL;
	uint32_t	size;

	PTP_CNT_INIT(ptp, PTP_OC_MTP_SetObjectReferences, handle);
	size = ptp_pack_uint32_t_array(params, ohArray, arraylen, &data);
	ret = ptp_transaction(params, &ptp, PTP_DP_SENDDATA, size, &data, NULL);
	free(data);
	return ret;
}

uint16_t
ptp_mtp_getobjectproplist (PTPParams* params, uint32_t handle, MTPProperties **props, int *nrofprops)
{
	PTPContainer	ptp;
	unsigned char	*data;
	unsigned int	size;

	PTP_CNT_INIT(ptp, PTP_OC_MTP_GetObjPropList, handle,
		     0x00000000U,  /* 0x00000000U should be "all formats" */
		     0xFFFFFFFFU,  /* 0xFFFFFFFFU should be "all properties" */
		     0x00000000U,
		     0xFFFFFFFFU  /* means - return full tree below the Param1 handle */
	);
	CHECK_PTP_RC(ptp_transaction(params, &ptp, PTP_DP_GETDATA, 0, &data, &size));
	*nrofprops = ptp_unpack_OPL(params, data, props, size);
	free(data);
	return PTP_RC_OK;
}

uint16_t
ptp_mtp_getobjectproplist_single (PTPParams* params, uint32_t handle, MTPProperties **props, int *nrofprops)
{
	PTPContainer	ptp;
	unsigned char	*data;
	unsigned int	size;

	PTP_CNT_INIT(ptp, PTP_OC_MTP_GetObjPropList, handle,
		     0x00000000U,  /* 0x00000000U should be "all formats" */
		     0xFFFFFFFFU,  /* 0xFFFFFFFFU should be "all properties" */
		     0x00000000U,
		     0x00000000U  /* means - return single tree below the Param1 handle */
	);
	CHECK_PTP_RC(ptp_transaction(params, &ptp, PTP_DP_GETDATA, 0, &data, &size));
	*nrofprops = ptp_unpack_OPL(params, data, props, size);
	free(data);
	return PTP_RC_OK;
}

uint16_t
ptp_mtp_sendobjectproplist (PTPParams* params, uint32_t* store, uint32_t* parenthandle, uint32_t* handle,
			    uint16_t objecttype, uint64_t objectsize, MTPProperties *props, int nrofprops)
{
	PTPContainer	ptp;
	uint16_t	ret;
	unsigned char	*data = NULL;
	uint32_t	size;

	PTP_CNT_INIT(ptp, PTP_OC_MTP_SendObjectPropList, *store, *parenthandle, (uint32_t) objecttype,
		     (uint32_t) (objectsize >> 32), (uint32_t) (objectsize & 0xffffffffU)
	);

	/* Set object handle to 0 for a new object */
	size = ptp_pack_OPL(params,props,nrofprops,&data);
	ret = ptp_transaction(params, &ptp, PTP_DP_SENDDATA, size, &data, NULL);
	free(data);
	*store = ptp.Param1;
	*parenthandle = ptp.Param2;
	*handle = ptp.Param3; 

	return ret;
}

uint16_t
ptp_mtp_setobjectproplist (PTPParams* params, MTPProperties *props, int nrofprops)
{
	PTPContainer	ptp;
	unsigned char	*data = NULL;
	uint32_t	size;

	PTP_CNT_INIT(ptp, PTP_OC_MTP_SetObjPropList);
	size = ptp_pack_OPL(params,props,nrofprops,&data);
	CHECK_PTP_RC(ptp_transaction(params, &ptp, PTP_DP_SENDDATA, size, &data, NULL));
	free(data);
	return PTP_RC_OK;
}

uint16_t
ptp_mtpz_sendwmdrmpdapprequest (PTPParams* params, unsigned char *appcertmsg, uint32_t size)
{
	PTPContainer ptp;

	PTP_CNT_INIT(ptp, PTP_OC_MTP_WMDRMPD_SendWMDRMPDAppRequest);
	return ptp_transaction (params, &ptp, PTP_DP_SENDDATA, size, &appcertmsg, NULL);
}

uint16_t
ptp_mtpz_getwmdrmpdappresponse (PTPParams* params, unsigned char **response, uint32_t *size)
{
	PTPContainer ptp;

	PTP_CNT_INIT(ptp, PTP_OC_MTP_WMDRMPD_GetWMDRMPDAppResponse);
	*size = 0;
	*response = NULL;
	return ptp_transaction (params, &ptp, PTP_DP_GETDATA, 0, response, size);
}

/****** CHDK interface ******/

uint16_t
ptp_chdk_get_memory(PTPParams* params, int start, int num, unsigned char **buf)
{
	PTPContainer ptp;

	PTP_CNT_INIT(ptp, PTP_OC_CHDK, PTP_CHDK_GetMemory, start, num);
	return ptp_transaction(params, &ptp, PTP_DP_GETDATA, 0, buf, NULL);
}

uint16_t
ptp_chdk_set_memory_long(PTPParams* params, int addr, int val)
{
	PTPContainer ptp;
	unsigned char *buf = (unsigned char *) &val; /* FIXME ... endianness? */

	PTP_CNT_INIT(ptp, PTP_OC_CHDK, PTP_CHDK_SetMemory, addr, 4);
	return ptp_transaction(params, &ptp, PTP_DP_SENDDATA, 4, &buf, NULL);
}

uint16_t
ptp_chdk_download(PTPParams* params, char *remote_fn, PTPDataHandler *handler)
{
	PTPContainer ptp;

	PTP_CNT_INIT(ptp, PTP_OC_CHDK, PTP_CHDK_TempData, 0);
	CHECK_PTP_RC (ptp_transaction(params, &ptp, PTP_DP_SENDDATA, strlen(remote_fn), (unsigned char**)&remote_fn, NULL));

	PTP_CNT_INIT(ptp, PTP_OC_CHDK, PTP_CHDK_DownloadFile);
	return ptp_transaction_new (params, &ptp, PTP_DP_GETDATA, 0, handler);
}

#if 0
int ptp_chdk_upload(PTPParams* params, char *local_fn, char *remote_fn)
{
  uint16_t ret;
  PTPContainer ptp;
  char *buf = NULL;
  FILE *f;
  unsigned file_len,data_len,file_name_len;

  PTP_CNT_INIT(ptp, PTP_OC_CHDK, PTP_CHDK_UploadFile);

  f = fopen(local_fn,"rb");
  if ( f == NULL )
  {
    ptp_error(params,"could not open file \'%s\'",local_fn);
    return 0;
  }

  fseek(f,0,SEEK_END);
  file_len = ftell(f);
  fseek(f,0,SEEK_SET);

  file_name_len = strlen(remote_fn);
  data_len = 4 + file_name_len + file_len;
  buf = malloc(data_len);
  memcpy(buf,&file_name_len,4);
  memcpy(buf+4,remote_fn,file_name_len);
  fread(buf+4+file_name_len,1,file_len,f);

  fclose(f);

  ret=ptp_transaction(params, &ptp, PTP_DP_SENDDATA, data_len, &buf, NULL);

  free(buf);

  if ( ret != PTP_RC_OK )
  {
    ptp_error(params,"unexpected return code 0x%x",ret);
    return 0;
  }
  return 1;
}

#endif

/*
 * Preliminary remote capture over USB code. Corresponding CHDK code is in the ptp-remote-capture-test
 * This is under development and should not be included in builds for general distribution
 */
/*
 * isready: 0: not ready, lowest 2 bits: available image formats, 0x10000000: error
 */
uint16_t
ptp_chdk_rcisready(PTPParams* params, int *isready, int *imgnum)
{
	PTPContainer ptp;

	PTP_CNT_INIT(ptp, PTP_OC_CHDK, PTP_CHDK_RemoteCaptureIsReady);
	*isready = *imgnum = 0;
	CHECK_PTP_RC(ptp_transaction(params, &ptp, PTP_DP_NODATA, 0, NULL, NULL));
	*isready=ptp.Param1;
	*imgnum=ptp.Param2;
	return PTP_RC_OK;
}

uint16_t
ptp_chdk_rcgetchunk(PTPParams* params, int fmt, ptp_chdk_rc_chunk *chunk)
{
	PTPContainer ptp;

	PTP_CNT_INIT(ptp, PTP_OC_CHDK, PTP_CHDK_RemoteCaptureGetData, fmt); //get chunk

	chunk->data = NULL;
	chunk->size = 0;
	chunk->offset = 0;
	chunk->last = 0;
	// TODO should allow ptp_getdata_transaction to send chunks directly to file, or to mem
	CHECK_PTP_RC(ptp_transaction(params, &ptp, PTP_DP_GETDATA, 0, &chunk->data, NULL));
	chunk->size = ptp.Param1;
	chunk->last = (ptp.Param2 == 0);
  	chunk->offset = ptp.Param3; //-1 for none
	return PTP_RC_OK;
}

uint16_t
ptp_chdk_exec_lua(PTPParams* params, char *script, int flags, int *script_id, int *status)
{
	PTPContainer ptp;

	PTP_CNT_INIT(ptp, PTP_OC_CHDK, PTP_CHDK_ExecuteScript, PTP_CHDK_SL_LUA | flags);
	*script_id = 0;
	*status = 0;
	CHECK_PTP_RC(ptp_transaction(params, &ptp, PTP_DP_SENDDATA, strlen(script)+1, (unsigned char**)&script, NULL));
	*script_id = ptp.Param1;
	*status = ptp.Param2;
	return PTP_RC_OK;
}

uint16_t
ptp_chdk_get_version(PTPParams* params, int *major, int *minor)
{
	PTPContainer ptp;

	PTP_CNT_INIT(ptp, PTP_OC_CHDK, PTP_CHDK_Version);
	CHECK_PTP_RC(ptp_transaction(params, &ptp, PTP_DP_NODATA, 0, NULL, NULL));
	*major = ptp.Param1;
	*minor = ptp.Param2;
	return PTP_RC_OK;
}

uint16_t
ptp_chdk_get_script_status(PTPParams* params, unsigned *status)
{
	PTPContainer ptp;

	PTP_CNT_INIT(ptp, PTP_OC_CHDK, PTP_CHDK_ScriptStatus);
	CHECK_PTP_RC(ptp_transaction(params, &ptp, PTP_DP_NODATA, 0, NULL, NULL));
	*status = ptp.Param1;
	return PTP_RC_OK;
}
uint16_t
ptp_chdk_get_script_support(PTPParams* params, unsigned *status)
{
	PTPContainer ptp;

	PTP_CNT_INIT(ptp, PTP_OC_CHDK, PTP_CHDK_ScriptSupport);
	CHECK_PTP_RC(ptp_transaction(params, &ptp, PTP_DP_NODATA, 0, NULL, NULL));
	*status = ptp.Param1;
	return PTP_RC_OK;
}

uint16_t
ptp_chdk_write_script_msg(PTPParams* params, char *data, unsigned size, int target_script_id, int *status)
{
	PTPContainer ptp;

	// a zero length data phase appears to do bad things, camera stops responding to PTP
	if(!size) {
		ptp_error(params,"zero length message not allowed");
		*status = 0;
		return PTP_ERROR_BADPARAM;
	}
	PTP_CNT_INIT(ptp, PTP_OC_CHDK, PTP_CHDK_WriteScriptMsg, target_script_id);
	*status = 0;
	CHECK_PTP_RC(ptp_transaction(params, &ptp, PTP_DP_SENDDATA, size, (unsigned char**)&data, NULL));
	*status = ptp.Param1;
	return PTP_RC_OK;
}
uint16_t
ptp_chdk_read_script_msg(PTPParams* params, ptp_chdk_script_msg **msg)
{
	PTPContainer	ptp;
	unsigned char	*data;

	PTP_CNT_INIT(ptp, PTP_OC_CHDK, PTP_CHDK_ReadScriptMsg);

	*msg = NULL;

	/* camera will always send data, otherwise getdata will cause problems */
	CHECK_PTP_RC(ptp_transaction(params, &ptp, PTP_DP_GETDATA, 0, &data, NULL));
	if (!data) {
		ptp_error(params,"no data received");
		return PTP_ERROR_BADPARAM;
	}

	/* for convenience, always allocate an extra byte and null it*/
	*msg = malloc(sizeof(ptp_chdk_script_msg) + ptp.Param4 + 1);
	(*msg)->type = ptp.Param1;
	(*msg)->subtype = ptp.Param2;
	(*msg)->script_id = ptp.Param3;
	(*msg)->size = ptp.Param4;
	memcpy((*msg)->data,data,(*msg)->size);
	(*msg)->data[(*msg)->size] = 0;
	free(data);
	return PTP_RC_OK;
}

uint16_t
ptp_chdk_get_live_data(PTPParams* params, unsigned flags, unsigned char **data, unsigned int *data_size)
{
	PTPContainer ptp;

	PTP_CNT_INIT(ptp, PTP_OC_CHDK, PTP_CHDK_GetDisplayData, flags);
	*data_size = 0;
	CHECK_PTP_RC(ptp_transaction(params, &ptp, PTP_DP_GETDATA, 0, data, NULL));
	*data_size = ptp.Param1;
	return PTP_RC_OK;
}

uint16_t
ptp_chdk_call_function(PTPParams* params, int *args, int size, int *ret)
{
	PTPContainer ptp;

	PTP_CNT_INIT(ptp, PTP_OC_CHDK, PTP_CHDK_CallFunction);
	CHECK_PTP_RC(ptp_transaction(params, &ptp, PTP_DP_SENDDATA, size*sizeof(int), (unsigned char **) &args, NULL));
	if (ret)
		*ret = ptp.Param1;
	return PTP_RC_OK;
}




/**
 * Android MTP Extensions
 */

/**
 * ptp_android_getpartialobject64:
 * params:	PTPParams*
 *		handle			- Object handle
 *		offset			- Offset into object
 *		maxbytes		- Maximum of bytes to read
 *		object			- pointer to data area
 *		len			- pointer to returned length
 *
 * Get object 'handle' from device and store the data in newly
 * allocated 'object'. Start from offset and read at most maxbytes.
 *
 * This is a 64bit offset version of the standard GetPartialObject.
 *
 * Return values: Some PTP_RC_* code.
 **/
uint16_t
ptp_android_getpartialobject64 (PTPParams* params, uint32_t handle, uint64_t offset,
				uint32_t maxbytes, unsigned char** object,
				uint32_t *len)
{
	PTPContainer ptp;

	/* casts due to varargs otherwise pushing 64bit values on the stack */
	PTP_CNT_INIT(ptp, PTP_OC_ANDROID_GetPartialObject64, handle, ((uint32_t)offset & 0xFFFFFFFF), (uint32_t)(offset >> 32), maxbytes);
	return ptp_transaction(params, &ptp, PTP_DP_GETDATA, 0, object, len);
}

uint16_t
ptp_android_sendpartialobject (PTPParams* params, uint32_t handle, uint64_t offset,
				unsigned char* object,	uint32_t len)
{
	PTPContainer	ptp;
	uint16_t	ret;

	PTP_CNT_INIT(ptp, PTP_OC_ANDROID_SendPartialObject, handle, (uint32_t)(offset & 0xFFFFFFFF), (uint32_t)(offset >> 32), len);

	/*
	 * MtpServer.cpp is buggy: it uses write() without offset
	 * rather than pwrite to send the data for data coming with
	 * the header packet
	 */
	params->split_header_data = 1;
	ret=ptp_transaction(params, &ptp, PTP_DP_SENDDATA, len, &object, NULL);
	params->split_header_data = 0;

	return ret;
}


/* Non PTP protocol functions */
/* devinfo testing functions */

int
ptp_event_issupported(PTPParams* params, uint16_t event)
{
	unsigned int i=0;

	for (;i<params->deviceinfo.EventsSupported_len;i++) {
		if (params->deviceinfo.EventsSupported[i]==event)
			return 1;
	}
	return 0;
}


int
ptp_property_issupported(PTPParams* params, uint16_t property)
{
	unsigned int i;

	for (i=0;i<params->deviceinfo.DevicePropertiesSupported_len;i++)
		if (params->deviceinfo.DevicePropertiesSupported[i]==property)
			return 1;
	return 0;
}

void
ptp_free_objectinfo (PTPObjectInfo *oi)
{
	if (!oi) return;
        free (oi->Filename); oi->Filename = NULL;
        free (oi->Keywords); oi->Keywords = NULL;
}

void
ptp_free_object (PTPObject *ob)
{
	unsigned int i;
	if (!ob) return;

	ptp_free_objectinfo (&ob->oi);
	for (i=0;i<ob->nrofmtpprops;i++)
		ptp_destroy_object_prop(&ob->mtpprops[i]);
	ob->flags = 0;
}

/* PTP error descriptions */
static struct {
	uint16_t rc;
	uint16_t vendor;
	const char *txt;
} ptp_errors[] = {
	{PTP_RC_Undefined,		0, N_("PTP Undefined Error")},
	{PTP_RC_OK,			0, N_("PTP OK!")},
	{PTP_RC_GeneralError,		0, N_("PTP General Error")},
	{PTP_RC_SessionNotOpen,		0, N_("PTP Session Not Open")},
	{PTP_RC_InvalidTransactionID,	0, N_("PTP Invalid Transaction ID")},
	{PTP_RC_OperationNotSupported,	0, N_("PTP Operation Not Supported")},
	{PTP_RC_ParameterNotSupported,	0, N_("PTP Parameter Not Supported")},
	{PTP_RC_IncompleteTransfer,	0, N_("PTP Incomplete Transfer")},
	{PTP_RC_InvalidStorageId,	0, N_("PTP Invalid Storage ID")},
	{PTP_RC_InvalidObjectHandle,	0, N_("PTP Invalid Object Handle")},
	{PTP_RC_DevicePropNotSupported,	0, N_("PTP Device Prop Not Supported")},
	{PTP_RC_InvalidObjectFormatCode,0, N_("PTP Invalid Object Format Code")},
	{PTP_RC_StoreFull,		0, N_("PTP Store Full")},
	{PTP_RC_ObjectWriteProtected,	0, N_("PTP Object Write Protected")},
	{PTP_RC_StoreReadOnly,		0, N_("PTP Store Read Only")},
	{PTP_RC_AccessDenied,		0, N_("PTP Access Denied")},
	{PTP_RC_NoThumbnailPresent,	0, N_("PTP No Thumbnail Present")},
	{PTP_RC_SelfTestFailed,		0, N_("PTP Self Test Failed")},
	{PTP_RC_PartialDeletion,	0, N_("PTP Partial Deletion")},
	{PTP_RC_StoreNotAvailable,	0, N_("PTP Store Not Available")},
	{PTP_RC_SpecificationByFormatUnsupported, 0, N_("PTP Specification By Format Unsupported")},
	{PTP_RC_NoValidObjectInfo,	0, N_("PTP No Valid Object Info")},
	{PTP_RC_InvalidCodeFormat,	0, N_("PTP Invalid Code Format")},
	{PTP_RC_UnknownVendorCode,	0, N_("PTP Unknown Vendor Code")},
	{PTP_RC_CaptureAlreadyTerminated, 0, N_("PTP Capture Already Terminated")},
	{PTP_RC_DeviceBusy,		0, N_("PTP Device Busy")},
	{PTP_RC_InvalidParentObject,	0, N_("PTP Invalid Parent Object")},
	{PTP_RC_InvalidDevicePropFormat,0, N_("PTP Invalid Device Prop Format")},
	{PTP_RC_InvalidDevicePropValue,	0, N_("PTP Invalid Device Prop Value")},
	{PTP_RC_InvalidParameter,	0, N_("PTP Invalid Parameter")},
	{PTP_RC_SessionAlreadyOpened,	0, N_("PTP Session Already Opened")},
	{PTP_RC_TransactionCanceled,	0, N_("PTP Transaction Canceled")},
	{PTP_RC_SpecificationOfDestinationUnsupported, 0, N_("PTP Specification Of Destination Unsupported")},

	{PTP_RC_EK_FilenameRequired,	PTP_VENDOR_EASTMAN_KODAK, N_("Filename Required")},
	{PTP_RC_EK_FilenameConflicts,	PTP_VENDOR_EASTMAN_KODAK, N_("Filename Conflicts")},
	{PTP_RC_EK_FilenameInvalid,	PTP_VENDOR_EASTMAN_KODAK, N_("Filename Invalid")},

	{PTP_RC_NIKON_HardwareError,		PTP_VENDOR_NIKON, N_("Hardware Error")},
	{PTP_RC_NIKON_OutOfFocus,		PTP_VENDOR_NIKON, N_("Out of Focus")},
	{PTP_RC_NIKON_ChangeCameraModeFailed,	PTP_VENDOR_NIKON, N_("Change Camera Mode Failed")},
	{PTP_RC_NIKON_InvalidStatus,		PTP_VENDOR_NIKON, N_("Invalid Status")},
	{PTP_RC_NIKON_SetPropertyNotSupported,	PTP_VENDOR_NIKON, N_("Set Property Not Supported")},
	{PTP_RC_NIKON_WbResetError,		PTP_VENDOR_NIKON, N_("Whitebalance Reset Error")},
	{PTP_RC_NIKON_DustReferenceError,	PTP_VENDOR_NIKON, N_("Dust Reference Error")},
	{PTP_RC_NIKON_ShutterSpeedBulb,		PTP_VENDOR_NIKON, N_("Shutter Speed Bulb")},
	{PTP_RC_NIKON_MirrorUpSequence,		PTP_VENDOR_NIKON, N_("Mirror Up Sequence")},
	{PTP_RC_NIKON_CameraModeNotAdjustFNumber, PTP_VENDOR_NIKON, N_("Camera Mode Not Adjust FNumber")},
	{PTP_RC_NIKON_NotLiveView,		PTP_VENDOR_NIKON, N_("Not in Liveview")},
	{PTP_RC_NIKON_MfDriveStepEnd,		PTP_VENDOR_NIKON, N_("Mf Drive Step End")},
	{PTP_RC_NIKON_MfDriveStepInsufficiency,	PTP_VENDOR_NIKON, N_("Mf Drive Step Insufficiency")},
	{PTP_RC_NIKON_AdvancedTransferCancel,	PTP_VENDOR_NIKON, N_("Advanced Transfer Cancel")},

	{PTP_RC_CANON_UNKNOWN_COMMAND,	PTP_VENDOR_CANON, N_("Unknown Command")},
	{PTP_RC_CANON_OPERATION_REFUSED,PTP_VENDOR_CANON, N_("Operation Refused")},
	{PTP_RC_CANON_LENS_COVER,	PTP_VENDOR_CANON, N_("Lens Cover Present")},
	{PTP_RC_CANON_BATTERY_LOW,	PTP_VENDOR_CANON, N_("Battery Low")},
	{PTP_RC_CANON_NOT_READY,	PTP_VENDOR_CANON, N_("Camera Not Ready")},

	{PTP_ERROR_TIMEOUT,		0, N_("PTP Timeout")},
	{PTP_ERROR_CANCEL,		0, N_("PTP Cancel Request")},
	{PTP_ERROR_BADPARAM,		0, N_("PTP Invalid Parameter")},
	{PTP_ERROR_RESP_EXPECTED,	0, N_("PTP Response Expected")},
	{PTP_ERROR_DATA_EXPECTED,	0, N_("PTP Data Expected")},
	{PTP_ERROR_IO,			0, N_("PTP I/O Error")},
	{0, 0, NULL}
};

const char *
ptp_strerror(uint16_t ret, uint16_t vendor)
{
	int i;

	for (i=0; ptp_errors[i].txt != NULL; i++)
		if ((ptp_errors[i].rc == ret) && ((ptp_errors[i].vendor == 0) || (ptp_errors[i].vendor == vendor)))
			return ptp_errors[i].txt;
	return NULL;
}

const char*
ptp_get_property_description(PTPParams* params, uint16_t dpc)
{
	int i;
	/* Device Property descriptions */
	struct {
		uint16_t dpc;
		const char *txt;
	} ptp_device_properties[] = {
		{PTP_DPC_Undefined,		N_("Undefined PTP Property")},
		{PTP_DPC_BatteryLevel,		N_("Battery Level")},
		{PTP_DPC_FunctionalMode,	N_("Functional Mode")},
		{PTP_DPC_ImageSize,		N_("Image Size")},
		{PTP_DPC_CompressionSetting,	N_("Compression Setting")},
		{PTP_DPC_WhiteBalance,		N_("White Balance")},
		{PTP_DPC_RGBGain,		N_("RGB Gain")},
		{PTP_DPC_FNumber,		N_("F-Number")},
		{PTP_DPC_FocalLength,		N_("Focal Length")},
		{PTP_DPC_FocusDistance,		N_("Focus Distance")},
		{PTP_DPC_FocusMode,		N_("Focus Mode")},
		{PTP_DPC_ExposureMeteringMode,	N_("Exposure Metering Mode")},
		{PTP_DPC_FlashMode,		N_("Flash Mode")},
		{PTP_DPC_ExposureTime,		N_("Exposure Time")},
		{PTP_DPC_ExposureProgramMode,	N_("Exposure Program Mode")},
		{PTP_DPC_ExposureIndex,
					N_("Exposure Index (film speed ISO)")},
		{PTP_DPC_ExposureBiasCompensation,
					N_("Exposure Bias Compensation")},
		{PTP_DPC_DateTime,		N_("Date & Time")},
		{PTP_DPC_CaptureDelay,		N_("Pre-Capture Delay")},
		{PTP_DPC_StillCaptureMode,	N_("Still Capture Mode")},
		{PTP_DPC_Contrast,		N_("Contrast")},
		{PTP_DPC_Sharpness,		N_("Sharpness")},
		{PTP_DPC_DigitalZoom,		N_("Digital Zoom")},
		{PTP_DPC_EffectMode,		N_("Effect Mode")},
		{PTP_DPC_BurstNumber,		N_("Burst Number")},
		{PTP_DPC_BurstInterval,		N_("Burst Interval")},
		{PTP_DPC_TimelapseNumber,	N_("Timelapse Number")},
		{PTP_DPC_TimelapseInterval,	N_("Timelapse Interval")},
		{PTP_DPC_FocusMeteringMode,	N_("Focus Metering Mode")},
		{PTP_DPC_UploadURL,		N_("Upload URL")},
		{PTP_DPC_Artist,		N_("Artist")},
		{PTP_DPC_CopyrightInfo,		N_("Copyright Info")},
		{PTP_DPC_SupportedStreams,	N_("Supported Streams")},
		{PTP_DPC_EnabledStreams,	N_("Enabled Streams")},
		{PTP_DPC_VideoFormat,		N_("Video Format")},
		{PTP_DPC_VideoResolution,	N_("Video Resolution")},
		{PTP_DPC_VideoQuality,		N_("Video Quality")},
		{PTP_DPC_VideoFrameRate,	N_("Video Framerate")},
		{PTP_DPC_VideoContrast,		N_("Video Contrast")},
		{PTP_DPC_VideoBrightness,	N_("Video Brightness")},
		{PTP_DPC_AudioFormat,		N_("Audio Format")},
		{PTP_DPC_AudioBitrate,		N_("Audio Bitrate")},
		{PTP_DPC_AudioSamplingRate,	N_("Audio Samplingrate")},
		{PTP_DPC_AudioBitPerSample,	N_("Audio Bits per sample")},
		{PTP_DPC_AudioVolume,		N_("Audio Volume")},
		{0,NULL}
	};
	struct {
		uint16_t dpc;
		const char *txt;
	} ptp_device_properties_EK[] = {
		{PTP_DPC_EK_ColorTemperature,	N_("Color Temperature")},
		{PTP_DPC_EK_DateTimeStampFormat,
					N_("Date Time Stamp Format")},
		{PTP_DPC_EK_BeepMode,		N_("Beep Mode")},
		{PTP_DPC_EK_VideoOut,		N_("Video Out")},
		{PTP_DPC_EK_PowerSaving,	N_("Power Saving")},
		{PTP_DPC_EK_UI_Language,	N_("UI Language")},
		{0,NULL}
	};

	struct {
		uint16_t dpc;
		const char *txt;
	} ptp_device_properties_Canon[] = {
		{PTP_DPC_CANON_BeepMode,	N_("Beep Mode")},
		{PTP_DPC_CANON_BatteryKind,	N_("Battery Type")},
		{PTP_DPC_CANON_BatteryStatus,	N_("Battery Mode")},
		{PTP_DPC_CANON_UILockType,	N_("UILockType")},
		{PTP_DPC_CANON_CameraMode,	N_("Camera Mode")},
		{PTP_DPC_CANON_ImageQuality,	N_("Image Quality")},
		{PTP_DPC_CANON_FullViewFileFormat,	N_("Full View File Format")},
		{PTP_DPC_CANON_ImageSize,	N_("Image Size")},
		{PTP_DPC_CANON_SelfTime,	N_("Self Time")},
		{PTP_DPC_CANON_FlashMode,	N_("Flash Mode")},
		{PTP_DPC_CANON_Beep,		N_("Beep")},
		{PTP_DPC_CANON_ShootingMode,	N_("Shooting Mode")},
		{PTP_DPC_CANON_ImageMode,	N_("Image Mode")},
		{PTP_DPC_CANON_DriveMode,	N_("Drive Mode")},
		{PTP_DPC_CANON_EZoom,		N_("Zoom")},
		{PTP_DPC_CANON_MeteringMode,	N_("Metering Mode")},
		{PTP_DPC_CANON_AFDistance,	N_("AF Distance")},
		{PTP_DPC_CANON_FocusingPoint,	N_("Focusing Point")},
		{PTP_DPC_CANON_WhiteBalance,	N_("White Balance")},
		{PTP_DPC_CANON_SlowShutterSetting,	N_("Slow Shutter Setting")},
		{PTP_DPC_CANON_AFMode,		N_("AF Mode")},
		{PTP_DPC_CANON_ImageStabilization,		N_("Image Stabilization")},
		{PTP_DPC_CANON_Contrast,	N_("Contrast")},
		{PTP_DPC_CANON_ColorGain,	N_("Color Gain")},
		{PTP_DPC_CANON_Sharpness,	N_("Sharpness")},
		{PTP_DPC_CANON_Sensitivity,	N_("Sensitivity")},
		{PTP_DPC_CANON_ParameterSet,	N_("Parameter Set")},
		{PTP_DPC_CANON_ISOSpeed,	N_("ISO Speed")},
		{PTP_DPC_CANON_Aperture,	N_("Aperture")},
		{PTP_DPC_CANON_ShutterSpeed,	N_("Shutter Speed")},
		{PTP_DPC_CANON_ExpCompensation,	N_("Exposure Compensation")},
		{PTP_DPC_CANON_FlashCompensation,	N_("Flash Compensation")},
		{PTP_DPC_CANON_AEBExposureCompensation,	N_("AEB Exposure Compensation")},
		{PTP_DPC_CANON_AvOpen,		N_("Av Open")},
		{PTP_DPC_CANON_AvMax,		N_("Av Max")},
		{PTP_DPC_CANON_FocalLength,	N_("Focal Length")},
		{PTP_DPC_CANON_FocalLengthTele,	N_("Focal Length Tele")},
		{PTP_DPC_CANON_FocalLengthWide,	N_("Focal Length Wide")},
		{PTP_DPC_CANON_FocalLengthDenominator,	N_("Focal Length Denominator")},
		{PTP_DPC_CANON_CaptureTransferMode,	N_("Capture Transfer Mode")},
		{PTP_DPC_CANON_Zoom,		N_("Zoom")},
		{PTP_DPC_CANON_NamePrefix,	N_("Name Prefix")},
		{PTP_DPC_CANON_SizeQualityMode,	N_("Size Quality Mode")},
		{PTP_DPC_CANON_SupportedThumbSize,	N_("Supported Thumb Size")},
		{PTP_DPC_CANON_SizeOfOutputDataFromCamera,	N_("Size of Output Data from Camera")},
		{PTP_DPC_CANON_SizeOfInputDataToCamera,		N_("Size of Input Data to Camera")},
		{PTP_DPC_CANON_RemoteAPIVersion,N_("Remote API Version")},
		{PTP_DPC_CANON_FirmwareVersion,	N_("Firmware Version")},
		{PTP_DPC_CANON_CameraModel,	N_("Camera Model")},
		{PTP_DPC_CANON_CameraOwner,	N_("Camera Owner")},
		{PTP_DPC_CANON_UnixTime,	N_("UNIX Time")},
		{PTP_DPC_CANON_CameraBodyID,	N_("Camera Body ID")},
		{PTP_DPC_CANON_CameraOutput,	N_("Camera Output")},
		{PTP_DPC_CANON_DispAv,		N_("Disp Av")},
		{PTP_DPC_CANON_AvOpenApex,	N_("Av Open Apex")},
		{PTP_DPC_CANON_DZoomMagnification,	N_("Digital Zoom Magnification")},
		{PTP_DPC_CANON_MlSpotPos,	N_("Ml Spot Position")},
		{PTP_DPC_CANON_DispAvMax,	N_("Disp Av Max")},
		{PTP_DPC_CANON_AvMaxApex,	N_("Av Max Apex")},
		{PTP_DPC_CANON_EZoomStartPosition,	N_("EZoom Start Position")},
		{PTP_DPC_CANON_FocalLengthOfTele,	N_("Focal Length Tele")},
		{PTP_DPC_CANON_EZoomSizeOfTele,	N_("EZoom Size of Tele")},
		{PTP_DPC_CANON_PhotoEffect,	N_("Photo Effect")},
		{PTP_DPC_CANON_AssistLight,	N_("Assist Light")},
		{PTP_DPC_CANON_FlashQuantityCount,	N_("Flash Quantity Count")},
		{PTP_DPC_CANON_RotationAngle,	N_("Rotation Angle")},
		{PTP_DPC_CANON_RotationScene,	N_("Rotation Scene")},
		{PTP_DPC_CANON_EventEmulateMode,N_("Event Emulate Mode")},
		{PTP_DPC_CANON_DPOFVersion,	N_("DPOF Version")},
		{PTP_DPC_CANON_TypeOfSupportedSlideShow,	N_("Type of Slideshow")},
		{PTP_DPC_CANON_AverageFilesizes,N_("Average Filesizes")},
		{PTP_DPC_CANON_ModelID,		N_("Model ID")},
		{0,NULL}
	};

	struct {
		uint16_t dpc;
		const char *txt;
	} ptp_device_properties_Nikon[] = {
		{PTP_DPC_NIKON_ShootingBank, 			/* 0xD010 */
		 N_("Shooting Bank")},
		{PTP_DPC_NIKON_ShootingBankNameA,		/* 0xD011 */
		 N_("Shooting Bank Name A")},
		{PTP_DPC_NIKON_ShootingBankNameB,		/* 0xD012 */
		 N_("Shooting Bank Name B")},
		{PTP_DPC_NIKON_ShootingBankNameC,		/* 0xD013 */
		 N_("Shooting Bank Name C")},
		{PTP_DPC_NIKON_ShootingBankNameD,		/* 0xD014 */
		 N_("Shooting Bank Name D")},
		{PTP_DPC_NIKON_ResetBank0,			/* 0xD015 */
		 N_("Reset Bank 0")},
		{PTP_DPC_NIKON_RawCompression,			/* 0xD016 */
		 N_("Raw Compression")},
		{PTP_DPC_NIKON_WhiteBalanceAutoBias,		/* 0xD017 */
		 N_("Auto White Balance Bias")},
		{PTP_DPC_NIKON_WhiteBalanceTungstenBias,	/* 0xD018 */
		 N_("Tungsten White Balance Bias")},
		{PTP_DPC_NIKON_WhiteBalanceFluorescentBias,	/* 0xD019 */
		 N_("Fluorescent White Balance Bias")},
		{PTP_DPC_NIKON_WhiteBalanceDaylightBias,	/* 0xD01a */
		 N_("Daylight White Balance Bias")},
		{PTP_DPC_NIKON_WhiteBalanceFlashBias,		/* 0xD01b */
		 N_("Flash White Balance Bias")},
		{PTP_DPC_NIKON_WhiteBalanceCloudyBias,		/* 0xD01c */
		 N_("Cloudy White Balance Bias")},
		{PTP_DPC_NIKON_WhiteBalanceShadeBias,		/* 0xD01d */
		 N_("Shady White Balance Bias")},
		{PTP_DPC_NIKON_WhiteBalanceColorTemperature,	/* 0xD01e */
		 N_("White Balance Colour Temperature")},
		{PTP_DPC_NIKON_WhiteBalancePresetNo,		/* 0xD01f */
		 N_("White Balance Preset Number")},
		{PTP_DPC_NIKON_WhiteBalancePresetName0,		/* 0xD020 */
		 N_("White Balance Preset Name 0")},
		{PTP_DPC_NIKON_WhiteBalancePresetName1,		/* 0xD021 */
		 N_("White Balance Preset Name 1")},
		{PTP_DPC_NIKON_WhiteBalancePresetName2,		/* 0xD022 */
		 N_("White Balance Preset Name 2")},
		{PTP_DPC_NIKON_WhiteBalancePresetName3,		/* 0xD023 */
		 N_("White Balance Preset Name 3")},
		{PTP_DPC_NIKON_WhiteBalancePresetName4,		/* 0xD024 */
		 N_("White Balance Preset Name 4")},
		{PTP_DPC_NIKON_WhiteBalancePresetVal0,		/* 0xD025 */
		 N_("White Balance Preset Value 0")},
		{PTP_DPC_NIKON_WhiteBalancePresetVal1,		/* 0xD026 */
		 N_("White Balance Preset Value 1")},
		{PTP_DPC_NIKON_WhiteBalancePresetVal2,		/* 0xD027 */
		 N_("White Balance Preset Value 2")},
		{PTP_DPC_NIKON_WhiteBalancePresetVal3,		/* 0xD028 */
		 N_("White Balance Preset Value 3")},
		{PTP_DPC_NIKON_WhiteBalancePresetVal4,		/* 0xD029 */
		 N_("White Balance Preset Value 4")},
		{PTP_DPC_NIKON_ImageSharpening,			/* 0xD02a */
		 N_("Sharpening")},
		{PTP_DPC_NIKON_ToneCompensation,		/* 0xD02b */
		 N_("Tone Compensation")},
		{PTP_DPC_NIKON_ColorModel,			/* 0xD02c */
		 N_("Color Model")},
		{PTP_DPC_NIKON_HueAdjustment,			/* 0xD02d */
		 N_("Hue Adjustment")},
		{PTP_DPC_NIKON_NonCPULensDataFocalLength,	/* 0xD02e */
		 N_("Lens Focal Length (Non CPU)")},
		{PTP_DPC_NIKON_NonCPULensDataMaximumAperture,	/* 0xD02f */
		 N_("Lens Maximum Aperture (Non CPU)")},
		{PTP_DPC_NIKON_ShootingMode,			/* 0xD030 */
		 N_("Shooting Mode")},
		{PTP_DPC_NIKON_JPEG_Compression_Policy,		/* 0xD031 */
		 N_("JPEG Compression Policy")},
		{PTP_DPC_NIKON_ColorSpace,			/* 0xD032 */
		 N_("Color Space")},
		{PTP_DPC_NIKON_AutoDXCrop,			/* 0xD033 */
		 N_("Auto DX Crop")},
		{PTP_DPC_NIKON_FlickerReduction,		/* 0xD034 */
		 N_("Flicker Reduction")},
		{PTP_DPC_NIKON_RemoteMode,			/* 0xD035 */
		 N_("Remote Mode")},
		{PTP_DPC_NIKON_VideoMode,			/* 0xD036 */
		 N_("Video Mode")},
		{PTP_DPC_NIKON_EffectMode,			/* 0xD037 */
		 N_("Effect Mode")},
		{PTP_DPC_NIKON_CSMMenuBankSelect,		/* 0xD040 */
		 "PTP_DPC_NIKON_CSMMenuBankSelect"},
		{PTP_DPC_NIKON_MenuBankNameA,			/* 0xD041 */
		 N_("Menu Bank Name A")},
		{PTP_DPC_NIKON_MenuBankNameB,			/* 0xD042 */
		 N_("Menu Bank Name B")},
		{PTP_DPC_NIKON_MenuBankNameC,			/* 0xD043 */
		 N_("Menu Bank Name C")},
		{PTP_DPC_NIKON_MenuBankNameD,			/* 0xD044 */
		 N_("Menu Bank Name D")},
		{PTP_DPC_NIKON_ResetBank,			/* 0xD045 */
		 N_("Reset Menu Bank")},
		{PTP_DPC_NIKON_A1AFCModePriority,		/* 0xD048 */
		 "PTP_DPC_NIKON_A1AFCModePriority"},
		{PTP_DPC_NIKON_A2AFSModePriority,		/* 0xD049 */
		 "PTP_DPC_NIKON_A2AFSModePriority"},
		{PTP_DPC_NIKON_A3GroupDynamicAF,		/* 0xD04a */
		 "PTP_DPC_NIKON_A3GroupDynamicAF"},
		{PTP_DPC_NIKON_A4AFActivation,			/* 0xD04b */
		 "PTP_DPC_NIKON_A4AFActivation"},
		{PTP_DPC_NIKON_FocusAreaIllumManualFocus,	/* 0xD04c */
		 "PTP_DPC_NIKON_FocusAreaIllumManualFocus"},
		{PTP_DPC_NIKON_FocusAreaIllumContinuous,	/* 0xD04d */
		 "PTP_DPC_NIKON_FocusAreaIllumContinuous"},
		{PTP_DPC_NIKON_FocusAreaIllumWhenSelected,	/* 0xD04e */
		 "PTP_DPC_NIKON_FocusAreaIllumWhenSelected"},
		{PTP_DPC_NIKON_FocusAreaWrap,			/* 0xD04f */
		 N_("Focus Area Wrap")},
		{PTP_DPC_NIKON_VerticalAFON,			/* 0xD050 */
		 N_("Vertical AF On")},
		{PTP_DPC_NIKON_AFLockOn,			/* 0xD051 */
		 N_("AF Lock On")},
		{PTP_DPC_NIKON_FocusAreaZone,			/* 0xD052 */
		 N_("Focus Area Zone")},
		{PTP_DPC_NIKON_EnableCopyright,			/* 0xD053 */
		 N_("Enable Copyright")},
		{PTP_DPC_NIKON_ISOAuto,				/* 0xD054 */
		 N_("Auto ISO")},
		{PTP_DPC_NIKON_EVISOStep,			/* 0xD055 */
		 N_("Exposure ISO Step")},
		{PTP_DPC_NIKON_EVStep,				/* 0xD056 */
		 N_("Exposure Step")},
		{PTP_DPC_NIKON_EVStepExposureComp,		/* 0xD057 */
		 N_("Exposure Compensation (EV)")},
		{PTP_DPC_NIKON_ExposureCompensation,		/* 0xD058 */
		 N_("Exposure Compensation")},
		{PTP_DPC_NIKON_CenterWeightArea,		/* 0xD059 */
		 N_("Centre Weight Area")},
		{PTP_DPC_NIKON_ExposureBaseMatrix,		/* 0xD05A */
		 N_("Exposure Base Matrix")},
		{PTP_DPC_NIKON_ExposureBaseCenter,		/* 0xD05B */
		 N_("Exposure Base Center")},
		{PTP_DPC_NIKON_ExposureBaseSpot,		/* 0xD05C */
		 N_("Exposure Base Spot")},
		{PTP_DPC_NIKON_LiveViewAFArea,			/* 0xD05D */
		 N_("Live View AF Area")},
		{PTP_DPC_NIKON_AELockMode,			/* 0xD05E */
		 N_("Exposure Lock")},
		{PTP_DPC_NIKON_AELAFLMode,			/* 0xD05F */
		 N_("Focus Lock")},
		{PTP_DPC_NIKON_LiveViewAFFocus,			/* 0xD061 */
		 N_("Live View AF Focus")},
		{PTP_DPC_NIKON_MeterOff,			/* 0xD062 */
		 N_("Auto Meter Off Time")},
		{PTP_DPC_NIKON_SelfTimer,			/* 0xD063 */
		 N_("Self Timer Delay")},
		{PTP_DPC_NIKON_MonitorOff,			/* 0xD064 */
		 N_("LCD Off Time")},
		{PTP_DPC_NIKON_ImgConfTime,			/* 0xD065 */
		 N_("Img Conf Time")},
		{PTP_DPC_NIKON_AutoOffTimers,			/* 0xD066 */
		 N_("Auto Off Timers")},
		{PTP_DPC_NIKON_AngleLevel,			/* 0xD067 */
		 N_("Angle Level")},
		{PTP_DPC_NIKON_D1ShootingSpeed,			/* 0xD068 */
		 N_("Shooting Speed")},
		{PTP_DPC_NIKON_D2MaximumShots,			/* 0xD069 */
		 N_("Maximum Shots")},
		{PTP_DPC_NIKON_ExposureDelayMode,		/* 0xD06A */
		 N_("Exposure delay mode")},
		{PTP_DPC_NIKON_LongExposureNoiseReduction,	/* 0xD06B */
		 N_("Long Exposure Noise Reduction")},
		{PTP_DPC_NIKON_FileNumberSequence,		/* 0xD06C */
		 N_("File Number Sequencing")},
		{PTP_DPC_NIKON_ControlPanelFinderRearControl,	/* 0xD06D */
		 "PTP_DPC_NIKON_ControlPanelFinderRearControl"},
		{PTP_DPC_NIKON_ControlPanelFinderViewfinder,	/* 0xD06E */
		 "PTP_DPC_NIKON_ControlPanelFinderViewfinder"},
		{PTP_DPC_NIKON_D7Illumination,			/* 0xD06F */
		 N_("LCD Illumination")},
		{PTP_DPC_NIKON_NrHighISO,			/* 0xD070 */
		 N_("High ISO noise reduction")},
		{PTP_DPC_NIKON_SHSET_CH_GUID_DISP,		/* 0xD071 */
		 N_("On screen tips")},
		{PTP_DPC_NIKON_ArtistName,			/* 0xD072 */
		 N_("Artist Name")},
		{PTP_DPC_NIKON_CopyrightInfo,			/* 0xD073 */
		 N_("Copyright Information")},
		{PTP_DPC_NIKON_FlashSyncSpeed,			/* 0xD074 */
		 N_("Flash Sync. Speed")},
		{PTP_DPC_NIKON_FlashShutterSpeed,		/* 0xD075 */
		 N_("Flash Shutter Speed")},
		{PTP_DPC_NIKON_E3AAFlashMode,			/* 0xD076 */
		 N_("Flash Mode")},
		{PTP_DPC_NIKON_E4ModelingFlash,			/* 0xD077 */
		 N_("Modeling Flash")},
		{PTP_DPC_NIKON_BracketSet,			/* 0xD078 */
		 N_("Bracket Set")},
		{PTP_DPC_NIKON_E6ManualModeBracketing,		/* 0xD079 */
		 N_("Manual Mode Bracketing")},
		{PTP_DPC_NIKON_BracketOrder,			/* 0xD07A */
		 N_("Bracket Order")},
		{PTP_DPC_NIKON_E8AutoBracketSelection,		/* 0xD07B */
		 N_("Auto Bracket Selection")},
		{PTP_DPC_NIKON_BracketingSet, N_("NIKON Auto Bracketing Set")},	/* 0xD07C */
		{PTP_DPC_NIKON_F1CenterButtonShootingMode,	/* 0xD080 */
		 N_("Center Button Shooting Mode")},
		{PTP_DPC_NIKON_CenterButtonPlaybackMode,	/* 0xD081 */
		 N_("Center Button Playback Mode")},
		{PTP_DPC_NIKON_F2Multiselector,			/* 0xD082 */
		 N_("Multiselector")},
		{PTP_DPC_NIKON_F3PhotoInfoPlayback,		/* 0xD083 */
		 N_("Photo Info. Playback")},
		{PTP_DPC_NIKON_F4AssignFuncButton,		/* 0xD084 */
		 N_("Assign Func. Button")},
		{PTP_DPC_NIKON_F5CustomizeCommDials,		/* 0xD085 */
		 N_("Customise Command Dials")},
		{PTP_DPC_NIKON_ReverseCommandDial,		/* 0xD086 */
		 N_("Reverse Command Dial")},
		{PTP_DPC_NIKON_ApertureSetting,			/* 0xD087 */
		 N_("Aperture Setting")},
		{PTP_DPC_NIKON_MenusAndPlayback,		/* 0xD088 */
		 N_("Menus and Playback")},
		{PTP_DPC_NIKON_F6ButtonsAndDials,		/* 0xD089 */
		 N_("Buttons and Dials")},
		{PTP_DPC_NIKON_NoCFCard,			/* 0xD08A */
		 N_("No CF Card Release")},
		{PTP_DPC_NIKON_CenterButtonZoomRatio,		/* 0xD08B */
		 N_("Center Button Zoom Ratio")},
		{PTP_DPC_NIKON_FunctionButton2,			/* 0xD08C */
		 N_("Function Button 2")},
		{PTP_DPC_NIKON_AFAreaPoint,			/* 0xD08D */
		 N_("AF Area Point")},
		{PTP_DPC_NIKON_NormalAFOn,			/* 0xD08E */
		 N_("Normal AF On")},
		{PTP_DPC_NIKON_CleanImageSensor,		/* 0xD08F */
		 N_("Clean Image Sensor")},
		{PTP_DPC_NIKON_ImageCommentString,		/* 0xD090 */
		 N_("Image Comment String")},
		{PTP_DPC_NIKON_ImageCommentEnable,		/* 0xD091 */
		 N_("Image Comment Enable")},
		{PTP_DPC_NIKON_ImageRotation,			/* 0xD092 */
		 N_("Image Rotation")},
		{PTP_DPC_NIKON_ManualSetLensNo,			/* 0xD093 */
		 N_("Manual Set Lens Number")},
		{PTP_DPC_NIKON_MovScreenSize,			/* 0xD0A0 */
		 N_("Movie Screen Size")},
		{PTP_DPC_NIKON_MovVoice,			/* 0xD0A1 */
		 N_("Movie Voice")},
		{PTP_DPC_NIKON_MovMicrophone,			/* 0xD0A2 */
		 N_("Movie Microphone")},
		{PTP_DPC_NIKON_MovFileSlot,			/* 0xD0A3 */
		 N_("Movie Card Slot")},
		{PTP_DPC_NIKON_ManualMovieSetting,		/* 0xD0A6 */
		 N_("Manual Movie Setting")},
		{PTP_DPC_NIKON_MovQuality,			/* 0xD0A7 */
		 N_("Movie Quality")},
		{PTP_DPC_NIKON_MonitorOffDelay,			/* 0xD0B3 */
		 N_("Monitor Off Delay")},
		{PTP_DPC_NIKON_Bracketing,			/* 0xD0C0 */
		 N_("Bracketing Enable")},
		{PTP_DPC_NIKON_AutoExposureBracketStep,		/* 0xD0C1 */
		 N_("Exposure Bracketing Step")},
		{PTP_DPC_NIKON_AutoExposureBracketProgram,	/* 0xD0C2 */
		 N_("Exposure Bracketing Program")},
		{PTP_DPC_NIKON_AutoExposureBracketCount,	/* 0xD0C3 */
		 N_("Auto Exposure Bracket Count")},
		{PTP_DPC_NIKON_WhiteBalanceBracketStep, N_("White Balance Bracket Step")}, /* 0xD0C4 */
		{PTP_DPC_NIKON_WhiteBalanceBracketProgram, N_("White Balance Bracket Program")}, /* 0xD0C5 */
		{PTP_DPC_NIKON_LensID,				/* 0xD0E0 */
		 N_("Lens ID")},
		{PTP_DPC_NIKON_LensSort,			/* 0xD0E1 */
		 N_("Lens Sort")},
		{PTP_DPC_NIKON_LensType,			/* 0xD0E2 */
		 N_("Lens Type")},
		{PTP_DPC_NIKON_FocalLengthMin,			/* 0xD0E3 */
		 N_("Min. Focal Length")},
		{PTP_DPC_NIKON_FocalLengthMax,			/* 0xD0E4 */
		 N_("Max. Focal Length")},
		{PTP_DPC_NIKON_MaxApAtMinFocalLength,		/* 0xD0E5 */
		 N_("Max. Aperture at Min. Focal Length")},
		{PTP_DPC_NIKON_MaxApAtMaxFocalLength,		/* 0xD0E6 */
		 N_("Max. Aperture at Max. Focal Length")},
		{PTP_DPC_NIKON_FinderISODisp,			/* 0xD0F0 */
		 N_("Finder ISO Display")},
		{PTP_DPC_NIKON_AutoOffPhoto,			/* 0xD0F2 */
		 N_("Auto Off Photo")},
		{PTP_DPC_NIKON_AutoOffMenu,			/* 0xD0F3 */
		 N_("Auto Off Menu")},
		{PTP_DPC_NIKON_AutoOffInfo,			/* 0xD0F4 */
		 N_("Auto Off Info")},
		{PTP_DPC_NIKON_SelfTimerShootNum,		/* 0xD0F5 */
		 N_("Self Timer Shot Number")},
		{PTP_DPC_NIKON_VignetteCtrl,			/* 0xD0F7 */
		 N_("Vignette Control")},
		{PTP_DPC_NIKON_AutoDistortionControl,		/* 0xD0F8 */
		 N_("Auto Distortion Control")},
		{PTP_DPC_NIKON_SceneMode,			/* 0xD0F9 */
		 N_("Scene Mode")},
		{PTP_DPC_NIKON_ExposureTime,			/* 0xD100 */
		 N_("Nikon Exposure Time")},
		{PTP_DPC_NIKON_ACPower, N_("AC Power")},	/* 0xD101 */
		{PTP_DPC_NIKON_WarningStatus, N_("Warning Status")},/* 0xD102 */
		{PTP_DPC_NIKON_MaximumShots,			/* 0xD103 */
		 N_("Maximum Shots")},
		{PTP_DPC_NIKON_AFLockStatus, N_("AF Locked")},/* 0xD104 */
		{PTP_DPC_NIKON_AELockStatus, N_("AE Locked")},/* 0xD105 */
		{PTP_DPC_NIKON_FVLockStatus, N_("FV Locked")},/* 0xD106 */
		{PTP_DPC_NIKON_AutofocusLCDTopMode2,		/* 0xD107 */
		 N_("AF LCD Top Mode 2")},
		{PTP_DPC_NIKON_AutofocusArea,			/* 0xD108 */
		 N_("Active AF Sensor")},
		{PTP_DPC_NIKON_FlexibleProgram,			/* 0xD109 */
		 N_("Flexible Program")},
		{PTP_DPC_NIKON_LightMeter,			/* 0xD10A */
		 N_("Exposure Meter")},
		{PTP_DPC_NIKON_RecordingMedia,			/* 0xD10B */
		 N_("Recording Media")},
		{PTP_DPC_NIKON_USBSpeed,			/* 0xD10C */
		 N_("USB Speed")},
		{PTP_DPC_NIKON_CCDNumber,			/* 0xD10D */
		 N_("CCD Serial Number")},
		{PTP_DPC_NIKON_CameraOrientation,		/* 0xD10E */
		 N_("Camera Orientation")},
		{PTP_DPC_NIKON_GroupPtnType,			/* 0xD10F */
		 N_("Group PTN Type")},
		{PTP_DPC_NIKON_FNumberLock,			/* 0xD110 */
		 N_("FNumber Lock")},
		{PTP_DPC_NIKON_ExposureApertureLock,		/* 0xD111 */
		 N_("Exposure Aperture Lock")},
		{PTP_DPC_NIKON_TVLockSetting,			/* 0xD112 */
		 N_("TV Lock Setting")},
		{PTP_DPC_NIKON_AVLockSetting,			/* 0xD113 */
		 N_("AV Lock Setting")},
		{PTP_DPC_NIKON_IllumSetting,			/* 0xD114 */
		 N_("Illum Setting")},
		{PTP_DPC_NIKON_FocusPointBright,		/* 0xD115 */
		 N_("Focus Point Bright")},
		{PTP_DPC_NIKON_ExternalFlashAttached,		/* 0xD120 */
		 N_("External Flash Attached")},
		{PTP_DPC_NIKON_ExternalFlashStatus,		/* 0xD121 */
		 N_("External Flash Status")},
		{PTP_DPC_NIKON_ExternalFlashSort,		/* 0xD122 */
		 N_("External Flash Sort")},
		{PTP_DPC_NIKON_ExternalFlashMode,		/* 0xD123 */
		 N_("External Flash Mode")},
		{PTP_DPC_NIKON_ExternalFlashCompensation,	/* 0xD124 */
		 N_("External Flash Compensation")},
		{PTP_DPC_NIKON_NewExternalFlashMode,		/* 0xD125 */
		 N_("External Flash Mode")},
		{PTP_DPC_NIKON_FlashExposureCompensation,	/* 0xD126 */
		 N_("Flash Exposure Compensation")},
		{PTP_DPC_NIKON_HDRMode,				/* 0xD130 */
		 N_("HDR Mode")},
		{PTP_DPC_NIKON_HDRHighDynamic,			/* 0xD131 */
		 N_("HDR High Dynamic")},
		{PTP_DPC_NIKON_HDRSmoothing,			/* 0xD132 */
		 N_("HDR Smoothing")},
		{PTP_DPC_NIKON_OptimizeImage,			/* 0xD140 */
		 N_("Optimize Image")},
		{PTP_DPC_NIKON_Saturation,			/* 0xD142 */
		 N_("Saturation")},
		{PTP_DPC_NIKON_BW_FillerEffect,			/* 0xD143 */
		 N_("BW Filler Effect")},
		{PTP_DPC_NIKON_BW_Sharpness,			/* 0xD144 */
		 N_("BW Sharpness")},
		{PTP_DPC_NIKON_BW_Contrast,			/* 0xD145 */
		 N_("BW Contrast")},
		{PTP_DPC_NIKON_BW_Setting_Type,			/* 0xD146 */
		 N_("BW Setting Type")},
		{PTP_DPC_NIKON_Slot2SaveMode,			/* 0xD148 */
		 N_("Slot 2 Save Mode")},
		{PTP_DPC_NIKON_RawBitMode,			/* 0xD149 */
		 N_("Raw Bit Mode")},
		{PTP_DPC_NIKON_ActiveDLighting,			/* 0xD14E */
		 N_("Active D-Lighting")},
		{PTP_DPC_NIKON_FlourescentType,			/* 0xD14F */
		 N_("Flourescent Type")},
		{PTP_DPC_NIKON_TuneColourTemperature,		/* 0xD150 */
		 N_("Tune Colour Temperature")},
		{PTP_DPC_NIKON_TunePreset0,			/* 0xD151 */
		 N_("Tune Preset 0")},
		{PTP_DPC_NIKON_TunePreset1,			/* 0xD152 */
		 N_("Tune Preset 1")},
		{PTP_DPC_NIKON_TunePreset2,			/* 0xD153 */
		 N_("Tune Preset 2")},
		{PTP_DPC_NIKON_TunePreset3,			/* 0xD154 */
		 N_("Tune Preset 3")},
		{PTP_DPC_NIKON_TunePreset4,			/* 0xD155 */
		 N_("Tune Preset 4")},
		{PTP_DPC_NIKON_BeepOff,				/* 0xD160 */
		 N_("AF Beep Mode")},
		{PTP_DPC_NIKON_AutofocusMode,			/* 0xD161 */
		 N_("Autofocus Mode")},
		{PTP_DPC_NIKON_AFAssist,			/* 0xD163 */
		 N_("AF Assist Lamp")},
		{PTP_DPC_NIKON_PADVPMode,			/* 0xD164 */
		 N_("Auto ISO P/A/DVP Setting")},
		{PTP_DPC_NIKON_ImageReview,			/* 0xD165 */
		 N_("Image Review")},
		{PTP_DPC_NIKON_AFAreaIllumination,		/* 0xD166 */
		 N_("AF Area Illumination")},
		{PTP_DPC_NIKON_FlashMode,			/* 0xD167 */
		 N_("Flash Mode")},
		{PTP_DPC_NIKON_FlashCommanderMode,	 	/* 0xD168 */
		 N_("Flash Commander Mode")},
		{PTP_DPC_NIKON_FlashSign,			/* 0xD169 */
		 N_("Flash Sign")},
		{PTP_DPC_NIKON_ISO_Auto,			/* 0xD16A */
		 N_("ISO Auto")},
		{PTP_DPC_NIKON_RemoteTimeout,			/* 0xD16B */
		 N_("Remote Timeout")},
		{PTP_DPC_NIKON_GridDisplay,			/* 0xD16C */
		 N_("Viewfinder Grid Display")},
		{PTP_DPC_NIKON_FlashModeManualPower,		/* 0xD16D */
		 N_("Flash Mode Manual Power")},
		{PTP_DPC_NIKON_FlashModeCommanderPower,		/* 0xD16E */
		 N_("Flash Mode Commander Power")},
		{PTP_DPC_NIKON_AutoFP,				/* 0xD16F */
		 N_("Auto FP")},
		{PTP_DPC_NIKON_CSMMenu,				/* 0xD180 */
		 N_("CSM Menu")},
		{PTP_DPC_NIKON_WarningDisplay,			/* 0xD181 */
		 N_("Warning Display")},
		{PTP_DPC_NIKON_BatteryCellKind,			/* 0xD182 */
		 N_("Battery Cell Kind")},
		{PTP_DPC_NIKON_ISOAutoHiLimit,			/* 0xD183 */
		 N_("ISO Auto High Limit")},
		{PTP_DPC_NIKON_DynamicAFArea,			/* 0xD184 */
		 N_("Dynamic AF Area")},
		{PTP_DPC_NIKON_ContinuousSpeedHigh,		/* 0xD186 */
		 N_("Continuous Speed High")},
		{PTP_DPC_NIKON_InfoDispSetting,			/* 0xD187 */
		 N_("Info Disp Setting")},
		{PTP_DPC_NIKON_PreviewButton,			/* 0xD189 */
		 N_("Preview Button")},
		{PTP_DPC_NIKON_PreviewButton2,			/* 0xD18A */
		 N_("Preview Button 2")},
		{PTP_DPC_NIKON_AEAFLockButton2,			/* 0xD18B */
		 N_("AEAF Lock Button 2")},
		{PTP_DPC_NIKON_IndicatorDisp,			/* 0xD18D */
		 N_("Indicator Display")},
		{PTP_DPC_NIKON_CellKindPriority,		/* 0xD18E */
		 N_("Cell Kind Priority")},
		{PTP_DPC_NIKON_BracketingFramesAndSteps,	/* 0xD190 */
		 N_("Bracketing Frames and Steps")},
		{PTP_DPC_NIKON_LiveViewMode,			/* 0xD1A0 */
		 N_("Live View Mode")},
		{PTP_DPC_NIKON_LiveViewDriveMode,		/* 0xD1A1 */
		 N_("Live View Drive Mode")},
		{PTP_DPC_NIKON_LiveViewStatus,			/* 0xD1A2 */
		 N_("Live View Status")},
		{PTP_DPC_NIKON_LiveViewImageZoomRatio,		/* 0xD1A3 */
		 N_("Live View Image Zoom Ratio")},
		{PTP_DPC_NIKON_LiveViewProhibitCondition,	/* 0xD1A4 */
		 N_("Live View Prohibit Condition")},
		{PTP_DPC_NIKON_ExposureDisplayStatus,		/* 0xD1B0 */
		 N_("Exposure Display Status")},
		{PTP_DPC_NIKON_ExposureIndicateStatus,		/* 0xD1B1 */
		 N_("Exposure Indicate Status")},
		{PTP_DPC_NIKON_InfoDispErrStatus,		/* 0xD1B2 */
		 N_("Info Display Error Status")},
		{PTP_DPC_NIKON_ExposureIndicateLightup,		/* 0xD1B3 */
		 N_("Exposure Indicate Lightup")},
		{PTP_DPC_NIKON_FlashOpen,			/* 0xD1C0 */
		 N_("Flash Open")},
		{PTP_DPC_NIKON_FlashCharged,			/* 0xD1C1 */
		 N_("Flash Charged")},
		{PTP_DPC_NIKON_FlashMRepeatValue,		/* 0xD1D0 */
		 N_("Flash MRepeat Value")},
		{PTP_DPC_NIKON_FlashMRepeatCount,		/* 0xD1D1 */
		 N_("Flash MRepeat Count")},
		{PTP_DPC_NIKON_FlashMRepeatInterval,		/* 0xD1D2 */
		 N_("Flash MRepeat Interval")},
		{PTP_DPC_NIKON_FlashCommandChannel,		/* 0xD1D3 */
		 N_("Flash Command Channel")},
		{PTP_DPC_NIKON_FlashCommandSelfMode,		/* 0xD1D4 */
		 N_("Flash Command Self Mode")},
		{PTP_DPC_NIKON_FlashCommandSelfCompensation,	/* 0xD1D5 */
		 N_("Flash Command Self Compensation")},
		{PTP_DPC_NIKON_FlashCommandSelfValue,		/* 0xD1D6 */
		 N_("Flash Command Self Value")},
		{PTP_DPC_NIKON_FlashCommandAMode,		/* 0xD1D7 */
		 N_("Flash Command A Mode")},
		{PTP_DPC_NIKON_FlashCommandACompensation,	/* 0xD1D8 */
		 N_("Flash Command A Compensation")},
		{PTP_DPC_NIKON_FlashCommandAValue,		/* 0xD1D9 */
		 N_("Flash Command A Value")},
		{PTP_DPC_NIKON_FlashCommandBMode,		/* 0xD1DA */
		 N_("Flash Command B Mode")},
		{PTP_DPC_NIKON_FlashCommandBCompensation,	/* 0xD1DB */
		 N_("Flash Command B Compensation")},
		{PTP_DPC_NIKON_FlashCommandBValue,		/* 0xD1DC */
		 N_("Flash Command B Value")},
		{PTP_DPC_NIKON_ActivePicCtrlItem,		/* 0xD200 */
		 N_("Active Pic Ctrl Item")},
		{PTP_DPC_NIKON_ChangePicCtrlItem,		/* 0xD201 */
		 N_("Change Pic Ctrl Item")},
		/* nikon 1 stuff */
		{PTP_DPC_NIKON_1_ISO,				/* 0xf002 */
		 N_("ISO")},
		{PTP_DPC_NIKON_1_ImageSize,			/* 0xf00a */
		 N_("Image Size")},
		{PTP_DPC_NIKON_1_LongExposureNoiseReduction,    /* 0xF00D */
		 N_("Long Exposure Noise Reduction")},
		{PTP_DPC_NIKON_1_MovQuality,                    /* 0xF01C */
		 N_("Movie Quality")},
		{PTP_DPC_NIKON_1_HiISONoiseReduction,           /* 0xF00E */
		 N_("High ISO Noise Reduction")},
		{PTP_DPC_NIKON_1_WhiteBalance,           	/* 0xF00C */
		 N_("White Balance")},
		{PTP_DPC_NIKON_1_ImageCompression,           	/* 0xF009 */
		 N_("Image Compression")},
		{PTP_DPC_NIKON_1_ActiveDLighting,           	/* 0xF00F */
		 N_("Active D-Lighting")},
		{0,NULL}
	};
        struct {
		uint16_t dpc;
		const char *txt;
        } ptp_device_properties_MTP[] = {
		{PTP_DPC_MTP_SecureTime,        N_("Secure Time")},		/* D101 */
		{PTP_DPC_MTP_DeviceCertificate, N_("Device Certificate")},	/* D102 */
		{PTP_DPC_MTP_RevocationInfo,    N_("Revocation Info")},		/* D103 */
		{PTP_DPC_MTP_SynchronizationPartner,				/* D401 */
		 N_("Synchronization Partner")},
		{PTP_DPC_MTP_DeviceFriendlyName,				/* D402 */
		 N_("Friendly Device Name")},
		{PTP_DPC_MTP_VolumeLevel,       N_("Volume Level")},		/* D403 */
		{PTP_DPC_MTP_DeviceIcon,        N_("Device Icon")},		/* D405 */
		{PTP_DPC_MTP_SessionInitiatorInfo,	N_("Session Initiator Info")},/* D406 */
		{PTP_DPC_MTP_PerceivedDeviceType,	N_("Perceived Device Type")},/* D407 */
		{PTP_DPC_MTP_PlaybackRate,      N_("Playback Rate")},		/* D410 */
		{PTP_DPC_MTP_PlaybackObject,    N_("Playback Object")},		/* D411 */
		{PTP_DPC_MTP_PlaybackContainerIndex,				/* D412 */
		 N_("Playback Container Index")},
		{PTP_DPC_MTP_PlaybackPosition,  N_("Playback Position")},	/* D413 */
		{PTP_DPC_MTP_PlaysForSureID,    N_("PlaysForSure ID")},		/* D131 (?) */
		{0,NULL}
        };
        struct {
		uint16_t dpc;
		const char *txt;
        } ptp_device_properties_FUJI[] = {
		{PTP_DPC_FUJI_ColorTemperature, N_("Color Temperature")},	/* 0xD017 */
		{PTP_DPC_FUJI_Quality, N_("Quality")},				/* 0xD018 */
		{PTP_DPC_FUJI_Quality, N_("Release Mode")},			/* 0xD201 */
		{PTP_DPC_FUJI_Quality, N_("Focus Areas")},			/* 0xD206 */
		{PTP_DPC_FUJI_Quality, N_("AE Lock")},				/* 0xD213 */
		{PTP_DPC_FUJI_Quality, N_("Aperture")},				/* 0xD218 */
		{PTP_DPC_FUJI_Quality, N_("Shutter Speed")},			/* 0xD219 */
		{0,NULL}
        };

        struct {
		uint16_t dpc;
		const char *txt;
        } ptp_device_properties_SONY[] = {
		{PTP_DPC_SONY_DPCCompensation, ("DOC Compensation")},	/* 0xD200 */
		{PTP_DPC_SONY_DRangeOptimize, ("DRangeOptimize")},	/* 0xD201 */
		{PTP_DPC_SONY_ImageSize, N_("Image size")},		/* 0xD203 */
		{PTP_DPC_SONY_ShutterSpeed, N_("Shutter speed")},	/* 0xD20D */
		{PTP_DPC_SONY_ColorTemp, N_("Color temperature")},	/* 0xD20F */
		{PTP_DPC_SONY_CCFilter, ("CC Filter")},			/* 0xD210 */
		{PTP_DPC_SONY_AspectRatio, N_("Aspect Ratio")}, 	/* 0xD211 */
		{PTP_DPC_SONY_FocusFound, N_("Focus status")},		/* 0xD213 */
		{PTP_DPC_SONY_ObjectInMemory, N_("Objects in memory")},	/* 0xD215 */
		{PTP_DPC_SONY_ExposeIndex, N_("Expose Index")},		/* 0xD216 */
		{PTP_DPC_SONY_BatteryLevel, N_("Battery Level")},	/* 0xD218 */
		{PTP_DPC_SONY_PictureEffect, N_("Picture Effect")},	/* 0xD21B */
		{PTP_DPC_SONY_ABFilter, N_("AB Filter")},		/* 0xD21C */
		{PTP_DPC_SONY_ISO, N_("ISO")},				/* 0xD21E */
		{PTP_DPC_SONY_Movie, N_("Movie")},			/* 0xD2C8 */
		{PTP_DPC_SONY_StillImage, N_("Still Image")},		/* 0xD2C7 */
		{0,NULL}
        };

        struct {
		uint16_t dpc;
		const char *txt;
        } ptp_device_properties_PARROT[] = {
		{PTP_DPC_PARROT_PhotoSensorEnableMask,		"PhotoSensorEnableMask"}, /* 0xD201 */
		{PTP_DPC_PARROT_PhotoSensorsKeepOn,		"PhotoSensorsKeepOn"}, /* 0xD202 */
		{PTP_DPC_PARROT_MultispectralImageSize,		"MultispectralImageSize"}, /* 0xD203 */
		{PTP_DPC_PARROT_MainBitDepth,			"MainBitDepth"}, /* 0xD204 */
		{PTP_DPC_PARROT_MultispectralBitDepth,		"MultispectralBitDepth"}, /* 0xD205 */
		{PTP_DPC_PARROT_HeatingEnable,			"HeatingEnable"}, /* 0xD206 */
		{PTP_DPC_PARROT_WifiStatus,			"WifiStatus"}, /* 0xD207 */
		{PTP_DPC_PARROT_WifiSSID,			"WifiSSID"}, /* 0xD208 */
		{PTP_DPC_PARROT_WifiEncryptionType,		"WifiEncryptionType"}, /* 0xD209 */
		{PTP_DPC_PARROT_WifiPassphrase,			"WifiPassphrase"}, /* 0xD20A */
		{PTP_DPC_PARROT_WifiChannel,			"WifiChannel"}, /* 0xD20B */
		{PTP_DPC_PARROT_Localization,			"Localization"}, /* 0xD20C */
		{PTP_DPC_PARROT_WifiMode,			"WifiMode"}, /* 0xD20D */
		{PTP_DPC_PARROT_AntiFlickeringFrequency,	"AntiFlickeringFrequency"}, /* 0xD210 */
		{PTP_DPC_PARROT_DisplayOverlayMask,		"DisplayOverlayMask"}, /* 0xD211 */
		{PTP_DPC_PARROT_GPSInterval,			"GPSInterval"}, /* 0xD212 */
		{PTP_DPC_PARROT_MultisensorsExposureMeteringMode,"MultisensorsExposureMeteringMode"}, /* 0xD213 */
		{PTP_DPC_PARROT_MultisensorsExposureTime,	"MultisensorsExposureTime"}, /* 0xD214 */
		{PTP_DPC_PARROT_MultisensorsExposureProgramMode,"MultisensorsExposureProgramMode"}, /* 0xD215 */
		{PTP_DPC_PARROT_MultisensorsExposureIndex,	"MultisensorsExposureIndex"}, /* 0xD216 */
		{PTP_DPC_PARROT_MultisensorsIrradianceGain,	"MultisensorsIrradianceGain"}, /* 0xD217 */
		{PTP_DPC_PARROT_MultisensorsIrradianceIntegrationTime,"MultisensorsIrradianceIntegrationTime"}, /* 0xD218 */
		{PTP_DPC_PARROT_OverlapRate,			"OverlapRate"}, /* 0xD219 */
		{0,NULL}
        };


	for (i=0; ptp_device_properties[i].txt!=NULL; i++)
		if (ptp_device_properties[i].dpc==dpc)
			return (ptp_device_properties[i].txt);

	if (params->deviceinfo.VendorExtensionID==PTP_VENDOR_MICROSOFT
	    || params->deviceinfo.VendorExtensionID==PTP_VENDOR_MTP)
		for (i=0; ptp_device_properties_MTP[i].txt!=NULL; i++)
			if (ptp_device_properties_MTP[i].dpc==dpc)
				return (ptp_device_properties_MTP[i].txt);

	if (params->deviceinfo.VendorExtensionID==PTP_VENDOR_EASTMAN_KODAK)
		for (i=0; ptp_device_properties_EK[i].txt!=NULL; i++)
			if (ptp_device_properties_EK[i].dpc==dpc)
				return (ptp_device_properties_EK[i].txt);

	if (params->deviceinfo.VendorExtensionID==PTP_VENDOR_CANON)
		for (i=0; ptp_device_properties_Canon[i].txt!=NULL; i++)
			if (ptp_device_properties_Canon[i].dpc==dpc)
				return (ptp_device_properties_Canon[i].txt);

	if (params->deviceinfo.VendorExtensionID==PTP_VENDOR_NIKON)
		for (i=0; ptp_device_properties_Nikon[i].txt!=NULL; i++)
			if (ptp_device_properties_Nikon[i].dpc==dpc)
				return (ptp_device_properties_Nikon[i].txt);

	if (params->deviceinfo.VendorExtensionID==PTP_VENDOR_FUJI)
		for (i=0; ptp_device_properties_FUJI[i].txt!=NULL; i++)
			if (ptp_device_properties_FUJI[i].dpc==dpc)
				return (ptp_device_properties_FUJI[i].txt);

	if (params->deviceinfo.VendorExtensionID==PTP_VENDOR_SONY)
		for (i=0; ptp_device_properties_SONY[i].txt!=NULL; i++)
			if (ptp_device_properties_SONY[i].dpc==dpc)
				return (ptp_device_properties_SONY[i].txt);
	if (params->deviceinfo.VendorExtensionID==PTP_VENDOR_PARROT)
		for (i=0; ptp_device_properties_PARROT[i].txt!=NULL; i++)
			if (ptp_device_properties_PARROT[i].dpc==dpc)
				return (ptp_device_properties_PARROT[i].txt);


	return NULL;
}

static int64_t
_value_to_num(PTPPropertyValue *data, uint16_t dt) {
	if (dt == PTP_DTC_STR) {
		if (!data->str)
			return 0;
		return atol(data->str);
	}
	if (dt & PTP_DTC_ARRAY_MASK) {
		return 0;
	} else {
		switch (dt) {
		case PTP_DTC_UNDEF: 
			return 0;
		case PTP_DTC_INT8:
			return data->i8;
		case PTP_DTC_UINT8:
			return data->u8;
		case PTP_DTC_INT16:
			return data->i16;
		case PTP_DTC_UINT16:
			return data->u16;
		case PTP_DTC_INT32:
			return data->i32;
		case PTP_DTC_UINT32:
			return data->u32;
	/*
		PTP_DTC_INT64           
		PTP_DTC_UINT64         
		PTP_DTC_INT128        
		PTP_DTC_UINT128      
	*/
		default:
			return 0;
		}
	}

	return 0;
}

#define PTP_VAL_BOOL(dpc) {dpc, 0, N_("Off")}, {dpc, 1, N_("On")}
#define PTP_VENDOR_VAL_BOOL(dpc,vendor) {dpc, vendor, 0, N_("Off")}, {dpc, vendor, 1, N_("On")}
#define PTP_VENDOR_VAL_RBOOL(dpc,vendor) {dpc, vendor, 0, N_("On")}, {dpc, vendor, 1, N_("Off")}
#define PTP_VENDOR_VAL_YN(dpc,vendor) {dpc, vendor, 0, N_("No")}, {dpc, vendor, 1, N_("Yes")}

int
ptp_render_property_value(PTPParams* params, uint16_t dpc,
			  PTPDevicePropDesc *dpd, unsigned int length, char *out)
{
	unsigned int i;
	int64_t	kval;

	struct {
		uint16_t dpc;
		uint16_t vendor;
		double coef;
		double bias;
		const char *format;
	} ptp_value_trans[] = {
		{PTP_DPC_BatteryLevel, 0, 1.0, 0.0, "%.0f%%"},		/* 5001 */
		{PTP_DPC_FNumber, 0, 0.01, 0.0, "f/%.2g"},		/* 5007 */
		{PTP_DPC_FocalLength, 0, 0.01, 0.0, "%.0f mm"},		/* 5008 */
		{PTP_DPC_FocusDistance, 0, 0.01, 0.0, "%.0f mm"},	/* 5009 */
		{PTP_DPC_ExposureTime, 0, 0.00001, 0.0, "%.2g sec"},	/* 500D */
		{PTP_DPC_ExposureIndex, 0, 1.0, 0.0, "ISO %.0f"},	/* 500F */
		{PTP_DPC_ExposureBiasCompensation, 0, 0.001, 0.0, N_("%.1f stops")},/* 5010 */
		{PTP_DPC_CaptureDelay, 0, 0.001, 0.0, "%.1fs"},		/* 5012 */
		{PTP_DPC_DigitalZoom, 0, 0.1, 0.0, "%.1f"},		/* 5016 */
		{PTP_DPC_BurstInterval, 0, 0.001, 0.0, "%.1fs"},	/* 5019 */

		/* Nikon device properties */
		{PTP_DPC_NIKON_LightMeter, PTP_VENDOR_NIKON, 0.08333, 0.0, N_("%.1f stops")},/* D10A */
		{PTP_DPC_NIKON_FlashExposureCompensation, PTP_VENDOR_NIKON, 0.16666, 0.0, N_("%.1f stops")}, /* D126 */
		{PTP_DPC_NIKON_CenterWeightArea, PTP_VENDOR_NIKON, 2.0, 6.0, N_("%.0f mm")},/* D059 */
		{PTP_DPC_NIKON_FocalLengthMin, PTP_VENDOR_NIKON, 0.01, 0.0, "%.0f mm"}, /* D0E3 */
		{PTP_DPC_NIKON_FocalLengthMax, PTP_VENDOR_NIKON, 0.01, 0.0, "%.0f mm"}, /* D0E4 */
		{PTP_DPC_NIKON_MaxApAtMinFocalLength, PTP_VENDOR_NIKON, 0.01, 0.0, "f/%.2g"}, /* D0E5 */
		{PTP_DPC_NIKON_MaxApAtMaxFocalLength, PTP_VENDOR_NIKON, 0.01, 0.0, "f/%.2g"}, /* D0E6 */
		{PTP_DPC_NIKON_ExternalFlashCompensation, PTP_VENDOR_NIKON, 1.0/6.0, 0.0,"%.0f"}, /* D124 */
		{PTP_DPC_NIKON_ExposureIndicateStatus, PTP_VENDOR_NIKON, 0.08333, 0.0, N_("%.1f stops")},/* D1B1 - FIXME: check if correct. */
		{PTP_DPC_NIKON_AngleLevel, PTP_VENDOR_NIKON, 1.0/65536, 0.0, "%.1f'"},/* 0xD067 */
		{0, 0, 0.0, 0.0, NULL}
	};

	struct {
		uint16_t dpc;
		uint16_t vendor;
		int64_t key;
		char *value;
	} ptp_value_list[] = {
		{PTP_DPC_CompressionSetting, 0, 0, N_("JPEG Basic")},	/* 5004 */
		{PTP_DPC_CompressionSetting, 0, 1, N_("JPEG Norm")},
		{PTP_DPC_CompressionSetting, 0, 2, N_("JPEG Fine")},
		{PTP_DPC_CompressionSetting, 0, 4, N_("RAW")},
		{PTP_DPC_CompressionSetting, 0, 5, N_("RAW + JPEG Basic")},
		{PTP_DPC_WhiteBalance, 0, 1, N_("Manual")},
		{PTP_DPC_WhiteBalance, 0, 2, N_("Automatic")},		/* 5005 */
		{PTP_DPC_WhiteBalance, 0, 3, N_("One-push Automatic")},
		{PTP_DPC_WhiteBalance, 0, 4, N_("Daylight")},
		{PTP_DPC_WhiteBalance, 0, 5, N_("Fluorescent")},
		{PTP_DPC_WhiteBalance, 0, 6, N_("Incandescent")},
		{PTP_DPC_WhiteBalance, 0, 7, N_("Flash")},
		{PTP_DPC_WhiteBalance, PTP_VENDOR_NIKON, 32784, N_("Cloudy")},
		{PTP_DPC_WhiteBalance, PTP_VENDOR_NIKON, 32785, N_("Shade")},
		{PTP_DPC_WhiteBalance, PTP_VENDOR_NIKON, 32786, N_("Color Temperature")},
		{PTP_DPC_WhiteBalance, PTP_VENDOR_NIKON, 32787, N_("Preset")},
		{PTP_DPC_FocusMode, 0, 1, N_("Manual Focus")},		/* 500A */
		{PTP_DPC_FocusMode, 0, 2, N_("Automatic")},
		{PTP_DPC_FocusMode, 0, 3, N_("Automatic Macro (close-up)")},
		{PTP_DPC_FocusMode, PTP_VENDOR_NIKON, 32784, "AF-S"},
		{PTP_DPC_FocusMode, PTP_VENDOR_NIKON, 32785, "AF-C"},
		{PTP_DPC_FocusMode, PTP_VENDOR_NIKON, 32786, "AF-A"},
		{PTP_DPC_ExposureMeteringMode, 0, 1, N_("Average")},	/* 500B */
		{PTP_DPC_ExposureMeteringMode, 0, 2, N_("Center Weighted Average")},
		{PTP_DPC_ExposureMeteringMode, 0, 3, N_("Multi-spot")},
		{PTP_DPC_ExposureMeteringMode, 0, 4, N_("Center-spot")},
		{PTP_DPC_FlashMode, 0, 0, N_("Undefined")},		/* 500C */
		{PTP_DPC_FlashMode, 0, 1, N_("Automatic flash")},
		{PTP_DPC_FlashMode, 0, 2, N_("Flash off")},
		{PTP_DPC_FlashMode, 0, 3, N_("Fill flash")},
		{PTP_DPC_FlashMode, 0, 4, N_("Automatic Red-eye Reduction")},
		{PTP_DPC_FlashMode, 0, 5, N_("Red-eye fill flash")},
		{PTP_DPC_FlashMode, 0, 6, N_("External sync")},
		{PTP_DPC_FlashMode, PTP_VENDOR_NIKON, 32784, N_("Auto")},
		{PTP_DPC_FlashMode, PTP_VENDOR_NIKON, 32785, N_("Auto Slow Sync")},
		{PTP_DPC_FlashMode, PTP_VENDOR_NIKON, 32786, N_("Rear Curtain Sync + Slow Sync")},
		{PTP_DPC_FlashMode, PTP_VENDOR_NIKON, 32787, N_("Red-eye Reduction + Slow Sync")},
		{PTP_DPC_ExposureProgramMode, 0, 1, "M"},		/* 500E */
		{PTP_DPC_ExposureProgramMode, 0, 3, "A"},
		{PTP_DPC_ExposureProgramMode, 0, 4, "S"},
		{PTP_DPC_ExposureProgramMode, 0, 2, "P"},
		{PTP_DPC_ExposureProgramMode, PTP_VENDOR_NIKON, 32784, N_("Auto")},
		{PTP_DPC_ExposureProgramMode, PTP_VENDOR_NIKON, 32785, N_("Portrait")},
		{PTP_DPC_ExposureProgramMode, PTP_VENDOR_NIKON, 32786, N_("Landscape")},
		{PTP_DPC_ExposureProgramMode, PTP_VENDOR_NIKON, 32787, N_("Macro")},
		{PTP_DPC_ExposureProgramMode, PTP_VENDOR_NIKON, 32788, N_("Sports")},
		{PTP_DPC_ExposureProgramMode, PTP_VENDOR_NIKON, 32790, N_("Night Landscape")},
		{PTP_DPC_ExposureProgramMode, PTP_VENDOR_NIKON, 32789, N_("Night Portrait")},
		{PTP_DPC_StillCaptureMode, 0, 1, N_("Single Shot")},	/* 5013 */
		{PTP_DPC_StillCaptureMode, 0, 2, N_("Power Wind")},
		{PTP_DPC_StillCaptureMode, 0, 3, N_("Timelapse")},
		{PTP_DPC_StillCaptureMode, PTP_VENDOR_NIKON, 32784, N_("Continuous Low Speed")},
		{PTP_DPC_StillCaptureMode, PTP_VENDOR_NIKON, 32785, N_("Timer")},
		{PTP_DPC_StillCaptureMode, PTP_VENDOR_NIKON, 32787, N_("Remote")},
		{PTP_DPC_StillCaptureMode, PTP_VENDOR_NIKON, 32787, N_("Mirror Up")},
		{PTP_DPC_StillCaptureMode, PTP_VENDOR_NIKON, 32788, N_("Timer + Remote")},
		{PTP_DPC_FocusMeteringMode, 0, 1, N_("Centre-spot")},	/* 501C */
		{PTP_DPC_FocusMeteringMode, 0, 2, N_("Multi-spot")},
		{PTP_DPC_FocusMeteringMode, PTP_VENDOR_NIKON, 32784, N_("Single Area")},
		{PTP_DPC_FocusMeteringMode, PTP_VENDOR_NIKON, 32785, N_("Closest Subject")},
		{PTP_DPC_FocusMeteringMode, PTP_VENDOR_NIKON, 32786, N_("Group Dynamic")},


		/* Nikon specific device properties */
		{PTP_DPC_NIKON_ImageSharpening, PTP_VENDOR_NIKON, 0, N_("Auto")},	/* D02A */
		{PTP_DPC_NIKON_ImageSharpening, PTP_VENDOR_NIKON, 1, N_("Normal")},
		{PTP_DPC_NIKON_ImageSharpening, PTP_VENDOR_NIKON, 2, N_("Low")},
		{PTP_DPC_NIKON_ImageSharpening, PTP_VENDOR_NIKON, 3, N_("Medium Low")},
		{PTP_DPC_NIKON_ImageSharpening, PTP_VENDOR_NIKON, 4, N_("Medium high")},
		{PTP_DPC_NIKON_ImageSharpening, PTP_VENDOR_NIKON, 5, N_("High")},
		{PTP_DPC_NIKON_ImageSharpening, PTP_VENDOR_NIKON, 6, N_("None")},

		{PTP_DPC_NIKON_ToneCompensation, PTP_VENDOR_NIKON, 0, N_("Auto")},	/* D02B */
		{PTP_DPC_NIKON_ToneCompensation, PTP_VENDOR_NIKON, 1, N_("Normal")},
		{PTP_DPC_NIKON_ToneCompensation, PTP_VENDOR_NIKON, 2, N_("Low contrast")},
		{PTP_DPC_NIKON_ToneCompensation, PTP_VENDOR_NIKON, 3, N_("Medium Low")},
		{PTP_DPC_NIKON_ToneCompensation, PTP_VENDOR_NIKON, 4, N_("Medium High")},
		{PTP_DPC_NIKON_ToneCompensation, PTP_VENDOR_NIKON, 5, N_("High control")},
		{PTP_DPC_NIKON_ToneCompensation, PTP_VENDOR_NIKON, 6, N_("Custom")},

		{PTP_DPC_NIKON_ColorModel, PTP_VENDOR_NIKON, 0, "sRGB"},		/* D02C */
		{PTP_DPC_NIKON_ColorModel, PTP_VENDOR_NIKON, 1, "AdobeRGB"},
		{PTP_DPC_NIKON_ColorModel, PTP_VENDOR_NIKON, 2, "sRGB"},

		PTP_VENDOR_VAL_BOOL(PTP_DPC_NIKON_AutoDXCrop,PTP_VENDOR_NIKON),	   	/* D033 */

		PTP_VENDOR_VAL_BOOL(PTP_DPC_NIKON_FocusAreaWrap,PTP_VENDOR_NIKON),   	/* D04F */

		PTP_VENDOR_VAL_BOOL(PTP_DPC_NIKON_EnableCopyright,PTP_VENDOR_NIKON),   	/* D053 */
		PTP_VENDOR_VAL_BOOL(PTP_DPC_NIKON_ISOAuto,PTP_VENDOR_NIKON),	   	/* D054 */

		/* FIXME! this is not ISO Auto (which is a bool) Perhaps ISO Auto Time?*/
		{PTP_DPC_NIKON_ISOAuto,	PTP_VENDOR_NIKON, 0, "1/125"},			/* D054 */
		{PTP_DPC_NIKON_ISOAuto,	PTP_VENDOR_NIKON, 1, "1/60"},
		{PTP_DPC_NIKON_ISOAuto,	PTP_VENDOR_NIKON, 2, "1/30"},
		{PTP_DPC_NIKON_ISOAuto,	PTP_VENDOR_NIKON, 3, "1/15"},
		{PTP_DPC_NIKON_ISOAuto,	PTP_VENDOR_NIKON, 4, "1/8"},
		{PTP_DPC_NIKON_ISOAuto,	PTP_VENDOR_NIKON, 5, "1/4"},
		{PTP_DPC_NIKON_ISOAuto,	PTP_VENDOR_NIKON, 6, "1/2"},
		{PTP_DPC_NIKON_ISOAuto,	PTP_VENDOR_NIKON, 7, "1"},
		{PTP_DPC_NIKON_ISOAuto,	PTP_VENDOR_NIKON, 8, "2"},
		{PTP_DPC_NIKON_ISOAuto,	PTP_VENDOR_NIKON, 9, "4"},
		{PTP_DPC_NIKON_ISOAuto,	PTP_VENDOR_NIKON, 10, "8"},
		{PTP_DPC_NIKON_ISOAuto,	PTP_VENDOR_NIKON, 11, "15"},
		{PTP_DPC_NIKON_ISOAuto,	PTP_VENDOR_NIKON, 12, "30"},

		{PTP_DPC_NIKON_EVStep, PTP_VENDOR_NIKON, 0, "1/3"},			/* D056 */
		{PTP_DPC_NIKON_EVStep, PTP_VENDOR_NIKON, 1, "1/2"},

		PTP_VENDOR_VAL_BOOL(PTP_DPC_NIKON_ExposureCompensation,PTP_VENDOR_NIKON),/*D058 */
		PTP_VENDOR_VAL_BOOL(PTP_DPC_NIKON_AELockMode,PTP_VENDOR_NIKON),    	/* D05E */

		{PTP_DPC_NIKON_AELAFLMode, PTP_VENDOR_NIKON, 0, N_("AE/AF Lock")},	/* D05F */
		{PTP_DPC_NIKON_AELAFLMode, PTP_VENDOR_NIKON, 1, N_("AF Lock only")},
		{PTP_DPC_NIKON_AELAFLMode, PTP_VENDOR_NIKON, 2, N_("AE Lock only")},
		{PTP_DPC_NIKON_AELAFLMode, PTP_VENDOR_NIKON, 3, N_("AF Lock Hold")},
		{PTP_DPC_NIKON_AELAFLMode, PTP_VENDOR_NIKON, 4, N_("AF On")},
		{PTP_DPC_NIKON_AELAFLMode, PTP_VENDOR_NIKON, 5, N_("Flash Lock")},

		{PTP_DPC_NIKON_MeterOff, PTP_VENDOR_NIKON, 0, N_("4 seconds")},		/* D062 */
		{PTP_DPC_NIKON_MeterOff, PTP_VENDOR_NIKON, 1, N_("6 seconds")},
		{PTP_DPC_NIKON_MeterOff, PTP_VENDOR_NIKON, 2, N_("8 seconds")},
		{PTP_DPC_NIKON_MeterOff, PTP_VENDOR_NIKON, 3, N_("16 seconds")},
		{PTP_DPC_NIKON_MeterOff, PTP_VENDOR_NIKON, 4, N_("30 minutes")},
		{PTP_DPC_NIKON_MeterOff, PTP_VENDOR_NIKON, 5, N_("30 seconds")},

		{PTP_DPC_NIKON_SelfTimer, PTP_VENDOR_NIKON, 0, N_("2 seconds")},	/* D063 */
		{PTP_DPC_NIKON_SelfTimer, PTP_VENDOR_NIKON, 1, N_("5 seconds")},
		{PTP_DPC_NIKON_SelfTimer, PTP_VENDOR_NIKON, 2, N_("10 seconds")},
		{PTP_DPC_NIKON_SelfTimer, PTP_VENDOR_NIKON, 3, N_("20 seconds")},

		{PTP_DPC_NIKON_MonitorOff, PTP_VENDOR_NIKON, 0, N_("10 seconds")},	/* D064 */
		{PTP_DPC_NIKON_MonitorOff, PTP_VENDOR_NIKON, 1, N_("20 seconds")},
		{PTP_DPC_NIKON_MonitorOff, PTP_VENDOR_NIKON, 2, N_("1 minute")},
		{PTP_DPC_NIKON_MonitorOff, PTP_VENDOR_NIKON, 3, N_("5 minutes")},
		{PTP_DPC_NIKON_MonitorOff, PTP_VENDOR_NIKON, 4, N_("10 minutes")},
		{PTP_DPC_NIKON_MonitorOff, PTP_VENDOR_NIKON, 5, N_("5 seconds")}, /* d80 observed */

		PTP_VENDOR_VAL_BOOL(PTP_DPC_NIKON_ExposureDelayMode,PTP_VENDOR_NIKON),	/* D06A */
		PTP_VENDOR_VAL_BOOL(PTP_DPC_NIKON_LongExposureNoiseReduction,PTP_VENDOR_NIKON),	/* D06B */
		PTP_VENDOR_VAL_BOOL(PTP_DPC_NIKON_FileNumberSequence,PTP_VENDOR_NIKON),	/* D06C */
		PTP_VENDOR_VAL_BOOL(PTP_DPC_NIKON_D7Illumination,PTP_VENDOR_NIKON),	/* D06F */

		PTP_VENDOR_VAL_BOOL(PTP_DPC_NIKON_SHSET_CH_GUID_DISP,PTP_VENDOR_NIKON),	/* D071 */

		{PTP_DPC_NIKON_FlashShutterSpeed, PTP_VENDOR_NIKON, 0, "1/60s"},		/* D075 */
		{PTP_DPC_NIKON_FlashShutterSpeed, PTP_VENDOR_NIKON, 1, "1/30s"},
		{PTP_DPC_NIKON_FlashShutterSpeed, PTP_VENDOR_NIKON, 2, "1/15s"},
		{PTP_DPC_NIKON_FlashShutterSpeed, PTP_VENDOR_NIKON, 3, "1/8s"},
		{PTP_DPC_NIKON_FlashShutterSpeed, PTP_VENDOR_NIKON, 4, "1/4s"},
		{PTP_DPC_NIKON_FlashShutterSpeed, PTP_VENDOR_NIKON, 5, "1/2s"},
		{PTP_DPC_NIKON_FlashShutterSpeed, PTP_VENDOR_NIKON, 6, "1s"},
		{PTP_DPC_NIKON_FlashShutterSpeed, PTP_VENDOR_NIKON, 7, "2s"},
		{PTP_DPC_NIKON_FlashShutterSpeed, PTP_VENDOR_NIKON, 8, "4s"},
		{PTP_DPC_NIKON_FlashShutterSpeed, PTP_VENDOR_NIKON, 9, "8s"},
		{PTP_DPC_NIKON_FlashShutterSpeed, PTP_VENDOR_NIKON, 10, "15s"},
		{PTP_DPC_NIKON_FlashShutterSpeed, PTP_VENDOR_NIKON, 11, "30s"},

		PTP_VENDOR_VAL_BOOL(PTP_DPC_NIKON_E4ModelingFlash,PTP_VENDOR_NIKON),	/* D077 */

		{PTP_DPC_NIKON_BracketSet, PTP_VENDOR_NIKON, 0, N_("AE & Flash")},	/* D078 */
		{PTP_DPC_NIKON_BracketSet, PTP_VENDOR_NIKON, 1, N_("AE only")},
		{PTP_DPC_NIKON_BracketSet, PTP_VENDOR_NIKON, 2, N_("Flash only")},
		{PTP_DPC_NIKON_BracketSet, PTP_VENDOR_NIKON, 3, N_("WB bracketing")},

		{PTP_DPC_NIKON_BracketOrder, PTP_VENDOR_NIKON, 0, N_("MTR > Under")},	/* D07A */
		{PTP_DPC_NIKON_BracketOrder, PTP_VENDOR_NIKON, 1, N_("Under > MTR")},

		{PTP_DPC_NIKON_F1CenterButtonShootingMode, PTP_VENDOR_NIKON, 0, N_("Reset focus point to center")}, /* D080 */
		{PTP_DPC_NIKON_F1CenterButtonShootingMode, PTP_VENDOR_NIKON, 1, N_("Highlight active focus point")},
		{PTP_DPC_NIKON_F1CenterButtonShootingMode, PTP_VENDOR_NIKON, 2, N_("Unused")},

		PTP_VENDOR_VAL_BOOL(PTP_DPC_NIKON_F3PhotoInfoPlayback,PTP_VENDOR_NIKON),/* D083 */
		PTP_VENDOR_VAL_BOOL(PTP_DPC_NIKON_F5CustomizeCommDials,PTP_VENDOR_NIKON),/* D085 */
		PTP_VENDOR_VAL_BOOL(PTP_DPC_NIKON_ReverseCommandDial,PTP_VENDOR_NIKON),	/* D086 */
		PTP_VENDOR_VAL_RBOOL(PTP_DPC_NIKON_F6ButtonsAndDials,PTP_VENDOR_NIKON),	/* D089 */
		PTP_VENDOR_VAL_RBOOL(PTP_DPC_NIKON_NoCFCard,PTP_VENDOR_NIKON),		/* D08A */
		PTP_VENDOR_VAL_RBOOL(PTP_DPC_NIKON_AFAreaPoint,PTP_VENDOR_NIKON),	/* D08D */

		PTP_VENDOR_VAL_BOOL(PTP_DPC_NIKON_ImageCommentEnable,PTP_VENDOR_NIKON),	/* D091 */
		PTP_VENDOR_VAL_RBOOL(PTP_DPC_NIKON_ImageRotation,PTP_VENDOR_NIKON),	/* D092 */

		PTP_VENDOR_VAL_RBOOL(PTP_DPC_NIKON_MovVoice,PTP_VENDOR_NIKON),		/* D0A1 */

		PTP_VENDOR_VAL_BOOL(PTP_DPC_NIKON_Bracketing,PTP_VENDOR_NIKON),		/* D0C0 */

		/* http://www.rottmerhusen.com/objektives/lensid/nikkor.html is complete */
		{PTP_DPC_NIKON_LensID, PTP_VENDOR_NIKON, 0, N_("Unknown")},		/* D0E0 */
		{PTP_DPC_NIKON_LensID, PTP_VENDOR_NIKON, 38, "Sigma 70-300mm 1:4-5.6 D APO Macro"},
		{PTP_DPC_NIKON_LensID, PTP_VENDOR_NIKON, 83, "AF Nikkor 80-200mm 1:2.8 D ED"},
		{PTP_DPC_NIKON_LensID, PTP_VENDOR_NIKON, 118, "AF Nikkor 50mm 1:1.8 D"},
		{PTP_DPC_NIKON_LensID, PTP_VENDOR_NIKON, 127, "AF-S Nikkor 18-70mm 1:3.5-4.5G ED DX"},
		{PTP_DPC_NIKON_LensID, PTP_VENDOR_NIKON, 139, "AF-S Nikkor 18-200mm 1:3.5-5.6 GED DX VR"},
		{PTP_DPC_NIKON_LensID, PTP_VENDOR_NIKON, 147, "AF-S Nikkor 24-70mm 1:2.8G ED DX"},
		{PTP_DPC_NIKON_LensID, PTP_VENDOR_NIKON, 154, "AF-S Nikkor 18-55mm 1:3.5-F5.6G DX VR"},
		{PTP_DPC_NIKON_LensID, PTP_VENDOR_NIKON, 159, "AF-S Nikkor 35mm 1:1.8G DX"},
		{PTP_DPC_NIKON_FinderISODisp, PTP_VENDOR_NIKON, 0, "Show ISO sensitivity"},/* 0xD0F0 */
		{PTP_DPC_NIKON_FinderISODisp, PTP_VENDOR_NIKON, 1, "Show ISO/Easy ISO"},
		{PTP_DPC_NIKON_FinderISODisp, PTP_VENDOR_NIKON, 2, "Show frame count"},

		{PTP_DPC_NIKON_RawCompression, PTP_VENDOR_NIKON, 0, N_("Lossless")},	/* D016 */
		{PTP_DPC_NIKON_RawCompression, PTP_VENDOR_NIKON, 1, N_("Lossy")},

		PTP_VENDOR_VAL_YN(PTP_DPC_NIKON_ACPower,PTP_VENDOR_NIKON),		/* D101 */
		PTP_VENDOR_VAL_YN(PTP_DPC_NIKON_AFLockStatus,PTP_VENDOR_NIKON),		/* D104 */
		PTP_VENDOR_VAL_YN(PTP_DPC_NIKON_AELockStatus,PTP_VENDOR_NIKON),		/* D105 */
		PTP_VENDOR_VAL_YN(PTP_DPC_NIKON_FVLockStatus,PTP_VENDOR_NIKON),		/* D106 */

		{PTP_DPC_NIKON_AutofocusArea, PTP_VENDOR_NIKON, 0, N_("Centre")},	/* D108 */
		{PTP_DPC_NIKON_AutofocusArea, PTP_VENDOR_NIKON, 1, N_("Top")},
		{PTP_DPC_NIKON_AutofocusArea, PTP_VENDOR_NIKON, 2, N_("Bottom")},
		{PTP_DPC_NIKON_AutofocusArea, PTP_VENDOR_NIKON, 3, N_("Left")},
		{PTP_DPC_NIKON_AutofocusArea, PTP_VENDOR_NIKON, 4, N_("Right")},

		{PTP_DPC_NIKON_RecordingMedia, PTP_VENDOR_NIKON, 0, N_("Card")},	/* D10B */
		{PTP_DPC_NIKON_RecordingMedia, PTP_VENDOR_NIKON, 1, N_("SDRam")},

		{PTP_DPC_NIKON_USBSpeed, PTP_VENDOR_NIKON, 0, N_("USB 1.1")},		/* D10C */
		{PTP_DPC_NIKON_USBSpeed, PTP_VENDOR_NIKON, 1, N_("USB 2.0")},

		{PTP_DPC_NIKON_CameraOrientation, PTP_VENDOR_NIKON, 0, "0'"},		/* D10E */
		{PTP_DPC_NIKON_CameraOrientation, PTP_VENDOR_NIKON, 1, "270'"},
		{PTP_DPC_NIKON_CameraOrientation, PTP_VENDOR_NIKON, 2, "90'"},
		{PTP_DPC_NIKON_CameraOrientation, PTP_VENDOR_NIKON, 3, "180'"},

		PTP_VENDOR_VAL_YN(PTP_DPC_NIKON_FNumberLock,PTP_VENDOR_NIKON),		/* D110 */
		PTP_VENDOR_VAL_YN(PTP_DPC_NIKON_ExposureApertureLock,PTP_VENDOR_NIKON),	/* D111 */
		PTP_VENDOR_VAL_YN(PTP_DPC_NIKON_TVLockSetting,PTP_VENDOR_NIKON),	/* D112 */
		PTP_VENDOR_VAL_YN(PTP_DPC_NIKON_AVLockSetting,PTP_VENDOR_NIKON),	/* D113 */

		{PTP_DPC_NIKON_IllumSetting,PTP_VENDOR_NIKON,0,N_("LCD Backlight")},	/* D114 */
		{PTP_DPC_NIKON_IllumSetting,PTP_VENDOR_NIKON,1,N_("LCD Backlight and Info Display")},

		PTP_VENDOR_VAL_YN(PTP_DPC_NIKON_ExternalFlashAttached,PTP_VENDOR_NIKON),/* D120 */
		PTP_VENDOR_VAL_YN(PTP_DPC_NIKON_ExternalFlashStatus,PTP_VENDOR_NIKON),	/* D121 */

		{PTP_DPC_NIKON_OptimizeImage, PTP_VENDOR_NIKON, 0, N_("Normal")},	/* D140 */
		{PTP_DPC_NIKON_OptimizeImage, PTP_VENDOR_NIKON, 1, N_("Vivid")},
		{PTP_DPC_NIKON_OptimizeImage, PTP_VENDOR_NIKON, 2, N_("Sharper")},
		{PTP_DPC_NIKON_OptimizeImage, PTP_VENDOR_NIKON, 3, N_("Softer")},
		{PTP_DPC_NIKON_OptimizeImage, PTP_VENDOR_NIKON, 4, N_("Direct Print")},
		{PTP_DPC_NIKON_OptimizeImage, PTP_VENDOR_NIKON, 5, N_("Portrait")},
		{PTP_DPC_NIKON_OptimizeImage, PTP_VENDOR_NIKON, 6, N_("Landscape")},
		{PTP_DPC_NIKON_OptimizeImage, PTP_VENDOR_NIKON, 7, N_("Custom")},

		{PTP_DPC_NIKON_Saturation, PTP_VENDOR_NIKON, 0, N_("Normal")},		/* D142 */
		{PTP_DPC_NIKON_Saturation, PTP_VENDOR_NIKON, 1, N_("Moderate")},
		{PTP_DPC_NIKON_Saturation, PTP_VENDOR_NIKON, 2, N_("Enhanced")},

		PTP_VENDOR_VAL_RBOOL(PTP_DPC_NIKON_BeepOff,PTP_VENDOR_NIKON),		/* D160 */

		{PTP_DPC_NIKON_AutofocusMode, PTP_VENDOR_NIKON, 0, N_("AF-S")},	 	/* D161 */
		{PTP_DPC_NIKON_AutofocusMode, PTP_VENDOR_NIKON, 1, N_("AF-C")},
		{PTP_DPC_NIKON_AutofocusMode, PTP_VENDOR_NIKON, 2, N_("AF-A")},
		{PTP_DPC_NIKON_AutofocusMode, PTP_VENDOR_NIKON, 3, N_("MF (fixed)")},
		{PTP_DPC_NIKON_AutofocusMode, PTP_VENDOR_NIKON, 4, N_("MF (selection)")},

		PTP_VENDOR_VAL_RBOOL(PTP_DPC_NIKON_AFAssist,PTP_VENDOR_NIKON),   	/* D163 */

		{PTP_DPC_NIKON_PADVPMode, PTP_VENDOR_NIKON, 0,  "1/125"},		/* D164 */
		{PTP_DPC_NIKON_PADVPMode, PTP_VENDOR_NIKON, 1,  "1/60"},
		{PTP_DPC_NIKON_PADVPMode, PTP_VENDOR_NIKON, 2,  "1/30"},
		{PTP_DPC_NIKON_PADVPMode, PTP_VENDOR_NIKON, 3,  "1/15"},
		{PTP_DPC_NIKON_PADVPMode, PTP_VENDOR_NIKON, 4,  "1/8"},
		{PTP_DPC_NIKON_PADVPMode, PTP_VENDOR_NIKON, 5,  "1/4"},
		{PTP_DPC_NIKON_PADVPMode, PTP_VENDOR_NIKON, 6,  "1/2"},
		{PTP_DPC_NIKON_PADVPMode, PTP_VENDOR_NIKON, 7,  "1"},
		{PTP_DPC_NIKON_PADVPMode, PTP_VENDOR_NIKON, 8,  "2"},
		{PTP_DPC_NIKON_PADVPMode, PTP_VENDOR_NIKON, 9,  "4"},
		{PTP_DPC_NIKON_PADVPMode, PTP_VENDOR_NIKON, 10, "8"},
		{PTP_DPC_NIKON_PADVPMode, PTP_VENDOR_NIKON, 11, "15"},
		{PTP_DPC_NIKON_PADVPMode, PTP_VENDOR_NIKON, 12, "30"},

		PTP_VENDOR_VAL_RBOOL(PTP_DPC_NIKON_ImageReview,PTP_VENDOR_NIKON),	/* D165 */

		{PTP_DPC_NIKON_AFAreaIllumination, PTP_VENDOR_NIKON, 0, N_("Auto")},	/* D166 */
		{PTP_DPC_NIKON_AFAreaIllumination, PTP_VENDOR_NIKON, 1, N_("Off")},
		{PTP_DPC_NIKON_AFAreaIllumination, PTP_VENDOR_NIKON, 2, N_("On")},

		{PTP_DPC_NIKON_FlashMode, PTP_VENDOR_NIKON, 0, "iTTL"},			/* D167 */
		{PTP_DPC_NIKON_FlashMode, PTP_VENDOR_NIKON, 1, N_("Manual")},
		{PTP_DPC_NIKON_FlashMode, PTP_VENDOR_NIKON, 2, N_("Commander")},

		{PTP_DPC_NIKON_FlashCommanderMode, PTP_VENDOR_NIKON, 0, N_("TTL")},	/* D168 */
		{PTP_DPC_NIKON_FlashCommanderMode, PTP_VENDOR_NIKON, 1, N_("Auto Aperture")},
		{PTP_DPC_NIKON_FlashCommanderMode, PTP_VENDOR_NIKON, 2, N_("Full Manual")},

		PTP_VENDOR_VAL_RBOOL(PTP_DPC_NIKON_FlashSign,PTP_VENDOR_NIKON),		/* D169 */

		{PTP_DPC_NIKON_RemoteTimeout, PTP_VENDOR_NIKON, 0, N_("1 min")},	/* D16B */
		{PTP_DPC_NIKON_RemoteTimeout, PTP_VENDOR_NIKON, 1, N_("5 mins")},
		{PTP_DPC_NIKON_RemoteTimeout, PTP_VENDOR_NIKON, 2, N_("10 mins")},
		{PTP_DPC_NIKON_RemoteTimeout, PTP_VENDOR_NIKON, 3, N_("15 mins")},

		PTP_VENDOR_VAL_BOOL(PTP_DPC_NIKON_GridDisplay,PTP_VENDOR_NIKON),	/* D16C */

		{PTP_DPC_NIKON_FlashModeManualPower, PTP_VENDOR_NIKON, 0, N_("Full")},	/* D16D */
		{PTP_DPC_NIKON_FlashModeManualPower, PTP_VENDOR_NIKON, 1, "1/2"},
		{PTP_DPC_NIKON_FlashModeManualPower, PTP_VENDOR_NIKON, 2, "1/4"},
		{PTP_DPC_NIKON_FlashModeManualPower, PTP_VENDOR_NIKON, 3, "1/8"},
		{PTP_DPC_NIKON_FlashModeManualPower, PTP_VENDOR_NIKON, 4, "1/16"},

		{PTP_DPC_NIKON_FlashModeCommanderPower, PTP_VENDOR_NIKON, 0, N_("Full")},/* D16E */
		{PTP_DPC_NIKON_FlashModeCommanderPower, PTP_VENDOR_NIKON, 1, "1/2"},
		{PTP_DPC_NIKON_FlashModeCommanderPower, PTP_VENDOR_NIKON, 2, "1/4"},
		{PTP_DPC_NIKON_FlashModeCommanderPower, PTP_VENDOR_NIKON, 3, "1/8"},
		{PTP_DPC_NIKON_FlashModeCommanderPower, PTP_VENDOR_NIKON, 4, "1/16"},
		{PTP_DPC_NIKON_FlashModeCommanderPower, PTP_VENDOR_NIKON, 5, "1/32"},
		{PTP_DPC_NIKON_FlashModeCommanderPower, PTP_VENDOR_NIKON, 6, "1/64"},
		{PTP_DPC_NIKON_FlashModeCommanderPower, PTP_VENDOR_NIKON, 7, "1/128"},

		PTP_VENDOR_VAL_YN(PTP_DPC_NIKON_CSMMenu,PTP_VENDOR_NIKON),		/* D180 */
		PTP_VENDOR_VAL_YN(PTP_DPC_NIKON_WarningDisplay,PTP_VENDOR_NIKON),	/* D181 */

		{PTP_DPC_NIKON_BatteryCellKind, PTP_VENDOR_NIKON, 0, "LR6 (AA alkaline)"},/* D182 */
		{PTP_DPC_NIKON_BatteryCellKind, PTP_VENDOR_NIKON, 1, "HR6 (AA Ni-Mh)"},
		{PTP_DPC_NIKON_BatteryCellKind, PTP_VENDOR_NIKON, 2, "FR6 (AA Lithium)"},
		{PTP_DPC_NIKON_BatteryCellKind, PTP_VENDOR_NIKON, 3, "ZR6 (AA Ni-Mn)"},

		{PTP_DPC_NIKON_ISOAutoHiLimit, PTP_VENDOR_NIKON, 0, "400"},		/* D183 */
		{PTP_DPC_NIKON_ISOAutoHiLimit, PTP_VENDOR_NIKON, 1, "800"},
		{PTP_DPC_NIKON_ISOAutoHiLimit, PTP_VENDOR_NIKON, 2, "1600"},
		{PTP_DPC_NIKON_ISOAutoHiLimit, PTP_VENDOR_NIKON, 3, "3200"},
		{PTP_DPC_NIKON_ISOAutoHiLimit, PTP_VENDOR_NIKON, 4, "Hi 1"},
		{PTP_DPC_NIKON_ISOAutoHiLimit, PTP_VENDOR_NIKON, 5, "Hi 2"},

		{PTP_DPC_NIKON_InfoDispSetting, PTP_VENDOR_NIKON, 0, N_("Auto")},	/* 0xD187 */
		{PTP_DPC_NIKON_InfoDispSetting, PTP_VENDOR_NIKON, 1, N_("Dark on light")},
		{PTP_DPC_NIKON_InfoDispSetting, PTP_VENDOR_NIKON, 2, N_("Light on dark")},

		PTP_VENDOR_VAL_YN(PTP_DPC_NIKON_IndicatorDisp,PTP_VENDOR_NIKON),	/* D18D */

		PTP_VENDOR_VAL_YN(PTP_DPC_NIKON_LiveViewStatus,PTP_VENDOR_NIKON),	/* D1A2 */

		PTP_VENDOR_VAL_YN(PTP_DPC_NIKON_ExposureDisplayStatus,PTP_VENDOR_NIKON),/* D1B0 */
		PTP_VENDOR_VAL_YN(PTP_DPC_NIKON_InfoDispErrStatus,PTP_VENDOR_NIKON),	/* D1B2 */
		PTP_VENDOR_VAL_YN(PTP_DPC_NIKON_ExposureIndicateLightup,PTP_VENDOR_NIKON),/* D1B3 */

		PTP_VENDOR_VAL_YN(PTP_DPC_NIKON_FlashOpen,PTP_VENDOR_NIKON),		/* D1C0 */
		PTP_VENDOR_VAL_YN(PTP_DPC_NIKON_FlashCharged,PTP_VENDOR_NIKON),		/* D1C1 */

		PTP_VENDOR_VAL_YN(PTP_DPC_NIKON_ManualMovieSetting,PTP_VENDOR_NIKON),	/* 0xD0A6 */

		{PTP_DPC_NIKON_FlickerReduction, PTP_VENDOR_NIKON, 0, "50Hz"},		/* 0xD034 */
		{PTP_DPC_NIKON_FlickerReduction, PTP_VENDOR_NIKON, 1, "60Hz"},

		{PTP_DPC_NIKON_RemoteMode, PTP_VENDOR_NIKON, 0, N_("Delayed Remote")},	/* 0xD035 */
		{PTP_DPC_NIKON_RemoteMode, PTP_VENDOR_NIKON, 1, N_("Quick Response")},	/* 0xD035 */
		{PTP_DPC_NIKON_RemoteMode, PTP_VENDOR_NIKON, 2, N_("Remote Mirror Up")},/* 0xD035 */

		{PTP_DPC_NIKON_MonitorOffDelay, PTP_VENDOR_NIKON, 0, "5min"},	/* 0xD0b3 */
		{PTP_DPC_NIKON_MonitorOffDelay, PTP_VENDOR_NIKON, 1, "10min"},	/* 0xD0b3 */
		{PTP_DPC_NIKON_MonitorOffDelay, PTP_VENDOR_NIKON, 2, "15min"},	/* 0xD0b3 */
		{PTP_DPC_NIKON_MonitorOffDelay, PTP_VENDOR_NIKON, 3, "20min"},	/* 0xD0b3 */
		{PTP_DPC_NIKON_MonitorOffDelay, PTP_VENDOR_NIKON, 4, "30min"},	/* 0xD0b3 */


		/* Canon stuff */
		PTP_VENDOR_VAL_BOOL(PTP_DPC_CANON_AssistLight,PTP_VENDOR_CANON),
		PTP_VENDOR_VAL_RBOOL(PTP_DPC_CANON_RotationScene,PTP_VENDOR_CANON),
		PTP_VENDOR_VAL_RBOOL(PTP_DPC_CANON_BeepMode,PTP_VENDOR_CANON),
		PTP_VENDOR_VAL_BOOL(PTP_DPC_CANON_Beep,PTP_VENDOR_CANON),

		{PTP_DPC_CANON_RotationAngle, PTP_VENDOR_CANON, 0, "0'"},
		{PTP_DPC_CANON_RotationAngle, PTP_VENDOR_CANON, 3, "270'"},
		{PTP_DPC_CANON_RotationAngle, PTP_VENDOR_CANON, 1, "90'"},

		{PTP_DPC_CANON_BatteryKind, PTP_VENDOR_CANON, 0, N_("Unknown")},
		{PTP_DPC_CANON_BatteryKind, PTP_VENDOR_CANON, 1, N_("AC")},
		{PTP_DPC_CANON_BatteryKind, PTP_VENDOR_CANON, 2, N_("Lithium Ion")},
		{PTP_DPC_CANON_BatteryKind, PTP_VENDOR_CANON, 3, N_("Nickel hydride")},
		{PTP_DPC_CANON_BatteryKind, PTP_VENDOR_CANON, 4, N_("Nickel cadmium")},
		{PTP_DPC_CANON_BatteryKind, PTP_VENDOR_CANON, 5, N_("Alkalium manganese")},

		{PTP_DPC_CANON_BatteryStatus, PTP_VENDOR_CANON, 0, N_("Undefined")},
		{PTP_DPC_CANON_BatteryStatus, PTP_VENDOR_CANON, 1, N_("Normal")},
		{PTP_DPC_CANON_BatteryStatus, PTP_VENDOR_CANON, 2, N_("Warning Level 1")},
		{PTP_DPC_CANON_BatteryStatus, PTP_VENDOR_CANON, 3, N_("Emergency")},
		{PTP_DPC_CANON_BatteryStatus, PTP_VENDOR_CANON, 4, N_("Warning Level 0")},

		{PTP_DPC_CANON_ImageQuality, PTP_VENDOR_CANON, 0, N_("Undefined")},
		{PTP_DPC_CANON_ImageQuality, PTP_VENDOR_CANON, 1, N_("Economy")},
		{PTP_DPC_CANON_ImageQuality, PTP_VENDOR_CANON, 2, N_("Normal")},
		{PTP_DPC_CANON_ImageQuality, PTP_VENDOR_CANON, 3, N_("Fine")},
		{PTP_DPC_CANON_ImageQuality, PTP_VENDOR_CANON, 4, N_("Lossless")},
		{PTP_DPC_CANON_ImageQuality, PTP_VENDOR_CANON, 5, N_("SuperFine")},

		{PTP_DPC_CANON_FullViewFileFormat, PTP_VENDOR_CANON, 0, N_("Undefined")},
		{PTP_DPC_CANON_FullViewFileFormat, PTP_VENDOR_CANON, 1, N_("JPEG")},
		{PTP_DPC_CANON_FullViewFileFormat, PTP_VENDOR_CANON, 2, N_("CRW")},

		{PTP_DPC_CANON_ImageSize, PTP_VENDOR_CANON, 0, N_("Large")},
		{PTP_DPC_CANON_ImageSize, PTP_VENDOR_CANON, 1, N_("Medium 1")},
		{PTP_DPC_CANON_ImageSize, PTP_VENDOR_CANON, 2, N_("Small")},
		{PTP_DPC_CANON_ImageSize, PTP_VENDOR_CANON, 3, N_("Medium 2")},
		{PTP_DPC_CANON_ImageSize, PTP_VENDOR_CANON, 7, N_("Medium 3")},

		{PTP_DPC_CANON_SelfTime, PTP_VENDOR_CANON, 0,   N_("Not used")},
		{PTP_DPC_CANON_SelfTime, PTP_VENDOR_CANON, 100, N_("10 seconds")},
		{PTP_DPC_CANON_SelfTime, PTP_VENDOR_CANON, 20,  N_("2 seconds")},

		{PTP_DPC_CANON_FlashMode, PTP_VENDOR_CANON, 0,  N_("Off")},
		{PTP_DPC_CANON_FlashMode, PTP_VENDOR_CANON, 1,  N_("Auto")},
		{PTP_DPC_CANON_FlashMode, PTP_VENDOR_CANON, 2,  N_("On")},
		{PTP_DPC_CANON_FlashMode, PTP_VENDOR_CANON, 3,  N_("Red Eye Suppression")},
		{PTP_DPC_CANON_FlashMode, PTP_VENDOR_CANON, 4,  N_("Low Speed Synchronization")},
		{PTP_DPC_CANON_FlashMode, PTP_VENDOR_CANON, 5,  N_("Auto + Red Eye Suppression")},
		{PTP_DPC_CANON_FlashMode, PTP_VENDOR_CANON, 6,  N_("On + Red Eye Suppression")},

		{PTP_DPC_CANON_ShootingMode, PTP_VENDOR_CANON, 0,  N_("Auto")},
		{PTP_DPC_CANON_ShootingMode, PTP_VENDOR_CANON, 1,  N_("P")},
		{PTP_DPC_CANON_ShootingMode, PTP_VENDOR_CANON, 2,  N_("Tv")},
		{PTP_DPC_CANON_ShootingMode, PTP_VENDOR_CANON, 3,  N_("Av")},
		{PTP_DPC_CANON_ShootingMode, PTP_VENDOR_CANON, 4,  N_("M")},
		{PTP_DPC_CANON_ShootingMode, PTP_VENDOR_CANON, 5,  N_("A_DEP")},
		{PTP_DPC_CANON_ShootingMode, PTP_VENDOR_CANON, 6,  N_("M_DEP")},
		{PTP_DPC_CANON_ShootingMode, PTP_VENDOR_CANON, 7,  N_("Bulb")},
		/* more actually */

		{PTP_DPC_CANON_ImageMode, PTP_VENDOR_CANON, 0,  N_("Auto")},
		{PTP_DPC_CANON_ImageMode, PTP_VENDOR_CANON, 1,  N_("Manual")},
		{PTP_DPC_CANON_ImageMode, PTP_VENDOR_CANON, 2,  N_("Distant View")},
		{PTP_DPC_CANON_ImageMode, PTP_VENDOR_CANON, 3,  N_("High-Speed Shutter")},
		{PTP_DPC_CANON_ImageMode, PTP_VENDOR_CANON, 4,  N_("Low-Speed Shutter")},
		{PTP_DPC_CANON_ImageMode, PTP_VENDOR_CANON, 5,  N_("Night View")},
		{PTP_DPC_CANON_ImageMode, PTP_VENDOR_CANON, 6,  N_("Grayscale")},
		{PTP_DPC_CANON_ImageMode, PTP_VENDOR_CANON, 7,  N_("Sepia")},
		{PTP_DPC_CANON_ImageMode, PTP_VENDOR_CANON, 8,  N_("Portrait")},
		{PTP_DPC_CANON_ImageMode, PTP_VENDOR_CANON, 9,  N_("Sports")},
		{PTP_DPC_CANON_ImageMode, PTP_VENDOR_CANON, 10,  N_("Macro")},
		{PTP_DPC_CANON_ImageMode, PTP_VENDOR_CANON, 11,  N_("Monochrome")},
		{PTP_DPC_CANON_ImageMode, PTP_VENDOR_CANON, 12,  N_("Pan Focus")},
		{PTP_DPC_CANON_ImageMode, PTP_VENDOR_CANON, 13,  N_("Neutral")},
		{PTP_DPC_CANON_ImageMode, PTP_VENDOR_CANON, 14,  N_("Soft")},

		{PTP_DPC_CANON_DriveMode, PTP_VENDOR_CANON, 0,  N_("Single-Frame Shooting")},
		{PTP_DPC_CANON_DriveMode, PTP_VENDOR_CANON, 1,  N_("Continuous Shooting")},
		{PTP_DPC_CANON_DriveMode, PTP_VENDOR_CANON, 2,  N_("Timer (Single) Shooting")},
		{PTP_DPC_CANON_DriveMode, PTP_VENDOR_CANON, 4,  N_("Continuous Low-speed Shooting")},
		{PTP_DPC_CANON_DriveMode, PTP_VENDOR_CANON, 5,  N_("Continuous High-speed Shooting")},

		{PTP_DPC_CANON_EZoom, PTP_VENDOR_CANON, 0,  N_("Off")},
		{PTP_DPC_CANON_EZoom, PTP_VENDOR_CANON, 1,  N_("2x")},
		{PTP_DPC_CANON_EZoom, PTP_VENDOR_CANON, 2,  N_("4x")},
		{PTP_DPC_CANON_EZoom, PTP_VENDOR_CANON, 3,  N_("Smooth")},

		{PTP_DPC_CANON_MeteringMode, PTP_VENDOR_CANON, 0,  N_("Center-weighted Metering")},
		{PTP_DPC_CANON_MeteringMode, PTP_VENDOR_CANON, 1,  N_("Spot Metering")},
		{PTP_DPC_CANON_MeteringMode, PTP_VENDOR_CANON, 2,  N_("Average Metering")},
		{PTP_DPC_CANON_MeteringMode, PTP_VENDOR_CANON, 3,  N_("Evaluative Metering")},
		{PTP_DPC_CANON_MeteringMode, PTP_VENDOR_CANON, 4,  N_("Partial Metering")},
		{PTP_DPC_CANON_MeteringMode, PTP_VENDOR_CANON, 5,  N_("Center-weighted Average Metering")},
		{PTP_DPC_CANON_MeteringMode, PTP_VENDOR_CANON, 6,  N_("Spot Metering Interlocked with AF Frame")},
		{PTP_DPC_CANON_MeteringMode, PTP_VENDOR_CANON, 7,  N_("Multi-Spot Metering")},

		{PTP_DPC_CANON_AFDistance, PTP_VENDOR_CANON, 0,  N_("Manual")},
		{PTP_DPC_CANON_AFDistance, PTP_VENDOR_CANON, 1,  N_("Auto")},
		{PTP_DPC_CANON_AFDistance, PTP_VENDOR_CANON, 2,  N_("Unknown")},
		{PTP_DPC_CANON_AFDistance, PTP_VENDOR_CANON, 3,  N_("Zone Focus (Close-up)")},
		{PTP_DPC_CANON_AFDistance, PTP_VENDOR_CANON, 4,  N_("Zone Focus (Very Close)")},
		{PTP_DPC_CANON_AFDistance, PTP_VENDOR_CANON, 5,  N_("Zone Focus (Close)")},
		{PTP_DPC_CANON_AFDistance, PTP_VENDOR_CANON, 6,  N_("Zone Focus (Medium)")},
		{PTP_DPC_CANON_AFDistance, PTP_VENDOR_CANON, 7,  N_("Zone Focus (Far)")},

		{PTP_DPC_CANON_FocusingPoint, PTP_VENDOR_CANON, 0,  N_("Invalid")},
		{PTP_DPC_CANON_FocusingPoint, PTP_VENDOR_CANON, 0x1000,  N_("Focusing Point on Center Only, Manual")},
		{PTP_DPC_CANON_FocusingPoint, PTP_VENDOR_CANON, 0x1001,  N_("Focusing Point on Center Only, Auto")},
		{PTP_DPC_CANON_FocusingPoint, PTP_VENDOR_CANON, 0x3000,  N_("Multiple Focusing Points (No Specification), Manual")},
		{PTP_DPC_CANON_FocusingPoint, PTP_VENDOR_CANON, 0x3001,  N_("Multiple Focusing Points, Auto")},
		{PTP_DPC_CANON_FocusingPoint, PTP_VENDOR_CANON, 0x3002,  N_("Multiple Focusing Points (Right)")},
		{PTP_DPC_CANON_FocusingPoint, PTP_VENDOR_CANON, 0x3003,  N_("Multiple Focusing Points (Center)")},
		{PTP_DPC_CANON_FocusingPoint, PTP_VENDOR_CANON, 0x3004,  N_("Multiple Focusing Points (Left)")},

		{PTP_DPC_CANON_WhiteBalance, PTP_VENDOR_CANON, 0,  N_("Auto")},
		{PTP_DPC_CANON_WhiteBalance, PTP_VENDOR_CANON, 1,  N_("Daylight")},
		{PTP_DPC_CANON_WhiteBalance, PTP_VENDOR_CANON, 2,  N_("Cloudy")},
		{PTP_DPC_CANON_WhiteBalance, PTP_VENDOR_CANON, 3,  N_("Tungsten")},
		{PTP_DPC_CANON_WhiteBalance, PTP_VENDOR_CANON, 4,  N_("Fluorescent")},
		{PTP_DPC_CANON_WhiteBalance, PTP_VENDOR_CANON, 6,  N_("Preset")},
		{PTP_DPC_CANON_WhiteBalance, PTP_VENDOR_CANON, 7,  N_("Fluorescent H")},
		{PTP_DPC_CANON_WhiteBalance, PTP_VENDOR_CANON, 9,  N_("Color Temperature")},
		{PTP_DPC_CANON_WhiteBalance, PTP_VENDOR_CANON, 10,  N_("Custom Whitebalance PC-1")},
		{PTP_DPC_CANON_WhiteBalance, PTP_VENDOR_CANON, 11,  N_("Custom Whitebalance PC-2")},
		{PTP_DPC_CANON_WhiteBalance, PTP_VENDOR_CANON, 12,  N_("Custom Whitebalance PC-3")},
		{PTP_DPC_CANON_WhiteBalance, PTP_VENDOR_CANON, 13,  N_("Missing Number")},
		{PTP_DPC_CANON_WhiteBalance, PTP_VENDOR_CANON, 14,  N_("Fluorescent H")}, /* dup? */

		{PTP_DPC_CANON_SlowShutterSetting, PTP_VENDOR_CANON, 0,  N_("Off")},
		{PTP_DPC_CANON_SlowShutterSetting, PTP_VENDOR_CANON, 1,  N_("Night View")},
		{PTP_DPC_CANON_SlowShutterSetting, PTP_VENDOR_CANON, 2,  N_("On")},
		{PTP_DPC_CANON_SlowShutterSetting, PTP_VENDOR_CANON, 3,  N_("Low-speed shutter function not available")},

		{PTP_DPC_CANON_AFMode, PTP_VENDOR_CANON, 0,  N_("Single Shot")},
		{PTP_DPC_CANON_AFMode, PTP_VENDOR_CANON, 1,  N_("AI Servo")},
		{PTP_DPC_CANON_AFMode, PTP_VENDOR_CANON, 2,  N_("AI Focus")},
		{PTP_DPC_CANON_AFMode, PTP_VENDOR_CANON, 3,  N_("Manual")},
		{PTP_DPC_CANON_AFMode, PTP_VENDOR_CANON, 4,  N_("Continuous")},

		PTP_VENDOR_VAL_BOOL(PTP_DPC_CANON_ImageStabilization,PTP_VENDOR_CANON),

		{PTP_DPC_CANON_Contrast, PTP_VENDOR_CANON, -2,  N_("Low 2")},
		{PTP_DPC_CANON_Contrast, PTP_VENDOR_CANON, -1,  N_("Low")},
		{PTP_DPC_CANON_Contrast, PTP_VENDOR_CANON, 0,  N_("Standard")},
		{PTP_DPC_CANON_Contrast, PTP_VENDOR_CANON, 1,  N_("High")},
		{PTP_DPC_CANON_Contrast, PTP_VENDOR_CANON, 2,  N_("High 2")},

		{PTP_DPC_CANON_ColorGain, PTP_VENDOR_CANON, -2,  N_("Low 2")},
		{PTP_DPC_CANON_ColorGain, PTP_VENDOR_CANON, -1,  N_("Low")},
		{PTP_DPC_CANON_ColorGain, PTP_VENDOR_CANON, 0,  N_("Standard")},
		{PTP_DPC_CANON_ColorGain, PTP_VENDOR_CANON, 1,  N_("High")},
		{PTP_DPC_CANON_ColorGain, PTP_VENDOR_CANON, 2,  N_("High 2")},

		{PTP_DPC_CANON_Sharpness, PTP_VENDOR_CANON, -2,  N_("Low 2")},
		{PTP_DPC_CANON_Sharpness, PTP_VENDOR_CANON, -1,  N_("Low")},
		{PTP_DPC_CANON_Sharpness, PTP_VENDOR_CANON, 0,  N_("Standard")},
		{PTP_DPC_CANON_Sharpness, PTP_VENDOR_CANON, 1,  N_("High")},
		{PTP_DPC_CANON_Sharpness, PTP_VENDOR_CANON, 2,  N_("High 2")},

		{PTP_DPC_CANON_Sensitivity, PTP_VENDOR_CANON, 0,  N_("Standard")},
		{PTP_DPC_CANON_Sensitivity, PTP_VENDOR_CANON, 1,  N_("Upper 1")},
		{PTP_DPC_CANON_Sensitivity, PTP_VENDOR_CANON, 2,  N_("Upper 2")},

		{PTP_DPC_CANON_ParameterSet, PTP_VENDOR_CANON, 0x08,  N_("Standard Development Parameters")},
		{PTP_DPC_CANON_ParameterSet, PTP_VENDOR_CANON, 0x10,  N_("Development Parameters 1")},
		{PTP_DPC_CANON_ParameterSet, PTP_VENDOR_CANON, 0x20,  N_("Development Parameters 2")},
		{PTP_DPC_CANON_ParameterSet, PTP_VENDOR_CANON, 0x40,  N_("Development Parameters 3")},

		{PTP_DPC_CANON_ISOSpeed, PTP_VENDOR_CANON, 0x00,  N_("Auto")},
		{PTP_DPC_CANON_ISOSpeed, PTP_VENDOR_CANON, 0x28,  "6"},
		{PTP_DPC_CANON_ISOSpeed, PTP_VENDOR_CANON, 0x30,  "12"},
		{PTP_DPC_CANON_ISOSpeed, PTP_VENDOR_CANON, 0x38,  "25"},
		{PTP_DPC_CANON_ISOSpeed, PTP_VENDOR_CANON, 0x40,  "50"},
		{PTP_DPC_CANON_ISOSpeed, PTP_VENDOR_CANON, 0x43,  "64"},
		{PTP_DPC_CANON_ISOSpeed, PTP_VENDOR_CANON, 0x48,  "100"},
		{PTP_DPC_CANON_ISOSpeed, PTP_VENDOR_CANON, 0x50,  "200"},
		{PTP_DPC_CANON_ISOSpeed, PTP_VENDOR_CANON, 0x58,  "400"},
		{PTP_DPC_CANON_ISOSpeed, PTP_VENDOR_CANON, 0x60,  "800"},
		{PTP_DPC_CANON_ISOSpeed, PTP_VENDOR_CANON, 0x68,  "1600"},
		{PTP_DPC_CANON_ISOSpeed, PTP_VENDOR_CANON, 0x70,  "3200"},
		{PTP_DPC_CANON_ISOSpeed, PTP_VENDOR_CANON, 0x78,  "6400"},

		/* 0xd01d - PTP_DPC_CANON_Aperture */
		/* 0xd01e - PTP_DPC_CANON_ShutterSpeed */
		/* 0xd01f - PTP_DPC_CANON_ExpCompensation */
		/* 0xd020 - PTP_DPC_CANON_FlashCompensation */
		/* 0xd021 - PTP_DPC_CANON_AEBExposureCompensation */
		/* 0xd023 - PTP_DPC_CANON_AvOpen */
		/* 0xd024 - PTP_DPC_CANON_AvMax */

		{PTP_DPC_CANON_CameraOutput, PTP_VENDOR_CANON, 0,  N_("Undefined")},
		{PTP_DPC_CANON_CameraOutput, PTP_VENDOR_CANON, 1,  N_("LCD")},
		{PTP_DPC_CANON_CameraOutput, PTP_VENDOR_CANON, 2,  N_("Video OUT")},
		{PTP_DPC_CANON_CameraOutput, PTP_VENDOR_CANON, 3,  N_("Off")},

		{PTP_DPC_CANON_MlSpotPos, PTP_VENDOR_CANON, 0, N_("MlSpotPosCenter")},
		{PTP_DPC_CANON_MlSpotPos, PTP_VENDOR_CANON, 1, N_("MlSpotPosAfLink")},

		{PTP_DPC_CANON_PhotoEffect, PTP_VENDOR_CANON, 0, N_("Off")},
		{PTP_DPC_CANON_PhotoEffect, PTP_VENDOR_CANON, 1, N_("Vivid")},
		{PTP_DPC_CANON_PhotoEffect, PTP_VENDOR_CANON, 2, N_("Neutral")},
		{PTP_DPC_CANON_PhotoEffect, PTP_VENDOR_CANON, 3, N_("Soft")},
		{PTP_DPC_CANON_PhotoEffect, PTP_VENDOR_CANON, 4, N_("Sepia")},
		{PTP_DPC_CANON_PhotoEffect, PTP_VENDOR_CANON, 5, N_("Monochrome")},

		{0, 0, 0, NULL}
	};
	for (i=0; ptp_value_trans[i].dpc!=0; i++) {
		if ((ptp_value_trans[i].dpc == dpc) && 
			(((ptp_value_trans[i].dpc & 0xf000) == 0x5000) ||
		         (ptp_value_trans[i].vendor == params->deviceinfo.VendorExtensionID))
		) {
			double value = _value_to_num(&(dpd->CurrentValue), dpd->DataType);

			return snprintf(out, length, 
				_(ptp_value_trans[i].format),
				value * ptp_value_trans[i].coef +
				ptp_value_trans[i].bias);
		}
	}

	kval = _value_to_num(&(dpd->CurrentValue), dpd->DataType);
	for (i=0; ptp_value_list[i].dpc!=0; i++) {
		if ((ptp_value_list[i].dpc == dpc) && 
			(((ptp_value_list[i].dpc & 0xf000) == 0x5000) ||
		          (ptp_value_list[i].vendor == params->deviceinfo.VendorExtensionID)) &&
		    (ptp_value_list[i].key==kval)
		) {
			return snprintf(out, length, "%s", _(ptp_value_list[i].value));
		}
	}
	if (params->deviceinfo.VendorExtensionID==PTP_VENDOR_MICROSOFT
	    || params->deviceinfo.VendorExtensionID==PTP_VENDOR_MTP) {
		switch (dpc) {
		case PTP_DPC_MTP_SynchronizationPartner:
		case PTP_DPC_MTP_DeviceFriendlyName:
			if (dpd->DataType == PTP_DTC_STR)
				return snprintf(out, length, "%s", dpd->CurrentValue.str);
			else
				return snprintf(out, length, "invalid type, expected STR");
		case PTP_DPC_MTP_SecureTime:
		case PTP_DPC_MTP_DeviceCertificate: {
			if (dpd->DataType != PTP_DTC_AUINT16)
				return snprintf(out, length, "invalid type, expected AUINT16");
			/* FIXME: Convert to use unicode demux functions */
			for (i=0;(i<dpd->CurrentValue.a.count) && (i<length);i++)
				out[i] = dpd->CurrentValue.a.v[i].u16;
			if (	dpd->CurrentValue.a.count &&
				(dpd->CurrentValue.a.count < length)) {
				out[dpd->CurrentValue.a.count-1] = 0;
				return dpd->CurrentValue.a.count-1;
			} else {
				out[length-1] = 0;
				return length;
			}
			break;
		}
		default:
			break;
		}
	}

	return 0;
}

struct {
	uint16_t ofc;
	const char *format;
} ptp_ofc_trans[] = {
	{PTP_OFC_Undefined,"Undefined Type"},
	{PTP_OFC_Defined,"Defined Type"},
	{PTP_OFC_Association,"Association/Directory"},
	{PTP_OFC_Script,"Script"},
	{PTP_OFC_Executable,"Executable"},
	{PTP_OFC_Text,"Text"},
	{PTP_OFC_HTML,"HTML"},
	{PTP_OFC_DPOF,"DPOF"},
	{PTP_OFC_AIFF,"AIFF"},
	{PTP_OFC_WAV,"MS Wave"},
	{PTP_OFC_MP3,"MP3"},
	{PTP_OFC_AVI,"MS AVI"},
	{PTP_OFC_MPEG,"MPEG"},
	{PTP_OFC_ASF,"ASF"},
	{PTP_OFC_QT,"Apple Quicktime"},
	{PTP_OFC_EXIF_JPEG,"JPEG"},
	{PTP_OFC_TIFF_EP,"TIFF EP"},
	{PTP_OFC_FlashPix,"FlashPix"},
	{PTP_OFC_BMP,"BMP"},
	{PTP_OFC_CIFF,"CIFF"},
	{PTP_OFC_GIF,"GIF"},
	{PTP_OFC_JFIF,"JFIF"},
	{PTP_OFC_PCD,"PCD"},
	{PTP_OFC_PICT,"PICT"},
	{PTP_OFC_PNG,"PNG"},
	{PTP_OFC_TIFF,"TIFF"},
	{PTP_OFC_TIFF_IT,"TIFF_IT"},
	{PTP_OFC_JP2,"JP2"},
	{PTP_OFC_JPX,"JPX"},
	{PTP_OFC_DNG,"DNG"},
};

struct {
	uint16_t ofc;
	const char *format;
} ptp_ofc_mtp_trans[] = {
	{PTP_OFC_MTP_MediaCard,N_("Media Card")},
	{PTP_OFC_MTP_MediaCardGroup,N_("Media Card Group")},
	{PTP_OFC_MTP_Encounter,N_("Encounter")},
	{PTP_OFC_MTP_EncounterBox,N_("Encounter Box")},
	{PTP_OFC_MTP_M4A,N_("M4A")},
	{PTP_OFC_MTP_Firmware,N_("Firmware")},
	{PTP_OFC_MTP_WindowsImageFormat,N_("Windows Image Format")},
	{PTP_OFC_MTP_UndefinedAudio,N_("Undefined Audio")},
	{PTP_OFC_MTP_WMA,"WMA"},
	{PTP_OFC_MTP_OGG,"OGG"},
	{PTP_OFC_MTP_AAC,"AAC"},
	{PTP_OFC_MTP_AudibleCodec,N_("Audible.com Codec")},
	{PTP_OFC_MTP_FLAC,"FLAC"},
	{PTP_OFC_MTP_SamsungPlaylist,N_("Samsung Playlist")},
	{PTP_OFC_MTP_UndefinedVideo,N_("Undefined Video")},
	{PTP_OFC_MTP_WMV,"WMV"},
	{PTP_OFC_MTP_MP4,"MP4"},
	{PTP_OFC_MTP_MP2,"MP2"},
	{PTP_OFC_MTP_3GP,"3GP"},
	{PTP_OFC_MTP_UndefinedCollection,N_("Undefined Collection")},
	{PTP_OFC_MTP_AbstractMultimediaAlbum,N_("Abstract Multimedia Album")},
	{PTP_OFC_MTP_AbstractImageAlbum,N_("Abstract Image Album")},
	{PTP_OFC_MTP_AbstractAudioAlbum,N_("Abstract Audio Album")},
	{PTP_OFC_MTP_AbstractVideoAlbum,N_("Abstract Video Album")},
	{PTP_OFC_MTP_AbstractAudioVideoPlaylist,N_("Abstract Audio Video Playlist")},
	{PTP_OFC_MTP_AbstractContactGroup,N_("Abstract Contact Group")},
	{PTP_OFC_MTP_AbstractMessageFolder,N_("Abstract Message Folder")},
	{PTP_OFC_MTP_AbstractChapteredProduction,N_("Abstract Chaptered Production")},
	{PTP_OFC_MTP_AbstractAudioPlaylist,N_("Abstract Audio Playlist")},
	{PTP_OFC_MTP_AbstractVideoPlaylist,N_("Abstract Video Playlist")},
	{PTP_OFC_MTP_AbstractMediacast,N_("Abstract Mediacast")},
	{PTP_OFC_MTP_WPLPlaylist,N_("WPL Playlist")},
	{PTP_OFC_MTP_M3UPlaylist,N_("M3U Playlist")},
	{PTP_OFC_MTP_MPLPlaylist,N_("MPL Playlist")},
	{PTP_OFC_MTP_ASXPlaylist,N_("ASX Playlist")},
	{PTP_OFC_MTP_PLSPlaylist,N_("PLS Playlist")},
	{PTP_OFC_MTP_UndefinedDocument,N_("Undefined Document")},
	{PTP_OFC_MTP_AbstractDocument,N_("Abstract Document")},
	{PTP_OFC_MTP_XMLDocument,N_("XMLDocument")},
	{PTP_OFC_MTP_MSWordDocument,N_("Microsoft Word Document")},
	{PTP_OFC_MTP_MHTCompiledHTMLDocument,N_("MHT Compiled HTML Document")},
	{PTP_OFC_MTP_MSExcelSpreadsheetXLS,N_("Microsoft Excel Spreadsheet (.xls)")},
	{PTP_OFC_MTP_MSPowerpointPresentationPPT,N_("Microsoft Powerpoint (.ppt)")},
	{PTP_OFC_MTP_UndefinedMessage,N_("Undefined Message")},
	{PTP_OFC_MTP_AbstractMessage,N_("Abstract Message")},
	{PTP_OFC_MTP_UndefinedContact,N_("Undefined Contact")},
	{PTP_OFC_MTP_AbstractContact,N_("Abstract Contact")},
	{PTP_OFC_MTP_vCard2,N_("vCard2")},
	{PTP_OFC_MTP_vCard3,N_("vCard3")},
	{PTP_OFC_MTP_UndefinedCalendarItem,N_("Undefined Calendar Item")},
	{PTP_OFC_MTP_AbstractCalendarItem,N_("Abstract Calendar Item")},
	{PTP_OFC_MTP_vCalendar1,N_("vCalendar1")},
	{PTP_OFC_MTP_vCalendar2,N_("vCalendar2")},
	{PTP_OFC_MTP_UndefinedWindowsExecutable,N_("Undefined Windows Executable")},
	{PTP_OFC_MTP_MediaCast,N_("Media Cast")},
	{PTP_OFC_MTP_Section,N_("Section")},
};

int
ptp_render_ofc(PTPParams* params, uint16_t ofc, int spaceleft, char *txt)
{
	unsigned int i;

	if (!(ofc & 0x8000)) {
		for (i=0;i<sizeof(ptp_ofc_trans)/sizeof(ptp_ofc_trans[0]);i++)
			if (ofc == ptp_ofc_trans[i].ofc)
				return snprintf(txt, spaceleft, "%s", _(ptp_ofc_trans[i].format));
	} else {
		switch (params->deviceinfo.VendorExtensionID) {
		case PTP_VENDOR_EASTMAN_KODAK:
			switch (ofc) {
			case PTP_OFC_EK_M3U:
				return snprintf (txt, spaceleft,"M3U");
			default:
				break;
			}
			break;
		case PTP_VENDOR_CANON:
			switch (ofc) {
			case PTP_OFC_CANON_CRW:
				return snprintf (txt, spaceleft,"CRW");
			default:
				break;
			}
			break;
		case PTP_VENDOR_SONY:
			switch (ofc) {
			case PTP_OFC_SONY_RAW:
				return snprintf (txt, spaceleft,"ARW");
			default:
				break;
			}
			break;
		case PTP_VENDOR_MICROSOFT:
		case PTP_VENDOR_MTP:		  
			for (i=0;i<sizeof(ptp_ofc_mtp_trans)/sizeof(ptp_ofc_mtp_trans[0]);i++)
				if (ofc == ptp_ofc_mtp_trans[i].ofc)
					return snprintf(txt, spaceleft, "%s", _(ptp_ofc_mtp_trans[i].format));
			break;
		default:break;
		}
	}
	return snprintf (txt, spaceleft,_("Unknown(%04x)"), ofc);
}

typedef struct {
	uint16_t opcode;
	const char *name;
} ptp_opcode_trans_t;

ptp_opcode_trans_t ptp_opcode_trans[] = {
	{PTP_OC_Undefined,N_("Undefined")},
	{PTP_OC_GetDeviceInfo,N_("Get device info")},
	{PTP_OC_OpenSession,N_("Open session")},
	{PTP_OC_CloseSession,N_("Close session")},
	{PTP_OC_GetStorageIDs,N_("Get storage IDs")},
	{PTP_OC_GetStorageInfo,N_("Get storage info")},
	{PTP_OC_GetNumObjects,N_("Get number of objects")},
	{PTP_OC_GetObjectHandles,N_("Get object handles")},
	{PTP_OC_GetObjectInfo,N_("Get object info")},
	{PTP_OC_GetObject,N_("Get object")},
	{PTP_OC_GetThumb,N_("Get thumbnail")},
	{PTP_OC_DeleteObject,N_("Delete object")},
	{PTP_OC_SendObjectInfo,N_("Send object info")},
	{PTP_OC_SendObject,N_("Send object")},
	{PTP_OC_InitiateCapture,N_("Initiate capture")},
	{PTP_OC_FormatStore,N_("Format storage")},
	{PTP_OC_ResetDevice,N_("Reset device")},
	{PTP_OC_SelfTest,N_("Self test device")},
	{PTP_OC_SetObjectProtection,N_("Set object protection")},
	{PTP_OC_PowerDown,N_("Power down device")},
	{PTP_OC_GetDevicePropDesc,N_("Get device property description")},
	{PTP_OC_GetDevicePropValue,N_("Get device property value")},
	{PTP_OC_SetDevicePropValue,N_("Set device property value")},
	{PTP_OC_ResetDevicePropValue,N_("Reset device property value")},
	{PTP_OC_TerminateOpenCapture,N_("Terminate open capture")},
	{PTP_OC_MoveObject,N_("Move object")},
	{PTP_OC_CopyObject,N_("Copy object")},
	{PTP_OC_GetPartialObject,N_("Get partial object")},
	{PTP_OC_InitiateOpenCapture,N_("Initiate open capture")},
	/* PTP v1.1 operation codes */
	{PTP_OC_StartEnumHandles,N_("Start Enumerate Handles")},
	{PTP_OC_EnumHandles,N_("Enumerate Handles")},
	{PTP_OC_StopEnumHandles,N_("Stop Enumerate Handles")},
	{PTP_OC_GetVendorExtensionMaps,N_("Get Vendor Extension Maps")},
	{PTP_OC_GetVendorDeviceInfo,N_("Get Vendor Device Info")},
	{PTP_OC_GetResizedImageObject,N_("Get Resized Image Object")},
	{PTP_OC_GetFilesystemManifest,N_("Get Filesystem Manifest")},
	{PTP_OC_GetStreamInfo,N_("Get Stream Info")},
	{PTP_OC_GetStream,N_("Get Stream")},
};

ptp_opcode_trans_t ptp_opcode_mtp_trans[] = {
	{PTP_OC_MTP_GetObjectPropsSupported,N_("Get object properties supported")},
	{PTP_OC_MTP_GetObjectPropDesc,N_("Get object property description")},
	{PTP_OC_MTP_GetObjectPropValue,N_("Get object property value")},
	{PTP_OC_MTP_SetObjectPropValue,N_("Set object property value")},
	{PTP_OC_MTP_GetObjPropList,N_("Get object property list")},
	{PTP_OC_MTP_SetObjPropList,N_("Set object property list")},
	{PTP_OC_MTP_GetInterdependendPropdesc,N_("Get interdependent property description")},
	{PTP_OC_MTP_SendObjectPropList,N_("Send object property list")},
	{PTP_OC_MTP_GetObjectReferences,N_("Get object references")},
	{PTP_OC_MTP_SetObjectReferences,N_("Set object references")},
	{PTP_OC_MTP_UpdateDeviceFirmware,N_("Update device firmware")},
	{PTP_OC_MTP_Skip,N_("Skip to next position in playlist")},

	/* WMDRMPD Extensions */
	{PTP_OC_MTP_WMDRMPD_GetSecureTimeChallenge,N_("Get secure time challenge")},
	{PTP_OC_MTP_WMDRMPD_GetSecureTimeResponse,N_("Get secure time response")},
	{PTP_OC_MTP_WMDRMPD_SetLicenseResponse,N_("Set license response")},
	{PTP_OC_MTP_WMDRMPD_GetSyncList,N_("Get sync list")},
	{PTP_OC_MTP_WMDRMPD_SendMeterChallengeQuery,N_("Send meter challenge query")},
	{PTP_OC_MTP_WMDRMPD_GetMeterChallenge,N_("Get meter challenge")},
	{PTP_OC_MTP_WMDRMPD_SetMeterResponse,N_("Get meter response")},
	{PTP_OC_MTP_WMDRMPD_CleanDataStore,N_("Clean data store")},
	{PTP_OC_MTP_WMDRMPD_GetLicenseState,N_("Get license state")},
	{PTP_OC_MTP_WMDRMPD_SendWMDRMPDCommand,N_("Send WMDRM-PD Command")},
	{PTP_OC_MTP_WMDRMPD_SendWMDRMPDRequest,N_("Send WMDRM-PD Request")},

	/* WMPPD Extensions */
	{PTP_OC_MTP_WMPPD_ReportAddedDeletedItems,N_("Report Added/Deleted Items")},
	{PTP_OC_MTP_WMPPD_ReportAcquiredItems,N_("Report Acquired Items")},
	{PTP_OC_MTP_WMPPD_PlaylistObjectPref,N_("Get transferable playlist types")},

	/* WMDRMPD Extensions... these have no identifiers associated with them */
	{PTP_OC_MTP_WMDRMPD_SendWMDRMPDAppRequest,N_("Send WMDRM-PD Application Request")},
	{PTP_OC_MTP_WMDRMPD_GetWMDRMPDAppResponse,N_("Get WMDRM-PD Application Response")},
	{PTP_OC_MTP_WMDRMPD_EnableTrustedFilesOperations,N_("Enable trusted file operations")},
	{PTP_OC_MTP_WMDRMPD_DisableTrustedFilesOperations,N_("Disable trusted file operations")},
	{PTP_OC_MTP_WMDRMPD_EndTrustedAppSession,N_("End trusted application session")},

	/* AAVT Extensions */
	{PTP_OC_MTP_AAVT_OpenMediaSession,N_("Open Media Session")},
	{PTP_OC_MTP_AAVT_CloseMediaSession,N_("Close Media Session")},
	{PTP_OC_MTP_AAVT_GetNextDataBlock,N_("Get Next Data Block")},
	{PTP_OC_MTP_AAVT_SetCurrentTimePosition,N_("Set Current Time Position")},

	/* WMDRMND Extensions */
	{PTP_OC_MTP_WMDRMND_SendRegistrationRequest,N_("Send Registration Request")},
	{PTP_OC_MTP_WMDRMND_GetRegistrationResponse,N_("Get Registration Response")},
	{PTP_OC_MTP_WMDRMND_GetProximityChallenge,N_("Get Proximity Challenge")},
	{PTP_OC_MTP_WMDRMND_SendProximityResponse,N_("Send Proximity Response")},
	{PTP_OC_MTP_WMDRMND_SendWMDRMNDLicenseRequest,N_("Send WMDRM-ND License Request")},
	{PTP_OC_MTP_WMDRMND_GetWMDRMNDLicenseResponse,N_("Get WMDRM-ND License Response")},

	/* WiFi Provisioning MTP Extension Codes (microsoft.com/WPDWCN: 1.0) */
	{PTP_OC_MTP_WPDWCN_ProcessWFCObject,N_("Process WFC Object")},

	/* Android Direct I/O Extensions */
	{PTP_OC_ANDROID_GetPartialObject64,N_("Get Partial Object (64bit Offset)")},
	{PTP_OC_ANDROID_SendPartialObject,N_("Send Partial Object")},
	{PTP_OC_ANDROID_TruncateObject,N_("Truncate Object")},
	{PTP_OC_ANDROID_BeginEditObject,N_("Begin Edit Object")},
	{PTP_OC_ANDROID_EndEditObject,N_("End Edit Object")},
};

ptp_opcode_trans_t ptp_opcode_nikon_trans[] = {
	{PTP_OC_NIKON_GetProfileAllData,"PTP_OC_NIKON_GetProfileAllData"},
	{PTP_OC_NIKON_SendProfileData,"PTP_OC_NIKON_SendProfileData"},
	{PTP_OC_NIKON_DeleteProfile,"PTP_OC_NIKON_DeleteProfile"},
	{PTP_OC_NIKON_SetProfileData,"PTP_OC_NIKON_SetProfileData"},
	{PTP_OC_NIKON_AdvancedTransfer,"PTP_OC_NIKON_AdvancedTransfer"},
	{PTP_OC_NIKON_GetFileInfoInBlock,"PTP_OC_NIKON_GetFileInfoInBlock"},
	{PTP_OC_NIKON_Capture,"PTP_OC_NIKON_Capture"},
	{PTP_OC_NIKON_AfDrive,"PTP_OC_NIKON_AfDrive"},
	{PTP_OC_NIKON_SetControlMode,"PTP_OC_NIKON_SetControlMode"},
	{PTP_OC_NIKON_DelImageSDRAM,"PTP_OC_NIKON_DelImageSDRAM"},
	{PTP_OC_NIKON_GetLargeThumb,"PTP_OC_NIKON_GetLargeThumb"},
	{PTP_OC_NIKON_CurveDownload,"PTP_OC_NIKON_CurveDownload"},
	{PTP_OC_NIKON_CurveUpload,"PTP_OC_NIKON_CurveUpload"},
	{PTP_OC_NIKON_CheckEvent,"PTP_OC_NIKON_CheckEvent"},
	{PTP_OC_NIKON_DeviceReady,"PTP_OC_NIKON_DeviceReady"},
	{PTP_OC_NIKON_SetPreWBData,"PTP_OC_NIKON_SetPreWBData"},
	{PTP_OC_NIKON_GetVendorPropCodes,"PTP_OC_NIKON_GetVendorPropCodes"},
	{PTP_OC_NIKON_AfCaptureSDRAM,"PTP_OC_NIKON_AfCaptureSDRAM"},
	{PTP_OC_NIKON_GetPictCtrlData,"PTP_OC_NIKON_GetPictCtrlData"},
	{PTP_OC_NIKON_SetPictCtrlData,"PTP_OC_NIKON_SetPictCtrlData"},
	{PTP_OC_NIKON_DelCstPicCtrl,"PTP_OC_NIKON_DelCstPicCtrl"},
	{PTP_OC_NIKON_GetPicCtrlCapability,"PTP_OC_NIKON_GetPicCtrlCapability"},
	{PTP_OC_NIKON_GetPreviewImg,"PTP_OC_NIKON_GetPreviewImg"},
	{PTP_OC_NIKON_StartLiveView,"PTP_OC_NIKON_StartLiveView"},
	{PTP_OC_NIKON_EndLiveView,"PTP_OC_NIKON_EndLiveView"},
	{PTP_OC_NIKON_GetLiveViewImg,"PTP_OC_NIKON_GetLiveViewImg"},
	{PTP_OC_NIKON_MfDrive,"PTP_OC_NIKON_MfDrive"},
	{PTP_OC_NIKON_ChangeAfArea,"PTP_OC_NIKON_ChangeAfArea"},
	{PTP_OC_NIKON_AfDriveCancel,"PTP_OC_NIKON_AfDriveCancel"},
	{PTP_OC_NIKON_InitiateCaptureRecInMedia,"PTP_OC_NIKON_InitiateCaptureRecInMedia"},
	{PTP_OC_NIKON_GetVendorStorageIDs,"PTP_OC_NIKON_GetVendorStorageIDs"},
	{PTP_OC_NIKON_StartMovieRecInCard,"PTP_OC_NIKON_StartMovieRecInCard"},
	{PTP_OC_NIKON_EndMovieRec,"PTP_OC_NIKON_EndMovieRec"},
	{PTP_OC_NIKON_TerminateCapture,"PTP_OC_NIKON_TerminateCapture"},
	{PTP_OC_NIKON_GetDevicePTPIPInfo,"PTP_OC_NIKON_GetDevicePTPIPInfo"},
	{PTP_OC_NIKON_GetPartialObjectHiSpeed,"PTP_OC_NIKON_GetPartialObjectHiSpeed"},
	{PTP_OC_NIKON_GetDevicePropEx,"PTP_OC_NIKON_GetDevicePropEx"},
};

ptp_opcode_trans_t ptp_opcode_canon_trans[] = {
	{PTP_OC_CANON_GetPartialObjectInfo,"PTP_OC_CANON_GetPartialObjectInfo"},
	{PTP_OC_CANON_SetObjectArchive,"PTP_OC_CANON_SetObjectArchive"},
	{PTP_OC_CANON_KeepDeviceOn,"PTP_OC_CANON_KeepDeviceOn"},
	{PTP_OC_CANON_LockDeviceUI,"PTP_OC_CANON_LockDeviceUI"},
	{PTP_OC_CANON_UnlockDeviceUI,"PTP_OC_CANON_UnlockDeviceUI"},
	{PTP_OC_CANON_GetObjectHandleByName,"PTP_OC_CANON_GetObjectHandleByName"},
	{PTP_OC_CANON_InitiateReleaseControl,"PTP_OC_CANON_InitiateReleaseControl"},
	{PTP_OC_CANON_TerminateReleaseControl,"PTP_OC_CANON_TerminateReleaseControl"},
	{PTP_OC_CANON_TerminatePlaybackMode,"PTP_OC_CANON_TerminatePlaybackMode"},
	{PTP_OC_CANON_ViewfinderOn,"PTP_OC_CANON_ViewfinderOn"},
	{PTP_OC_CANON_ViewfinderOff,"PTP_OC_CANON_ViewfinderOff"},
	{PTP_OC_CANON_DoAeAfAwb,"PTP_OC_CANON_DoAeAfAwb"},
	{PTP_OC_CANON_GetCustomizeSpec,"PTP_OC_CANON_GetCustomizeSpec"},
	{PTP_OC_CANON_GetCustomizeItemInfo,"PTP_OC_CANON_GetCustomizeItemInfo"},
	{PTP_OC_CANON_GetCustomizeData,"PTP_OC_CANON_GetCustomizeData"},
	{PTP_OC_CANON_SetCustomizeData,"PTP_OC_CANON_SetCustomizeData"},
	{PTP_OC_CANON_GetCaptureStatus,"PTP_OC_CANON_GetCaptureStatus"},
	{PTP_OC_CANON_CheckEvent,"PTP_OC_CANON_CheckEvent"},
	{PTP_OC_CANON_FocusLock,"PTP_OC_CANON_FocusLock"},
	{PTP_OC_CANON_FocusUnlock,"PTP_OC_CANON_FocusUnlock"},
	{PTP_OC_CANON_GetLocalReleaseParam,"PTP_OC_CANON_GetLocalReleaseParam"},
	{PTP_OC_CANON_SetLocalReleaseParam,"PTP_OC_CANON_SetLocalReleaseParam"},
	{PTP_OC_CANON_AskAboutPcEvf,"PTP_OC_CANON_AskAboutPcEvf"},
	{PTP_OC_CANON_SendPartialObject,"PTP_OC_CANON_SendPartialObject"},
	{PTP_OC_CANON_InitiateCaptureInMemory,"PTP_OC_CANON_InitiateCaptureInMemory"},
	{PTP_OC_CANON_GetPartialObjectEx,"PTP_OC_CANON_GetPartialObjectEx"},
	{PTP_OC_CANON_SetObjectTime,"PTP_OC_CANON_SetObjectTime"},
	{PTP_OC_CANON_GetViewfinderImage,"PTP_OC_CANON_GetViewfinderImage"},
	{PTP_OC_CANON_GetObjectAttributes,"PTP_OC_CANON_GetObjectAttributes"},
	{PTP_OC_CANON_ChangeUSBProtocol,"PTP_OC_CANON_ChangeUSBProtocol"},
	{PTP_OC_CANON_GetChanges,"PTP_OC_CANON_GetChanges"},
	{PTP_OC_CANON_GetObjectInfoEx,"PTP_OC_CANON_GetObjectInfoEx"},
	{PTP_OC_CANON_InitiateDirectTransfer,"PTP_OC_CANON_InitiateDirectTransfer"},
	{PTP_OC_CANON_TerminateDirectTransfer ,"PTP_OC_CANON_TerminateDirectTransfer "},
	{PTP_OC_CANON_SendObjectInfoByPath ,"PTP_OC_CANON_SendObjectInfoByPath "},
	{PTP_OC_CANON_SendObjectByPath ,"PTP_OC_CANON_SendObjectByPath "},
	{PTP_OC_CANON_InitiateDirectTansferEx,"PTP_OC_CANON_InitiateDirectTansferEx"},
	{PTP_OC_CANON_GetAncillaryObjectHandles,"PTP_OC_CANON_GetAncillaryObjectHandles"},
	{PTP_OC_CANON_GetTreeInfo ,"PTP_OC_CANON_GetTreeInfo "},
	{PTP_OC_CANON_GetTreeSize ,"PTP_OC_CANON_GetTreeSize "},
	{PTP_OC_CANON_NotifyProgress ,"PTP_OC_CANON_NotifyProgress "},
	{PTP_OC_CANON_NotifyCancelAccepted,"PTP_OC_CANON_NotifyCancelAccepted"},
	{PTP_OC_CANON_902C,"PTP_OC_CANON_902C"},
	{PTP_OC_CANON_GetDirectory,"PTP_OC_CANON_GetDirectory"},
	{PTP_OC_CANON_SetPairingInfo,"PTP_OC_CANON_SetPairingInfo"},
	{PTP_OC_CANON_GetPairingInfo,"PTP_OC_CANON_GetPairingInfo"},
	{PTP_OC_CANON_DeletePairingInfo,"PTP_OC_CANON_DeletePairingInfo"},
	{PTP_OC_CANON_GetMACAddress,"PTP_OC_CANON_GetMACAddress"},
	{PTP_OC_CANON_SetDisplayMonitor,"PTP_OC_CANON_SetDisplayMonitor"},
	{PTP_OC_CANON_PairingComplete,"PTP_OC_CANON_PairingComplete"},
	{PTP_OC_CANON_GetWirelessMAXChannel,"PTP_OC_CANON_GetWirelessMAXChannel"},
	{PTP_OC_CANON_GetWebServiceSpec,"PTP_OC_CANON_GetWebServiceSpec"},
	{PTP_OC_CANON_GetWebServiceData,"PTP_OC_CANON_GetWebServiceData"},
	{PTP_OC_CANON_SetWebServiceData,"PTP_OC_CANON_SetWebServiceData"},
	{PTP_OC_CANON_GetRootCertificateSpec,"PTP_OC_CANON_GetRootCertificateSpec"},
	{PTP_OC_CANON_GetRootCertificateData,"PTP_OC_CANON_GetRootCertificateData"},
	{PTP_OC_CANON_SetRootCertificateData,"PTP_OC_CANON_SetRootCertificateData"},
	{PTP_OC_CANON_EOS_GetStorageIDs,"PTP_OC_CANON_EOS_GetStorageIDs"},
	{PTP_OC_CANON_EOS_GetStorageInfo,"PTP_OC_CANON_EOS_GetStorageInfo"},
	{PTP_OC_CANON_EOS_GetObjectInfo,"PTP_OC_CANON_EOS_GetObjectInfo"},
	{PTP_OC_CANON_EOS_GetObject,"PTP_OC_CANON_EOS_GetObject"},
	{PTP_OC_CANON_EOS_DeleteObject,"PTP_OC_CANON_EOS_DeleteObject"},
	{PTP_OC_CANON_EOS_FormatStore,"PTP_OC_CANON_EOS_FormatStore"},
	{PTP_OC_CANON_EOS_GetPartialObject,"PTP_OC_CANON_EOS_GetPartialObject"},
	{PTP_OC_CANON_EOS_GetDeviceInfoEx,"PTP_OC_CANON_EOS_GetDeviceInfoEx"},
	{PTP_OC_CANON_EOS_GetObjectInfoEx,"PTP_OC_CANON_EOS_GetObjectInfoEx"},
	{PTP_OC_CANON_EOS_GetThumbEx,"PTP_OC_CANON_EOS_GetThumbEx"},
	{PTP_OC_CANON_EOS_SendPartialObject,"PTP_OC_CANON_EOS_SendPartialObject"},
	{PTP_OC_CANON_EOS_SetObjectAttributes,"PTP_OC_CANON_EOS_SetObjectAttributes"},
	{PTP_OC_CANON_EOS_GetObjectTime,"PTP_OC_CANON_EOS_GetObjectTime"},
	{PTP_OC_CANON_EOS_SetObjectTime,"PTP_OC_CANON_EOS_SetObjectTime"},
	{PTP_OC_CANON_EOS_RemoteRelease,"PTP_OC_CANON_EOS_RemoteRelease"},
	{PTP_OC_CANON_EOS_SetDevicePropValueEx,"PTP_OC_CANON_EOS_SetDevicePropValueEx"},
	{PTP_OC_CANON_EOS_GetRemoteMode,"PTP_OC_CANON_EOS_GetRemoteMode"},
	{PTP_OC_CANON_EOS_SetRemoteMode,"PTP_OC_CANON_EOS_SetRemoteMode"},
	{PTP_OC_CANON_EOS_SetEventMode,"PTP_OC_CANON_EOS_SetEventMode"},
	{PTP_OC_CANON_EOS_GetEvent,"PTP_OC_CANON_EOS_GetEvent"},
	{PTP_OC_CANON_EOS_TransferComplete,"PTP_OC_CANON_EOS_TransferComplete"},
	{PTP_OC_CANON_EOS_CancelTransfer,"PTP_OC_CANON_EOS_CancelTransfer"},
	{PTP_OC_CANON_EOS_ResetTransfer,"PTP_OC_CANON_EOS_ResetTransfer"},
	{PTP_OC_CANON_EOS_PCHDDCapacity,"PTP_OC_CANON_EOS_PCHDDCapacity"},
	{PTP_OC_CANON_EOS_SetUILock,"PTP_OC_CANON_EOS_SetUILock"},
	{PTP_OC_CANON_EOS_ResetUILock,"PTP_OC_CANON_EOS_ResetUILock"},
	{PTP_OC_CANON_EOS_KeepDeviceOn,"PTP_OC_CANON_EOS_KeepDeviceOn"},
	{PTP_OC_CANON_EOS_SetNullPacketMode,"PTP_OC_CANON_EOS_SetNullPacketMode"},
	{PTP_OC_CANON_EOS_UpdateFirmware,"PTP_OC_CANON_EOS_UpdateFirmware"},
	{PTP_OC_CANON_EOS_TransferCompleteDT,"PTP_OC_CANON_EOS_TransferCompleteDT"},
	{PTP_OC_CANON_EOS_CancelTransferDT,"PTP_OC_CANON_EOS_CancelTransferDT"},
	{PTP_OC_CANON_EOS_SetWftProfile,"PTP_OC_CANON_EOS_SetWftProfile"},
	{PTP_OC_CANON_EOS_GetWftProfile,"PTP_OC_CANON_EOS_GetWftProfile"},
	{PTP_OC_CANON_EOS_SetProfileToWft,"PTP_OC_CANON_EOS_SetProfileToWft"},
	{PTP_OC_CANON_EOS_BulbStart,"PTP_OC_CANON_EOS_BulbStart"},
	{PTP_OC_CANON_EOS_BulbEnd,"PTP_OC_CANON_EOS_BulbEnd"},
	{PTP_OC_CANON_EOS_RequestDevicePropValue,"PTP_OC_CANON_EOS_RequestDevicePropValue"},
	{PTP_OC_CANON_EOS_RemoteReleaseOn,"PTP_OC_CANON_EOS_RemoteReleaseOn"},
	{PTP_OC_CANON_EOS_RemoteReleaseOff,"PTP_OC_CANON_EOS_RemoteReleaseOff"},
	{PTP_OC_CANON_EOS_RegistBackgroundImage,"PTP_OC_CANON_EOS_RegistBackgroundImage"},
	{PTP_OC_CANON_EOS_ChangePhotoStudioMode,"PTP_OC_CANON_EOS_ChangePhotoStudioMode"},
	{PTP_OC_CANON_EOS_GetPartialObjectEx,"PTP_OC_CANON_EOS_GetPartialObjectEx"},
	{PTP_OC_CANON_EOS_ResetMirrorLockupState,"PTP_OC_CANON_EOS_ResetMirrorLockupState"},
	{PTP_OC_CANON_EOS_PopupBuiltinFlash,"PTP_OC_CANON_EOS_PopupBuiltinFlash"},
	{PTP_OC_CANON_EOS_EndGetPartialObjectEx,"PTP_OC_CANON_EOS_EndGetPartialObjectEx"},
	{PTP_OC_CANON_EOS_MovieSelectSWOn,"PTP_OC_CANON_EOS_MovieSelectSWOn"},
	{PTP_OC_CANON_EOS_MovieSelectSWOff,"PTP_OC_CANON_EOS_MovieSelectSWOff"},
	{PTP_OC_CANON_EOS_GetCTGInfo,"PTP_OC_CANON_EOS_GetCTGInfo"},
	{PTP_OC_CANON_EOS_GetLensAdjust,"PTP_OC_CANON_EOS_GetLensAdjust"},
	{PTP_OC_CANON_EOS_SetLensAdjust,"PTP_OC_CANON_EOS_SetLensAdjust"},
	{PTP_OC_CANON_EOS_GetMusicInfo,"PTP_OC_CANON_EOS_GetMusicInfo"},
	{PTP_OC_CANON_EOS_CreateHandle,"PTP_OC_CANON_EOS_CreateHandle"},
	{PTP_OC_CANON_EOS_SendPartialObjectEx,"PTP_OC_CANON_EOS_SendPartialObjectEx"},
	{PTP_OC_CANON_EOS_EndSendPartialObjectEx,"PTP_OC_CANON_EOS_EndSendPartialObjectEx"},
	{PTP_OC_CANON_EOS_SetCTGInfo,"PTP_OC_CANON_EOS_SetCTGInfo"},
	{PTP_OC_CANON_EOS_SetRequestOLCInfoGroup,"PTP_OC_CANON_EOS_SetRequestOLCInfoGroup"},
	{PTP_OC_CANON_EOS_SetRequestRollingPitchingLevel,"PTP_OC_CANON_EOS_SetRequestRollingPitchingLevel"},
	{PTP_OC_CANON_EOS_GetCameraSupport,"PTP_OC_CANON_EOS_GetCameraSupport"},
	{PTP_OC_CANON_EOS_SetRating,"PTP_OC_CANON_EOS_SetRating"},
	{PTP_OC_CANON_EOS_RequestInnerDevelopStart,"PTP_OC_CANON_EOS_RequestInnerDevelopStart"},
	{PTP_OC_CANON_EOS_RequestInnerDevelopParamChange,"PTP_OC_CANON_EOS_RequestInnerDevelopParamChange"},
	{PTP_OC_CANON_EOS_RequestInnerDevelopEnd,"PTP_OC_CANON_EOS_RequestInnerDevelopEnd"},
	{PTP_OC_CANON_EOS_GpsLoggingDataMode,"PTP_OC_CANON_EOS_GpsLoggingDataMode"},
	{PTP_OC_CANON_EOS_GetGpsLogCurrentHandle,"PTP_OC_CANON_EOS_GetGpsLogCurrentHandle"},
	{PTP_OC_CANON_EOS_InitiateViewfinder,"PTP_OC_CANON_EOS_InitiateViewfinder"},
	{PTP_OC_CANON_EOS_TerminateViewfinder,"PTP_OC_CANON_EOS_TerminateViewfinder"},
	{PTP_OC_CANON_EOS_GetViewFinderData,"PTP_OC_CANON_EOS_GetViewFinderData"},
	{PTP_OC_CANON_EOS_DoAf,"PTP_OC_CANON_EOS_DoAf"},
	{PTP_OC_CANON_EOS_DriveLens,"PTP_OC_CANON_EOS_DriveLens"},
	{PTP_OC_CANON_EOS_DepthOfFieldPreview,"PTP_OC_CANON_EOS_DepthOfFieldPreview"},
	{PTP_OC_CANON_EOS_ClickWB,"PTP_OC_CANON_EOS_ClickWB"},
	{PTP_OC_CANON_EOS_Zoom,"PTP_OC_CANON_EOS_Zoom"},
	{PTP_OC_CANON_EOS_ZoomPosition,"PTP_OC_CANON_EOS_ZoomPosition"},
	{PTP_OC_CANON_EOS_SetLiveAfFrame,"PTP_OC_CANON_EOS_SetLiveAfFrame"},
	{PTP_OC_CANON_EOS_TouchAfPosition,"PTP_OC_CANON_EOS_TouchAfPosition"},
	{PTP_OC_CANON_EOS_SetLvPcFlavoreditMode,"PTP_OC_CANON_EOS_SetLvPcFlavoreditMode"},
	{PTP_OC_CANON_EOS_SetLvPcFlavoreditParam,"PTP_OC_CANON_EOS_SetLvPcFlavoreditParam"},
	{PTP_OC_CANON_EOS_AfCancel,"PTP_OC_CANON_EOS_AfCancel"},
	{PTP_OC_CANON_EOS_SetDefaultCameraSetting,"PTP_OC_CANON_EOS_SetDefaultCameraSetting"},
	{PTP_OC_CANON_EOS_GetAEData,"PTP_OC_CANON_EOS_GetAEData"},
	{PTP_OC_CANON_EOS_NotifyNetworkError,"PTP_OC_CANON_EOS_NotifyNetworkError"},
	{PTP_OC_CANON_EOS_AdapterTransferProgress,"PTP_OC_CANON_EOS_AdapterTransferProgress"},
	{PTP_OC_CANON_EOS_TransferComplete2,"PTP_OC_CANON_EOS_TransferComplete2"},
	{PTP_OC_CANON_EOS_CancelTransfer2,"PTP_OC_CANON_EOS_CancelTransfer2"},
	{PTP_OC_CANON_EOS_FAPIMessageTX,"PTP_OC_CANON_EOS_FAPIMessageTX"},
	{PTP_OC_CANON_EOS_FAPIMessageRX,"PTP_OC_CANON_EOS_FAPIMessageRX"},
};

ptp_opcode_trans_t ptp_opcode_sony_trans[] = {
	{PTP_OC_SONY_SDIOConnect,"PTP_OC_SONY_SDIOConnect"},
	{PTP_OC_SONY_GetSDIOGetExtDeviceInfo,"PTP_OC_SONY_GetSDIOGetExtDeviceInfo"},
	{PTP_OC_SONY_GetDevicePropdesc,"PTP_OC_SONY_GetDevicePropdesc"},
	{PTP_OC_SONY_GetDevicePropertyValue,"PTP_OC_SONY_GetDevicePropertyValue"},
	{PTP_OC_SONY_SetControlDeviceA,"PTP_OC_SONY_SetControlDeviceA"},
	{PTP_OC_SONY_GetControlDeviceDesc,"PTP_OC_SONY_GetControlDeviceDesc"},
	{PTP_OC_SONY_SetControlDeviceB,"PTP_OC_SONY_SetControlDeviceB"},
	{PTP_OC_SONY_GetAllDevicePropData,"PTP_OC_SONY_GetAllDevicePropData"},
};

ptp_opcode_trans_t ptp_opcode_parrot_trans[] = {
	{PTP_OC_PARROT_GetSunshineValues,"PTP_OC_PARROT_GetSunshineValues"},
	{PTP_OC_PARROT_GetTemperatureValues,"PTP_OC_PARROT_GetTemperatureValues"},
	{PTP_OC_PARROT_GetAngleValues,"PTP_OC_PARROT_GetAngleValues"},
	{PTP_OC_PARROT_GetGpsValues,"PTP_OC_PARROT_GetGpsValues"},
	{PTP_OC_PARROT_GetGyroscopeValues,"PTP_OC_PARROT_GetGyroscopeValues"},
	{PTP_OC_PARROT_GetAccelerometerValues,"PTP_OC_PARROT_GetAccelerometerValues"},
	{PTP_OC_PARROT_GetMagnetometerValues,"PTP_OC_PARROT_GetMagnetometerValues"},
	{PTP_OC_PARROT_GetImuValues,"PTP_OC_PARROT_GetImuValues"},
	{PTP_OC_PARROT_GetStatusMask,"PTP_OC_PARROT_GetStatusMask"},
	{PTP_OC_PARROT_EjectStorage,"PTP_OC_PARROT_EjectStorage"},
	{PTP_OC_PARROT_StartMagnetoCalib,"PTP_OC_PARROT_StartMagnetoCalib"},
	{PTP_OC_PARROT_StopMagnetoCalib,"PTP_OC_PARROT_StopMagnetoCalib"},
	{PTP_OC_PARROT_MagnetoCalibStatus,"PTP_OC_PARROT_MagnetoCalibStatus"},
	{PTP_OC_PARROT_SendFirmwareUpdate,"PTP_OC_PARROT_SendFirmwareUpdate"},
};

const char*
ptp_get_opcode_name(PTPParams* params, uint16_t opcode)
{
#define RETURN_NAME_FROM_TABLE(TABLE, OPCODE) \
{ \
	unsigned int i; \
	for (i=0; i<sizeof(TABLE)/sizeof(TABLE[0]); i++) \
		if (OPCODE == TABLE[i].opcode) \
			return _(TABLE[i].name); \
	return _("Unknown PTP_OC"); \
}

	if (!(opcode & 0x8000))
		RETURN_NAME_FROM_TABLE(ptp_opcode_trans, opcode);

	switch (params->deviceinfo.VendorExtensionID) {
	case PTP_VENDOR_MICROSOFT:
	case PTP_VENDOR_MTP:	RETURN_NAME_FROM_TABLE(ptp_opcode_mtp_trans, opcode);
	case PTP_VENDOR_NIKON:	RETURN_NAME_FROM_TABLE(ptp_opcode_nikon_trans, opcode);
	case PTP_VENDOR_CANON:	RETURN_NAME_FROM_TABLE(ptp_opcode_canon_trans, opcode);
	case PTP_VENDOR_SONY:	RETURN_NAME_FROM_TABLE(ptp_opcode_sony_trans, opcode);
	case PTP_VENDOR_PARROT:	RETURN_NAME_FROM_TABLE(ptp_opcode_parrot_trans, opcode);
	default:
		break;
	}
#undef RETURN_NAME_FROM_TABLE

	return _("Unknown VendorExtensionID");
}


struct {
	uint16_t id;
	const char *name;
} ptp_opc_trans[] = {
	{PTP_OPC_StorageID,"StorageID"},
	{PTP_OPC_ObjectFormat,"ObjectFormat"},
	{PTP_OPC_ProtectionStatus,"ProtectionStatus"},
	{PTP_OPC_ObjectSize,"ObjectSize"},
	{PTP_OPC_AssociationType,"AssociationType"},
	{PTP_OPC_AssociationDesc,"AssociationDesc"},
	{PTP_OPC_ObjectFileName,"ObjectFileName"},
	{PTP_OPC_DateCreated,"DateCreated"},
	{PTP_OPC_DateModified,"DateModified"},
	{PTP_OPC_Keywords,"Keywords"},
	{PTP_OPC_ParentObject,"ParentObject"},
	{PTP_OPC_AllowedFolderContents,"AllowedFolderContents"},
	{PTP_OPC_Hidden,"Hidden"},
	{PTP_OPC_SystemObject,"SystemObject"},
	{PTP_OPC_PersistantUniqueObjectIdentifier,"PersistantUniqueObjectIdentifier"},
	{PTP_OPC_SyncID,"SyncID"},
	{PTP_OPC_PropertyBag,"PropertyBag"},
	{PTP_OPC_Name,"Name"},
	{PTP_OPC_CreatedBy,"CreatedBy"},
	{PTP_OPC_Artist,"Artist"},
	{PTP_OPC_DateAuthored,"DateAuthored"},
	{PTP_OPC_Description,"Description"},
	{PTP_OPC_URLReference,"URLReference"},
	{PTP_OPC_LanguageLocale,"LanguageLocale"},
	{PTP_OPC_CopyrightInformation,"CopyrightInformation"},
	{PTP_OPC_Source,"Source"},
	{PTP_OPC_OriginLocation,"OriginLocation"},
	{PTP_OPC_DateAdded,"DateAdded"},
	{PTP_OPC_NonConsumable,"NonConsumable"},
	{PTP_OPC_CorruptOrUnplayable,"CorruptOrUnplayable"},
	{PTP_OPC_ProducerSerialNumber,"ProducerSerialNumber"},
	{PTP_OPC_RepresentativeSampleFormat,"RepresentativeSampleFormat"},
	{PTP_OPC_RepresentativeSampleSize,"RepresentativeSampleSize"},
	{PTP_OPC_RepresentativeSampleHeight,"RepresentativeSampleHeight"},
	{PTP_OPC_RepresentativeSampleWidth,"RepresentativeSampleWidth"},
	{PTP_OPC_RepresentativeSampleDuration,"RepresentativeSampleDuration"},
	{PTP_OPC_RepresentativeSampleData,"RepresentativeSampleData"},
	{PTP_OPC_Width,"Width"},
	{PTP_OPC_Height,"Height"},
	{PTP_OPC_Duration,"Duration"},
	{PTP_OPC_Rating,"Rating"},
	{PTP_OPC_Track,"Track"},
	{PTP_OPC_Genre,"Genre"},
	{PTP_OPC_Credits,"Credits"},
	{PTP_OPC_Lyrics,"Lyrics"},
	{PTP_OPC_SubscriptionContentID,"SubscriptionContentID"},
	{PTP_OPC_ProducedBy,"ProducedBy"},
	{PTP_OPC_UseCount,"UseCount"},
	{PTP_OPC_SkipCount,"SkipCount"},
	{PTP_OPC_LastAccessed,"LastAccessed"},
	{PTP_OPC_ParentalRating,"ParentalRating"},
	{PTP_OPC_MetaGenre,"MetaGenre"},
	{PTP_OPC_Composer,"Composer"},
	{PTP_OPC_EffectiveRating,"EffectiveRating"},
	{PTP_OPC_Subtitle,"Subtitle"},
	{PTP_OPC_OriginalReleaseDate,"OriginalReleaseDate"},
	{PTP_OPC_AlbumName,"AlbumName"},
	{PTP_OPC_AlbumArtist,"AlbumArtist"},
	{PTP_OPC_Mood,"Mood"},
	{PTP_OPC_DRMStatus,"DRMStatus"},
	{PTP_OPC_SubDescription,"SubDescription"},
	{PTP_OPC_IsCropped,"IsCropped"},
	{PTP_OPC_IsColorCorrected,"IsColorCorrected"},
	{PTP_OPC_ImageBitDepth,"ImageBitDepth"},
	{PTP_OPC_Fnumber,"Fnumber"},
	{PTP_OPC_ExposureTime,"ExposureTime"},
	{PTP_OPC_ExposureIndex,"ExposureIndex"},
	{PTP_OPC_DisplayName,"DisplayName"},
	{PTP_OPC_BodyText,"BodyText"},
	{PTP_OPC_Subject,"Subject"},
	{PTP_OPC_Priority,"Priority"},
	{PTP_OPC_GivenName,"GivenName"},
	{PTP_OPC_MiddleNames,"MiddleNames"},
	{PTP_OPC_FamilyName,"FamilyName"},

	{PTP_OPC_Prefix,"Prefix"},
	{PTP_OPC_Suffix,"Suffix"},
	{PTP_OPC_PhoneticGivenName,"PhoneticGivenName"},
	{PTP_OPC_PhoneticFamilyName,"PhoneticFamilyName"},
	{PTP_OPC_EmailPrimary,"EmailPrimary"},
	{PTP_OPC_EmailPersonal1,"EmailPersonal1"},
	{PTP_OPC_EmailPersonal2,"EmailPersonal2"},
	{PTP_OPC_EmailBusiness1,"EmailBusiness1"},
	{PTP_OPC_EmailBusiness2,"EmailBusiness2"},
	{PTP_OPC_EmailOthers,"EmailOthers"},
	{PTP_OPC_PhoneNumberPrimary,"PhoneNumberPrimary"},
	{PTP_OPC_PhoneNumberPersonal,"PhoneNumberPersonal"},
	{PTP_OPC_PhoneNumberPersonal2,"PhoneNumberPersonal2"},
	{PTP_OPC_PhoneNumberBusiness,"PhoneNumberBusiness"},
	{PTP_OPC_PhoneNumberBusiness2,"PhoneNumberBusiness2"},
	{PTP_OPC_PhoneNumberMobile,"PhoneNumberMobile"},
	{PTP_OPC_PhoneNumberMobile2,"PhoneNumberMobile2"},
	{PTP_OPC_FaxNumberPrimary,"FaxNumberPrimary"},
	{PTP_OPC_FaxNumberPersonal,"FaxNumberPersonal"},
	{PTP_OPC_FaxNumberBusiness,"FaxNumberBusiness"},
	{PTP_OPC_PagerNumber,"PagerNumber"},
	{PTP_OPC_PhoneNumberOthers,"PhoneNumberOthers"},
	{PTP_OPC_PrimaryWebAddress,"PrimaryWebAddress"},
	{PTP_OPC_PersonalWebAddress,"PersonalWebAddress"},
	{PTP_OPC_BusinessWebAddress,"BusinessWebAddress"},
	{PTP_OPC_InstantMessengerAddress,"InstantMessengerAddress"},
	{PTP_OPC_InstantMessengerAddress2,"InstantMessengerAddress2"},
	{PTP_OPC_InstantMessengerAddress3,"InstantMessengerAddress3"},
	{PTP_OPC_PostalAddressPersonalFull,"PostalAddressPersonalFull"},
	{PTP_OPC_PostalAddressPersonalFullLine1,"PostalAddressPersonalFullLine1"},
	{PTP_OPC_PostalAddressPersonalFullLine2,"PostalAddressPersonalFullLine2"},
	{PTP_OPC_PostalAddressPersonalFullCity,"PostalAddressPersonalFullCity"},
	{PTP_OPC_PostalAddressPersonalFullRegion,"PostalAddressPersonalFullRegion"},
	{PTP_OPC_PostalAddressPersonalFullPostalCode,"PostalAddressPersonalFullPostalCode"},
	{PTP_OPC_PostalAddressPersonalFullCountry,"PostalAddressPersonalFullCountry"},
	{PTP_OPC_PostalAddressBusinessFull,"PostalAddressBusinessFull"},
	{PTP_OPC_PostalAddressBusinessLine1,"PostalAddressBusinessLine1"},
	{PTP_OPC_PostalAddressBusinessLine2,"PostalAddressBusinessLine2"},
	{PTP_OPC_PostalAddressBusinessCity,"PostalAddressBusinessCity"},
	{PTP_OPC_PostalAddressBusinessRegion,"PostalAddressBusinessRegion"},
	{PTP_OPC_PostalAddressBusinessPostalCode,"PostalAddressBusinessPostalCode"},
	{PTP_OPC_PostalAddressBusinessCountry,"PostalAddressBusinessCountry"},
	{PTP_OPC_PostalAddressOtherFull,"PostalAddressOtherFull"},
	{PTP_OPC_PostalAddressOtherLine1,"PostalAddressOtherLine1"},
	{PTP_OPC_PostalAddressOtherLine2,"PostalAddressOtherLine2"},
	{PTP_OPC_PostalAddressOtherCity,"PostalAddressOtherCity"},
	{PTP_OPC_PostalAddressOtherRegion,"PostalAddressOtherRegion"},
	{PTP_OPC_PostalAddressOtherPostalCode,"PostalAddressOtherPostalCode"},
	{PTP_OPC_PostalAddressOtherCountry,"PostalAddressOtherCountry"},
	{PTP_OPC_OrganizationName,"OrganizationName"},
	{PTP_OPC_PhoneticOrganizationName,"PhoneticOrganizationName"},
	{PTP_OPC_Role,"Role"},
	{PTP_OPC_Birthdate,"Birthdate"},
	{PTP_OPC_MessageTo,"MessageTo"},
	{PTP_OPC_MessageCC,"MessageCC"},
	{PTP_OPC_MessageBCC,"MessageBCC"},
	{PTP_OPC_MessageRead,"MessageRead"},
	{PTP_OPC_MessageReceivedTime,"MessageReceivedTime"},
	{PTP_OPC_MessageSender,"MessageSender"},
	{PTP_OPC_ActivityBeginTime,"ActivityBeginTime"},
	{PTP_OPC_ActivityEndTime,"ActivityEndTime"},
	{PTP_OPC_ActivityLocation,"ActivityLocation"},
	{PTP_OPC_ActivityRequiredAttendees,"ActivityRequiredAttendees"},
	{PTP_OPC_ActivityOptionalAttendees,"ActivityOptionalAttendees"},
	{PTP_OPC_ActivityResources,"ActivityResources"},
	{PTP_OPC_ActivityAccepted,"ActivityAccepted"},
	{PTP_OPC_Owner,"Owner"},
	{PTP_OPC_Editor,"Editor"},
	{PTP_OPC_Webmaster,"Webmaster"},
	{PTP_OPC_URLSource,"URLSource"},
	{PTP_OPC_URLDestination,"URLDestination"},
	{PTP_OPC_TimeBookmark,"TimeBookmark"},
	{PTP_OPC_ObjectBookmark,"ObjectBookmark"},
	{PTP_OPC_ByteBookmark,"ByteBookmark"},
	{PTP_OPC_LastBuildDate,"LastBuildDate"},
	{PTP_OPC_TimetoLive,"TimetoLive"},
	{PTP_OPC_MediaGUID,"MediaGUID"},
	{PTP_OPC_TotalBitRate,"TotalBitRate"},
	{PTP_OPC_BitRateType,"BitRateType"},
	{PTP_OPC_SampleRate,"SampleRate"},
	{PTP_OPC_NumberOfChannels,"NumberOfChannels"},
	{PTP_OPC_AudioBitDepth,"AudioBitDepth"},
	{PTP_OPC_ScanDepth,"ScanDepth"},
	{PTP_OPC_AudioWAVECodec,"AudioWAVECodec"},
	{PTP_OPC_AudioBitRate,"AudioBitRate"},
	{PTP_OPC_VideoFourCCCodec,"VideoFourCCCodec"},
	{PTP_OPC_VideoBitRate,"VideoBitRate"},
	{PTP_OPC_FramesPerThousandSeconds,"FramesPerThousandSeconds"},
	{PTP_OPC_KeyFrameDistance,"KeyFrameDistance"},
	{PTP_OPC_BufferSize,"BufferSize"},
	{PTP_OPC_EncodingQuality,"EncodingQuality"},
	{PTP_OPC_EncodingProfile,"EncodingProfile"},
	{PTP_OPC_BuyFlag,"BuyFlag"},
};

int
ptp_render_mtp_propname(uint16_t propid, int spaceleft, char *txt)
{
	unsigned int i;
	for (i=0;i<sizeof(ptp_opc_trans)/sizeof(ptp_opc_trans[0]);i++)
		if (propid == ptp_opc_trans[i].id)
			return snprintf(txt, spaceleft, "%s", ptp_opc_trans[i].name);
	return snprintf (txt, spaceleft,"unknown(%04x)", propid);
}

/*
 * Allocate and default-initialize a few object properties.
 */
MTPProperties *
ptp_get_new_object_prop_entry(MTPProperties **props, int *nrofprops)
{
	MTPProperties *newprops;
	MTPProperties *prop;

	newprops = realloc(*props,sizeof(MTPProperties)*(*nrofprops+1));
	if (newprops == NULL)
		return NULL;
	prop = &newprops[*nrofprops];
	prop->property = PTP_OPC_StorageID; /* Should be "unknown" */
	prop->datatype = PTP_DTC_UNDEF;
	prop->ObjectHandle = 0x00000000U;
	prop->propval.str = NULL;
	
	(*props) = newprops;
	(*nrofprops)++;
	return prop;
}

void 
ptp_destroy_object_prop(MTPProperties *prop)
{
  if (!prop)
    return;
  
  if (prop->datatype == PTP_DTC_STR && prop->propval.str != NULL)
    free(prop->propval.str);
  else if ((prop->datatype == PTP_DTC_AINT8 || prop->datatype == PTP_DTC_AINT16 ||
            prop->datatype == PTP_DTC_AINT32 || prop->datatype == PTP_DTC_AINT64 || prop->datatype == PTP_DTC_AINT128 ||
            prop->datatype == PTP_DTC_AUINT8 || prop->datatype == PTP_DTC_AUINT16 ||
            prop->datatype == PTP_DTC_AUINT32 || prop->datatype == PTP_DTC_AUINT64 || prop->datatype ==  PTP_DTC_AUINT128)
            && prop->propval.a.v != NULL)
    free(prop->propval.a.v);
}

void 
ptp_destroy_object_prop_list(MTPProperties *props, int nrofprops)
{
  int i;
  MTPProperties *prop = props;

  for (i=0;i<nrofprops;i++,prop++)
    ptp_destroy_object_prop(prop);
  free(props);
}

/*
 * Find a certain object property in the cache, i.e. a certain metadata
 * item for a certain object handle.
 */
MTPProperties *
ptp_find_object_prop_in_cache(PTPParams *params, uint32_t const handle, uint32_t const attribute_id)
{
	unsigned int	i;
	MTPProperties	*prop;
	PTPObject	*ob;
	uint16_t	ret;

	ret = ptp_object_find (params, handle, &ob);
	if (ret != PTP_RC_OK)
		return NULL;
	prop = ob->mtpprops;
	for (i=0;i<ob->nrofmtpprops;i++) {
		if (attribute_id == prop->property)
			return prop;
		prop++;
	}
	return NULL;
}

uint16_t
ptp_remove_object_from_cache(PTPParams *params, uint32_t handle)
{
	unsigned int i;
	PTPObject	*ob;

	CHECK_PTP_RC(ptp_object_find (params, handle, &ob));
	i = ob-params->objects;
	/* remove object from object info cache */
	ptp_free_object (ob);

	if (i < params->nrofobjects-1)
		memmove (ob,ob+1,(params->nrofobjects-1-i)*sizeof(PTPObject));
	params->nrofobjects--;
	/* We use less memory than before so this shouldn't fail */
	params->objects = realloc(params->objects, sizeof(PTPObject)*params->nrofobjects);
	return PTP_RC_OK;
}

static int _cmp_ob (const void *a, const void *b)
{
	PTPObject *oa = (PTPObject*)a;
	PTPObject *ob = (PTPObject*)b;

	/* Do not subtract the oids and return ...
	 * the unsigned int -> int conversion will overflow in cases
	 * like 0xfffc0000 vs 0x0004000. */
	if (oa->oid > ob->oid) return 1;
	if (oa->oid < ob->oid) return -1;
	return 0;
}
	
void
ptp_objects_sort (PTPParams *params)
{
	qsort (params->objects, params->nrofobjects, sizeof(PTPObject), _cmp_ob);
}

/* Binary search in objects. Needs "objects" to be a sorted by objectid list!  */
uint16_t
ptp_object_find (PTPParams *params, uint32_t handle, PTPObject **retob)
{
	PTPObject	tmpob;

	tmpob.oid = handle;
	*retob = bsearch (&tmpob, params->objects, params->nrofobjects, sizeof(tmpob), _cmp_ob);
	if (!*retob)
		return PTP_RC_GeneralError;
	return PTP_RC_OK;
}

/* Binary search in objects + insert of not found. Needs "objects" to be a sorted by objectid list!  */
uint16_t
ptp_object_find_or_insert (PTPParams *params, uint32_t handle, PTPObject **retob)
{
	unsigned int 	begin, end, cursor;
	unsigned int	insertat;
	PTPObject	*newobs;

	if (!handle) return PTP_RC_GeneralError;
	*retob = NULL;
	if (!params->nrofobjects) {
		params->objects = calloc(1,sizeof(PTPObject));
		params->nrofobjects = 1;
		params->objects[0].oid = handle;
		*retob = &params->objects[0];
		return PTP_RC_OK;
	}
	begin = 0;
	end = params->nrofobjects-1;
	/*ptp_debug (params, "searching %08x, total=%d", handle, params->nrofobjects);*/
	while (1) {
		cursor = (end-begin)/2+begin;
		/*ptp_debug (params, "ob %d: %08x [%d-%d]", cursor, params->objects[cursor].oid, begin, end);*/
		if (params->objects[cursor].oid == handle) {
			*retob = &params->objects[cursor];
			return PTP_RC_OK;
		}
		if (params->objects[cursor].oid < handle)
			begin = cursor;
		else
			end = cursor;
		if ((end - begin) <= 1)
			break;
	}
	if (params->objects[begin].oid == handle) {
		*retob = &params->objects[begin];
		return PTP_RC_OK;
	}
	if (params->objects[end].oid == handle) {
		*retob = &params->objects[end];
		return PTP_RC_OK;
	}
	if ((begin == 0) && (handle < params->objects[0].oid)) {
		insertat=begin;
	} else {
		if ((end == params->nrofobjects-1) && (handle > params->objects[end].oid))
			insertat=end+1;
		else
			insertat=begin+1;
	}
	/*ptp_debug (params, "inserting oid %x at [%x,%x], begin=%d, end=%d, insertat=%d\n", handle, params->objects[begin].oid, params->objects[end].oid, begin, end, insertat);*/
	newobs = realloc (params->objects, sizeof(PTPObject)*(params->nrofobjects+1));
	if (!newobs) return PTP_RC_GeneralError;
	params->objects = newobs;
	if (insertat<params->nrofobjects)
		memmove (&params->objects[insertat+1],&params->objects[insertat],(params->nrofobjects-insertat)*sizeof(PTPObject));
	memset(&params->objects[insertat],0,sizeof(PTPObject));
	params->objects[insertat].oid = handle;
	*retob = &params->objects[insertat];
	params->nrofobjects++;
	return PTP_RC_OK;
}

uint16_t
ptp_object_want (PTPParams *params, uint32_t handle, unsigned int want, PTPObject **retob)
{
	uint16_t	ret;
	PTPObject	*ob;
	/*Camera 		*camera = ((PTPData *)params->data)->camera;*/

	/* If GetObjectInfo is broken, force GetPropList */
	if (params->device_flags & DEVICE_FLAG_PROPLIST_OVERRIDES_OI)
		want |= PTPOBJECT_MTPPROPLIST_LOADED;

	*retob = NULL;
	if (!handle) {
		ptp_debug (params, "ptp_object_want: querying handle 0?\n");
		return PTP_RC_GeneralError;
	}
	CHECK_PTP_RC(ptp_object_find_or_insert (params, handle, &ob));
	*retob = ob;
	/* Do we have all of it already? */
	if ((ob->flags & want) == want)
		return PTP_RC_OK;

#define X (PTPOBJECT_OBJECTINFO_LOADED|PTPOBJECT_STORAGEID_LOADED|PTPOBJECT_PARENTOBJECT_LOADED)
	if ((want & X) && ((ob->flags & X) != X)) {
		uint32_t	saveparent = 0;
		
		/* One EOS issue, where getobjecthandles(root) returns obs without root flag. */
		if (ob->flags & PTPOBJECT_PARENTOBJECT_LOADED)
			saveparent = ob->oi.ParentObject;

		ret = ptp_getobjectinfo (params, handle, &ob->oi);
		if (ret != PTP_RC_OK) {
			/* kill it from the internal list ... */
			ptp_remove_object_from_cache(params, handle);
			return ret;
		}
		if (!ob->oi.Filename) ob->oi.Filename=strdup("<none>");
		if (ob->flags & PTPOBJECT_PARENTOBJECT_LOADED)
			ob->oi.ParentObject = saveparent;

		/* Second EOS issue, 0x20000000 has 0x20000000 as parent */
		if (ob->oi.ParentObject == handle)
			ob->oi.ParentObject = 0;

		/* Read out the canon special flags */
		if ((params->deviceinfo.VendorExtensionID == PTP_VENDOR_CANON) &&
		    ptp_operation_issupported(params,PTP_OC_CANON_GetObjectInfoEx)) {
			PTPCANONFolderEntry *ents = NULL;
			uint32_t            numents = 0;

			ret = ptp_canon_getobjectinfo(params,
				ob->oi.StorageID,0,
				ob->oi.ParentObject,handle,
				&ents,&numents
			);
			if ((ret == PTP_RC_OK) && (numents >= 1))
				ob->canon_flags = ents[0].Flags;
			free (ents);
		}

		ob->flags |= X;
	}
#undef X
	if (	(want & PTPOBJECT_MTPPROPLIST_LOADED) &&
		(!(ob->flags & PTPOBJECT_MTPPROPLIST_LOADED))
	) {
		int		nrofprops = 0;
		MTPProperties 	*props = NULL;

		if (params->device_flags & DEVICE_FLAG_BROKEN_MTPGETOBJPROPLIST) {
			want &= ~PTPOBJECT_MTPPROPLIST_LOADED;
			goto fallback;
		}
		/* Microsoft/MTP has fast directory retrieval. */
		if (!ptp_operation_issupported(params,PTP_OC_MTP_GetObjPropList)) {
			want &= ~PTPOBJECT_MTPPROPLIST_LOADED;
			goto fallback;
		}

		ptp_debug (params, "ptp2/mtpfast: reading mtp proplist of %08x", handle);
		/* We just want this one object, not all at once. */
		ret = ptp_mtp_getobjectproplist_single (params, handle, &props, &nrofprops);
		if (ret != PTP_RC_OK)
			goto fallback;
		ob->mtpprops = props;
		ob->nrofmtpprops = nrofprops;

		/* Override the ObjectInfo data with data from properties */
		if (params->device_flags & DEVICE_FLAG_PROPLIST_OVERRIDES_OI) {
			unsigned int i;
			MTPProperties *prop = ob->mtpprops;

			for (i=0;i<ob->nrofmtpprops;i++,prop++) {
				/* in case we got all subtree objects */
				if (prop->ObjectHandle != handle) continue;

				switch (prop->property) {
				case PTP_OPC_StorageID:
					ob->oi.StorageID = prop->propval.u32;
					break;
				case PTP_OPC_ObjectFormat:
					ob->oi.ObjectFormat = prop->propval.u16;
					break;
				case PTP_OPC_ProtectionStatus:
					ob->oi.ProtectionStatus = prop->propval.u16;
					break;
				case PTP_OPC_ObjectSize:
					if (prop->datatype == PTP_DTC_UINT64) {
						if (prop->propval.u64 > 0xFFFFFFFFU)
							ob->oi.ObjectCompressedSize = 0xFFFFFFFFU;
						else
							ob->oi.ObjectCompressedSize = (uint32_t)prop->propval.u64;
					} else if (prop->datatype == PTP_DTC_UINT32) {
						ob->oi.ObjectCompressedSize = prop->propval.u32;
					}
					break;
				case PTP_OPC_AssociationType:
					ob->oi.AssociationType = prop->propval.u16;
					break;
				case PTP_OPC_AssociationDesc:
					ob->oi.AssociationDesc = prop->propval.u32;
					break;
				case PTP_OPC_ObjectFileName:
					if (prop->propval.str) {
						free(ob->oi.Filename);
						ob->oi.Filename = strdup(prop->propval.str);
					}
					break;
				case PTP_OPC_DateCreated:
					ob->oi.CaptureDate = ptp_unpack_PTPTIME(prop->propval.str);
					break;
				case PTP_OPC_DateModified:
					ob->oi.ModificationDate = ptp_unpack_PTPTIME(prop->propval.str);
					break;
				case PTP_OPC_Keywords:
					if (prop->propval.str) {
						free(ob->oi.Keywords);
						ob->oi.Keywords = strdup(prop->propval.str);
					}
					break;
				case PTP_OPC_ParentObject:
					ob->oi.ParentObject = prop->propval.u32;
					break;
				}
			}
		}

#if 0
		MTPProperties 	*xpl;
		int j;
		PTPObjectInfo	oinfo;	

		memset (&oinfo,0,sizeof(oinfo));
		/* hmm, not necessary ... only if we would use it */
		for (j=0;j<nrofprops;j++) {
			xpl = &props[j];
			switch (xpl->property) {
			case PTP_OPC_ParentObject:
				if (xpl->datatype != PTP_DTC_UINT32) {
					ptp_debug (params, "ptp2/mtpfast: parentobject has type 0x%x???", xpl->datatype);
					break;
				}
				oinfo.ParentObject = xpl->propval.u32;
				ptp_debug (params, "ptp2/mtpfast: parent 0x%x", xpl->propval.u32);
				break;
			case PTP_OPC_ObjectFormat:
				if (xpl->datatype != PTP_DTC_UINT16) {
					ptp_debug (params, "ptp2/mtpfast: objectformat has type 0x%x???", xpl->datatype);
					break;
				}
				oinfo.ObjectFormat = xpl->propval.u16;
				ptp_debug (params, "ptp2/mtpfast: ofc 0x%x", xpl->propval.u16);
				break;
			case PTP_OPC_ObjectSize:
				switch (xpl->datatype) {
				case PTP_DTC_UINT32:
					oinfo.ObjectCompressedSize = xpl->propval.u32;
					break;
				case PTP_DTC_UINT64:
					oinfo.ObjectCompressedSize = xpl->propval.u64;
					break;
				default:
					ptp_debug (params, "ptp2/mtpfast: objectsize has type 0x%x???", xpl->datatype);
					break;
				}
				ptp_debug (params, "ptp2/mtpfast: objectsize %u", xpl->propval.u32);
				break;
			case PTP_OPC_StorageID:
				if (xpl->datatype != PTP_DTC_UINT32) {
					ptp_debug (params, "ptp2/mtpfast: storageid has type 0x%x???", xpl->datatype);
					break;
				}
				oinfo.StorageID = xpl->propval.u32;
				ptp_debug (params, "ptp2/mtpfast: storageid 0x%x", xpl->propval.u32);
				break;
			case PTP_OPC_ProtectionStatus:/*UINT16*/
				if (xpl->datatype != PTP_DTC_UINT16) {
					ptp_debug (params, "ptp2/mtpfast: protectionstatus has type 0x%x???", xpl->datatype);
					break;
				}
				oinfo.ProtectionStatus = xpl->propval.u16;
				ptp_debug (params, "ptp2/mtpfast: protection 0x%x", xpl->propval.u16);
				break;
			case PTP_OPC_ObjectFileName:
				if (xpl->datatype != PTP_DTC_STR) {
					ptp_debug (params, "ptp2/mtpfast: filename has type 0x%x???", xpl->datatype);
					break;
				}
				if (xpl->propval.str) {
					ptp_debug (params, "ptp2/mtpfast: filename %s", xpl->propval.str);
					oinfo.Filename = strdup(xpl->propval.str);
				} else {
					oinfo.Filename = NULL;
				}
				break;
			case PTP_OPC_DateCreated:
				if (xpl->datatype != PTP_DTC_STR) {
					ptp_debug (params, "ptp2/mtpfast: datecreated has type 0x%x???", xpl->datatype);
					break;
				}
				ptp_debug (params, "ptp2/mtpfast: capturedate %s", xpl->propval.str);
				oinfo.CaptureDate = ptp_unpack_PTPTIME (xpl->propval.str);
				break;
			case PTP_OPC_DateModified:
				if (xpl->datatype != PTP_DTC_STR) {
					ptp_debug (params, "ptp2/mtpfast: datemodified has type 0x%x???", xpl->datatype);
					break;
				}
				ptp_debug (params, "ptp2/mtpfast: moddate %s", xpl->propval.str);
				oinfo.ModificationDate = ptp_unpack_PTPTIME (xpl->propval.str);
				break;
			default:
				if ((xpl->property & 0xfff0) == 0xdc00)
					ptp_debug (params, "ptp2/mtpfast:case %x type %x unhandled", xpl->property, xpl->datatype);
				break;
			}
		}
		if (!oinfo.Filename)
			/* i have one such file on my Creative */
			oinfo.Filename = strdup("<null>");
#endif
		ob->flags |= PTPOBJECT_MTPPROPLIST_LOADED;
fallback:	;
	}
	if ((ob->flags & want) == want)
		return PTP_RC_OK;
	ptp_debug (params, "ptp_object_want: oid 0x%08x, want flags %x, have only %x?", handle, want, ob->flags);
	return PTP_RC_GeneralError;
}


uint16_t
ptp_add_object_to_cache(PTPParams *params, uint32_t handle)
{
	PTPObject *ob;
	return ptp_object_want (params, handle, PTPOBJECT_OBJECTINFO_LOADED|PTPOBJECT_MTPPROPLIST_LOADED, &ob);
}
