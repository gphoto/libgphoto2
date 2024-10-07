/* ptp-pack.c
 *
 * Copyright (C) 2001-2004 Mariusz Woloszyn <emsi@ipartners.pl>
 * Copyright (C) 2003-2019 Marcus Meissner <marcus@jet.franken.de>
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

/* currently this file is included into ptp.c */

#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif
#ifndef UINT_MAX
# define UINT_MAX 0xFFFFFFFF
#endif
#if defined(HAVE_ICONV) && defined(HAVE_LANGINFO_H)
#include <iconv.h>
#endif

#ifndef MAX
# define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif
#ifndef MIN
# define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

/*
 * PTP strings ... if the size field is:
 * size 0  : "empty string" ... we interpret that as string with just \0 terminator, return 1
 *  (the whole PTP standard is not that clear, it occasionally refers to strings as optional in such cases, but no clear guidance).
 * size > 0: all other strings have a terminating \0, included in the length (not sure how conforming everyone is here)
 *
 * len - in ptp string characters currently
 */
static inline int
ptp_unpack_string(PTPParams *params, const unsigned char* data, uint32_t *offset, uint32_t size, char **result)
{
	uint8_t ucs2len;		/* length of the string in UCS-2 chars, including terminating \0 */
	uint16_t ucs2src[PTP_MAXSTRLEN+1];
	/* allow for UTF-8: max of 3 bytes per UCS-2 char, plus final null */
	char utf8dest[PTP_MAXSTRLEN*3+1] = { 0 };

	if (!data || !offset || !result)
		return 0;

	*result = NULL;

	if (*offset + 1 > size)
		return 0;

	ucs2len = dtoh8o(data, *offset);	/* PTP_MAXSTRLEN == 255, 8 bit len */
	if (ucs2len == 0) {		/* nothing to do? */
		*result = strdup("");	/* return an empty string, not NULL */
		return 1;
	}

	if (*offset + ucs2len * sizeof(ucs2src[0]) > size)
		return 0;

	/* copy to string[] to ensure correct alignment for iconv(3) */
	memcpy(ucs2src, data + *offset, ucs2len * sizeof(ucs2src[0]));
	ucs2src[ucs2len] = 0;   /* be paranoid!  add a terminator. */

	/* convert from camera UCS-2 to our locale */
	size_t nconv = (size_t)-1;
#if defined(HAVE_ICONV) && defined(HAVE_LANGINFO_H)
	char* src = (char *)ucs2src;
	size_t srclen = ucs2len * sizeof(ucs2src[0]);
	char* dest = utf8dest;
	size_t destlen = sizeof(utf8dest)-1;
	if (params->cd_ucs2_to_locale != (iconv_t)-1)
		nconv = iconv(params->cd_ucs2_to_locale, &src, &srclen, &dest, &destlen);
#endif
	if (nconv == (size_t) -1) { /* do it the hard way */
		/* try the old way, in case iconv is broken */
		for (int i=0;i<=ucs2len;i++)
			utf8dest[i] = ucs2src[i] > 127 ? '?' : (char)ucs2src[i];
	}
	*result = strdup(utf8dest);
	*offset += 2 * ucs2len;
	return 1;
}

static inline int
ucs2strlen(uint16_t const * const unicstr)
{
	int length = 0;

	/* Unicode strings are terminated with 2 * 0x00 */
	for(length = 0; unicstr[length] != 0x0000U; length ++);
	return length;
}


static inline void
ptp_pack_string(PTPParams *params, const char *string, unsigned char* data, uint16_t offset, uint8_t *len)
{
	int packedlen = 0;
	uint16_t ucs2str[PTP_MAXSTRLEN+1];
	char *ucs2strp = (char *) ucs2str;
	size_t convlen = strlen(string);

	/* Cannot exceed 255 (PTP_MAXSTRLEN) since it is a single byte, duh ... */
	memset(ucs2strp, 0, sizeof(ucs2str));  /* XXX: necessary? */
#if defined(HAVE_ICONV) && defined(HAVE_LANGINFO_H)
	if (params->cd_locale_to_ucs2 != (iconv_t)-1) {
		size_t nconv;
		size_t convmax = PTP_MAXSTRLEN * 2; /* Includes the terminator */
		char *inbuf = (char*)string; /* the 'internet' says iconv will not change the input string */

		nconv = iconv(params->cd_locale_to_ucs2, &inbuf, &convlen,
			&ucs2strp, &convmax);
		if (nconv == (size_t) -1)
			ucs2str[0] = 0x0000U;
	} else
#endif
	{
		unsigned int i;

		for (i=0;i<convlen;i++) {
			ucs2str[i] = string[i];
		}
		ucs2str[convlen] = 0;
	}
	/*
	 * XXX: isn't packedlen just ( (uint16_t *)ucs2strp - ucs2str )?
	 *      why do we need ucs2strlen()?
	 */
	packedlen = ucs2strlen(ucs2str);
	if (packedlen > PTP_MAXSTRLEN-1) {
		*len=0;
		return;
	}

	/* number of characters including terminating 0 (PTP standard confirmed) */
	htod8a(&data[offset],packedlen+1);
	memcpy(&data[offset+1], &ucs2str[0], packedlen * sizeof(ucs2str[0]));
	htod16a(&data[offset+packedlen*2+1], 0x0000);  /* terminate 0 */

	/* The returned length is in number of characters */
	*len = (uint8_t) packedlen+1;
}

static inline unsigned char *
ptp_get_packed_stringcopy(PTPParams *params, const char *string, uint32_t *packed_size)
{
	uint8_t packed[PTP_MAXSTRLEN*2+3], len;
	size_t plen;
	unsigned char *retcopy = NULL;

	if (string == NULL)
	  ptp_pack_string(params, "", (unsigned char*) packed, 0, &len);
	else
	  ptp_pack_string(params, string, (unsigned char*) packed, 0, &len);

	/* returned length is in characters, then one byte for string length */
	plen = len*2 + 1;

	retcopy = malloc(plen);
	if (!retcopy) {
		*packed_size = 0;
		return NULL;
	}
	memcpy(retcopy, packed, plen);
	*packed_size = plen;
	return (retcopy);
}

static inline int
ptp_unpack_uint32_t_array(PTPParams *params, const uint8_t* data, uint32_t *offset, uint32_t datalen,
                          uint32_t **array, uint32_t* arraylen)
{
	if (!array || !arraylen)
		return 0;

	*array = NULL;
	*arraylen = 0;

	if (!data || *offset + sizeof(uint32_t) > datalen)
		return 0;

	uint32_t n = dtoh32o(data, *offset);
	if (n == 0)
		return 1;

	if (*offset + n * sizeof(uint32_t) > datalen) {
		ptp_debug (params ,"array runs over datalen buffer end (%ld vs %u)", *offset + n * sizeof(uint32_t) , datalen);
		return 0;
	}

	*array = calloc (n, sizeof(uint32_t));
	if (!*array)
		return 0;

	for (unsigned i=0;i<n;i++)
		(*array)[i] = dtoh32o(data, *offset);
	*arraylen = n;

	return 1;
}

static inline uint32_t
ptp_pack_uint32_t_array(PTPParams *params, const uint32_t *array, uint32_t arraylen, unsigned char **data )
{
	uint32_t i=0;

	*data = calloc ((arraylen+1),sizeof(uint32_t));
	if (!*data)
		return 0;
	htod32a(&(*data)[0],arraylen);
	for (i=0;i<arraylen;i++)
		htod32a(&(*data)[sizeof(uint32_t)*(i+1)], array[i]);
	return (arraylen+1)*sizeof(uint32_t);
}

static inline int
ptp_unpack_uint16_t_array(PTPParams *params, const uint8_t* data, uint32_t *offset, uint32_t datalen,
                          uint16_t **array, uint32_t* arraylen)
{
	if (!array || !arraylen)
		return 0;

	*array = NULL;
	*arraylen = 0;

	if (!data || *offset + sizeof(uint32_t) > datalen)
		return 0;

	uint32_t n = dtoh32o(data, *offset);
	if (n == 0)
		return 1;

	if (*offset + n * sizeof(uint16_t) > datalen) {
		ptp_debug (params ,"array runs over datalen buffer end (%ld vs %u)", *offset + n * sizeof(uint16_t) , datalen);
		return 0;
	}

	*array = calloc (n, sizeof(uint16_t));
	if (!*array)
		return 0;

	for (unsigned i=0;i<n;i++)
		(*array)[i] = dtoh16o(data, *offset);
	*arraylen = n;

	return 1;
}

/* DeviceInfo pack/unpack */

#define PTP_di_StandardVersion		 0
#define PTP_di_VendorExtensionID	 2
#define PTP_di_VendorExtensionVersion	 6
#define PTP_di_VendorExtensionDesc	 8
#define PTP_di_FunctionalMode		 8
#define PTP_di_Operations	10

static inline int
ptp_unpack_DI (PTPParams *params, const unsigned char* data, PTPDeviceInfo *di, unsigned int datalen)
{
	unsigned int offset = 0;

	if (!data) return 0;
	if (datalen < 12) return 0;
	memset (di, 0, sizeof(*di));
	di->StandardVersion        = dtoh16a(data + PTP_di_StandardVersion);
	di->VendorExtensionID      = dtoh32a(data + PTP_di_VendorExtensionID);
	di->VendorExtensionVersion = dtoh16a(data + PTP_di_VendorExtensionVersion);
	offset = PTP_di_VendorExtensionDesc;

	if (!ptp_unpack_string(params, data, &offset, datalen, &di->VendorExtensionDesc))
		return 0;
	if (datalen <= offset + sizeof(uint16_t)) {
		ptp_debug (params, "FunctionalMode outside buffer bounds (%ld > %u)", offset + sizeof(uint16_t), datalen);
		return 0;
	}
	di->FunctionalMode = dtoh16o(data, offset);
	if (!ptp_unpack_uint16_t_array(params, data, &offset, datalen, &di->Operations, &di->Operations_len)) {
		ptp_debug (params, "failed to unpack Operations array");
		return 0;
	}
	if (!ptp_unpack_uint16_t_array(params, data, &offset, datalen, &di->Events, &di->Events_len)) {
		ptp_debug (params, "failed to unpack Events array");
		return 0;
	}
	if (!ptp_unpack_uint16_t_array(params, data, &offset, datalen, &di->DeviceProps, &di->DeviceProps_len)) {
		ptp_debug (params, "failed to unpack DeviceProps array");
		return 0;
	}
	if (!ptp_unpack_uint16_t_array(params, data, &offset, datalen, &di->CaptureFormats, &di->CaptureFormats_len)) {
		ptp_debug (params, "failed to unpack CaptureFormats array");
		return 0;
	}
	if (!ptp_unpack_uint16_t_array(params, data, &offset, datalen, &di->ImageFormats, &di->ImageFormats_len)) {
		ptp_debug (params, "failed to unpack ImageFormats array");
		return 0;
	}

	/* be more relaxed ... as these are optional its ok if they are not here */
	if (!ptp_unpack_string(params, data, &offset, datalen, &di->Manufacturer))
		ptp_debug (params, "failed to unpack Manufacturer string");

	if (!ptp_unpack_string(params, data, &offset, datalen, &di->Model))
		ptp_debug (params, "failed to unpack Model string");

	if (!ptp_unpack_string(params, data, &offset, datalen, &di->DeviceVersion))
		ptp_debug (params, "failed to unpack DeviceVersion string");

	if (!ptp_unpack_string(params, data, &offset, datalen, &di->SerialNumber))
		ptp_debug (params, "failed to unpack SerialNumber string");

	return 1;
}

/* EOS Device Info unpack */
static inline int
ptp_unpack_EOS_DI (PTPParams *params, const unsigned char* data, PTPCanonEOSDeviceInfo *di, unsigned int datalen)
{
	uint32_t offset = 4;

	memset (di,0, sizeof(*di));

	ptp_unpack_uint32_t_array(params, data, &offset, datalen, &di->Events, &di->Events_len);
	ptp_unpack_uint32_t_array(params, data, &offset, datalen, &di->DeviceProps, &di->DeviceProps_len);
	ptp_unpack_uint32_t_array(params, data, &offset, datalen, &di->unk, &di->unk_len);

	return offset >= 16;
}

/* ObjectHandles array pack/unpack */

static inline void
ptp_unpack_OH (PTPParams *params, const unsigned char* data, PTPObjectHandles *oh, unsigned int len)
{
	uint32_t offset = 0;
	ptp_unpack_uint32_t_array(params, data, &offset, len, &oh->Handler, &oh->n);
}

/* StoreIDs array pack/unpack */

static inline void
ptp_unpack_SIDs (PTPParams *params, const unsigned char* data, PTPStorageIDs *sids, unsigned int len)
{
	uint32_t offset = 0;
	ptp_unpack_uint32_t_array(params, data, &offset, len, &sids->Storage, &sids->n);
}

/* StorageInfo pack/unpack */

#define PTP_si_StorageType		 0
#define PTP_si_FilesystemType		 2
#define PTP_si_AccessCapability		 4
#define PTP_si_MaxCapability		 6
#define PTP_si_FreeSpaceInBytes		14
#define PTP_si_FreeSpaceInImages	22
#define PTP_si_StorageDescription	26

static inline int
ptp_unpack_SI (PTPParams *params, const unsigned char* data, PTPStorageInfo *si, unsigned int len)
{
	if (!data || len < 26) return 0;
	si->StorageType       = dtoh16a(data + PTP_si_StorageType);
	si->FilesystemType    = dtoh16a(data + PTP_si_FilesystemType);
	si->AccessCapability  = dtoh16a(data + PTP_si_AccessCapability);
	si->MaxCapability     = dtoh64a(data + PTP_si_MaxCapability);
	si->FreeSpaceInBytes  = dtoh64a(data + PTP_si_FreeSpaceInBytes);
	si->FreeSpaceInImages = dtoh32a(data + PTP_si_FreeSpaceInImages);

	uint32_t offset = PTP_si_StorageDescription;

	if (!ptp_unpack_string(params, data, &offset, len, &si->StorageDescription)) {
		ptp_debug(params, "could not unpack StorageDescription");
		return 0;
	}
	if (!ptp_unpack_string(params, data, &offset, len, &si->VolumeLabel)) {
		ptp_debug(params, "could not unpack VolumeLabel");
		return 0;
	}
	return 1;
}

/* ObjectInfo pack/unpack */

#define PTP_oi_StorageID		 0
#define PTP_oi_ObjectFormat		 4
#define PTP_oi_ProtectionStatus		 6
#define PTP_oi_ObjectSize		 8
#define PTP_oi_ThumbFormat		12
#define PTP_oi_ThumbSize		14
#define PTP_oi_ThumbPixWidth		18
#define PTP_oi_ThumbPixHeight		22
#define PTP_oi_ImagePixWidth		26
#define PTP_oi_ImagePixHeight		30
#define PTP_oi_ImageBitDepth		34
#define PTP_oi_ParentObject		38
#define PTP_oi_AssociationType		42
#define PTP_oi_AssociationDesc		44
#define PTP_oi_SequenceNumber		48
#define PTP_oi_filenamelen		52
#define PTP_oi_Filename			53

/* the max length assuming zero length dates. We have need 3 */
/* bytes for these. */
#define PTP_oi_MaxLen PTP_oi_Filename+(PTP_MAXSTRLEN+1)*2+3

static inline uint32_t
ptp_pack_OI (PTPParams *params, PTPObjectInfo *oi, unsigned char** oidataptr)
{
	unsigned char* oidata;
	uint8_t filenamelen;
	uint8_t capturedatelen=0;
	/* let's allocate some memory first; correct assuming zero length dates */
	oidata=malloc(PTP_oi_MaxLen + params->ocs64*4);
	*oidataptr=oidata;
	/* the caller should free it after use! */
#if 0
	char *capture_date="20020101T010101"; /* XXX Fake date */
#endif
	memset (oidata, 0, PTP_oi_MaxLen + params->ocs64*4);
	htod32a(&oidata[PTP_oi_StorageID],oi->StorageID);
	htod16a(&oidata[PTP_oi_ObjectFormat],oi->ObjectFormat);
	htod16a(&oidata[PTP_oi_ProtectionStatus],oi->ProtectionStatus);
	htod32a(&oidata[PTP_oi_ObjectSize],oi->ObjectSize);
	if (params->ocs64)
		oidata += 4;
	htod16a(&oidata[PTP_oi_ThumbFormat],oi->ThumbFormat);
	htod32a(&oidata[PTP_oi_ThumbSize],oi->ThumbSize);
	htod32a(&oidata[PTP_oi_ThumbPixWidth],oi->ThumbPixWidth);
	htod32a(&oidata[PTP_oi_ThumbPixHeight],oi->ThumbPixHeight);
	htod32a(&oidata[PTP_oi_ImagePixWidth],oi->ImagePixWidth);
	htod32a(&oidata[PTP_oi_ImagePixHeight],oi->ImagePixHeight);
	htod32a(&oidata[PTP_oi_ImageBitDepth],oi->ImageBitDepth);
	htod32a(&oidata[PTP_oi_ParentObject],oi->ParentObject);
	htod16a(&oidata[PTP_oi_AssociationType],oi->AssociationType);
	htod32a(&oidata[PTP_oi_AssociationDesc],oi->AssociationDesc);
	htod32a(&oidata[PTP_oi_SequenceNumber],oi->SequenceNumber);

	ptp_pack_string(params, oi->Filename, oidata, PTP_oi_filenamelen, &filenamelen);
/*
	filenamelen=(uint8_t)strlen(oi->Filename);
	htod8a(&req->data[PTP_oi_filenamelen],filenamelen+1);
	for (i=0;i<filenamelen && i< PTP_MAXSTRLEN; i++) {
		req->data[PTP_oi_Filename+i*2]=oi->Filename[i];
	}
*/
	/*
	 *XXX Fake date.
	 * for example Kodak sets Capture date on the basis of EXIF data.
	 * Spec says that this field is from perspective of Initiator.
	 */
#if 0	/* seems now we don't need any data packed in OI dataset... for now ;)*/
	capturedatelen=strlen(capture_date);
	htod8a(&data[PTP_oi_Filename+(filenamelen+1)*2],
		capturedatelen+1);
	for (i=0;i<capturedatelen && i< PTP_MAXSTRLEN; i++) {
		data[PTP_oi_Filename+(i+filenamelen+1)*2+1]=capture_date[i];
	}
	htod8a(&data[PTP_oi_Filename+(filenamelen+capturedatelen+2)*2+1],
		capturedatelen+1);
	for (i=0;i<capturedatelen && i< PTP_MAXSTRLEN; i++) {
		data[PTP_oi_Filename+(i+filenamelen+capturedatelen+2)*2+2]=
		  capture_date[i];
	}
#endif
	/* XXX this function should return dataset length */
	return (PTP_oi_Filename+filenamelen*2+(capturedatelen+1)*3)+params->ocs64*4;
}

static time_t
ptp_unpack_PTPTIME (const char *str) {
	char ptpdate[40];
	char tmp[5];
	size_t  ptpdatelen;
	struct tm tm;

	if (!str)
		return 0;
	ptpdatelen = strlen(str);
	if (ptpdatelen >= sizeof (ptpdate)) {
		/*ptp_debug (params ,"datelen is larger then size of buffer", ptpdatelen, (int)sizeof(ptpdate));*/
		return 0;
	}
	if (ptpdatelen<15) {
		/*ptp_debug (params ,"datelen is less than 15 (%d)", ptpdatelen);*/
		return 0;
	}
	strncpy (ptpdate, str, sizeof(ptpdate));
	ptpdate[sizeof(ptpdate) - 1] = '\0';

	memset(&tm,0,sizeof(tm));
	strncpy (tmp, ptpdate, 4);
	tmp[4] = 0;
	tm.tm_year=atoi (tmp) - 1900;
	strncpy (tmp, ptpdate + 4, 2);
	tmp[2] = 0;
	tm.tm_mon = atoi (tmp) - 1;
	strncpy (tmp, ptpdate + 6, 2);
	tmp[2] = 0;
	tm.tm_mday = atoi (tmp);
	strncpy (tmp, ptpdate + 9, 2);
	tmp[2] = 0;
	tm.tm_hour = atoi (tmp);
	strncpy (tmp, ptpdate + 11, 2);
	tmp[2] = 0;
	tm.tm_min = atoi (tmp);
	strncpy (tmp, ptpdate + 13, 2);
	tmp[2] = 0;
	tm.tm_sec = atoi (tmp);
	tm.tm_isdst = -1;
	return mktime (&tm);
}

static inline void
ptp_unpack_OI (PTPParams *params, const unsigned char* data, PTPObjectInfo *oi, unsigned int len)
{
	char *capture_date;

	if (!data || len < PTP_oi_SequenceNumber)
		return;

	oi->Filename = oi->Keywords = NULL;

	/* FIXME: also handle leng th with all the strings at the end */
	oi->StorageID            = dtoh32a(data + PTP_oi_StorageID);
	oi->ObjectFormat         = dtoh16a(data + PTP_oi_ObjectFormat);
	oi->ProtectionStatus     = dtoh16a(data + PTP_oi_ProtectionStatus);
	oi->ObjectSize           = dtoh32a(data + PTP_oi_ObjectSize);

	/* Stupid Samsung Galaxy developers emit a 64bit objectcompressedsize */
	if ((data[PTP_oi_filenamelen] == 0) && (data[PTP_oi_filenamelen+4] != 0)) {
		ptp_debug (params, "objectsize 64bit detected!");
		params->ocs64 = 1;
		data += 4;
		len -= 4;
	}
	oi->ThumbFormat         = dtoh16a(data + PTP_oi_ThumbFormat);
	oi->ThumbSize           = dtoh32a(data + PTP_oi_ThumbSize);
	oi->ThumbPixWidth       = dtoh32a(data + PTP_oi_ThumbPixWidth);
	oi->ThumbPixHeight      = dtoh32a(data + PTP_oi_ThumbPixHeight);
	oi->ImagePixWidth       = dtoh32a(data + PTP_oi_ImagePixWidth);
	oi->ImagePixHeight      = dtoh32a(data + PTP_oi_ImagePixHeight);
	oi->ImageBitDepth       = dtoh32a(data + PTP_oi_ImageBitDepth);
	oi->ParentObject        = dtoh32a(data + PTP_oi_ParentObject);
	oi->AssociationType     = dtoh16a(data + PTP_oi_AssociationType);
	oi->AssociationDesc     = dtoh32a(data + PTP_oi_AssociationDesc);
	oi->SequenceNumber      = dtoh32a(data + PTP_oi_SequenceNumber);

	uint32_t offset = PTP_oi_filenamelen;
	ptp_unpack_string(params, data, &offset, len, &oi->Filename);
	ptp_unpack_string(params, data, &offset, len, &capture_date);
	/* subset of ISO 8601, without '.s' tenths of second and time zone */
	oi->CaptureDate = ptp_unpack_PTPTIME(capture_date);
	free(capture_date);

	/* now the modification date ... */
	ptp_unpack_string(params, data, &offset, len, &capture_date);
	oi->ModificationDate = ptp_unpack_PTPTIME(capture_date);
	free(capture_date);
}

/* Custom Type Value Assignment (without Length) macro frequently used below */
#define CTVAL(target,func) {			\
	if (total - *offset < sizeof(target))	\
		return 0;			\
	target = func(data + *offset);		\
	*offset += sizeof(target);		\
}

#define RARR(val,member,func)	{			\
	unsigned int n,j;				\
	if (total - *offset < sizeof(uint32_t))		\
		return 0;				\
	n = dtoh32a (data + *offset);			\
	*offset += sizeof(uint32_t);			\
							\
	if (n >= UINT_MAX/sizeof(val->a.v[0]))		\
		return 0;				\
	if (n > (total - (*offset))/sizeof(val->a.v[0].member))\
		return 0;				\
	val->a.count = n;				\
	val->a.v = calloc(n, sizeof(val->a.v[0]));	\
	if (!val->a.v) return 0;			\
	for (j=0;j<n;j++)				\
		CTVAL(val->a.v[j].member, func);	\
}

static inline unsigned int
ptp_unpack_DPV (
	PTPParams *params, const unsigned char* data, unsigned int *offset, unsigned int total,
	PTPPropValue* value, uint16_t datatype
) {
	if (*offset >= total)	/* we are at the end or over the end of the buffer */
		return 0;

	switch (datatype) {
	case PTP_DTC_INT8:   CTVAL(value->i8,dtoh8a); break;
	case PTP_DTC_UINT8:  CTVAL(value->u8,dtoh8a); break;
	case PTP_DTC_INT16:  CTVAL(value->i16,dtoh16a); break;
	case PTP_DTC_UINT16: CTVAL(value->u16,dtoh16a); break;
	case PTP_DTC_INT32:  CTVAL(value->i32,dtoh32a); break;
	case PTP_DTC_UINT32: CTVAL(value->u32,dtoh32a); break;
	case PTP_DTC_INT64:  CTVAL(value->i64,dtoh64a); break;
	case PTP_DTC_UINT64: CTVAL(value->u64,dtoh64a); break;

	case PTP_DTC_UINT128:
		*offset += 16;
		/*fprintf(stderr,"unhandled unpack of uint128n");*/
		break;
	case PTP_DTC_INT128:
		*offset += 16;
		/*fprintf(stderr,"unhandled unpack of int128n");*/
		break;

	case PTP_DTC_AINT8:   RARR(value,i8,dtoh8a); break;
	case PTP_DTC_AUINT8:  RARR(value,u8,dtoh8a); break;
	case PTP_DTC_AUINT16: RARR(value,u16,dtoh16a); break;
	case PTP_DTC_AINT16:  RARR(value,i16,dtoh16a); break;
	case PTP_DTC_AUINT32: RARR(value,u32,dtoh32a); break;
	case PTP_DTC_AINT32:  RARR(value,i32,dtoh32a); break;
	case PTP_DTC_AUINT64: RARR(value,u64,dtoh64a); break;
	case PTP_DTC_AINT64:  RARR(value,i64,dtoh64a); break;
	/* XXX: other int types are unimplemented */
	/* XXX: other int arrays are unimplemented also */
	case PTP_DTC_STR: {
		/* XXX: max size */
		if (!ptp_unpack_string(params, data, offset, total, &value->str))
			return 0;
		break;
	}
	default:
		return 0;
	}
	return 1;
}

/* Device Property pack/unpack */
#define PTP_dpd_DevicePropCode	0
#define PTP_dpd_DataType	2
#define PTP_dpd_GetSet		4
#define PTP_dpd_DefaultValue	5

static inline int
ptp_unpack_DPD (PTPParams *params, const unsigned char* data, PTPDevicePropDesc *dpd, unsigned int dpdlen, uint32_t *offset)
{
	int ret;

	memset (dpd, 0, sizeof(*dpd));
	if (dpdlen <= 5)
		return 0;
	dpd->DevicePropCode = dtoh16a(data + PTP_dpd_DevicePropCode);
	dpd->DataType       = dtoh16a(data + PTP_dpd_DataType);
	dpd->GetSet         = dtoh8a (data + PTP_dpd_GetSet);
	dpd->FormFlag       = PTP_DPFF_None;

	*offset = PTP_dpd_DefaultValue;
	ret = ptp_unpack_DPV (params, data, offset, dpdlen, &dpd->DefaultValue, dpd->DataType);
	if (!ret) goto outofmemory;
	if ((dpd->DataType == PTP_DTC_STR) && (*offset == dpdlen))
		return 1;

	ret = ptp_unpack_DPV (params, data, offset, dpdlen, &dpd->CurrentValue, dpd->DataType);
	if (!ret) goto outofmemory;

	/* if offset==0 then Data Type format is not supported by this
	   code or the Data Type is a string (with two empty strings as
	   values). In both cases Form Flag should be set to 0x00 and FORM is
	   not present. */

	if (*offset + sizeof(uint8_t) > dpdlen)
		return 1;

	dpd->FormFlag = dtoh8o(data, *offset);

	switch (dpd->FormFlag) {
	case PTP_DPFF_Range:
		ret = ptp_unpack_DPV (params, data, offset, dpdlen, &dpd->FORM.Range.MinValue, dpd->DataType);
		if (!ret) goto outofmemory;
		ret = ptp_unpack_DPV (params, data, offset, dpdlen, &dpd->FORM.Range.MaxValue, dpd->DataType);
		if (!ret) goto outofmemory;
		ret = ptp_unpack_DPV (params, data, offset, dpdlen, &dpd->FORM.Range.StepSize, dpd->DataType);
		if (!ret) goto outofmemory;
		break;
	case PTP_DPFF_Enumeration: {
		int i;
#define N	dpd->FORM.Enum.NumberOfValues

		if (*offset + sizeof(uint16_t) > dpdlen) goto outofmemory;

		N = dtoh16o(data, *offset);
		dpd->FORM.Enum.SupportedValue = calloc(N,sizeof(dpd->FORM.Enum.SupportedValue[0]));
		if (!dpd->FORM.Enum.SupportedValue)
			goto outofmemory;

		for (i=0;i<N;i++) {
			ret = ptp_unpack_DPV (params, data, offset, dpdlen, &dpd->FORM.Enum.SupportedValue[i], dpd->DataType);

			/* Slightly different handling here. The HP PhotoSmart 120
			 * specifies an enumeration with N in wrong endian
			 * 00 01 instead of 01 00, so we count the enum just until the
			 * the end of the packet.
			 */
			if (!ret) {
				if (!i)
					goto outofmemory;
				dpd->FORM.Enum.NumberOfValues = i;
				break;
			}
		}
		}
	}
#undef N
	return 1;
outofmemory:
	ptp_free_devicepropdesc(dpd);
	return 0;
}

/* Device Property pack/unpack */
#define PTP_dpd_Sony_DevicePropCode		0
#define PTP_dpd_Sony_DataType			2
#define PTP_dpd_Sony_GetSet		4
#define PTP_dpd_Sony_IsEnabled			5
#define PTP_dpd_Sony_DefaultValue		6
	/* PTP_dpd_SonyCurrentValue 		6 + sizeof(DataType) */

static inline int
ptp_unpack_Sony_DPD (PTPParams *params, const unsigned char* data, PTPDevicePropDesc *dpd, unsigned int dpdlen, unsigned int *poffset)
{
	unsigned int ret;
	unsigned int isenabled;

	if (!data || dpdlen < PTP_dpd_Sony_DefaultValue)
		return 0;

	memset (dpd, 0, sizeof(*dpd));
	dpd->DevicePropCode = dtoh16a(data + PTP_dpd_Sony_DevicePropCode);
	dpd->DataType       = dtoh16a(data + PTP_dpd_Sony_DataType);
	dpd->GetSet         = dtoh8a (data + PTP_dpd_Sony_GetSet);
	isenabled           = dtoh8a (data + PTP_dpd_Sony_IsEnabled);

	ptp_debug (params, "prop 0x%04x, datatype 0x%04x, isEnabled %d getset %d", dpd->DevicePropCode, dpd->DataType, isenabled, dpd->GetSet);

	switch (isenabled) {
	case 0: /* grayed out */
		dpd->GetSet = 0;	/* just to be safe */
		break;
	case 1: /* enabled */
		break;
	case 2: /* display only */
		dpd->GetSet = 0;	/* just to be safe */
		break;
	}

	dpd->FormFlag=PTP_DPFF_None;

	*poffset = PTP_dpd_Sony_DefaultValue;
	ret = ptp_unpack_DPV (params, data, poffset, dpdlen, &dpd->DefaultValue, dpd->DataType);
	if (!ret) goto outofmemory;
	if ((dpd->DataType == PTP_DTC_STR) && (*poffset == dpdlen))
		return 1;
	ret = ptp_unpack_DPV (params, data, poffset, dpdlen, &dpd->CurrentValue, dpd->DataType);
	if (!ret) goto outofmemory;

	/* if offset==0 then Data Type format is not supported by this
	   code or the Data Type is a string (with two empty strings as
	   values). In both cases Form Flag should be set to 0x00 and FORM is
	   not present. */

	if (*poffset==PTP_dpd_Sony_DefaultValue)
		return 1;

	dpd->FormFlag = dtoh8o(data, *poffset);
	ptp_debug (params, "formflag 0x%04x", dpd->FormFlag);

	switch (dpd->FormFlag) {
	case PTP_DPFF_Range:
		ret = ptp_unpack_DPV (params, data, poffset, dpdlen, &dpd->FORM.Range.MinValue, dpd->DataType);
		if (!ret) goto outofmemory;
		ret = ptp_unpack_DPV (params, data, poffset, dpdlen, &dpd->FORM.Range.MaxValue, dpd->DataType);
		if (!ret) goto outofmemory;
		ret = ptp_unpack_DPV (params, data, poffset, dpdlen, &dpd->FORM.Range.StepSize, dpd->DataType);
		if (!ret) goto outofmemory;
		break;
	case PTP_DPFF_Enumeration: {
		int i;
#define N	dpd->FORM.Enum.NumberOfValues
		N = dtoh16o(data, *poffset);
		dpd->FORM.Enum.SupportedValue = calloc(N,sizeof(dpd->FORM.Enum.SupportedValue[0]));
		if (!dpd->FORM.Enum.SupportedValue)
			goto outofmemory;

		for (i=0;i<N;i++) {
			ret = ptp_unpack_DPV (params, data, poffset, dpdlen, &dpd->FORM.Enum.SupportedValue[i], dpd->DataType);

			/* Slightly different handling here. The HP PhotoSmart 120
			 * specifies an enumeration with N in wrong endian
			 * 00 01 instead of 01 00, so we count the enum just until the
			 * the end of the packet.
			 */
			if (!ret) {
				if (!i)
					goto outofmemory;
				dpd->FORM.Enum.NumberOfValues = i;
				break;
			}
		}
		}
	}
	if (dpdlen >= *poffset + 2) {
		uint16_t val = dtoh16a(data + *poffset);

		/* check if we have a secondary list of items, this is for newer Sonys (2024) */
		if (val < 0x200) {	/* actually would be 0x5XXX or 0xDxxx */
			if (dpd->FormFlag == PTP_DPFF_Enumeration) {
				int i;

				N = dtoh16o(data, *poffset);
				dpd->FORM.Enum.SupportedValue = calloc(N,sizeof(dpd->FORM.Enum.SupportedValue[0]));
				if (!dpd->FORM.Enum.SupportedValue)
					goto outofmemory;

				for (i=0;i<N;i++) {
					ret = ptp_unpack_DPV (params, data, poffset, dpdlen, &dpd->FORM.Enum.SupportedValue[i], dpd->DataType);

					/* Slightly different handling here. The HP PhotoSmart 120
					 * specifies an enumeration with N in wrong endian
					 * 00 01 instead of 01 00, so we count the enum just until the
					 * the end of the packet.
					 */
					if (!ret) {
						if (!i)
							goto outofmemory;
						dpd->FORM.Enum.NumberOfValues = i;
						break;
					}
				}
			} else {
				ptp_debug (params, "apparently not a enum, but also no value (formflags is %d, value is 0x%04x)", dpd->FormFlag, val);
			}
		}
	}
#undef N
	return 1;
outofmemory:
	ptp_free_devicepropdesc(dpd);
	return 0;
}

static inline void
duplicate_PropertyValue (const PTPPropValue *src, PTPPropValue *dst, uint16_t type) {
	if (type == PTP_DTC_STR) {
		if (src->str)
			dst->str = strdup(src->str);
		else
			dst->str = NULL;
		return;
	}

	if (type & PTP_DTC_ARRAY_MASK) {
		unsigned int i;

		dst->a.count = src->a.count;
		dst->a.v = calloc (src->a.count, sizeof(src->a.v[0]));
		for (i=0;i<src->a.count;i++)
			duplicate_PropertyValue (&src->a.v[i], &dst->a.v[i], type & ~PTP_DTC_ARRAY_MASK);
		return;
	}
	switch (type & ~PTP_DTC_ARRAY_MASK) {
	case PTP_DTC_INT8:	dst->i8 = src->i8; break;
	case PTP_DTC_UINT8:	dst->u8 = src->u8; break;
	case PTP_DTC_INT16:	dst->i16 = src->i16; break;
	case PTP_DTC_UINT16:	dst->u16 = src->u16; break;
	case PTP_DTC_INT32:	dst->i32 = src->i32; break;
	case PTP_DTC_UINT32:	dst->u32 = src->u32; break;
	case PTP_DTC_UINT64:	dst->u64 = src->u64; break;
	case PTP_DTC_INT64:	dst->i64 = src->i64; break;
#if 0
	case PTP_DTC_INT128:	dst->i128 = src->i128; break;
	case PTP_DTC_UINT128:	dst->u128 = src->u128; break;
#endif
	default:		break;
	}
	return;
}

static inline void
duplicate_DevicePropDesc(const PTPDevicePropDesc *src, PTPDevicePropDesc *dst) {
	int i;

	dst->DevicePropCode	= src->DevicePropCode;
	dst->DataType		= src->DataType;
	dst->GetSet		= src->GetSet;

	duplicate_PropertyValue (&src->DefaultValue, &dst->DefaultValue, src->DataType);
	duplicate_PropertyValue (&src->CurrentValue, &dst->CurrentValue, src->DataType);

	dst->FormFlag		= src->FormFlag;
	switch (src->FormFlag) {
	case PTP_DPFF_Range:
		duplicate_PropertyValue (&src->FORM.Range.MinValue, &dst->FORM.Range.MinValue, src->DataType);
		duplicate_PropertyValue (&src->FORM.Range.MaxValue, &dst->FORM.Range.MaxValue, src->DataType);
		duplicate_PropertyValue (&src->FORM.Range.StepSize,     &dst->FORM.Range.StepSize,     src->DataType);
		break;
	case PTP_DPFF_Enumeration:
		dst->FORM.Enum.NumberOfValues = src->FORM.Enum.NumberOfValues;
		dst->FORM.Enum.SupportedValue = calloc (src->FORM.Enum.NumberOfValues, sizeof(dst->FORM.Enum.SupportedValue[0]));
		for (i = 0; i<src->FORM.Enum.NumberOfValues ; i++)
			duplicate_PropertyValue (&src->FORM.Enum.SupportedValue[i], &dst->FORM.Enum.SupportedValue[i], src->DataType);
		break;
	case PTP_DPFF_None:
		break;
	}
}

#define PTP_opd_ObjectPropCode	0
#define PTP_opd_DataType	2
#define PTP_opd_GetSet		4
#define PTP_opd_DefaultValue	5

static inline int
ptp_unpack_OPD (PTPParams *params, const unsigned char* data, PTPObjectPropDesc *opd, unsigned int opdlen)
{
	unsigned int offset=0, ret;

	memset (opd, 0, sizeof(*opd));

	if (opdlen < 5)
		return 0;

	opd->ObjectPropCode = dtoh16a(data + PTP_opd_ObjectPropCode);
	opd->DataType       = dtoh16a(data + PTP_opd_DataType);
	opd->GetSet         = dtoh8a (data + PTP_opd_GetSet);

	offset = PTP_opd_DefaultValue;
	ret = ptp_unpack_DPV (params, data, &offset, opdlen, &opd->DefaultValue, opd->DataType);
	if (!ret) goto outofmemory;

	if (offset + sizeof(uint32_t) > opdlen) goto outofmemory;
	opd->GroupCode = dtoh32o(data, offset);

	if (offset + sizeof(uint8_t) > opdlen) goto outofmemory;
	opd->FormFlag = dtoh8o(data, offset);

	switch (opd->FormFlag) {
	case PTP_OPFF_Range:
		ret = ptp_unpack_DPV (params, data, &offset, opdlen, &opd->FORM.Range.MinValue, opd->DataType);
		if (!ret) goto outofmemory;
		ret = ptp_unpack_DPV (params, data, &offset, opdlen, &opd->FORM.Range.MaxValue, opd->DataType);
		if (!ret) goto outofmemory;
		ret = ptp_unpack_DPV (params, data, &offset, opdlen, &opd->FORM.Range.StepSize, opd->DataType);
		if (!ret) goto outofmemory;
		break;
	case PTP_OPFF_Enumeration: {
		unsigned int i;
#define N	opd->FORM.Enum.NumberOfValues

		if (offset + sizeof(uint16_t) > opdlen) goto outofmemory;
		N = dtoh16o(data, offset);

		opd->FORM.Enum.SupportedValue = calloc(N,sizeof(opd->FORM.Enum.SupportedValue[0]));
		if (!opd->FORM.Enum.SupportedValue)
			goto outofmemory;

		for (i=0;i<N;i++) {
			ret = ptp_unpack_DPV (params, data, &offset, opdlen, &opd->FORM.Enum.SupportedValue[i], opd->DataType);

			/* Slightly different handling here. The HP PhotoSmart 120
			 * specifies an enumeration with N in wrong endian
			 * 00 01 instead of 01 00, so we count the enum just until the
			 * the end of the packet.
			 */
			if (!ret) {
				if (!i)
					goto outofmemory;
				opd->FORM.Enum.NumberOfValues = i;
				break;
			}
		}
#undef N
		}
		break;
	case PTP_OPFF_DateTime:
		ptp_unpack_string(params, data, &offset, opdlen, &opd->FORM.DateTime.String);
		break;
	case PTP_OPFF_RegularExpression:
		ptp_unpack_string(params, data, &offset, opdlen, &opd->FORM.RegularExpression.String);
		break;
	case PTP_OPFF_FixedLengthArray:
		if (offset + sizeof(uint16_t) > opdlen) goto outofmemory;
		opd->FORM.FixedLengthArray.NumberOfValues = dtoh16o(data, offset);
		break;
	case PTP_OPFF_ByteArray:
		if (offset + sizeof(uint16_t) > opdlen) goto outofmemory;
		opd->FORM.ByteArray.NumberOfValues = dtoh16o(data, offset);
		break;
	case PTP_OPFF_LongString:
		break;
	}
	return 1;
outofmemory:
	ptp_free_objectpropdesc(opd);
	return 0;
}

static inline uint32_t
ptp_pack_DPV (PTPParams *params, PTPPropValue* value, unsigned char** dpvptr, uint16_t datatype)
{
	unsigned char* dpv=NULL;
	uint32_t size=0;
	unsigned int i;

	switch (datatype) {
	case PTP_DTC_INT8:
		size=sizeof(int8_t);
		dpv=malloc(size);
		htod8a(dpv,value->i8);
		break;
	case PTP_DTC_UINT8:
		size=sizeof(uint8_t);
		dpv=malloc(size);
		htod8a(dpv,value->u8);
		break;
	case PTP_DTC_INT16:
		size=sizeof(int16_t);
		dpv=malloc(size);
		htod16a(dpv,value->i16);
		break;
	case PTP_DTC_UINT16:
		size=sizeof(uint16_t);
		dpv=malloc(size);
		htod16a(dpv,value->u16);
		break;
	case PTP_DTC_INT32:
		size=sizeof(int32_t);
		dpv=malloc(size);
		htod32a(dpv,value->i32);
		break;
	case PTP_DTC_UINT32:
		size=sizeof(uint32_t);
		dpv=malloc(size);
		htod32a(dpv,value->u32);
		break;
	case PTP_DTC_INT64:
		size=sizeof(int64_t);
		dpv=malloc(size);
		htod64a(dpv,value->i64);
		break;
	case PTP_DTC_UINT64:
		size=sizeof(uint64_t);
		dpv=malloc(size);
		htod64a(dpv,value->u64);
		break;
	case PTP_DTC_AUINT8:
		size=sizeof(uint32_t)+value->a.count*sizeof(uint8_t);
		dpv=malloc(size);
		htod32a(dpv,value->a.count);
		for (i=0;i<value->a.count;i++)
			htod8a(&dpv[sizeof(uint32_t)+i*sizeof(uint8_t)],value->a.v[i].u8);
		break;
	case PTP_DTC_AINT8:
		size=sizeof(uint32_t)+value->a.count*sizeof(int8_t);
		dpv=malloc(size);
		htod32a(dpv,value->a.count);
		for (i=0;i<value->a.count;i++)
			htod8a(&dpv[sizeof(uint32_t)+i*sizeof(int8_t)],value->a.v[i].i8);
		break;
	case PTP_DTC_AUINT16:
		size=sizeof(uint32_t)+value->a.count*sizeof(uint16_t);
		dpv=malloc(size);
		htod32a(dpv,value->a.count);
		for (i=0;i<value->a.count;i++)
			htod16a(&dpv[sizeof(uint32_t)+i*sizeof(uint16_t)],value->a.v[i].u16);
		break;
	case PTP_DTC_AINT16:
		size=sizeof(uint32_t)+value->a.count*sizeof(int16_t);
		dpv=malloc(size);
		htod32a(dpv,value->a.count);
		for (i=0;i<value->a.count;i++)
			htod16a(&dpv[sizeof(uint32_t)+i*sizeof(int16_t)],value->a.v[i].i16);
		break;
	case PTP_DTC_AUINT32:
		size=sizeof(uint32_t)+value->a.count*sizeof(uint32_t);
		dpv=malloc(size);
		htod32a(dpv,value->a.count);
		for (i=0;i<value->a.count;i++)
			htod32a(&dpv[sizeof(uint32_t)+i*sizeof(uint32_t)],value->a.v[i].u32);
		break;
	case PTP_DTC_AINT32:
		size=sizeof(uint32_t)+value->a.count*sizeof(int32_t);
		dpv=malloc(size);
		htod32a(dpv,value->a.count);
		for (i=0;i<value->a.count;i++)
			htod32a(&dpv[sizeof(uint32_t)+i*sizeof(int32_t)],value->a.v[i].i32);
		break;
	case PTP_DTC_AUINT64:
		size=sizeof(uint32_t)+value->a.count*sizeof(uint64_t);
		dpv=malloc(size);
		htod32a(dpv,value->a.count);
		for (i=0;i<value->a.count;i++)
			htod64a(&dpv[sizeof(uint32_t)+i*sizeof(uint64_t)],value->a.v[i].u64);
		break;
	case PTP_DTC_AINT64:
		size=sizeof(uint32_t)+value->a.count*sizeof(int64_t);
		dpv=malloc(size);
		htod32a(dpv,value->a.count);
		for (i=0;i<value->a.count;i++)
			htod64a(&dpv[sizeof(uint32_t)+i*sizeof(int64_t)],value->a.v[i].i64);
		break;
	/* XXX: other int types are unimplemented */
	case PTP_DTC_STR: {
		dpv=ptp_get_packed_stringcopy(params, value->str, &size);
		break;
	}
	}
	*dpvptr=dpv;
	return size;
}

#define MAX_MTP_PROPS 127
static inline uint32_t
ptp_pack_OPL (PTPParams *params, MTPProperties *props, int nrofprops, unsigned char** opldataptr)
{
	unsigned char* opldata;
	MTPProperties *propitr;
	unsigned char *packedprops[MAX_MTP_PROPS];
	uint32_t packedpropslens[MAX_MTP_PROPS];
	uint32_t packedobjecthandles[MAX_MTP_PROPS];
	uint16_t packedpropsids[MAX_MTP_PROPS];
	uint16_t packedpropstypes[MAX_MTP_PROPS];
	uint32_t totalsize = 0;
	uint32_t bufp = 0;
	uint32_t noitems = 0;
	uint32_t i;

	totalsize = sizeof(uint32_t); /* 4 bytes to store the number of elements */
	propitr = props;
	while (nrofprops-- && noitems < MAX_MTP_PROPS) {
		/* Object Handle */
		packedobjecthandles[noitems]=propitr->ObjectHandle;
		totalsize += sizeof(uint32_t); /* Object ID */
		/* Metadata type */
		packedpropsids[noitems]=propitr->property;
		totalsize += sizeof(uint16_t);
		/* Data type */
		packedpropstypes[noitems]= propitr->datatype;
		totalsize += sizeof(uint16_t);
		/* Add each property to be sent. */
		packedpropslens[noitems] = ptp_pack_DPV (params, &propitr->propval, &packedprops[noitems], propitr->datatype);
		totalsize += packedpropslens[noitems];
		noitems ++;
		propitr ++;
	}

	/* Allocate memory for the packed property list */
	opldata = malloc(totalsize);

	htod32a(&opldata[bufp],noitems);
	bufp += 4;

	/* Copy into a nice packed list */
	for (i = 0; i < noitems; i++) {
		/* Object ID */
		htod32a(&opldata[bufp],packedobjecthandles[i]);
		bufp += sizeof(uint32_t);
		htod16a(&opldata[bufp],packedpropsids[i]);
		bufp += sizeof(uint16_t);
		htod16a(&opldata[bufp],packedpropstypes[i]);
		bufp += sizeof(uint16_t);
		/* The copy the actual property */
		memcpy(&opldata[bufp], packedprops[i], packedpropslens[i]);
		bufp += packedpropslens[i];
		free(packedprops[i]);
	}
	*opldataptr = opldata;
	return totalsize;
}

static int
_compare_func(const void* x, const void *y) {
	const MTPProperties *px = x;
	const MTPProperties *py = y;

	return px->ObjectHandle - py->ObjectHandle;
}

static inline int
ptp_unpack_OPL (PTPParams *params, const unsigned char* data, MTPProperties **pprops, unsigned int len)
{
	uint32_t prop_count;
	MTPProperties *props = NULL;
	unsigned int offset = 0, i;

	if (len < sizeof(uint32_t)) {
		ptp_debug (params ,"must have at least 4 bytes data, not %d", len);
		return 0;
	}

	prop_count = dtoh32o(data, offset);
	*pprops = NULL;
	if (prop_count == 0)
		return 0;

	if (prop_count >= INT_MAX/sizeof(MTPProperties)) {
		ptp_debug (params ,"prop_count %d is too large", prop_count);
		return 0;
	}
	ptp_debug (params ,"Unpacking MTP OPL, size %d (prop_count %d)", len, prop_count);

	props = calloc(prop_count , sizeof(MTPProperties));
	if (!props) return 0;
	for (i = 0; i < prop_count; i++) {
		if (len <= offset + 4 + 2 + 2) {
			ptp_debug (params ,"short MTP Object Property List at property %d (of %d)", i, prop_count);
			ptp_debug (params ,"device probably needs DEVICE_FLAG_BROKEN_MTPGETOBJPROPLIST_ALL");
			ptp_debug (params ,"or even DEVICE_FLAG_BROKEN_MTPGETOBJPROPLIST");
			qsort (props, i, sizeof(MTPProperties),_compare_func);
			*pprops = props;
			return i;
		}


		props[i].ObjectHandle = dtoh32o(data, offset);
		props[i].property     = dtoh16o(data, offset);
		props[i].datatype     = dtoh16o(data, offset);

		if (!ptp_unpack_DPV(params, data, &offset, len, &props[i].propval, props[i].datatype)) {
			ptp_debug (params ,"unpacking DPV of property %d encountered insufficient buffer. attack?", i);
			qsort (props, i, sizeof(MTPProperties),_compare_func);
			*pprops = props;
			return i;
		}
	}
	qsort (props, prop_count, sizeof(MTPProperties),_compare_func);
	*pprops = props;
	return prop_count;
}

/*
    PTP USB Event container unpack
    Copyright (c) 2003 Nikolai Kopanygin
*/

#define PTP_ec_Length		0
#define PTP_ec_Type		4
#define PTP_ec_Code		6
#define PTP_ec_TransId		8
#define PTP_ec_Param1		12
#define PTP_ec_Param2		16
#define PTP_ec_Param3		20

static inline void
ptp_unpack_canon_event (PTPParams *params, const unsigned char* data, PTPContainer *ec, unsigned int len)
{
	unsigned int	length;
	int	type;

	if (data==NULL)
		return;
	memset(ec,0,sizeof(*ec));

	length = dtoh32a(data + PTP_ec_Length);
	if (length > len) {
		ptp_debug (params, "length %d in container, but data only %d bytes?!", length, len);
		return;
	}
	type = dtoh16a(data + PTP_ec_Type);

	ec->Code           = dtoh16a(data + PTP_ec_Code);
	ec->Transaction_ID = dtoh32a(data + PTP_ec_TransId);

	if (type!=PTP_USB_CONTAINER_EVENT) {
		ptp_debug (params, "Unknown canon event type %d (code=%x,tid=%x), please report!",type,ec->Code,ec->Transaction_ID);
		return;
	}
	if (length>=(PTP_ec_Param1+4)) {
		ec->Param1 = dtoh32a(data + PTP_ec_Param1);
		ec->Nparam = 1;
	}
	if (length>=(PTP_ec_Param2+4)) {
		ec->Param2 = dtoh32a(data + PTP_ec_Param2);
		ec->Nparam = 2;
	}
	if (length>=(PTP_ec_Param3+4)) {
		ec->Param3 = dtoh32a(data + PTP_ec_Param3);
		ec->Nparam = 3;
	}
}

/*
    PTP Canon Folder Entry unpack
    Copyright (c) 2003 Nikolai Kopanygin
*/
#define PTP_cfe_ObjectHandle		0
#define PTP_cfe_ObjectFormatCode	4
#define PTP_cfe_Flags			6
#define PTP_cfe_ObjectSize		7
#define PTP_cfe_Time			11
#define PTP_cfe_Filename		15

static inline void
ptp_unpack_Canon_FE (PTPParams *params, const unsigned char* data, PTPCANONFolderEntry *fe)
{
	if (data==NULL)
		return;
	fe->ObjectHandle     = dtoh32a(data + PTP_cfe_ObjectHandle);
	fe->ObjectFormatCode = dtoh16a(data + PTP_cfe_ObjectFormatCode);
	fe->Flags            = dtoh8a (data + PTP_cfe_Flags);
	fe->ObjectSize       = dtoh32a(data + PTP_cfe_ObjectSize);
	fe->Time     = (time_t)dtoh32a(data + PTP_cfe_Time);
	strncpy(fe->Filename, (char*)data + PTP_cfe_Filename, PTP_CANON_FilenameBufferLen);
}

/*
    PTP Canon EOS Folder Entry unpack
0: 00 00 08 a0     objectid
4: 01 00 02 00     storageid
8: 01 30 00 00     ofc
12: 01 00
14: 00 00
16: 11 00 00 00
20: 00 00 00 00
24: 00 00 00 80
28: 00 00 08 a0
32: 4d 49 53 43-00 00 00 00 00 00 00 00     name
00 00 00 00
84 bc 74 46     objectime


(normal PTP GetObjectInfo)
ObjectInfo for 'IMG_0199.JPG':
  Object ID: 0x92740c72
  StorageID: 0x00020001
  ObjectFormat: 0x3801
  ProtectionStatus: 0x0000
  ObjectSize: 2217241
  ThumbFormat: 0x3808
  ThumbSize: 5122
  ThumbPixWidth: 160
  ThumbPixHeight: 120
  ImagePixWidth: 4000
  ImagePixHeight: 3000
  ImageBitDepth: 24
  ParentObject: 0x92740000
  AssociationType: 0x0000
  AssociationDesc: 0x00000000
  SequenceNumber: 0x00000000
  ModificationDate: 0x4d985ff0
  CaptureDate: 0x4d985ff0

0010  38 00 00 00  Size of this entry
0014  72 0c 74 92  OID
0018  01 00 02 00  StorageID
001c  01 38 00 00  OFC
0020  00 00 00 00 ??
0024  21 00 00 00  flags (4 bytes? 1 byte?)
0028  19 d5 21 00  Size
002c  00 00 74 92  ?
0030  70 0c 74 92  OID
0034  49 4d 47 5f-30 31 39 39 2e 4a 50 47  IMG_0199.JPG
0040  00 00 00 00
0044  10 7c 98 4d Time


*/
#define PTP_cefe_ObjectHandle		0
#define PTP_cefe_StorageID		4
#define PTP_cefe_ObjectFormatCode	8
#define PTP_cefe_Flags			16
#define PTP_cefe_ObjectSize		20
#define PTP_cefe_Filename		32
#define PTP_cefe_Time			48

static inline void
ptp_unpack_Canon_EOS_FE (PTPParams *params, const unsigned char* data, unsigned int size, PTPCANONFolderEntry *fe)
{
	if (size < PTP_cefe_Time + 4) return;

	fe->ObjectHandle     = dtoh32a(data + PTP_cefe_ObjectHandle);
	fe->ObjectFormatCode = dtoh16a(data + PTP_cefe_ObjectFormatCode);
	fe->Flags            = dtoh8a (data + PTP_cefe_Flags);
	fe->ObjectSize       = dtoh32a(data + PTP_cefe_ObjectSize);
	fe->Time     = (time_t)dtoh32a(data + PTP_cefe_Time);

	strncpy(fe->Filename, (char*)data + PTP_cefe_Filename, PTP_CANON_FilenameBufferLen);
	fe->Filename[PTP_CANON_FilenameBufferLen-1] = 0;
}


static inline uint16_t
ptp_unpack_EOS_ImageFormat (PTPParams* params, const unsigned char** data )
{
	/*
	  EOS ImageFormat entries look are a sequence of u32 values:
		0: number of entries / generated files (1 or 2)
		1: size of this entry in bytes (most likely always 0x10 = 4 x u32)
		2: image type:
			 1 == JPG
			 6 == RAW
		3: image size:
			 0 == L
			 1 == M
			 2 == S
			 5 == M1      (e.g. 5Ds)
			 6 == M2
			 e == S1      (e.g. 5Dm3)
			 f == S2
			10 == S3
		4: image compression:
			 0 == user:      JPG       (e.g. 1DX, R5m2)
			 1 == ???:       JPG       (e.g. 1DXm2, 1DXm3)
			 2 == coarse:    JPG       (all)
			 3 == fine:      JPG/cRAW  (all)
			 4 == lossless:  RAW       (all)

	  If the number of entries is 2 the values 1-4 repeat

	  example (cRAW + coarse S1 JPEG): 2 10 6 0 3 10 1 e 2

	  The idea is to simply 'condense' these values to just one uint16 to be able to conveniently
	  use the available enumeration facilities (look-up table).
	  Hence we generate a u16 value with the four nibles set as follows:

	  entry 1 size | entry 1 type + compression | entry 2 size | entry 2 type + compression.

	  * The S3 value (0xf) would overflow the nible, hence we decrease all S1,S2,S3 values by 1.
	  * The to encode the type RAW, we set the 4th bit in the compression nible to 1 (|= 8).
	  * To distinguish an "empty" second entry from the "custom L JPEG", we set it to 0xff.

	  The above example would result in the value 0x0bd2.
	*/

	const uint8_t* d = *data;
	uint32_t offset = 0;
	uint32_t n = dtoh32o (d, offset);
	uint32_t l, t1, s1, c1, t2 = 0, s2 = 0, c2 = 0;

	if (n != 1 && n !=2) {
		ptp_debug (params, "parsing EOS ImageFormat property failed (n != 1 && n != 2: %d)", n);
		return 0;
	}

	l = dtoh32o (d, offset);
	if (l != 0x10) {
		ptp_debug (params, "parsing EOS ImageFormat property failed (l != 0x10: 0x%x)", l);
		return 0;
	}

	t1 = dtoh32o (d, offset);
	s1 = dtoh32o (d, offset);
	c1 = dtoh32o (d, offset);

	if (n == 2) {
		l = dtoh32o (d, offset);
		if (l != 0x10) {
			ptp_debug (params, "parsing EOS ImageFormat property failed (l != 0x10: 0x%x)", l);
			return 0;
		}
		t2 = dtoh32o (d, offset);
		s2 = dtoh32o (d, offset);
		c2 = dtoh32o (d, offset);
	}

	*data += offset;

	/* deal with S1/S2/S3 JPEG sizes, see above. */
	if( s1 >= 0xe )
		s1--;
	if( s2 >= 0xe )
		s2--;

	/* encode RAW flag */
	c1 |= (t1 == 6) ? 8 : 0;
	c2 |= (t2 == 6) ? 8 : 0;

	if (s2 == 0 && c2 == 0)
		s2 = c2 = 0xF;

	return ((s1 & 0xF) << 12) | ((c1 & 0xF) << 8) | ((s2 & 0xF) << 4) | ((c2 & 0xF) << 0);
}

static inline uint32_t
ptp_pack_EOS_ImageFormat (PTPParams* params, unsigned char* data, uint16_t value)
{
	uint32_t n = (value & 0xFF) == 0xFF ? 1 : 2;
	uint32_t s = 4 + 0x10 * n;

	if( !data )
		return s;

#define PACK_EOS_S123_JPEG_SIZE( X ) (X) >= 0xd ? (X)+1 : (X)

	htod32a(data+=0, n);

	htod32a(data+=4, 0x10);
	htod32a(data+=4, value & 0x0800 ? 6 : 1);
	htod32a(data+=4, PACK_EOS_S123_JPEG_SIZE((value >> 12) & 0xF));
	htod32a(data+=4, (value >> 8) & 0x7);

	if (n==2) {
		htod32a(data+=4, 0x10);
		htod32a(data+=4, value & 0x08 ? 6 : 1);
		htod32a(data+=4, PACK_EOS_S123_JPEG_SIZE((value >> 4) & 0xF));
		htod32a(data+=4, (value >> 0) & 0x7);
	}

#undef PACK_EOS_S123_JPEG_SIZE

	return s;
}

/* 32 bit size
 * 16 bit subsize
 * 16 bit version (?)
 * 16 bit focus_points_in_struct
 * 16 bit focus_points_in_use
 * variable arrays:
 * 	16 bit sizex, 16 bit sizey
 * 	16 bit othersizex, 16 bit othersizey
 * 	16 bit array height[focus_points_in_struct]
 * 	16 bit array width[focus_points_in_struct]
 * 	16 bit array offsetheight[focus_points_in_struct] middle is 0
 * 	16 bit array offsetwidth[focus_points_in_struct] middle is ?
 * bitfield of selected focus points, starting with 0 [size focus_points_in_struct in bits]
 * unknown stuff, likely which are active
 * 16 bit 0xffff
 *
 * size=NxN,size2=NxN,points={NxNxNxN,NxNxNxN,...},selected={0,1,2}
 */
static inline char*
ptp_unpack_EOS_FocusInfoEx (PTPParams* params, const unsigned char** data, uint32_t datasize)
{
	uint32_t size 			= dtoh32a( *data );
	uint32_t halfsize		= dtoh16a( (*data) + 4);
	uint32_t version		= dtoh16a( (*data) + 6);
	uint32_t focus_points_in_struct	= dtoh16a( (*data) + 8);
	uint32_t focus_points_in_use	= dtoh16a( (*data) + 10);
	uint32_t sizeX			= dtoh16a( (*data) + 12);
	uint32_t sizeY			= dtoh16a( (*data) + 14);
	uint32_t size2X			= dtoh16a( (*data) + 16);
	uint32_t size2Y			= dtoh16a( (*data) + 18);
	uint32_t i;
	uint32_t maxlen;
	char	*str, *p;

	if ((size > datasize) || (size < 20)) {
		ptp_error(params, "FocusInfoEx has invalid size (%d) vs datasize (%d)", size, datasize);
		return strdup("bad size 1");
	}
	/* If data is zero-filled, then it is just a placeholder, so nothing
	   useful, but also not an error */
	if (!focus_points_in_struct || !focus_points_in_use) {
		ptp_debug(params, "skipped FocusInfoEx data (zero filled)");
		return strdup("no focus points returned by camera");
	}
	if (size < focus_points_in_struct*8) {
		ptp_error(params, "focus_points_in_struct %d is too large vs size %d", focus_points_in_struct, size);
		return strdup("bad size 2");
	}
	if (focus_points_in_use > focus_points_in_struct) {
		ptp_error(params, "focus_points_in_use %d is larger than focus_points_in_struct %d", focus_points_in_use, focus_points_in_struct);
		return strdup("bad size 3");
	}
	if (halfsize != size-4) {
		ptp_debug(params, "halfsize %d is not expected %d", halfsize, size-4);
	}

	if (20 + focus_points_in_struct*8 + (focus_points_in_struct+7)/8 > size) {
		ptp_error(params, "size %d is too large for fp in struct %d", focus_points_in_struct*8 + 20 + (focus_points_in_struct+7)/8, size);
		return strdup("bad size 5");
	}

	ptp_debug(params,"                prop d1d3 version is %d with %d focus points in struct and %d in use, size=%ux%u, size2=%ux%u",
	          version, focus_points_in_struct, focus_points_in_use, sizeX, sizeY, size2X, size2Y);
#if 0
	ptp_debug_data(params, *data, datasize);
#endif

	/* every selected focus_point gets an entry like "{N,N,N,N}," where N can be 5 chars long */
	maxlen = 1 + focus_points_in_use * 26 + 2;
	str = (char*)malloc( maxlen );
	if (!str)
		return NULL;
	p = str;

	/* output only the selected AF-points, so no AF means you get an empty list: "{}" */
	p += sprintf(p,"{");
	for (i=0;i<focus_points_in_use;i++) {
		if (((1<<(i%8)) & (*data)[focus_points_in_struct*8+20+i/8]) == 0)
			continue;
		int16_t x = dtoh16a((*data) + focus_points_in_struct*4 + 20 + 2*i);
		int16_t y = dtoh16a((*data) + focus_points_in_struct*6 + 20 + 2*i);
		int16_t w = dtoh16a((*data) + focus_points_in_struct*2 + 20 + 2*i);
		int16_t h = dtoh16a((*data) + focus_points_in_struct*0 + 20 + 2*i);

		int n = snprintf(p, maxlen - (p - str), "{%d,%d,%d,%d},", x, y, w, h);
		if (n < 0 || n > maxlen - (p - str)) {
			ptp_error(params, "snprintf buffer overflow in %s", __func__);
			break;
		}
		p += n;
	}
	if (p[-1] == ',')
		p--;
	p += sprintf(p, "}");
	return str;
}


static inline char*
ptp_unpack_EOS_CustomFuncEx (PTPParams* params, const unsigned char** data )
{
	uint32_t s = dtoh32a( *data );
	uint32_t n = s/4, i;
	char	*str, *p;

	if (s > 1024) {
		ptp_debug (params, "customfuncex data is larger than 1k / %d... unexpected?", s);
		return strdup("bad length");
	}
	str = (char*)malloc( s*2+s/4+1 ); /* n is size in uint32, maximum %x len is 8 chars and \0*/
	if (!str)
		return strdup("malloc failed");

	p = str;
	for (i=0; i < n; ++i)
		p += sprintf(p, "%x,", dtoh32a( *data + 4*i ));
	return str;
}

static inline uint32_t
ptp_pack_EOS_CustomFuncEx (PTPParams* params, unsigned char* data, char* str)
{
	uint32_t s = strtoul(str, NULL, 16);
	uint32_t n = s/4, i, v;

	if (!data)
		return s;

	for (i=0; i<n; i++)
	{
		v = strtoul(str, &str, 16);
		str++; /* skip the ',' delimiter */
		htod32a(data + i*4, v);
	}

	return s;
}

/*
    PTP EOS Event unpack
*/
#define PTP_cee_Size		0x00
#define PTP_cee_Code		0x04

#define PTP_cee_DPC	0x08
#define PTP_cee_Prop_Val_Data	0x0c
#define PTP_cee_DPD_Type	0x0c
#define PTP_cee_DPD_Count	0x10
#define PTP_cee_DPD_Data	0x14

/* for PTP_EC_CANON_EOS_RequestObjectTransfer */
#define PTP_cee_OI_ObjectID	0x08
#define PTP_cee_OI_OFC		0x0c
#define PTP_cee_OI_Size		0x14
#define PTP_cee_OI_Name		0x1c

/* for PTP_EC_CANON_EOS_ObjectAddedEx */
#define PTP_cee_OA_ObjectID	0x08
#define PTP_cee_OA_StorageID	0x0c
#define PTP_cee_OA_OFC		0x10
#define PTP_cee_OA_Size		0x1c
#define PTP_cee_OA_Parent	0x20
#define PTP_cee_OA_Name		0x28

#define PTP_cee_OA64_ObjectID	0x08	/* OK */
#define PTP_cee_OA64_StorageID	0x0c	/* OK */
#define PTP_cee_OA64_OFC	0x10	/* OK */
#define PTP_cee_OA64_Size	0x1c	/* OK, might be 64 bit now? */
#define PTP_cee_OA64_Parent	0x24
#define PTP_cee_OA64_2ndOID	0x28
#define PTP_cee_OA64_Name	0x2c	/* OK */

/* for PTP_EC_CANON_EOS_ObjectAddedNew */
#define PTP_cee_OAN_OFC		0x0c
#define PTP_cee_OAN_Size	0x14

static inline PTPDevicePropDesc*
ptp_find_eos_devicepropdesc(PTPParams *params, uint32_t dpc)
{
	for (unsigned j=0; j < params->nrofcanon_props; j++)
		if (params->canon_props[j].dpd.DevicePropCode == dpc)
			return &params->canon_props[j].dpd;
	return NULL;
}

static PTPDevicePropDesc*
_lookup_or_allocate_canon_prop(PTPParams *params, uint32_t dpc)
{
	PTPDevicePropDesc *dpd = ptp_find_eos_devicepropdesc(params, dpc);

	if (dpd)
		return dpd;

	unsigned j = params->nrofcanon_props;
	params->canon_props = realloc(params->canon_props, sizeof(params->canon_props[0])*(j+1));
	memset (&params->canon_props[j].dpd,0,sizeof(params->canon_props[j].dpd));
	params->canon_props[j].dpd.DevicePropCode = dpc;
	params->canon_props[j].dpd.GetSet = 1;
	params->canon_props[j].dpd.FormFlag = PTP_DPFF_None;
	params->nrofcanon_props = j+1;
	return &params->canon_props[j].dpd;
}

#define PTP_CANON_SET_INFO( ENTRY, MSG, ...) \
	do {						\
		int c = snprintf(ENTRY.u.info, sizeof(ENTRY.u.info), MSG, ##__VA_ARGS__);		\
		if (c > (int)sizeof(ENTRY.u.info))							\
			ptp_debug(params, "buffer overflow in PTP_CANON_SET_INFO, complete msg is: "	\
					MSG, ##__VA_ARGS__);						\
	} while (0)

static inline int
ptp_unpack_EOS_events (PTPParams *params, const unsigned char* data, unsigned int datasize, PTPCanonEOSEvent **events)
{
	int	i = 0, event_count = 0;
	const unsigned char *curdata = data;
	char prefix[18 + 12] = { 0 }; /* strlen("event 123 (c1xx):") + 12 bytes to silence snprintf warning */

	if (data==NULL)
		return 0;
	while (curdata - data + 8 < datasize) {
		uint32_t size = dtoh32a(curdata + PTP_cee_Size);
		uint32_t ec   = dtoh32a(curdata + PTP_cee_Code);

		if (size > datasize) {
			ptp_debug (params, "size %d is larger than datasize %d", size, datasize);
			break;
		}
		if (size < 8) {
			ptp_debug (params, "size %d is smaller than 8", size);
			break;
		}
		if ((size == 8) && (ec == 0))
			break;
		if ((curdata - data) + size >= datasize) {
			ptp_debug (params, "canon eos event decoder ran over supplied data, skipping entries");
			break;
		}
		if (ec == PTP_EC_CANON_EOS_OLCInfoChanged) {
			unsigned int j;

			if (size >= 12+2) {
				for (j=0;j<31;j++)
					if (dtoh16a(curdata+12) & (1<<j))
						event_count++;
				event_count--;  /* account for the event_count++ at the end of the outer loop */
			}
		}
		curdata += size;
		event_count++;
	}

	PTPCanonEOSEvent *e = calloc (event_count + 1, sizeof(PTPCanonEOSEvent));
	if (!e) return 0;

	curdata = data;
	while (curdata - data  + 8 < datasize) {
		uint32_t size = dtoh32a(curdata + PTP_cee_Size);
		uint32_t ec   = dtoh32a(curdata + PTP_cee_Code);

		if (size > datasize) {
			ptp_debug (params, "size %d is larger than datasize %d", size, datasize);
			break;
		}
		if (size < 8) {
			ptp_debug (params, "size %d is smaller than 8", size);
			break;
		}

		if ((size == 8) && (ec == 0))
			break;

		if ((curdata - data) + size >= datasize) {
			ptp_debug (params, "canon eos event decoder ran over supplied data, skipping entries");
			break;
		}

		snprintf(prefix, sizeof(prefix), "event %3d:%04x:", i, ec);
		#define INDENT "                "

		e[i].type = PTP_EOSEvent_Unknown;
		e[i].u.info[0] = 0;
		switch (ec) {
		case PTP_EC_CANON_EOS_ObjectContentChanged:
			if (size < PTP_cee_OA_ObjectID+1) {
				ptp_debug (params, "%s size %d is smaller than %d", prefix, size, PTP_cee_OA_ObjectID+1);
				break;
			}
			e[i].type = PTP_EOSEvent_ObjectContentChanged;
			e[i].u.object.oid = dtoh32a(curdata + PTP_cee_OA_ObjectID);
			break;
		case PTP_EC_CANON_EOS_ObjectInfoChangedEx:
		case PTP_EC_CANON_EOS_ObjectAddedEx:
			if (size < PTP_cee_OA_Name+1) {
				ptp_debug (params, "%s size %d is smaller than %d", prefix, size, PTP_cee_OA_Name+1);
				break;
			}
			e[i].type = ((ec == PTP_EC_CANON_EOS_ObjectAddedEx) ? PTP_EOSEvent_ObjectAdded : PTP_EOSEvent_ObjectInfoChanged);
			e[i].u.object.oid              = dtoh32a(curdata + PTP_cee_OA_ObjectID);
			e[i].u.object.oi.StorageID     = dtoh32a(curdata + PTP_cee_OA_StorageID);
			e[i].u.object.oi.ParentObject  = dtoh32a(curdata + PTP_cee_OA_Parent);
			e[i].u.object.oi.ObjectFormat  = dtoh16a(curdata + PTP_cee_OA_OFC);
			e[i].u.object.oi.ObjectSize    = dtoh32a(curdata + PTP_cee_OA_Size);
			e[i].u.object.oi.Filename      = strdup(((char*)curdata + PTP_cee_OA_Name));

			ptp_debug (params, "%s objectinfo %s oid %08x, parent %08x, ofc %04x, size %ld, filename %s",
			           prefix, ec == PTP_EC_CANON_EOS_ObjectAddedEx ? "added" : "changed",
			           e[i].u.object.oid, e[i].u.object.oi.ParentObject, e[i].u.object.oi.ObjectFormat,
			           e[i].u.object.oi.ObjectSize, e[i].u.object.oi.Filename);
			break;
		case PTP_EC_CANON_EOS_ObjectAddedEx64:	/* FIXME: review if the data used is correct */
			if (size < PTP_cee_OA64_Name+1) {
				ptp_debug (params, "%s size %d is smaller than %d", prefix, size, PTP_cee_OA64_Name+1);
				break;
			}
			e[i].type = PTP_EOSEvent_ObjectAdded;
			e[i].u.object.oid              = dtoh32a(curdata + PTP_cee_OA64_ObjectID);
			e[i].u.object.oi.StorageID     = dtoh32a(curdata + PTP_cee_OA64_StorageID);
			e[i].u.object.oi.ParentObject  = dtoh32a(curdata + PTP_cee_OA64_Parent);
			e[i].u.object.oi.ObjectFormat  = dtoh16a(curdata + PTP_cee_OA64_OFC);
			e[i].u.object.oi.ObjectSize    = dtoh32a(curdata + PTP_cee_OA64_Size);	/* FIXME: might be 64bit now */
			e[i].u.object.oi.Filename      = strdup(((char*)curdata + PTP_cee_OA64_Name));
			ptp_debug (params, "%s objectinfo added oid %08x, parent %08x, ofc %04x, size %ld, filename %s",
			           prefix, e[i].u.object.oid, e[i].u.object.oi.ParentObject, e[i].u.object.oi.ObjectFormat,
			           e[i].u.object.oi.ObjectSize, e[i].u.object.oi.Filename);
			break;
		case PTP_EC_CANON_EOS_RequestObjectTransfer:
		case PTP_EC_CANON_EOS_RequestObjectTransfer64:
			if (size < PTP_cee_OI_Name+1) {
				ptp_debug (params, "%s size %d is smaller than %d", prefix, size, PTP_cee_OI_Name+1);
				break;
			}
			e[i].type = PTP_EOSEvent_ObjectTransfer;
			e[i].u.object.oid              = dtoh32a(curdata + PTP_cee_OI_ObjectID);
			e[i].u.object.oi.StorageID     = 0; /* use as marker */
			e[i].u.object.oi.ObjectFormat  = dtoh16a(curdata + PTP_cee_OI_OFC);
			e[i].u.object.oi.ParentObject  = 0; /* check, but use as marker */
			e[i].u.object.oi.ObjectSize    = dtoh32a(curdata + PTP_cee_OI_Size);
			e[i].u.object.oi.Filename      = strdup(((char*)curdata + PTP_cee_OI_Name));

			ptp_debug (params, "%s request object transfer oid %08x, ofc %04x, size %ld, filename %p",
			           prefix, e[i].u.object.oid, e[i].u.object.oi.ObjectFormat,
			           e[i].u.object.oi.ObjectSize, e[i].u.object.oi.Filename);
			break;
		case PTP_EC_CANON_EOS_AvailListChanged: {	/* property desc */
			if (size < PTP_cee_DPD_Data) {
				ptp_debug (params, "%s size %d is smaller than %d", prefix, size, PTP_cee_DPD_Data);
				break;
			}

			uint32_t	dpc       = dtoh32a(curdata + PTP_cee_DPC);
			uint32_t	dpd_type  = dtoh32a(curdata + PTP_cee_DPD_Type);
			uint32_t	dpd_count = dtoh32a(curdata + PTP_cee_DPD_Count);
			const uint8_t	*xdata    = curdata + PTP_cee_DPD_Data;
			unsigned int	xsize     = size - PTP_cee_DPD_Data;
			unsigned int	j;
			PTPDevicePropDesc   *dpd = ptp_find_eos_devicepropdesc(params, dpc);

			ptp_debug (params, "%s prop %04x options changed, type %d, count %2d (%s) %s",
			           prefix, dpc, dpd_type, dpd_count, ptp_get_property_description (params, dpc),
			           dpd ? "" : "(unknown)");
			/* 1 - uint16 ?
			 * 3 - uint16
			 * 7 - string?
			 */
			if (dpd_type != 3) {
				ptp_debug_data (params, xdata, xsize);
				break;
			}

			if (!dpd || dpd_count == 0 || dpd_count >= 2<<16) /* buggy or exploit */
				break;

			dpd->FormFlag = PTP_DPFF_Enumeration;
			dpd->FORM.Enum.NumberOfValues = dpd_count;
			free (dpd->FORM.Enum.SupportedValue);
			dpd->FORM.Enum.SupportedValue = calloc (dpd_count, sizeof (PTPPropValue));

			switch (dpc) {
			case PTP_DPC_CANON_EOS_ImageFormat:
			case PTP_DPC_CANON_EOS_ImageFormatCF:
			case PTP_DPC_CANON_EOS_ImageFormatSD:
			case PTP_DPC_CANON_EOS_ImageFormatExtHD:
				/* special handling of ImageFormat properties */
				for (j=0;j<dpd_count;j++) {
					dpd->FORM.Enum.SupportedValue[j].u16 = ptp_unpack_EOS_ImageFormat( params, &xdata );
					ptp_debug (params, INDENT "prop %x option[%2d] == 0x%04x", dpc, j, dpd->FORM.Enum.SupportedValue[j].u16);
				}
				break;
			default:
				/* 'normal' enumerated types */
				switch (dpd->DataType) {
#define XX( TYPE, CONV )\
					if (sizeof(dpd->FORM.Enum.SupportedValue[j].TYPE)*dpd_count > xsize) {	\
						ptp_debug (params, "%s size %lu does not match needed %u", prefix, sizeof(dpd->FORM.Enum.SupportedValue[j].TYPE)*dpd_count, xsize);	\
						break;							\
					}								\
					for (j=0;j<dpd_count;j++) { 					\
						dpd->FORM.Enum.SupportedValue[j].TYPE = CONV(xdata); 	\
						ptp_debug (params, INDENT "prop %x option[%2d] == 0x%02x", dpc, j, CONV(xdata)); \
						xdata += 4; /* might only be for propxtype 3 */ \
					} \
					break;

				case PTP_DTC_INT16:	XX( i16, dtoh16a );
				case PTP_DTC_UINT16:	XX( u16, dtoh16a );
				case PTP_DTC_UINT32:	XX( u32, dtoh32a );
				case PTP_DTC_INT32:	XX( i32, dtoh32a );
				case PTP_DTC_UINT8:	XX( u8,  dtoh8a );
				case PTP_DTC_INT8:	XX( i8,  dtoh8a );
#undef XX
				default:
					free (dpd->FORM.Enum.SupportedValue);
					dpd->FORM.Enum.SupportedValue = NULL;
					dpd->FORM.Enum.NumberOfValues = 0;
					ptp_debug_data (params, xdata, xsize);
					break;
				}
			}
			break;
		}
		case PTP_EC_CANON_EOS_PropValueChanged: {	/* property info */
			if (size < PTP_cee_Prop_Val_Data) {
				ptp_debug (params, "%s size %d is smaller than %d", prefix, size, PTP_cee_Prop_Val_Data);
				break;
			}

			unsigned int j;
			uint32_t	dpc = dtoh32a(curdata + PTP_cee_DPC);
			const uint8_t	*xdata = curdata + PTP_cee_Prop_Val_Data;
			unsigned int	xsize = size - PTP_cee_Prop_Val_Data;

			ptp_debug (params, "%s prop %04x value changed, size %2d (%s)",
			           prefix, dpc, xsize, ptp_get_property_description(params, dpc));

			PTPDevicePropDesc *dpd = _lookup_or_allocate_canon_prop(params, dpc);

			e[i].type = PTP_EOSEvent_PropertyChanged;
			e[i].u.propid = dpc;

			/* fix GetSet value */
			switch (dpc) {
#define XX(x) case PTP_DPC_CANON_##x:
				XX(EOS_FocusMode)
				XX(EOS_BatteryPower)
				XX(EOS_BatterySelect)
				XX(EOS_ModelID)
				XX(EOS_PTPExtensionVersion)
				XX(EOS_DPOFVersion)
				XX(EOS_AvailableShots)
				XX(EOS_CurrentStorage)
				XX(EOS_CurrentFolder)
				XX(EOS_MyMenu)
				XX(EOS_MyMenuList)
				XX(EOS_HDDirectoryStructure)
				XX(EOS_BatteryInfo)
				XX(EOS_AdapterInfo)
				XX(EOS_LensStatus)
				XX(EOS_CardExtension)
				XX(EOS_TempStatus)
				XX(EOS_ShutterCounter)
				XX(EOS_ShutterReleaseCounter)
				XX(EOS_SerialNumber)
				XX(EOS_DepthOfFieldPreview)
				XX(EOS_EVFRecordStatus)
				XX(EOS_LvAfSystem)
				XX(EOS_FocusInfoEx)
				XX(EOS_DepthOfField)
				XX(EOS_Brightness)
				XX(EOS_EFComp)
				XX(EOS_LensName)
				XX(EOS_LensID)
				XX(EOS_FixedMovie)
#undef XX
					dpd->GetSet = PTP_DPGS_Get;
					break;
			}

			/* set DataType */
			switch (dpc) {
			case PTP_DPC_CANON_EOS_CameraTime:
			case PTP_DPC_CANON_EOS_UTCTime:
			case PTP_DPC_CANON_EOS_Summertime: /* basical the DST flag */
			case PTP_DPC_CANON_EOS_AvailableShots:
			case PTP_DPC_CANON_EOS_CaptureDestination:
			case PTP_DPC_CANON_EOS_WhiteBalanceXA:
			case PTP_DPC_CANON_EOS_WhiteBalanceXB:
			case PTP_DPC_CANON_EOS_CurrentStorage:
			case PTP_DPC_CANON_EOS_CurrentFolder:
			case PTP_DPC_CANON_EOS_ShutterCounter:
			case PTP_DPC_CANON_EOS_ModelID:
			case PTP_DPC_CANON_EOS_LensID:
			case PTP_DPC_CANON_EOS_StroboFiring:
			case PTP_DPC_CANON_EOS_StroboDispState:
			case PTP_DPC_CANON_EOS_LvCFilterKind:
			case PTP_DPC_CANON_EOS_CADarkBright:
			case PTP_DPC_CANON_EOS_ErrorForDisplay:
			case PTP_DPC_CANON_EOS_ExposureSimMode:
			case PTP_DPC_CANON_EOS_WindCut:
			case PTP_DPC_CANON_EOS_MovieRecordVolume:
			case PTP_DPC_CANON_EOS_ExtenderType:
			case PTP_DPC_CANON_EOS_AEModeMovie:
			case PTP_DPC_CANON_EOS_AFSelectFocusArea:
			case PTP_DPC_CANON_EOS_ContinousAFMode:
			case PTP_DPC_CANON_EOS_MirrorUpSetting:
			case PTP_DPC_CANON_EOS_MirrorDownStatus:
			case PTP_DPC_CANON_EOS_OLCInfoVersion:
			case PTP_DPC_CANON_EOS_PowerZoomPosition:
			case PTP_DPC_CANON_EOS_PowerZoomSpeed:
			case PTP_DPC_CANON_EOS_BuiltinStroboMode:
			case PTP_DPC_CANON_EOS_StroboETTL2Metering:
			case PTP_DPC_CANON_EOS_ColorTemperature:
			case PTP_DPC_CANON_EOS_FixedMovie:
			case PTP_DPC_CANON_EOS_AloMode:
			case PTP_DPC_CANON_EOS_LvViewTypeSelect:
			case PTP_DPC_CANON_EOS_EVFColorTemp:
			case PTP_DPC_CANON_EOS_LvAfSystem:
			case PTP_DPC_CANON_EOS_OneShotRawOn:
			case PTP_DPC_CANON_EOS_FlashChargingState:
			case PTP_DPC_CANON_EOS_MovieServoAF:
			case PTP_DPC_CANON_EOS_MultiAspect:
			case PTP_DPC_CANON_EOS_EVFOutputDevice:
			case PTP_DPC_CANON_EOS_FocusMode:
			case PTP_DPC_CANON_EOS_MirrorLockupState:
			case PTP_DPC_CANON_EOS_LensStatus:
			case PTP_DPC_CANON_EOS_TempStatus:
			case PTP_DPC_CANON_EOS_DepthOfFieldPreview:
			case PTP_DPC_CANON_EOS_EVFSharpness:
			case PTP_DPC_CANON_EOS_EVFWBMode:
			case PTP_DPC_CANON_EOS_MovieSoundRecord:
			case PTP_DPC_CANON_EOS_NetworkCommunicationMode:
			case PTP_DPC_CANON_EOS_NetworkServerRegion:
				dpd->DataType = PTP_DTC_UINT32;
				break;
			/* enumeration for AEM is never provided, but is available to set */
			case PTP_DPC_CANON_EOS_AEModeDial:
			case PTP_DPC_CANON_EOS_AutoExposureMode:
				dpd->DataType = PTP_DTC_UINT16;
				dpd->FormFlag = PTP_DPFF_Enumeration;
				dpd->FORM.Enum.NumberOfValues = 0;
				break;
			case PTP_DPC_CANON_EOS_Aperture:
			case PTP_DPC_CANON_EOS_ShutterSpeed:
			case PTP_DPC_CANON_EOS_ISOSpeed:
			case PTP_DPC_CANON_EOS_ColorSpace:
			case PTP_DPC_CANON_EOS_BatteryPower:
			case PTP_DPC_CANON_EOS_BatterySelect:
			case PTP_DPC_CANON_EOS_PTPExtensionVersion:
			case PTP_DPC_CANON_EOS_DriveMode:
			case PTP_DPC_CANON_EOS_AEB:
			case PTP_DPC_CANON_EOS_BracketMode:
			case PTP_DPC_CANON_EOS_QuickReviewTime:
			case PTP_DPC_CANON_EOS_EVFMode:
			case PTP_DPC_CANON_EOS_EVFRecordStatus:
			case PTP_DPC_CANON_EOS_HighISONoiseReduction:
				dpd->DataType = PTP_DTC_UINT16;
				break;
			case PTP_DPC_CANON_EOS_PictureStyle:
			case PTP_DPC_CANON_EOS_WhiteBalance:
			case PTP_DPC_CANON_EOS_MeteringMode:
			case PTP_DPC_CANON_EOS_ExpCompensation:
				dpd->DataType = PTP_DTC_UINT8;
				break;
			case PTP_DPC_CANON_EOS_Owner:
			case PTP_DPC_CANON_EOS_Artist:
			case PTP_DPC_CANON_EOS_Copyright:
			case PTP_DPC_CANON_EOS_SerialNumber:
			case PTP_DPC_CANON_EOS_LensName:
			case PTP_DPC_CANON_EOS_CameraNickname:
				dpd->DataType = PTP_DTC_STR;
				break;
			case PTP_DPC_CANON_EOS_AutoPowerOff:
			case PTP_DPC_CANON_EOS_WhiteBalanceAdjustA:
			case PTP_DPC_CANON_EOS_WhiteBalanceAdjustB:
				dpd->DataType = PTP_DTC_INT32;
				break;
			/* unknown props, listed from dump.... all 16 bit, but vals might be smaller */
			case PTP_DPC_CANON_EOS_DPOFVersion:
				dpd->DataType = PTP_DTC_UINT16;
				ptp_debug (params, INDENT "prop %04x is unknown", dpc);
				if (xsize > 2)
					for (j=0;j<xsize/2;j++)
						ptp_debug (params, "           %2d: 0x%4x", j, dtoh16a(xdata+j*2));
				break;
			case PTP_DPC_CANON_EOS_CustomFunc1:
			case PTP_DPC_CANON_EOS_CustomFunc2:
			case PTP_DPC_CANON_EOS_CustomFunc3:
			case PTP_DPC_CANON_EOS_CustomFunc4:
			case PTP_DPC_CANON_EOS_CustomFunc5:
			case PTP_DPC_CANON_EOS_CustomFunc6:
			case PTP_DPC_CANON_EOS_CustomFunc7:
			case PTP_DPC_CANON_EOS_CustomFunc8:
			case PTP_DPC_CANON_EOS_CustomFunc9:
			case PTP_DPC_CANON_EOS_CustomFunc10:
			case PTP_DPC_CANON_EOS_CustomFunc11:
				dpd->DataType = PTP_DTC_UINT8;
				ptp_debug (params, INDENT "prop %04x is unknown", dpc);
				ptp_debug_data (params, xdata, xsize);
				/* custom func entries look like this on the 400D: '5 0 0 0 ?' = 4 bytes size + 1 byte data */
				xdata += 4;
				xsize -= 4;
				break;
			/* yet unknown 32bit props */
			case PTP_DPC_CANON_EOS_WftStatus:
			case PTP_DPC_CANON_EOS_CardExtension:
			case PTP_DPC_CANON_EOS_PhotoStudioMode:
			case PTP_DPC_CANON_EOS_EVFClickWBCoeffs:
			case PTP_DPC_CANON_EOS_MovSize:
			case PTP_DPC_CANON_EOS_DepthOfField:
			case PTP_DPC_CANON_EOS_Brightness:
			case PTP_DPC_CANON_EOS_GPSLogCtrl:
			case PTP_DPC_CANON_EOS_GPSDeviceActive:
				dpd->DataType = PTP_DTC_UINT32;
				ptp_debug (params, INDENT "prop %04x is unknown", dpc);
				if (xsize % sizeof(uint32_t) != 0)
					ptp_debug (params, INDENT "Warning: datasize modulo sizeof(uint32) is not 0: %lu", xsize % sizeof(uint32_t));
				if (xsize > 4)
					for (j=0;j<xsize/sizeof(uint32_t);j++)
						ptp_debug (params, "           %2d: 0x%8x", j, dtoh32a(xdata+j*4));
				break;
			/* Some properties have to be ignored here, see special handling below */
			case PTP_DPC_CANON_EOS_ImageFormat:
			case PTP_DPC_CANON_EOS_ImageFormatCF:
			case PTP_DPC_CANON_EOS_ImageFormatSD:
			case PTP_DPC_CANON_EOS_ImageFormatExtHD:
			case PTP_DPC_CANON_EOS_CustomFuncEx:
			case PTP_DPC_CANON_EOS_FocusInfoEx:
				dpd->DataType = PTP_DTC_UNDEF;
				break;
			default:
				ptp_debug_data (params, xdata, xsize);
				break;
			}
			switch (dpd->DataType) {
			case PTP_DTC_INT32:
				dpd->DefaultValue.i32 = dtoh32a(xdata);
				dpd->CurrentValue.i32 = dtoh32a(xdata);
				ptp_debug (params, INDENT "prop %x value == %d (i32)", dpc, dpd->CurrentValue.i32);
				break;
			case PTP_DTC_UINT32:
				dpd->DefaultValue.u32 = dtoh32a(xdata);
				dpd->CurrentValue.u32 = dtoh32a(xdata);
				ptp_debug (params, INDENT "prop %x value == 0x%08x (u32)", dpc, dpd->CurrentValue.u32);
				break;
			case PTP_DTC_INT16:
				dpd->DefaultValue.i16 = dtoh16a(xdata);
				dpd->CurrentValue.i16 = dtoh16a(xdata);
				ptp_debug (params, INDENT "prop %x value == %d (i16)", dpc, dpd->CurrentValue.i16);
				break;
			case PTP_DTC_UINT16:
				dpd->DefaultValue.u16 = dtoh16a(xdata);
				dpd->CurrentValue.u16 = dtoh16a(xdata);
				ptp_debug (params, INDENT "prop %x value == 0x%04x (u16)", dpc, dpd->CurrentValue.u16);
				break;
			case PTP_DTC_UINT8:
				dpd->DefaultValue.u8  = dtoh8a(xdata);
				dpd->CurrentValue.u8  = dtoh8a(xdata);
				ptp_debug (params, INDENT "prop %x value == 0x%02x (u8)", dpc, dpd->CurrentValue.u8);
				break;
			case PTP_DTC_INT8:
				dpd->DefaultValue.i8  = dtoh8a(xdata);
				dpd->CurrentValue.i8  = dtoh8a(xdata);
				ptp_debug (params, INDENT "prop %x value == %d (i8)", dpc, dpd->CurrentValue.i8);
				break;
			case PTP_DTC_STR: {
#if 0 /* 5D MII and 400D aktually store plain ASCII in their string properties */
				dpd->DefaultValue.str	= ptp_unpack_string(params, data, 0, &len);
				dpd->CurrentValue.str		= ptp_unpack_string(params, data, 0, &len);
#else
				free (dpd->DefaultValue.str);
				dpd->DefaultValue.str = strdup( (char*)xdata );

				free (dpd->CurrentValue.str);
				dpd->CurrentValue.str = strdup( (char*)xdata );
#endif
				ptp_debug (params, INDENT "prop %x value == '%s' (str)", dpc, dpd->CurrentValue.str);
				break;
			}
			default:
				/* debug is printed in switch above this one */
				break;
			}

			/* ImageFormat and customFuncEx special handling (WARNING: dont move this in front of the dpd->DataType switch!) */
			switch (dpc) {
			case PTP_DPC_CANON_EOS_ImageFormat:
			case PTP_DPC_CANON_EOS_ImageFormatCF:
			case PTP_DPC_CANON_EOS_ImageFormatSD:
			case PTP_DPC_CANON_EOS_ImageFormatExtHD:
				dpd->DataType = PTP_DTC_UINT16;
				dpd->DefaultValue.u16 = ptp_unpack_EOS_ImageFormat( params, &xdata );
				dpd->CurrentValue.u16 = dpd->DefaultValue.u16;
				ptp_debug (params, INDENT "prop %x value == 0x%04x (u16)", dpc, dpd->CurrentValue.u16);
				break;
			case PTP_DPC_CANON_EOS_CustomFuncEx:
				dpd->DataType = PTP_DTC_STR;
				free (dpd->DefaultValue.str);
				free (dpd->CurrentValue.str);
				dpd->DefaultValue.str = ptp_unpack_EOS_CustomFuncEx( params, &xdata );
				dpd->CurrentValue.str = strdup( (char*)dpd->DefaultValue.str );
				ptp_debug (params, INDENT "prop %x value == %s", dpc, dpd->CurrentValue.str);
				break;
			case PTP_DPC_CANON_EOS_FocusInfoEx:
				dpd->DataType = PTP_DTC_STR;
				free (dpd->DefaultValue.str);
				free (dpd->CurrentValue.str);
				dpd->DefaultValue.str = ptp_unpack_EOS_FocusInfoEx( params, &xdata, xsize );
				dpd->CurrentValue.str = strdup( (char*)dpd->DefaultValue.str );
				ptp_debug (params, INDENT "prop %x value == %s", dpc, dpd->CurrentValue.str);
				break;
			/* case PTP_DPC_CANON_EOS_ShutterReleaseCounter:
				* There are 16 bytes sent by an R8, which look like 4 int numbers: 16, 1, 1000, 1000
				* But don't change after a shutter release, Maybe the name for this property is wrong?
				*/
			}
			break;
		}

/* largely input from users, CONFIRMED is really confirmed from debug
 * traces via "testolc", rest is guessed */
static unsigned int olcsizes[0x15][13] = {
	/* 1,2,4,8,0x10,  0x20,0x40,0x80,0x100,0x200, 0x400,0x800,0x1000*/
	{0,0,0,0,0, 0,0,0,0,0, 0,0,0 },	/* 0x0 */
	{0,0,0,0,0, 0,0,0,0,0, 0,0,0 },	/* 0x1 */
	{0,0,0,0,0, 0,0,0,0,0, 0,0,0 },	/* 0x2 */
	{0,0,0,0,0, 0,0,0,0,0, 0,0,0 },	/* 0x3 */
	{0,0,0,0,0, 0,0,0,0,0, 0,0,0 },	/* 0x4 */
	{0,0,0,0,0, 0,0,0,0,0, 0,0,0 },	/* 0x5 */
	{0,0,0,0,0, 0,0,0,0,0, 0,0,0 },	/* 0x6 */
	{2,6,5,4,4, 6,7,4,6,5, 5,8,1 },	/* 0x7 */	/* CONFIRMED: 100D, 5Dm3, 650D, 6D */
	{2,6,5,4,4, 6,7,4,6,7, 7,8,1 },	/* 0x8 */	/* CONFIRMED: 70D, M10, PowerShot SX720HS: only report 0x1, 0x2, 0x4 and 0x8 masks, separately */
	{2,6,5,4,4, 6,7,4,6,7, 7,8,1 },	/* 0x9 */	/* guessed */
	{2,6,5,4,4, 6,7,4,6,7, 7,8,1 },	/* 0xa */	/* guessed */
	{2,6,5,4,4, 6,8,4,6,5, 5,9,1 }, /* 0xb */	/* CONFIRMED: 750D, 5Ds */
	{2,6,5,4,4, 6,8,4,6,5, 5,9,1 },	/* 0xc */	/* guessed */
	{2,6,5,4,4, 6,8,4,6,5, 5,9,1 },	/* 0xd */	/* guessed */
	{2,6,5,4,4, 6,8,4,6,5, 5,9,1 },	/* 0xe */	/* guessed */
	{2,7,6,4,4, 6,8,4,6,5, 5,9,1 },	/* 0xf */	/* CONFIRMED: 200D */
	{2,7,6,4,4, 6,8,4,6,5, 5,9,1 },	/* 0x10 */	/* guessed */
	{2,7,6,6,4, 6,8,4,6,5, 5,9,8 },	/* 0x11 */	/* CONFIRMED: R */
	{2,7,9,6,4, 6,8,5,7,5, 5,9,8 },	/* 0x12 */	/* CONFIRMED: M6m2 */
	{2,7,9,7,4, 6,8,5,7,5, 5,9,8 },	/* 0x13 */	/* CONFIRMED: R5, R5 C, M50m2 */
	{2,9,9,7,4, 6,8,5,7,5, 5,9,8 },	/* 0x14 */	/* CONFIRMED: R8, R10, R5m2 */
};
		/* still unclear what OLC stands for */
		case PTP_EC_CANON_EOS_OLCInfoChanged: {
			uint32_t		len, curoff;
			uint16_t		mask;
			PTPDevicePropDesc	*dpd;
			unsigned int		olcver = 0, j;

			dpd = _lookup_or_allocate_canon_prop(params, PTP_DPC_CANON_EOS_OLCInfoVersion);
			if (dpd)
				olcver = dpd->CurrentValue.u32;
			if (olcver == 0) {
				e[i].type = PTP_EOSEvent_Unknown;
				PTP_CANON_SET_INFO(e[i], "OLC version is unknown");
				ptp_debug (params, "%s OLC version is 0, skipping (might get set later)", prefix);
				break;
			}
			if (olcver >= ARRAYSIZE(olcsizes)) {
				ptp_debug (params, "%s OLC version is 0x%02x, assuming latest known", prefix, olcver);
				olcver = ARRAYSIZE(olcsizes)-1;
			}

			mask = size >= 14 ? dtoh16a(curdata+8+4) : 0;
			ptp_debug (params, "%s OLCInfoChanged (size %d, version 0x%02x, mask 0x%04x)", prefix, size, olcver, mask);
			if (size >= 8) {	/* event info */
				ptp_debug_data (params, curdata + 8, size - 8);
			}
			if (size < 14) {
				e[i].type = PTP_EOSEvent_Unknown;
				PTP_CANON_SET_INFO(e[i], "OLC size too small");
				ptp_debug (params, "%s OLC unexpected size %d", prefix, size);
				break;
			}
			len = dtoh32a(curdata+8);
			if ((len != size-8) && (len != size-4)) {
				e[i].type = PTP_EOSEvent_Unknown;
				PTP_CANON_SET_INFO(e[i], "OLC size unexpected");
				ptp_debug (params, "%s OLC unexpected size %d for blob len %d (not -4 nor -8)", prefix, size, len);
				break;
			}
			curoff = 8+4+4;

			for (j = 0; j <= 12; j++) {
				unsigned int curmask = 1 << j;
				unsigned int cursize = MIN(olcsizes[olcver][j], size - curoff);
				if (curoff > size)
					break;
				if (!(mask & curmask))
					continue;
				if (olcsizes[olcver][j] != cursize) {
					ptp_debug (params, "%s mask 0x%04x entry truncated (%d bytes), olcsizes table (%d bytes) wrong?",
					           prefix, curmask, cursize, olcsizes[olcver][j]);
				}
				ptp_debug (params, "event %3d:%04x: (olcmask) %d bytes: %s", i, curmask, cursize,
				           ptp_bytes2str(curdata + curoff, cursize, "%02x "));
				switch (curmask) {
				case 0x0001: { /* Button */
					e[i].type = PTP_EOSEvent_Unknown;
					PTP_CANON_SET_INFO(e[i], "Button %x",  dtoh16a(curdata+curoff));
					break;
				}
				case 0x0002: { /* Shutter Speed */
					/* 6 bytes: 01 01 98 10 00 60 */
					/* this seem to be the shutter speed record */
					/* EOS 200D seems to have 7 bytes here, sample:
					 * 7 bytes: 01 03 98 10 00 70 00
					 * EOS R also 7 bytes
					 * 7 bytes: 01 01 a0 0c 00 0c 00
					 */
					uint16_t dpc = PTP_DPC_CANON_EOS_ShutterSpeed;
					dpd = _lookup_or_allocate_canon_prop(params, dpc);
					if (olcver >= 0x14) {	/* taken from northofyou branch */
						dpd->CurrentValue.u16 = curdata[curoff+7];
					} else {
						dpd->CurrentValue.u16 = curdata[curoff+5];
					}

					e[i].type = PTP_EOSEvent_PropertyChanged;
					e[i].u.propid = dpc;
					break;
				}
				case 0x0004: { /* Aperture */
					/* 5 bytes: 01 01 5b 30 30 */
					/* this seem to be the aperture record */
					/* EOS 200D seems to have 6 bytes here?
					 * 6 bytes: 01 01 50 20 20 00 *
					 * EOS M6 Mark 2:
					 * 9 bytes: 01 03 00 58 00 2d 00 30 00
					 */
					uint16_t dpc = PTP_DPC_CANON_EOS_Aperture;
					dpd = _lookup_or_allocate_canon_prop(params, dpc);
					if (olcver >= 0x12) {
						dpd->CurrentValue.u16 = curdata[curoff+7]; /* RP, R5, etc */
					} else {
						dpd->CurrentValue.u16 = curdata[curoff+4]; /* just use last byte */
					}

					e[i].type = PTP_EOSEvent_PropertyChanged;
					e[i].u.propid = dpc;
					break;
				}
				case 0x0008: { /* ISO */
					/* 4 bytes: 01 01 00 78 */
					/* EOS M6 Mark2: 01 01 00 6b 68 28 */
					/* this seem to be the ISO record */
					uint16_t dpc = PTP_DPC_CANON_EOS_ISOSpeed;
					dpd = _lookup_or_allocate_canon_prop(params, dpc);
					dpd->CurrentValue.u16 = curdata[curoff+3]; /* just use last byte */

					e[i].type = PTP_EOSEvent_PropertyChanged;
					e[i].u.propid = dpc;
					break;
				}
				case 0x0040: { /* Exposure Indicator */
					int	value = (signed char)curdata[curoff+2];
					/* mask 0x0040: 7 bytes, 01 01 00 00 00 00 00 observed */
					/* exposure indicator */
					e[i].type = PTP_EOSEvent_Unknown;
					PTP_CANON_SET_INFO(e[i], "OLCInfo exposure indicator %d,%d,%d.%d (%s)",
						curdata[curoff+0],
						curdata[curoff+1],
						value/10, abs(value)%10,
						ptp_bytes2str(curdata + curoff + 3, olcsizes[olcver][j] - 3, "%02x ")
					);
					break;
				}
				case 0x0100: /* Focus Info */
					/* mask 0x0100: 6 bytes, 00 00 00 00 00 00 (before focus) and
					 *                       00 00 00 00 01 00 (on focus) observed */
					/* a full trigger capture cycle on the 5Ds with enabled and acting auto-focus looks like this
						0.098949  6 bytes: 00 00 00 00 00 00 (first GetEvent)
						0.705762  6 bytes: 00 00 00 00 00 01 (first GetEvent after half-press-on, together with FocusInfoEx == {})
						0.758275  6 bytes: 00 00 00 00 01 01 (second GetEvent after half-press-on, together with FocusInfoEx == {...})
						0.962160  6 bytes: 00 00 00 00 01 00 (couple GetEvents later, together with 3x FocusInfoEx == {} and next line)
						0.962300  6 bytes: 00 00 00 00 00 00
					   On AF-failure, the 5Ds sequence is 0-1, 2-2, 2-0, 0-0.
					   The R8 looks similar except another 00 byte is appended and on sucess it jumps directly from 1-1 to 0-0.
					   On an AF-failure, it jumps from 0-1 to 0-0. The R5m2 has seen to fail with 0-1, 2-1, 2-0, 0-0.
					*/
					e[i].type = PTP_EOSEvent_FocusInfo;
					PTP_CANON_SET_INFO(e[i], "%s", ptp_bytes2str(curdata + curoff, olcsizes[olcver][j], "%02x"));
					break;
				case 0x0200: /* Focus Mask */
					/* mask 0x0200: 7 bytes, 00 00 00 00 00 00 00 observed */
					e[i].type = PTP_EOSEvent_FocusMask;
					PTP_CANON_SET_INFO(e[i], "%s", ptp_bytes2str(curdata + curoff, olcsizes[olcver][j], "%02x"));
					break;
				case 0x0010:
					/* mask 0x0010: 4 bytes, 04 00 00 00 observed */
					/* a full trigger capture cycle on the 5Ds with storing to card looks like this
					   (first column is the timestamp):
						0.132778  4 bytes: 04 01 01 71  after EOS_SetRequestOLCInfoGroup
						0.246439  4 bytes: 04 02 01 70  after EOS_RemoteReleaseOn (0x2,0x0)
						1.291545  4 bytes: 04 02 01 71  after first "non-busy" EOS_SetDevicePropValueEx
						2.251470  4 bytes: 04 01 01 71  after object info added
					   and on the R8 it looks like this (after the same events):
						0.357627  4 bytes: 04 01 06 2f
						0.436084  4 bytes: 04 02 06 2e
						0.942222  4 bytes: 04 02 06 2f
						0.942969  4 bytes: 04 01 06 2f
					   This suggests that the very last bit indicates the readiness to process commands
					   and the second byte may be related to accessing storage. The latter might be true
					   the former does not hold true when repeating captures.
					*/
				case 0x0020:
					/* mask 0x0020: 6 bytes, 00 00 00 00 00 00 observed.
					 * This seems to be the self-timer record: when active,
					 * has the form of 00 00 01 00 XX XX, where the last two bytes
					 * stand for the number of seconds remaining until the shot */
				case 0x0080:
					/* mask 0x0080: 4 bytes, 00 00 00 00 observed */
				case 0x0400:
					/* mask 0x0400: 7 bytes, 00 00 00 00 00 00 00 observed */
				case 0x0800:
					/* mask 0x0800: 8 bytes, 00 00 00 00 00 00 00 00 and 19 01 00 00 00 00 00 00 and others observed
					 * might be mask of focus points selected */
				case 0x1000:
					/* mask 0x1000: 1 byte, 00 observed */
				default:
					e[i].type = PTP_EOSEvent_Unknown;
					PTP_CANON_SET_INFO(e[i], "OLCInfo event 0x%04x, %d bytes: %s", curmask, olcsizes[olcver][j],
						ptp_bytes2str(curdata + curoff, olcsizes[olcver][j], "%02x "));
					break;
				}
				curoff += olcsizes[olcver][j];
				i++;
			}
			i--; /* account for the i++ at the end of the outer loop */
			break;
		}
		case PTP_EC_CANON_EOS_CameraStatusChanged:
			e[i].type = PTP_EOSEvent_CameraStatus;
			e[i].u.status =  dtoh32a(curdata+8);
			ptp_debug (params, "%s CameraStatusChanged (size %d) = %d", prefix, size, dtoh32a(curdata+8));
			params->eos_camerastatus = dtoh32a(curdata+8);
			break;
		case 0: /* end marker */
			if (size != 8) /* no output */
				ptp_debug (params, "%s EOS event list null terminator is expected to have size 8 instead of %d", prefix, size);
			break;
		case PTP_EC_CANON_EOS_BulbExposureTime:
			e[i].type = PTP_EOSEvent_Unknown;
			PTP_CANON_SET_INFO(e[i], "BulbExposureTime %u",  dtoh32a(curdata+8));
			ptp_debug (params, "%s %s", prefix, e[i].u.info);
			break;
		case PTP_EC_CANON_EOS_CTGInfoCheckComplete: /* some form of storage catalog ? */
			e[i].type = PTP_EOSEvent_Unknown;
			PTP_CANON_SET_INFO(e[i], "CTGInfoCheckComplete 0x%08x",  dtoh32a(curdata+8));
			ptp_debug (params, "%s %s", prefix, e[i].u.info);
			break;
		case PTP_EC_CANON_EOS_StorageStatusChanged:
			e[i].type = PTP_EOSEvent_Unknown;
			PTP_CANON_SET_INFO(e[i], "StorageStatusChanged 0x%08x",  dtoh32a(curdata+8));
			ptp_debug (params, "%s %s", prefix, e[i].u.info);
			break;
		case PTP_EC_CANON_EOS_StorageInfoChanged:
			e[i].type = PTP_EOSEvent_Unknown;
			PTP_CANON_SET_INFO(e[i], "StorageInfoChanged 0x%08x",  dtoh32a(curdata+8));
			ptp_debug (params, "%s %s", prefix, e[i].u.info);
			break;
		case PTP_EC_CANON_EOS_StoreAdded:
			e[i].type = PTP_EOSEvent_Unknown;
			PTP_CANON_SET_INFO(e[i], "StoreAdded 0x%08x",  dtoh32a(curdata+8));
			ptp_debug (params, "%s %s", prefix, e[i].u.info);
			break;
		case PTP_EC_CANON_EOS_StoreRemoved:
			e[i].type = PTP_EOSEvent_Unknown;
			PTP_CANON_SET_INFO(e[i], "StoreRemoved 0x%08x",  dtoh32a(curdata+8));
			ptp_debug (params, "%s %s", prefix, e[i].u.info);
			break;
		case PTP_EC_CANON_EOS_ObjectRemoved:
			e[i].type = PTP_EOSEvent_ObjectRemoved;
			e[i].u.object.oid = dtoh32a(curdata+8);
			ptp_debug (params, "%s object %08x removed", prefix, dtoh32a(curdata+8));
			break;
		default:
			switch (ec) {
#define XX(x)		case PTP_EC_CANON_EOS_##x: 								\
				ptp_debug (params, "%s unhandled EOS event "#x" (size %u)", prefix, size); 	\
				PTP_CANON_SET_INFO(e[i], "unhandled EOS event "#x" (size %u)",  size);		\
				break;
			XX(RequestGetEvent)
			XX(RequestGetObjectInfoEx)
			XX(ObjectInfoChangedEx)
			XX(ObjectContentChanged)
			XX(WillSoonShutdown)
			XX(ShutdownTimerUpdated)
			XX(RequestCancelTransfer)
			XX(RequestObjectTransferDT)
			XX(RequestCancelTransferDT)
			XX(RecordingTime)
			XX(RequestObjectTransferTS)
			XX(AfResult)
			XX(PowerZoomInfoChanged)
#undef XX
			default:
				ptp_debug (params, "%s unknown EOS event", prefix);
				break;
			}
			if (size >= 0x8) {	/* event info */
				ptp_debug_data (params, curdata + 8, size - 8);
			}
			e[i].type = PTP_EOSEvent_Unknown;
			break;
		}
		curdata += size;
		i++;
		if (i > event_count && datasize - (curdata - data) > 8) {
			ptp_debug (params, "BAD: ran out of allocated slots (%d) for EOS events, %ld bytes left to parse", event_count, datasize - (curdata - data));
			break;
		}
	}

	if (i != event_count)
		ptp_debug (params, "BAD: mismatch between number of allocated (%d) and parsed (%d) event", event_count, i);

	if (!i) {
		free (e);
		e = NULL;
	}
	*events = e;
	return i;
	#undef INDENT
}

/*
    PTP USB Event container unpack for Nikon events.
*/
#define PTP_nikon_ec_Length		0
#define PTP_nikon_ec_Code		2
#define PTP_nikon_ec_Param1		4
#define PTP_nikon_ec_Size		6
static inline void
ptp_unpack_Nikon_EC (PTPParams *params, const unsigned char* data, unsigned int len, PTPContainer **ec, unsigned int *cnt)
{
	unsigned int i;

	*ec = NULL;
	if (!data || len < PTP_nikon_ec_Code)
		return;
	*cnt = dtoh16a(&data[PTP_nikon_ec_Length]);
	if (*cnt > (len-PTP_nikon_ec_Code)/PTP_nikon_ec_Size) { /* broken cnt? */
		*cnt = 0;
		return;
	}
	if (!*cnt)
		return;

	*ec = calloc((*cnt), sizeof(PTPContainer));

	for (i=0;i<*cnt;i++) {
		memset(&(*ec)[i],0,sizeof(PTPContainer));
		(*ec)[i].Code	= dtoh16a(&data[PTP_nikon_ec_Code+PTP_nikon_ec_Size*i]);
		(*ec)[i].Param1	= dtoh32a(&data[PTP_nikon_ec_Param1+PTP_nikon_ec_Size*i]);
		(*ec)[i].Nparam	= 1;
	}
}

/*
 *  PTP USB Event container unpack for Nikon events, 2nd generation.
 */
#define PTP_nikon_ec_ex_Length		0
#define PTP_nikon_ec_ex_Code		2

static inline int
ptp_unpack_Nikon_EC_EX (PTPParams *params, const unsigned char* data, unsigned int len, PTPContainer **ec, unsigned int *cnt)
{
	unsigned int i, offset = 0;

	*ec = NULL;
	if (!data || len < PTP_nikon_ec_ex_Code)
		return 0;
	*cnt = dtoh16a(&data[PTP_nikon_ec_ex_Length]);
	if (*cnt > (len-PTP_nikon_ec_ex_Code)/4) { /* broken cnt? simple first check ... due to dynamic size, we need to do more later */
		*cnt = 0;
		return 0;
	}
	if (!*cnt)
		return 1;

	*ec = calloc((*cnt), sizeof(PTPContainer));
	offset = 4;

	for (i=0;i<*cnt;i++) {
		if (len < offset + 4)
			goto error;

		(*ec)[i].Code	= dtoh16o(data, offset);
		(*ec)[i].Nparam	= dtoh16o(data, offset);
		ptp_debug (params, "nikon eventex %d: code 0x%04x, params %d", i, (*ec)[i].Code, (*ec)[i].Nparam);
		if (((*ec)[i].Nparam > 5) || (len < offset + ((*ec)[i].Nparam*sizeof(uint32_t))))
			goto error;

		if ((*ec)[i].Nparam >= 1) (*ec)[i].Param1 = dtoh32o(data, offset);
		if ((*ec)[i].Nparam >= 2) (*ec)[i].Param2 = dtoh32o(data, offset);
		if ((*ec)[i].Nparam >= 3) (*ec)[i].Param3 = dtoh32o(data, offset);
		if ((*ec)[i].Nparam >= 4) (*ec)[i].Param4 = dtoh32o(data, offset);
		if ((*ec)[i].Nparam == 5) (*ec)[i].Param5 = dtoh32o(data, offset);
	}
	return 1;

error:
	free (*ec);
	*ec = NULL;
	*cnt = 0;
	return 0;
}

static inline uint32_t
ptp_pack_EK_text(PTPParams *params, PTPEKTextParams *text, unsigned char **data) {
	int i, len = 0;
	uint8_t	retlen;
	unsigned char *curdata;

	len =	2*(strlen(text->title)+1)+1+
		2*(strlen(text->line[0])+1)+1+
		2*(strlen(text->line[1])+1)+1+
		2*(strlen(text->line[2])+1)+1+
		2*(strlen(text->line[3])+1)+1+
		2*(strlen(text->line[4])+1)+1+
		4*2+2*4+2+4+2+5*4*2;
	*data = malloc(len);
	if (!*data) return 0;

	curdata = *data;
	htod16a(curdata,100);curdata+=2;
	htod16a(curdata,1);curdata+=2;
	htod16a(curdata,0);curdata+=2;
	htod16a(curdata,1000);curdata+=2;

	htod32a(curdata,0);curdata+=4;
	htod32a(curdata,0);curdata+=4;

	htod16a(curdata,6);curdata+=2;
	htod32a(curdata,0);curdata+=4;

	ptp_pack_string(params, text->title, curdata, 0, &retlen); curdata+=2*retlen+1;htod16a(curdata,0);curdata+=2;
	htod16a(curdata,0x10);curdata+=2;

	for (i=0;i<5;i++) {
		ptp_pack_string(params, text->line[i], curdata, 0, &retlen); curdata+=2*retlen+1;htod16a(curdata,0);curdata+=2;
		htod16a(curdata,0x10);curdata+=2;
		htod16a(curdata,0x01);curdata+=2;
		htod16a(curdata,0x02);curdata+=2;
		htod16a(curdata,0x06);curdata+=2;
	}
	return len;
}

#define ptp_canon_dir_version	0x00
#define ptp_canon_dir_ofc	0x02
#define ptp_canon_dir_unk1	0x04
#define ptp_canon_dir_objectid	0x08
#define ptp_canon_dir_parentid	0x0c
#define ptp_canon_dir_previd	0x10	/* in same dir */
#define ptp_canon_dir_nextid	0x14	/* in same dir */
#define ptp_canon_dir_nextchild	0x18	/* down one dir */
#define ptp_canon_dir_storageid	0x1c	/* only in storage entry */
#define ptp_canon_dir_name	0x20
#define ptp_canon_dir_flags	0x2c
#define ptp_canon_dir_size	0x30
#define ptp_canon_dir_unixtime	0x34
#define ptp_canon_dir_year	0x38
#define ptp_canon_dir_month	0x39
#define ptp_canon_dir_mday	0x3a
#define ptp_canon_dir_hour	0x3b
#define ptp_canon_dir_minute	0x3c
#define ptp_canon_dir_second	0x3d
#define ptp_canon_dir_unk2	0x3e
#define ptp_canon_dir_thumbsize	0x40
#define ptp_canon_dir_width	0x44
#define ptp_canon_dir_height	0x48

static inline uint16_t
ptp_unpack_canon_directory (
	PTPParams		*params,
	unsigned char		*dir,
	uint32_t		cnt,
	PTPObjectHandles	*handles,
	PTPObjectInfo		**oinfos,	/* size(handles->n) */
	uint32_t		**flags		/* size(handles->n) */
) {
	unsigned int	i, j, nrofobs = 0, curob = 0;

#define ISOBJECT(ptr) (dtoh32a((ptr)+ptp_canon_dir_storageid) == 0xffffffff)
	for (i=0;i<cnt;i++)
		if (ISOBJECT(dir+i*0x4c)) nrofobs++;
	handles->n = nrofobs;
	handles->Handler = calloc(nrofobs,sizeof(handles->Handler[0]));
	if (!handles->Handler) return PTP_RC_GeneralError;
	*oinfos = calloc(nrofobs,sizeof((*oinfos)[0]));
	if (!*oinfos) return PTP_RC_GeneralError;
	*flags  = calloc(nrofobs,sizeof((*flags)[0]));
	if (!*flags) return PTP_RC_GeneralError;

	/* Migrate data into objects ids, handles into
	 * the object handler array.
	 */
	curob = 0;
	for (i=0;i<cnt;i++) {
		unsigned char	*cur = dir+i*0x4c;
		PTPObjectInfo	*oi = (*oinfos)+curob;

		if (!ISOBJECT(cur))
			continue;

		handles->Handler[curob] = dtoh32a(cur + ptp_canon_dir_objectid);
		oi->StorageID		= 0xffffffff;
		oi->ObjectFormat	= dtoh16a(cur + ptp_canon_dir_ofc);
		oi->ParentObject	= dtoh32a(cur + ptp_canon_dir_parentid);
		oi->Filename		= strdup((char*)(cur + ptp_canon_dir_name));
		oi->ObjectSize		= dtoh32a(cur + ptp_canon_dir_size);
		oi->ThumbSize		= dtoh32a(cur + ptp_canon_dir_thumbsize);
		oi->ImagePixWidth	= dtoh32a(cur + ptp_canon_dir_width);
		oi->ImagePixHeight	= dtoh32a(cur + ptp_canon_dir_height);
		oi->CaptureDate		= oi->ModificationDate = dtoh32a(cur + ptp_canon_dir_unixtime);
		(*flags)[curob]		= dtoh32a(cur + ptp_canon_dir_flags);
		curob++;
	}
	/* Walk over Storage ID entries and distribute the IDs to
	 * the parent objects. */
	for (i=0;i<cnt;i++) {
		unsigned char	*cur = dir+i*0x4c;
		uint32_t	nextchild = dtoh32a(cur + ptp_canon_dir_nextchild);

		if (ISOBJECT(cur))
			continue;
		for (j=0;j<handles->n;j++) if (nextchild == handles->Handler[j]) break;
		if (j == handles->n) continue;
		(*oinfos)[j].StorageID = dtoh32a(cur + ptp_canon_dir_storageid);
	}
	/* Walk over all objects and distribute the storage ids */
	while (1) {
		unsigned int changed = 0;
		for (i=0;i<cnt;i++) {
			unsigned char	*cur = dir+i*0x4c;
			uint32_t	oid = dtoh32a(cur + ptp_canon_dir_objectid);
			uint32_t	nextoid = dtoh32a(cur + ptp_canon_dir_nextid);
			uint32_t	nextchild = dtoh32a(cur + ptp_canon_dir_nextchild);
			uint32_t	storageid;

			if (!ISOBJECT(cur))
				continue;
			for (j=0;j<handles->n;j++) if (oid == handles->Handler[j]) break;
			if (j == handles->n) {
				/*fprintf(stderr,"did not find oid in lookup pass for current oid\n");*/
				continue;
			}
	 		storageid = (*oinfos)[j].StorageID;
			if (storageid == 0xffffffff) continue;
			if (nextoid != 0xffffffff) {
				for (j=0;j<handles->n;j++) if (nextoid == handles->Handler[j]) break;
				if (j == handles->n) {
					/*fprintf(stderr,"did not find oid in lookup pass for next oid\n");*/
					continue;
				}
				if ((*oinfos)[j].StorageID == 0xffffffff) {
					(*oinfos)[j].StorageID = storageid;
					changed++;
				}
			}
			if (nextchild != 0xffffffff) {
				for (j=0;j<handles->n;j++) if (nextchild == handles->Handler[j]) break;
				if (j == handles->n) {
					/*fprintf(stderr,"did not find oid in lookup pass for next child\n");*/
					continue;
				}
				if ((*oinfos)[j].StorageID == 0xffffffff) {
					(*oinfos)[j].StorageID = storageid;
					changed++;
				}
			}
		}
		/* Check if we:
		 * - changed no entry (nothing more to do)
		 * - changed all of them at once (usually happens)
		 * break if we do.
		 */
		if (!changed || (changed==nrofobs-1))
			break;
	}
#undef ISOBJECT
	return PTP_RC_OK;
}

static inline int
ptp_unpack_ptp11_manifest (
	PTPParams		*params,
	const unsigned char	*data,
	unsigned int 		datalen,
	uint64_t		*numoifs,
	PTPObjectFilesystemInfo	**oifs
) {
	uint64_t		numberoifs, i;
	unsigned int		offset = 0;
	PTPObjectFilesystemInfo	*xoifs;

	if (!data || datalen < 8)
		return 0;
	numberoifs = dtoh64o(data, offset);
	xoifs = calloc(numberoifs, sizeof(PTPObjectFilesystemInfo));
	if (!xoifs)
		return 0;

	for (i = 0; i < numberoifs; i++) {
		char *modify_date;
		PTPObjectFilesystemInfo *oif = xoifs+i;

		if (offset + 34 + 2 > datalen)
			goto tooshort;

		oif->ObjectHandle      = dtoh32o(data, offset);
		oif->StorageID         = dtoh32o(data, offset);
		oif->ObjectFormat      = dtoh16o(data, offset);
		oif->ProtectionStatus  = dtoh16o(data, offset);
		oif->ObjectSize64      = dtoh64o(data, offset);
		oif->ParentObject      = dtoh32o(data, offset);
		oif->AssociationType   = dtoh16o(data, offset);
		oif->AssociationDesc   = dtoh32o(data, offset);
		oif->SequenceNumber    = dtoh32o(data, offset);

		if (!ptp_unpack_string(params, data, &offset, datalen, &oif->Filename))
			goto tooshort;

		if (!ptp_unpack_string(params, data, &offset, datalen, &modify_date))
			goto tooshort;

		oif->ModificationDate 		= ptp_unpack_PTPTIME(modify_date);
		free(modify_date);
	}
	*numoifs = numberoifs;
	*oifs = xoifs;
	return 1;
tooshort:
	for (i = 0; i < numberoifs; i++)
		free (xoifs[i].Filename);
	free (xoifs);
	return 0;
}

static inline void
ptp_unpack_chdk_lv_data_header (PTPParams *params, const unsigned char* data, lv_data_header *header)
{
	uint32_t offset = 0;
	if (data==NULL)
		return;
	header->version_major      = dtoh32o(data, offset);
	header->version_minor      = dtoh32o(data, offset);
	header->lcd_aspect_ratio   = dtoh32o(data, offset);
	header->palette_type       = dtoh32o(data, offset);
	header->palette_data_start = dtoh32o(data, offset);
	header->vp_desc_start      = dtoh32o(data, offset);
	header->bm_desc_start      = dtoh32o(data, offset);
	if (header->version_minor > 1)
		header->bmo_desc_start = dtoh32o(data, offset);
}

static inline void
ptp_unpack_chdk_lv_framebuffer_desc (PTPParams *params, const unsigned char* data, lv_framebuffer_desc *fd)
{
	uint32_t offset = 0;
	if (data==NULL)
		return;
	fd->fb_type        = dtoh32o(data, offset);
	fd->data_start     = dtoh32o(data, offset);
	fd->buffer_width   = dtoh32o(data, offset);
	fd->visible_width  = dtoh32o(data, offset);
	fd->visible_height = dtoh32o(data, offset);
	fd->margin_left    = dtoh32o(data, offset);
	fd->margin_top     = dtoh32o(data, offset);
	fd->margin_right   = dtoh32o(data, offset);
	fd->margin_bot     = dtoh32o(data, offset);
}

static inline int
ptp_unpack_StreamInfo (PTPParams *params, const unsigned char *data, PTPStreamInfo *si, unsigned int size)
{
	uint32_t offset = 0;
	if (!data) return PTP_RC_GeneralError;
	if (size < 36) return PTP_RC_GeneralError;

	si->DatasetSize      = dtoh64o(data, offset);
	si->TimeResolution   = dtoh64o(data, offset);
	si->FrameHeaderSize  = dtoh32o(data, offset);
	si->FrameMaxSize     = dtoh32o(data, offset);
	si->PacketHeaderSize = dtoh32o(data, offset);
	si->PacketMaxSize    = dtoh32o(data, offset);
	si->PacketAlignment  = dtoh32o(data, offset);
	return PTP_RC_OK;
}
