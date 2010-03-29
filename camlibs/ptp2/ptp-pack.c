/* currently this file is included into ptp.c */

#ifdef HAVE_ICONV
#include <iconv.h>
#endif

extern void
ptp_debug (PTPParams *params, const char *format, ...);

static inline uint16_t
htod16p (PTPParams *params, uint16_t var)
{
	return ((params->byteorder==PTP_DL_LE)?htole16(var):htobe16(var));
}

static inline uint32_t
htod32p (PTPParams *params, uint32_t var)
{
	return ((params->byteorder==PTP_DL_LE)?htole32(var):htobe32(var));
}

static inline void
htod16ap (PTPParams *params, unsigned char *a, uint16_t val)
{
	if (params->byteorder==PTP_DL_LE)
		htole16a(a,val);
	else 
		htobe16a(a,val);
}

static inline void
htod32ap (PTPParams *params, unsigned char *a, uint32_t val)
{
	if (params->byteorder==PTP_DL_LE)
		htole32a(a,val);
	else 
		htobe32a(a,val);
}

static inline void
htod64ap (PTPParams *params, unsigned char *a, uint64_t val)
{
	if (params->byteorder==PTP_DL_LE)
		htole64a(a,val);
	else 
		htobe64a(a,val);
}

static inline uint16_t
dtoh16p (PTPParams *params, uint16_t var)
{
	return ((params->byteorder==PTP_DL_LE)?le16toh(var):be16toh(var));
}

static inline uint32_t
dtoh32p (PTPParams *params, uint32_t var)
{
	return ((params->byteorder==PTP_DL_LE)?le32toh(var):be32toh(var));
}

static inline uint64_t
dtoh64p (PTPParams *params, uint64_t var)
{
	return ((params->byteorder==PTP_DL_LE)?le64toh(var):be64toh(var));
}

static inline uint16_t
dtoh16ap (PTPParams *params, const unsigned char *a)
{
	return ((params->byteorder==PTP_DL_LE)?le16atoh(a):be16atoh(a));
}

static inline uint32_t
dtoh32ap (PTPParams *params, const unsigned char *a)
{
	return ((params->byteorder==PTP_DL_LE)?le32atoh(a):be32atoh(a));
}

static inline uint64_t
dtoh64ap (PTPParams *params, const unsigned char *a)
{
	return ((params->byteorder==PTP_DL_LE)?le64atoh(a):be64atoh(a));
}

#define htod8a(a,x)	*(uint8_t*)(a) = x
#define htod16a(a,x)	htod16ap(params,a,x)
#define htod32a(a,x)	htod32ap(params,a,x)
#define htod64a(a,x)	htod64ap(params,a,x)
#define htod16(x)	htod16p(params,x)
#define htod32(x)	htod32p(params,x)
#define htod64(x)	htod64p(params,x)

#define dtoh8a(x)	(*(uint8_t*)(x))
#define dtoh16a(a)	dtoh16ap(params,a)
#define dtoh32a(a)	dtoh32ap(params,a)
#define dtoh64a(a)	dtoh64ap(params,a)
#define dtoh16(x)	dtoh16p(params,x)
#define dtoh32(x)	dtoh32p(params,x)
#define dtoh64(x)	dtoh64p(params,x)


static inline char*
ptp_unpack_string(PTPParams *params, unsigned char* data, uint16_t offset, uint8_t *len)
{
	uint8_t length;
	uint16_t string[PTP_MAXSTRLEN+1];
	/* allow for UTF-8: max of 3 bytes per UCS-2 char, plus final null */
	char loclstr[PTP_MAXSTRLEN*3+1]; 
	size_t nconv, srclen, destlen;
	char *src, *dest;

	length = dtoh8a(&data[offset]);	/* PTP_MAXSTRLEN == 255, 8 bit len */
	*len = length;
	if (length == 0)		/* nothing to do? */
		return(NULL);

	/* copy to string[] to ensure correct alignment for iconv(3) */
	memcpy(string, &data[offset+1], length * sizeof(string[0]));
	string[length] = 0x0000U;   /* be paranoid!  add a terminator. */
	loclstr[0] = '\0';
    
	/* convert from camera UCS-2 to our locale */
	src = (char *)string;
	srclen = length * sizeof(string[0]);
	dest = loclstr;
	destlen = sizeof(loclstr)-1;
	nconv = (size_t)-1;
#ifdef HAVE_ICONV
	nconv = iconv(params->cd_ucs2_to_locale, &src, &srclen, 
			&dest, &destlen);
#endif
	if (nconv == (size_t) -1) { /* do it the hard way */
		int i;
		/* try the old way, in case iconv is broken */
		for (i=0;i<length;i++) {
			if (dtoh16a(&data[offset+1+2*i])>127)
				loclstr[i] = '?';
			else
				loclstr[i] = dtoh16a(&data[offset+1+2*i]);
		}
		dest = loclstr+length;
	}
	*dest = '\0';
	loclstr[sizeof(loclstr)-1] = '\0';   /* be safe? */
	return(strdup(loclstr));
}

static inline int
ucs2strlen(uint16_t const * const unicstr)
{
	int length;
	
	/* Unicode strings are terminated with 2 * 0x00 */
	for(length = 0; unicstr[length] != 0x0000U; length ++);
	return length;
}


static inline void
ptp_pack_string(PTPParams *params, char *string, unsigned char* data, uint16_t offset, uint8_t *len)
{
	int packedlen;
	uint16_t ucs2str[PTP_MAXSTRLEN+1];
	char *ucs2strp = (char *) ucs2str;
	size_t convlen = strlen(string);

	/* Cannot exceed 255 (PTP_MAXSTRLEN) since it is a single byte, duh ... */
	memset(ucs2strp, 0, sizeof(ucs2str));  /* XXX: necessary? */
#ifdef HAVE_ICONV
	{ 
		size_t nconv;
		size_t convmax = PTP_MAXSTRLEN * 2; /* Includes the terminator */
		char *stringp = string;

		nconv = iconv(params->cd_locale_to_ucs2, &stringp, &convlen,
			&ucs2strp, &convmax);
		if (nconv == (size_t) -1)
			ucs2str[0] = 0x0000U;
	}
#else
	{
		int i;
		for (i=0;i<convlen;i++) {
			ucs2str[i] = string[i];
		}
		ucs2str[convlen] = 0;
	}
#endif
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
ptp_get_packed_stringcopy(PTPParams *params, char *string, uint32_t *packed_size)
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

static inline uint32_t
ptp_unpack_uint32_t_array(PTPParams *params, unsigned char* data, uint16_t offset, uint32_t **array)
{
	uint32_t n, i=0;

	n=dtoh32a(&data[offset]);
	*array = malloc (n*sizeof(uint32_t));
	while (n>i) {
		(*array)[i]=dtoh32a(&data[offset+(sizeof(uint32_t)*(i+1))]);
		i++;
	}
	return n;
}

static inline uint32_t
ptp_pack_uint32_t_array(PTPParams *params, uint32_t *array, uint32_t arraylen, unsigned char **data )
{
	uint32_t i=0;

	*data = malloc ((arraylen+1)*sizeof(uint32_t));
	htod32a(&(*data)[0],arraylen);
	for (i=0;i<arraylen;i++)
		htod32a(&(*data)[sizeof(uint32_t)*(i+1)], array[i]);
	return (arraylen+1)*sizeof(uint32_t);
}

static inline uint32_t
ptp_unpack_uint16_t_array(PTPParams *params, unsigned char* data, uint16_t offset, uint16_t **array)
{
	uint32_t n, i=0;

	n=dtoh32a(&data[offset]);
	*array = malloc (n*sizeof(uint16_t));
	while (n>i) {
		(*array)[i]=dtoh16a(&data[offset+(sizeof(uint16_t)*(i+2))]);
		i++;
	}
	return n;
}

/* DeviceInfo pack/unpack */

#define PTP_di_StandardVersion		 0
#define PTP_di_VendorExtensionID	 2
#define PTP_di_VendorExtensionVersion	 6
#define PTP_di_VendorExtensionDesc	 8
#define PTP_di_FunctionalMode		 8
#define PTP_di_OperationsSupported	10

static inline void
ptp_unpack_DI (PTPParams *params, unsigned char* data, PTPDeviceInfo *di, unsigned int datalen)
{
	uint8_t len;
	unsigned int totallen;

	if (!data) return;
	if (datalen < 12) return;
	di->StandardVersion = dtoh16a(&data[PTP_di_StandardVersion]);
	di->VendorExtensionID =
		dtoh32a(&data[PTP_di_VendorExtensionID]);
	di->VendorExtensionVersion =
		dtoh16a(&data[PTP_di_VendorExtensionVersion]);
	di->VendorExtensionDesc = 
		ptp_unpack_string(params, data,
		PTP_di_VendorExtensionDesc, &len); 
	totallen=len*2+1;
	di->FunctionalMode = 
		dtoh16a(&data[PTP_di_FunctionalMode+totallen]);
	di->OperationsSupported_len = ptp_unpack_uint16_t_array(params, data,
		PTP_di_OperationsSupported+totallen,
		&di->OperationsSupported);
	totallen=totallen+di->OperationsSupported_len*sizeof(uint16_t)+sizeof(uint32_t);
	di->EventsSupported_len = ptp_unpack_uint16_t_array(params, data,
		PTP_di_OperationsSupported+totallen,
		&di->EventsSupported);
	totallen=totallen+di->EventsSupported_len*sizeof(uint16_t)+sizeof(uint32_t);
	di->DevicePropertiesSupported_len =
		ptp_unpack_uint16_t_array(params, data,
		PTP_di_OperationsSupported+totallen,
		&di->DevicePropertiesSupported);
	totallen=totallen+di->DevicePropertiesSupported_len*sizeof(uint16_t)+sizeof(uint32_t);
	di->CaptureFormats_len = ptp_unpack_uint16_t_array(params, data,
		PTP_di_OperationsSupported+totallen,
		&di->CaptureFormats);
	totallen=totallen+di->CaptureFormats_len*sizeof(uint16_t)+sizeof(uint32_t);
	di->ImageFormats_len = ptp_unpack_uint16_t_array(params, data,
		PTP_di_OperationsSupported+totallen,
		&di->ImageFormats);
	totallen=totallen+di->ImageFormats_len*sizeof(uint16_t)+sizeof(uint32_t);
	di->Manufacturer = ptp_unpack_string(params, data,
		PTP_di_OperationsSupported+totallen,
		&len);
	totallen+=len*2+1;
	di->Model = ptp_unpack_string(params, data,
		PTP_di_OperationsSupported+totallen,
		&len);
	totallen+=len*2+1;
	di->DeviceVersion = ptp_unpack_string(params, data,
		PTP_di_OperationsSupported+totallen,
		&len);
	totallen+=len*2+1;
	di->SerialNumber = ptp_unpack_string(params, data,
		PTP_di_OperationsSupported+totallen,
		&len);
}

static void
ptp_free_DI (PTPDeviceInfo *di) {
	if (di->SerialNumber) free (di->SerialNumber);
	if (di->DeviceVersion) free (di->DeviceVersion);
	if (di->Model) free (di->Model);
	if (di->Manufacturer) free (di->Manufacturer);
	if (di->ImageFormats) free (di->ImageFormats);
	if (di->CaptureFormats) free (di->CaptureFormats);
	if (di->VendorExtensionDesc) free (di->VendorExtensionDesc);
	if (di->OperationsSupported) free (di->OperationsSupported);
	if (di->EventsSupported) free (di->EventsSupported);
	if (di->DevicePropertiesSupported) free (di->DevicePropertiesSupported);
}

/* EOS Device Info unpack */
static inline void
ptp_unpack_EOS_DI (PTPParams *params, unsigned char* data, PTPCanonEOSDeviceInfo *di, unsigned int datalen)
{
	int totallen = 4;
	if (datalen < 8) return;

	/* uint32_t struct len - ignore */
	di->EventsSupported_len = ptp_unpack_uint32_t_array(params, data,
		totallen, &di->EventsSupported);
	if (!di->EventsSupported) return;
	totallen += di->EventsSupported_len*sizeof(uint32_t)+4;
	if (totallen >= datalen) return;

	di->DevicePropertiesSupported_len = ptp_unpack_uint32_t_array(params, data,
		totallen, &di->DevicePropertiesSupported);
	if (!di->DevicePropertiesSupported) return;
	totallen += di->DevicePropertiesSupported_len*sizeof(uint32_t)+4;
	if (totallen >= datalen) return;

	di->unk_len = ptp_unpack_uint32_t_array(params, data,
		totallen, &di->unk);
	if (!di->unk) return;
	totallen += di->unk_len*sizeof(uint32_t)+4;
	return;
}

static inline void
ptp_free_EOS_DI (PTPCanonEOSDeviceInfo *di)
{
	free (di->EventsSupported);
	free (di->DevicePropertiesSupported);
	free (di->unk);
}
	
/* ObjectHandles array pack/unpack */

#define PTP_oh				 0

static inline void
ptp_unpack_OH (PTPParams *params, unsigned char* data, PTPObjectHandles *oh, unsigned int len)
{
	if (len) {
		oh->n = ptp_unpack_uint32_t_array(params, data, PTP_oh, &oh->Handler);
	} else {
		oh->n = 0;
		oh->Handler = NULL;
	} 
}

/* StoreIDs array pack/unpack */

#define PTP_sids			 0

static inline void
ptp_unpack_SIDs (PTPParams *params, unsigned char* data, PTPStorageIDs *sids, unsigned int len)
{
	sids->n = ptp_unpack_uint32_t_array(params, data, PTP_sids,
	&sids->Storage);
}

/* StorageInfo pack/unpack */

#define PTP_si_StorageType		 0
#define PTP_si_FilesystemType		 2
#define PTP_si_AccessCapability		 4
#define PTP_si_MaxCapability		 6
#define PTP_si_FreeSpaceInBytes		14
#define PTP_si_FreeSpaceInImages	22
#define PTP_si_StorageDescription	26

static inline void
ptp_unpack_SI (PTPParams *params, unsigned char* data, PTPStorageInfo *si, unsigned int len)
{
	uint8_t storagedescriptionlen;

	si->StorageType=dtoh16a(&data[PTP_si_StorageType]);
	si->FilesystemType=dtoh16a(&data[PTP_si_FilesystemType]);
	si->AccessCapability=dtoh16a(&data[PTP_si_AccessCapability]);
	si->MaxCapability=dtoh64a(&data[PTP_si_MaxCapability]);
	si->FreeSpaceInBytes=dtoh64a(&data[PTP_si_FreeSpaceInBytes]);
	si->FreeSpaceInImages=dtoh32a(&data[PTP_si_FreeSpaceInImages]);
	si->StorageDescription=ptp_unpack_string(params, data,
		PTP_si_StorageDescription, &storagedescriptionlen);
	si->VolumeLabel=ptp_unpack_string(params, data,
		PTP_si_StorageDescription+storagedescriptionlen*2+1,
		&storagedescriptionlen);
}

/* ObjectInfo pack/unpack */

#define PTP_oi_StorageID		 0
#define PTP_oi_ObjectFormat		 4
#define PTP_oi_ProtectionStatus		 6
#define PTP_oi_ObjectCompressedSize	 8
#define PTP_oi_ThumbFormat		12
#define PTP_oi_ThumbCompressedSize	14
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
	oidata=malloc(PTP_oi_MaxLen);
	/* the caller should free it after use! */
#if 0
	char *capture_date="20020101T010101"; /* XXX Fake date */
#endif
	memset (oidata, 0, PTP_oi_MaxLen);
	htod32a(&oidata[PTP_oi_StorageID],oi->StorageID);
	htod16a(&oidata[PTP_oi_ObjectFormat],oi->ObjectFormat);
	htod16a(&oidata[PTP_oi_ProtectionStatus],oi->ProtectionStatus);
	htod32a(&oidata[PTP_oi_ObjectCompressedSize],oi->ObjectCompressedSize);
	htod16a(&oidata[PTP_oi_ThumbFormat],oi->ThumbFormat);
	htod32a(&oidata[PTP_oi_ThumbCompressedSize],oi->ThumbCompressedSize);
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
	
	*oidataptr=oidata;
	return (PTP_oi_Filename+filenamelen*2+(capturedatelen+1)*3);
}

static time_t
ptp_unpack_PTPTIME (const char *str) {
	char ptpdate[40];
	char tmp[5];
	int  ptpdatelen;
	struct tm tm;

	if (!str)
		return 0;
	ptpdatelen = strlen(str);
	if (ptpdatelen >= sizeof (ptpdate)) {
		/*ptp_debug (params ,"datelen is larger then size of buffer", ptpdatelen, (int)sizeof(ptpdate));*/
		return 0;
	}
	strcpy (ptpdate, str);
	if (ptpdatelen<15) {
		/*ptp_debug (params ,"datelen is less than 15 (%d)", ptpdatelen);*/
		return 0;
	}

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
	return mktime (&tm);
}

static inline void
ptp_unpack_OI (PTPParams *params, unsigned char* data, PTPObjectInfo *oi, unsigned int len)
{
	uint8_t filenamelen;
	uint8_t capturedatelen;
	char *capture_date;

	oi->StorageID=dtoh32a(&data[PTP_oi_StorageID]);
	oi->ObjectFormat=dtoh16a(&data[PTP_oi_ObjectFormat]);
	oi->ProtectionStatus=dtoh16a(&data[PTP_oi_ProtectionStatus]);
	oi->ObjectCompressedSize=dtoh32a(&data[PTP_oi_ObjectCompressedSize]);
	oi->ThumbFormat=dtoh16a(&data[PTP_oi_ThumbFormat]);
	oi->ThumbCompressedSize=dtoh32a(&data[PTP_oi_ThumbCompressedSize]);
	oi->ThumbPixWidth=dtoh32a(&data[PTP_oi_ThumbPixWidth]);
	oi->ThumbPixHeight=dtoh32a(&data[PTP_oi_ThumbPixHeight]);
	oi->ImagePixWidth=dtoh32a(&data[PTP_oi_ImagePixWidth]);
	oi->ImagePixHeight=dtoh32a(&data[PTP_oi_ImagePixHeight]);
	oi->ImageBitDepth=dtoh32a(&data[PTP_oi_ImageBitDepth]);
	oi->ParentObject=dtoh32a(&data[PTP_oi_ParentObject]);
	oi->AssociationType=dtoh16a(&data[PTP_oi_AssociationType]);
	oi->AssociationDesc=dtoh32a(&data[PTP_oi_AssociationDesc]);
	oi->SequenceNumber=dtoh32a(&data[PTP_oi_SequenceNumber]);
	oi->Filename= ptp_unpack_string(params, data, PTP_oi_filenamelen, &filenamelen);

	capture_date = ptp_unpack_string(params, data,
		PTP_oi_filenamelen+filenamelen*2+1, &capturedatelen);
	/* subset of ISO 8601, without '.s' tenths of second and 
	 * time zone
	 */
	oi->CaptureDate = ptp_unpack_PTPTIME(capture_date);
	free(capture_date);

	/* now the modification date ... */
	capture_date = ptp_unpack_string(params, data,
		PTP_oi_filenamelen+filenamelen*2
		+capturedatelen*2+2,&capturedatelen);
	oi->ModificationDate = ptp_unpack_PTPTIME(capture_date);
	free(capture_date);
}

/* Custom Type Value Assignement (without Length) macro frequently used below */
#define CTVAL(target,func) {			\
	if (total - *offset < sizeof(target))	\
		return 0;			\
	target = func(&data[*offset]);		\
	*offset += sizeof(target);		\
}

#define RARR(val,member,func)	{			\
	int n,j;					\
	if (total - *offset < sizeof(uint32_t))		\
		return 0;				\
	n = dtoh32a (&data[*offset]);			\
	*offset += sizeof(uint32_t);			\
							\
	val->a.count = n;				\
	val->a.v = malloc(sizeof(val->a.v[0])*n);	\
	if (!val->a.v) return 0;			\
	for (j=0;j<n;j++)				\
		CTVAL(val->a.v[j].member, func);	\
}

static inline int
ptp_unpack_DPV (
	PTPParams *params, unsigned char* data, int *offset, int total,
	PTPPropertyValue* value, uint16_t datatype
) {
	switch (datatype) {
	case PTP_DTC_INT8:
		CTVAL(value->i8,dtoh8a);
		break;
	case PTP_DTC_UINT8:
		CTVAL(value->u8,dtoh8a);
		break;
	case PTP_DTC_INT16:
		CTVAL(value->i16,dtoh16a);
		break;
	case PTP_DTC_UINT16:
		CTVAL(value->u16,dtoh16a);
		break;
	case PTP_DTC_INT32:
		CTVAL(value->i32,dtoh32a);
		break;
	case PTP_DTC_UINT32:
		CTVAL(value->u32,dtoh32a);
		break;
	case PTP_DTC_INT64:
		CTVAL(value->i64,dtoh64a);
		break;
	case PTP_DTC_UINT64:
		CTVAL(value->u64,dtoh64a);
		break;

	case PTP_DTC_UINT128:
		*offset += 16;
		/*fprintf(stderr,"unhandled unpack of uint128n");*/
		break;
	case PTP_DTC_INT128:
		*offset += 16;
		/*fprintf(stderr,"unhandled unpack of int128n");*/
		break;



	case PTP_DTC_AINT8:
		RARR(value,i8,dtoh8a);
		break;
	case PTP_DTC_AUINT8:
		RARR(value,u8,dtoh8a);
		break;
	case PTP_DTC_AUINT16:
		RARR(value,u16,dtoh16a);
		break;
	case PTP_DTC_AINT16:
		RARR(value,i16,dtoh16a);
		break;
	case PTP_DTC_AUINT32:
		RARR(value,u32,dtoh32a);
		break;
	case PTP_DTC_AINT32:
		RARR(value,i32,dtoh32a);
		break;
	case PTP_DTC_AUINT64:
		RARR(value,u64,dtoh64a);
		break;
	case PTP_DTC_AINT64:
		RARR(value,i64,dtoh64a);
		break;
	/* XXX: other int types are unimplemented */
	/* XXX: other int arrays are unimplemented also */
	case PTP_DTC_STR: {
		uint8_t len;
		/* XXX: max size */
		value->str = ptp_unpack_string(params,data,*offset,&len);
		*offset += len*2+1;
		if (!value->str)
			return 1;
		break;
	}
	default:
		return 0;
	}
	return 1;
}

/* Device Property pack/unpack */

#define PTP_dpd_DevicePropertyCode	0
#define PTP_dpd_DataType		2
#define PTP_dpd_GetSet			4
#define PTP_dpd_FactoryDefaultValue	5

static inline int
ptp_unpack_DPD (PTPParams *params, unsigned char* data, PTPDevicePropDesc *dpd, unsigned int dpdlen)
{
	int offset=0, ret;

	memset (dpd, 0, sizeof(*dpd));
	dpd->DevicePropertyCode=dtoh16a(&data[PTP_dpd_DevicePropertyCode]);
	dpd->DataType=dtoh16a(&data[PTP_dpd_DataType]);
	dpd->GetSet=dtoh8a(&data[PTP_dpd_GetSet]);
	dpd->FormFlag=PTP_DPFF_None;

	offset = PTP_dpd_FactoryDefaultValue;
	ret = ptp_unpack_DPV (params, data, &offset, dpdlen, &dpd->FactoryDefaultValue, dpd->DataType);
	if (!ret) goto outofmemory;
	if ((dpd->DataType == PTP_DTC_STR) && (offset == dpdlen))
		return 1;
	ret = ptp_unpack_DPV (params, data, &offset, dpdlen, &dpd->CurrentValue, dpd->DataType);
	if (!ret) goto outofmemory;

	/* if offset==0 then Data Type format is not supported by this
	   code or the Data Type is a string (with two empty strings as
	   values). In both cases Form Flag should be set to 0x00 and FORM is
	   not present. */

	if (offset==PTP_dpd_FactoryDefaultValue)
		return 1;

	dpd->FormFlag=dtoh8a(&data[offset]);
	offset+=sizeof(uint8_t);

	switch (dpd->FormFlag) {
	case PTP_DPFF_Range:
		ret = ptp_unpack_DPV (params, data, &offset, dpdlen, &dpd->FORM.Range.MinimumValue, dpd->DataType);
		if (!ret) goto outofmemory;
		ret = ptp_unpack_DPV (params, data, &offset, dpdlen, &dpd->FORM.Range.MaximumValue, dpd->DataType);
		if (!ret) goto outofmemory;
		ret = ptp_unpack_DPV (params, data, &offset, dpdlen, &dpd->FORM.Range.StepSize, dpd->DataType);
		if (!ret) goto outofmemory;
		break;
	case PTP_DPFF_Enumeration: {
		int i;
#define N	dpd->FORM.Enum.NumberOfValues
		N = dtoh16a(&data[offset]);
		offset+=sizeof(uint16_t);
		dpd->FORM.Enum.SupportedValue = malloc(N*sizeof(dpd->FORM.Enum.SupportedValue[0]));
		if (!dpd->FORM.Enum.SupportedValue)
			goto outofmemory;

		memset (dpd->FORM.Enum.SupportedValue,0 , N*sizeof(dpd->FORM.Enum.SupportedValue[0]));
		for (i=0;i<N;i++) {
			ret = ptp_unpack_DPV (params, data, &offset, dpdlen, &dpd->FORM.Enum.SupportedValue[i], dpd->DataType);

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

/* (MTP) Object Property pack/unpack */
#define PTP_opd_ObjectPropertyCode	0
#define PTP_opd_DataType		2
#define PTP_opd_GetSet			4
#define PTP_opd_FactoryDefaultValue	5

static inline int
ptp_unpack_OPD (PTPParams *params, unsigned char* data, PTPObjectPropDesc *opd, unsigned int opdlen)
{
	int offset=0, ret;

	memset (opd, 0, sizeof(*opd));
	opd->ObjectPropertyCode=dtoh16a(&data[PTP_opd_ObjectPropertyCode]);
	opd->DataType=dtoh16a(&data[PTP_opd_DataType]);
	opd->GetSet=dtoh8a(&data[PTP_opd_GetSet]);

	offset = PTP_opd_FactoryDefaultValue;
	ret = ptp_unpack_DPV (params, data, &offset, opdlen, &opd->FactoryDefaultValue, opd->DataType);
	if (!ret) goto outofmemory;

	opd->GroupCode=dtoh32a(&data[offset]);
	offset+=sizeof(uint32_t);

	opd->FormFlag=dtoh8a(&data[offset]);
	offset+=sizeof(uint8_t);

	switch (opd->FormFlag) {
	case PTP_OPFF_Range:
		ret = ptp_unpack_DPV (params, data, &offset, opdlen, &opd->FORM.Range.MinimumValue, opd->DataType);
		if (!ret) goto outofmemory;
		ret = ptp_unpack_DPV (params, data, &offset, opdlen, &opd->FORM.Range.MaximumValue, opd->DataType);
		if (!ret) goto outofmemory;
		ret = ptp_unpack_DPV (params, data, &offset, opdlen, &opd->FORM.Range.StepSize, opd->DataType);
		if (!ret) goto outofmemory;
		break;
	case PTP_OPFF_Enumeration: {
		int i;
#define N	opd->FORM.Enum.NumberOfValues
		N = dtoh16a(&data[offset]);
		offset+=sizeof(uint16_t);
		opd->FORM.Enum.SupportedValue = malloc(N*sizeof(opd->FORM.Enum.SupportedValue[0]));
		if (!opd->FORM.Enum.SupportedValue)
			goto outofmemory;

		memset (opd->FORM.Enum.SupportedValue,0 , N*sizeof(opd->FORM.Enum.SupportedValue[0]));
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
	}
	return 1;
outofmemory:
	ptp_free_objectpropdesc(opd);
	return 0;
}


static inline uint32_t
ptp_pack_DPV (PTPParams *params, PTPPropertyValue* value, unsigned char** dpvptr, uint16_t datatype)
{
	unsigned char* dpv=NULL;
	uint32_t size=0;
	int	i;

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
ptp_unpack_OPL (PTPParams *params, unsigned char* data, MTPProperties **pprops, unsigned int len)
{ 
	uint32_t prop_count = dtoh32a(data);
	MTPProperties *props = NULL;
	int offset = 0, i;

	if (prop_count == 0) {
		*pprops = NULL;
		return 0;
	}
	ptp_debug (params ,"Unpacking MTP OPL, size %d (prop_count %d)", len, prop_count);
	data += sizeof(uint32_t);
	len -= sizeof(uint32_t);
	props = malloc(prop_count * sizeof(MTPProperties));
	if (!props) return 0;
	for (i = 0; i < prop_count; i++) {
		if (len <= 0) {
			ptp_debug (params ,"short MTP Object Property List at property %d (of %d)", i, prop_count);
			ptp_debug (params ,"device probably needs DEVICE_FLAG_BROKEN_MTPGETOBJPROPLIST_ALL", i);
			ptp_debug (params ,"or even DEVICE_FLAG_BROKEN_MTPGETOBJPROPLIST", i);
			qsort (props, i, sizeof(MTPProperties),_compare_func);
			*pprops = props;
			return i;
		}
		props[i].ObjectHandle = dtoh32a(data);
		data += sizeof(uint32_t);
		len -= sizeof(uint32_t);

		props[i].property = dtoh16a(data);
		data += sizeof(uint16_t);
		len -= sizeof(uint16_t);

		props[i].datatype = dtoh16a(data);
		data += sizeof(uint16_t);
		len -= sizeof(uint16_t);

		offset = 0;
		ptp_unpack_DPV(params, data, &offset, len, &props[i].propval, props[i].datatype);
		data += offset;
		len -= offset;
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
ptp_unpack_EC (PTPParams *params, unsigned char* data, PTPContainer *ec, unsigned int len)
{
	int	length;
	int	type;

	if (data==NULL)
		return;
	memset(ec,0,sizeof(*ec));
	length=dtoh32a(&data[PTP_ec_Length]);
	type = dtoh16a(&data[PTP_ec_Type]);

	ec->Code=dtoh16a(&data[PTP_ec_Code]);
	ec->Transaction_ID=dtoh32a(&data[PTP_ec_TransId]);

	if (type!=PTP_USB_CONTAINER_EVENT) {
		ptp_debug (params, "Unknown canon event type %d (code=%x,tid=%x), please report!",type,ec->Code,ec->Transaction_ID);
		return;
	}
	if (length>=(PTP_ec_Param1+4)) {
		ec->Param1=dtoh32a(&data[PTP_ec_Param1]);
		ec->Nparam=1;
	}
	if (length>=(PTP_ec_Param2+4)) {
		ec->Param2=dtoh32a(&data[PTP_ec_Param2]);
		ec->Nparam=2;
	}
	if (length>=(PTP_ec_Param3+4)) {
		ec->Param3=dtoh32a(&data[PTP_ec_Param3]);
		ec->Nparam=3;
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
ptp_unpack_Canon_FE (PTPParams *params, unsigned char* data, PTPCANONFolderEntry *fe)
{
	int i;
	if (data==NULL)
		return;
	fe->ObjectHandle=dtoh32a(&data[PTP_cfe_ObjectHandle]);
	fe->ObjectFormatCode=dtoh16a(&data[PTP_cfe_ObjectFormatCode]);
	fe->Flags=dtoh8a(&data[PTP_cfe_Flags]);
	fe->ObjectSize=dtoh32a((unsigned char*)&data[PTP_cfe_ObjectSize]);
	fe->Time=(time_t)dtoh32a(&data[PTP_cfe_Time]);
	for (i=0; i<PTP_CANON_FilenameBufferLen; i++)
		fe->Filename[i]=(char)dtoh8a(&data[PTP_cfe_Filename+i]);
}

static inline uint16_t
ptp_unpack_EOS_ImageFormat (PTPParams* params, unsigned char** data )
{
	/*
	  EOS ImageFormat entries (of at least the 5DMII and the 400D ) look like this:
		uint32: number of entries / generated files (1 or 2)
		uint32: size of this entry in bytes (most likely allways 0x10)
		uint32: image type (1 == JPG, 6 == RAW)
		uint32: image size (0 == Large, 1 == Medium, 2 == Small)
		uint32: image compression (2 == Standard/JPG, 3 == Fine/JPG, 4 == Lossles/RAW)
	  If number of entries is 2 the last uint32 repeat.

	  example:
		0: 0x       1
		1: 0x      10
		2: 0x       6
		3: 0x       1
		4: 0x       4

	  The idea is to simply 'condense' these values to just one uint16 to be able to conveniontly
	  use the available enumeration facilities (look-up table). The image size and compression
	  values fully describe the image format. Hence we generate a uint16 with the four nibles set
	  as follows: entry 1 size | entry 1 compression | entry 2 size | entry 2 compression.
	  The above example would result in the value 0x1400.
	  */

	const unsigned char* d = *data;
	uint32_t n = dtoh32a( d );
	uint32_t l, s1, c1, s2 = 0, c2 = 0;

	if (n != 1 && n !=2) {
		ptp_debug (params, "parsing EOS ImageFormat property failed (n != 1 && n != 2: %d)", n);
		return 0;
	}

	l = dtoh32a( d+=4 );
	if (l != 0x10) {
		ptp_debug (params, "parsing EOS ImageFormat property failed (l != 0x10: 0x%x)", l);
		return 0;
	}

	d+=4; /* skip type */
	s1 = dtoh32a( d+=4 );
	c1 = dtoh32a( d+=4 );

	if (n == 2) {
		l = dtoh32a( d+=4 );
		if (l != 0x10) {
			ptp_debug (params, "parsing EOS ImageFormat property failed (l != 0x10: 0x%x)", l);
			return 0;
		}
		d+=4; /* skip type */
		s2 = dtoh32a( d+=4 );
		c2 = dtoh32a( d+=4 );
	}

	*data = (unsigned char*) d+4;

	return ((s1 & 0xF) << 12) | ((c1 & 0xF) << 8) | ((s2 & 0xF) << 4) | ((c2 & 0xF) << 0);
}

static inline uint32_t
ptp_pack_EOS_ImageFormat (PTPParams* params, unsigned char* data, uint16_t value)
{
	uint32_t n = (value & 0xFF) ? 2 : 1;
	uint32_t s = 4 + 0x10 * n;

	if( !data )
		return s;

	htod32a(data+=0, n);
	htod32a(data+=4, 0x10);
	htod32a(data+=4, ((value >> 8) & 0xF) == 4 ? 6 : 1);
	htod32a(data+=4, (value >> 12) & 0xF);
	htod32a(data+=4, (value >> 8) & 0xF);

	if (n==2) {
		htod32a(data+=4, 0x10);
		htod32a(data+=4, ((value >> 0) & 0xF) == 4 ? 6 : 1);
		htod32a(data+=4, (value >> 4) & 0xF);
		htod32a(data+=4, (value >> 0) & 0xF);
	}

	return s;
}

/*
    PTP EOS Changes Entry unpack
*/
#define PTP_ece_Size		0
#define PTP_ece_Type		4

#define PTP_ece_Prop_Subtype	8	/* only for properties */
#define PTP_ece_Prop_Val_Data	0xc	/* only for properties */
#define PTP_ece_Prop_Desc_Type	0xc	/* only for property descs */
#define PTP_ece_Prop_Desc_Count	0x10	/* only for property descs */
#define PTP_ece_Prop_Desc_Data	0x14	/* only for property descs */

/* for PTP_EC_CANON_EOS_RequestObjectTransfer */
#define PTP_ece_OI_ObjectID	8
#define PTP_ece_OI_OFC		0x0c
#define PTP_ece_OI_Size		0x14
#define PTP_ece_OI_Name		0x1c

/* for PTP_EC_CANON_EOS_ObjectAddedEx */
#define PTP_ece_OA_ObjectID	8
#define PTP_ece_OA_StorageID	0x0c
#define PTP_ece_OA_OFC		0x10
#define PTP_ece_OA_Size		0x1c
#define PTP_ece_OA_Parent	0x20
#define PTP_ece_OA_Name		0x28

static inline int
ptp_unpack_CANON_changes (PTPParams *params, unsigned char* data, int datasize, PTPCanon_changes_entry **ce)
{
	int	i = 0, entries = 0;
	unsigned char	*curdata = data;

	if (data==NULL)
		return 0;
	while (curdata - data < datasize) {
		uint32_t	size = dtoh32a(&curdata[PTP_ece_Size]);
		uint32_t	type = dtoh32a(&curdata[PTP_ece_Type]);

		curdata += size;
		if ((size == 8) && (type == 0))
			break;
		entries++;
	}
	*ce = malloc (sizeof(PTPCanon_changes_entry)*(entries+1));
	if (!*ce) return 0;

	curdata = data;
	while (curdata - data < datasize) {
		uint32_t	size = dtoh32a(&curdata[PTP_ece_Size]);
		uint32_t	type = dtoh32a(&curdata[PTP_ece_Type]);

		(*ce)[i].type = PTP_CANON_EOS_CHANGES_TYPE_UNKNOWN;
		switch (type) {
		case  PTP_EC_CANON_EOS_ObjectAddedEx:
			(*ce)[i].type = PTP_CANON_EOS_CHANGES_TYPE_OBJECTINFO;
			(*ce)[i].u.object.oid    		= dtoh32a(&curdata[PTP_ece_OA_ObjectID]);
			(*ce)[i].u.object.oi.StorageID 		= dtoh32a(&curdata[PTP_ece_OA_StorageID]);
			(*ce)[i].u.object.oi.ParentObject	= dtoh32a(&curdata[PTP_ece_OA_Parent]);
			(*ce)[i].u.object.oi.ObjectFormat 	= dtoh16a(&curdata[PTP_ece_OA_OFC]);
			(*ce)[i].u.object.oi.ObjectCompressedSize= dtoh32a(&curdata[PTP_ece_OA_Size]);
			(*ce)[i].u.object.oi.Filename 		= strdup(((char*)&curdata[PTP_ece_OA_Name]));
			ptp_debug (params, "event %d: objectinfo added oid %08lx, parent %08lx, ofc %04x, size %d, filename %s", i, (*ce)[i].u.object.oid, (*ce)[i].u.object.oi.ParentObject, (*ce)[i].u.object.oi.ObjectFormat, (*ce)[i].u.object.oi.ObjectCompressedSize, (*ce)[i].u.object.oi.Filename);
			break;
		case  PTP_EC_CANON_EOS_RequestObjectTransfer:
			(*ce)[i].type = PTP_CANON_EOS_CHANGES_TYPE_OBJECTTRANSFER;
			(*ce)[i].u.object.oid    		= dtoh32a(&curdata[PTP_ece_OI_ObjectID]);
			(*ce)[i].u.object.oi.StorageID 		= 0; /* use as marker */
			(*ce)[i].u.object.oi.ObjectFormat 	= dtoh16a(&curdata[PTP_ece_OI_OFC]);
			(*ce)[i].u.object.oi.ParentObject	= 0; /* check, but use as marker */
			(*ce)[i].u.object.oi.ObjectCompressedSize = dtoh32a(&curdata[PTP_ece_OI_Size]);
			(*ce)[i].u.object.oi.Filename 		= strdup(((char*)&curdata[PTP_ece_OI_Name]));

			ptp_debug (params, "event %d: request object transfer oid %08lx, ofc %04x, size %d, filename %s", i, (*ce)[i].u.object.oid, (*ce)[i].u.object.oi.ObjectFormat, (*ce)[i].u.object.oi.ObjectCompressedSize, (*ce)[i].u.object.oi.Filename);
			break;
		case  PTP_EC_CANON_EOS_AvailListChanged: {	/* property desc */
			uint32_t	proptype = dtoh32a(&curdata[PTP_ece_Prop_Subtype]);
			uint32_t	propxtype = dtoh32a(&curdata[PTP_ece_Prop_Desc_Type]);
			uint32_t	propxcnt = dtoh32a(&curdata[PTP_ece_Prop_Desc_Count]);
			unsigned char	*data = &curdata[PTP_ece_Prop_Desc_Data];
			int		j;
			PTPDevicePropDesc	*dpd;

			ptp_debug (params, "event %d: EOS prop %04x desc record, datasize %d, propxtype %d", i, proptype, size-PTP_ece_Prop_Desc_Data, propxtype);
			for (j=0;j<params->nrofcanon_props;j++)
				if (params->canon_props[j].proptype == proptype)
					break;
			if (j==params->nrofcanon_props) {
				ptp_debug (params, "event %d: propdesc %x, default value not found.", i, proptype);
				break;
			}
			dpd = &params->canon_props[j].dpd;
			/* 1 - uint16 ? 
			 * 3 - uint16
			 * 7 - string?
			 */
			if (propxtype != 3) {
				ptp_debug (params, "event %d: propxtype is %x for %04x, unhandled.", i, propxtype, proptype);
				for (j=0;j<size-PTP_ece_Prop_Desc_Data;j++)
					ptp_debug (params, "    %d: %02x", j, data[j]);
				break;
			}
			if (! propxcnt)
				break;

			ptp_debug (params, "event %d: propxtype is %x, prop is 0x%04x, data type is 0x%04x, propxcnt is %d.",
				   i, propxtype, proptype, dpd->DataType, propxcnt);
			dpd->FormFlag = PTP_DPFF_Enumeration;
			dpd->FORM.Enum.NumberOfValues = propxcnt;
			if (dpd->FORM.Enum.SupportedValue) free (dpd->FORM.Enum.SupportedValue);
			dpd->FORM.Enum.SupportedValue = malloc (sizeof (PTPPropertyValue)*propxcnt);

			switch (proptype) {
			case PTP_DPC_CANON_EOS_ImageFormat:
			case PTP_DPC_CANON_EOS_ImageFormatCF:
			case PTP_DPC_CANON_EOS_ImageFormatSD:
			case PTP_DPC_CANON_EOS_ImageFormatExtHD:
				/* special handling of ImageFormat properties */
				for (j=0;j<propxcnt;j++) {
					dpd->FORM.Enum.SupportedValue[j].u16 =
							dtoh16( ptp_unpack_EOS_ImageFormat( params, &data ) );
					ptp_debug (params, "event %d: suppval[%d] of %x is 0x%x.", i, j, proptype, dpd->FORM.Enum.SupportedValue[j].u16);
				}
				break;
			default:
				/* 'normal' enumerated types */
				switch (dpd->DataType) {
#define XX( TYPE, CONV )\
					for (j=0;j<propxcnt;j++) { \
						dpd->FORM.Enum.SupportedValue[j].TYPE = CONV(data); \
						ptp_debug (params, "event %d: suppval[%d] of %x is 0x%x.", i, j, proptype, CONV(data)); \
						data += 4; /* might only be for propxtype 3 */ \
					} \
					break;

				case PTP_DTC_INT16:	XX( i16, dtoh16a );
				case PTP_DTC_UINT32:	XX( u32, dtoh32a );
				case PTP_DTC_UINT16:	XX( u16, dtoh16a );
				case PTP_DTC_UINT8:	XX( u8,  dtoh8a );
#undef XX
				default:
					ptp_debug (params ,"event %d: data type 0x%04x of %x unhandled, raw values:", i, dpd->DataType, proptype, dtoh32a(data));
					for (j=0;j<(size-PTP_ece_Prop_Desc_Data)/4;j++, data+=4) /* 4 is good for propxtype 3 */
						ptp_debug (params, "    %3d: 0x%8x", j, dtoh32a(data));
					break;
				}
			}
			break;
		}
		case PTP_EC_CANON_EOS_PropValueChanged:
			if (size >= 0xc) {	/* property info */
				int j;
				uint32_t	proptype = dtoh32a(&curdata[PTP_ece_Prop_Subtype]);
				unsigned char	*data = &curdata[PTP_ece_Prop_Val_Data];
				PTPDevicePropDesc	*dpd;

				ptp_debug (params, "event %d: EOS prop %04x info record, datasize is %d", i, proptype, size-PTP_ece_Prop_Val_Data);
				for (j=0;j<params->nrofcanon_props;j++)
					if (params->canon_props[j].proptype == proptype)
						break;
				if (j<params->nrofcanon_props) {
					if (	(params->canon_props[j].size != size) ||
						(memcmp(params->canon_props[j].data,data,size-PTP_ece_Prop_Val_Data))) {
						params->canon_props[j].data = realloc(params->canon_props[j].data,size-PTP_ece_Prop_Val_Data);
						memcpy (params->canon_props[j].data,data,size-PTP_ece_Prop_Val_Data);
					}
				} else {
					if (j)
						params->canon_props = realloc(params->canon_props, sizeof(params->canon_props[0])*(j+1));
					else
						params->canon_props = malloc(sizeof(params->canon_props[0]));
					params->canon_props[j].type = type;
					params->canon_props[j].proptype = proptype;
					params->canon_props[j].size = size;
					params->canon_props[j].data = malloc(size-PTP_ece_Prop_Val_Data);
					memcpy(params->canon_props[j].data, data, size-PTP_ece_Prop_Val_Data);
					memset (&params->canon_props[j].dpd,0,sizeof(params->canon_props[j].dpd));
					params->canon_props[j].dpd.GetSet = 1;
					params->canon_props[j].dpd.FormFlag = PTP_DPFF_None;
					params->nrofcanon_props = j+1;
				}
				dpd = &params->canon_props[j].dpd;

				/* fix GetSet value */
				switch (proptype) {
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
#undef XX
						dpd->GetSet = PTP_DPGS_Get;
						break;
				}

				/* set DataType */
				switch (proptype) {
				case PTP_DPC_CANON_EOS_CameraTime:
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
					dpd->DataType = PTP_DTC_UINT32;
					break;
				/* enumeration for AEM is never provided, but is available to set */
				case PTP_DPC_CANON_EOS_AutoExposureMode:
					dpd->DataType = PTP_DTC_UINT16;
					dpd->FormFlag = PTP_DPFF_Enumeration;
					dpd->FORM.Enum.NumberOfValues = 0;
					break;
				case PTP_DPC_CANON_EOS_Aperture:
				case PTP_DPC_CANON_EOS_ShutterSpeed:
				case PTP_DPC_CANON_EOS_ISOSpeed:
				case PTP_DPC_CANON_EOS_FocusMode:
				case PTP_DPC_CANON_EOS_ColorSpace:
				case PTP_DPC_CANON_EOS_BatteryPower:
				case PTP_DPC_CANON_EOS_BatterySelect:
				case PTP_DPC_CANON_EOS_PTPExtensionVersion:
				case PTP_DPC_CANON_EOS_DriveMode:
				case PTP_DPC_CANON_EOS_AEB:
				case PTP_DPC_CANON_EOS_BracketMode:
				case PTP_DPC_CANON_EOS_QuickReviewTime:
				case PTP_DPC_CANON_EOS_EVFMode:
				case PTP_DPC_CANON_EOS_EVFOutputDevice:
					dpd->DataType = PTP_DTC_UINT16;
					break;
				case PTP_DPC_CANON_EOS_PictureStyle:
				case PTP_DPC_CANON_EOS_WhiteBalance:
				case PTP_DPC_CANON_EOS_MeteringMode:
				case PTP_DPC_CANON_EOS_ExpCompensation: /* actually int8 if you calculate */
					dpd->DataType = PTP_DTC_UINT8;
					break;
				case PTP_DPC_CANON_EOS_Owner:
				case PTP_DPC_CANON_EOS_Artist:
				case PTP_DPC_CANON_EOS_Copyright:
				case PTP_DPC_CANON_EOS_SerialNumber:
				case PTP_DPC_CANON_EOS_LensName:
					dpd->DataType = PTP_DTC_STR;
					break;
				case PTP_DPC_CANON_EOS_WhiteBalanceAdjustA:
				case PTP_DPC_CANON_EOS_WhiteBalanceAdjustB:
					dpd->DataType = PTP_DTC_INT16;
					break;
				/* unknown props, listed from dump.... all 16 bit, but vals might be smaller */
				case 0xd114:
				case PTP_DPC_CANON_EOS_DPOFVersion:
					dpd->DataType = PTP_DTC_UINT16;
					ptp_debug (params, "event %d: Unknown EOS property %04x, datasize is %d, using uint16", i ,proptype, size-PTP_ece_Prop_Val_Data);
					for (j=0;j<size-PTP_ece_Prop_Val_Data;j++)
						ptp_debug (params, "    %d: %02x", j, data[j]);
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
					ptp_debug (params, "event %d: Unknown EOS property %04x, datasize is %d, using uint8", i ,proptype, size-PTP_ece_Prop_Val_Data);
					for (j=0;j<size-PTP_ece_Prop_Val_Data;j++)
						ptp_debug (params, "    %d: %02x", j, data[j]);
					/* custom func entries look like this on the 400D: '5 0 0 0 ?' = 4 bytes size + 1 byte data */
					data += 4;
					break;
				/* ImageFormat special handling */
				case PTP_DPC_CANON_EOS_ImageFormat:
				case PTP_DPC_CANON_EOS_ImageFormatCF:
				case PTP_DPC_CANON_EOS_ImageFormatSD:
				case PTP_DPC_CANON_EOS_ImageFormatExtHD:
					dpd->DataType = PTP_DTC_UINT16;
					dpd->FactoryDefaultValue.u16	= ptp_unpack_EOS_ImageFormat( params, &data );
					dpd->CurrentValue.u16		= dpd->FactoryDefaultValue.u16;
					ptp_debug (params,"event %d: decoded imageformat, currentvalue of %x is %x", i, proptype, dpd->CurrentValue.u16);
					break;
				/* yet unknown 32bit props */
				case PTP_DPC_CANON_EOS_ColorTemperature:
				case PTP_DPC_CANON_EOS_WftStatus:
				case PTP_DPC_CANON_EOS_LensStatus:
				case PTP_DPC_CANON_EOS_CardExtension:
				case PTP_DPC_CANON_EOS_TempStatus:
				case PTP_DPC_CANON_EOS_PhotoStudioMode:
				case PTP_DPC_CANON_EOS_DepthOfFieldPreview:
				case PTP_DPC_CANON_EOS_EVFSharpness:
				case PTP_DPC_CANON_EOS_EVFWBMode:
				case PTP_DPC_CANON_EOS_EVFClickWBCoeffs:
				case PTP_DPC_CANON_EOS_EVFColorTemp:
				case PTP_DPC_CANON_EOS_EVFRecordStatus:
				case PTP_DPC_CANON_EOS_ExposureSimMode:
				case PTP_DPC_CANON_EOS_LvAfSystem:
				case PTP_DPC_CANON_EOS_MovSize:
				case PTP_DPC_CANON_EOS_DepthOfField:
				case PTP_DPC_CANON_EOS_LvViewTypeSelect:
				case PTP_DPC_CANON_EOS_CustomFuncEx:
					dpd->DataType = PTP_DTC_UINT32;
					ptp_debug (params, "event %d: Unknown EOS property %04x, datasize is %d, using uint32", i ,proptype, size-PTP_ece_Prop_Val_Data);
					if ((size-PTP_ece_Prop_Val_Data) % sizeof(uint32_t) != 0)
						ptp_debug (params, "event %d: Warning: datasize modulo sizeof(uint32) is not 0: ", i, (size-PTP_ece_Prop_Val_Data) % sizeof(uint32_t) );
					for (j=0;j<(size-PTP_ece_Prop_Val_Data)/sizeof(uint32_t);j++)
						ptp_debug (params, "    %d: 0x%8x", j, ((uint32_t*)data)[j]);
					break;
				default:
					ptp_debug (params, "event %d: Unknown EOS property %04x, datasize is %d", i ,proptype, size-PTP_ece_Prop_Val_Data);
					for (j=0;j<size-PTP_ece_Prop_Val_Data;j++)
						ptp_debug (params, "    %d: %02x", j, data[j]);
					break;
				}
				switch (dpd->DataType) {
				case PTP_DTC_UINT32:
					dpd->FactoryDefaultValue.u32	= dtoh32a(data);
					dpd->CurrentValue.u32		= dtoh32a(data);
					ptp_debug (params ,"event %d: currentvalue of %x is %x", i, proptype, dpd->CurrentValue.u32);
					break;
				case PTP_DTC_UINT16:
					dpd->FactoryDefaultValue.u16	= dtoh16a(data);
					dpd->CurrentValue.u16		= dtoh16a(data);
					ptp_debug (params,"event %d: currentvalue of %x is %x", i, proptype, dpd->CurrentValue.u16);
					break;
				case PTP_DTC_UINT8:
					dpd->FactoryDefaultValue.u8	= dtoh8a(data);
					dpd->CurrentValue.u8		= dtoh8a(data);
					ptp_debug (params,"event %d: currentvalue of %x is %x", i, proptype, dpd->CurrentValue.u8);
					break;
				case PTP_DTC_STR: {
#if 0 /* 5D MII and 400D aktually store plain ASCII in their string properties */
					uint8_t len = 0;
					dpd->FactoryDefaultValue.str	= ptp_unpack_string(params, data, 0, &len);
					dpd->CurrentValue.str		= ptp_unpack_string(params, data, 0, &len);
#else
					if (dpd->FactoryDefaultValue.str) free (dpd->FactoryDefaultValue.str);
					dpd->FactoryDefaultValue.str	= strdup( (char*)data );

					if (dpd->CurrentValue.str) free (dpd->CurrentValue.str);
					dpd->CurrentValue.str		= strdup( (char*)data );
#endif
					ptp_debug (params,"event %d: currentvalue of %x is %s", i, proptype, dpd->CurrentValue.str);
					break;
				}
				default:
					/* debug is printed in switch above this one */
					break;
				}

				break;
		}
		case 0: /* end marker */
			if (size == 8) /* no output */
				break;
			ptp_debug (params, "event %d: EOS event 0, but size %d", i, size);
			break;
		default:
			switch (type) {
#define XX(x)		case PTP_EC_CANON_EOS_##x: ptp_debug (params, "event %d: unhandled EOS event "#x" (size %d)", i, size);break;
			XX(RequestGetEvent)
			XX(ObjectRemoved)
			XX(RequestGetObjectInfoEx)
			XX(StorageStatusChanged)
			XX(StorageInfoChanged)
			XX(ObjectInfoChangedEx)
			XX(ObjectContentChanged)
			XX(CameraStatusChanged)
			XX(WillSoonShutdown)
			XX(ShutdownTimerUpdated)
			XX(RequestCancelTransfer)
			XX(RequestObjectTransferDT)
			XX(RequestCancelTransferDT)
			XX(StoreAdded)
			XX(StoreRemoved)
			XX(BulbExposureTime)
			XX(RecordingTime)
			XX(RequestObjectTransferTS)
			XX(AfResult)
#undef XX
			default:
				ptp_debug (params, "event %d: unknown EOS event %04x", i, type);
				break;
			}
			if (size >= 0x8) {	/* event info */
				int j;
				for (j=8;j<size;j++)
					ptp_debug (params, "    %d: %02x", j, curdata[j]);
			}
			(*ce)[i].type = PTP_CANON_EOS_CHANGES_TYPE_UNKNOWN;
			break;
		}
		curdata += size;
		i++;
		if ((size == 8) && (type == 0))
			break;
	}
	if (!entries) {
		free (*ce);
		*ce = NULL;
	}
	return entries;
}

/*
    PTP USB Event container unpack for Nikon events.
*/
#define PTP_nikon_ec_Length		0
#define PTP_nikon_ec_Code		2
#define PTP_nikon_ec_Param1		4
#define PTP_nikon_ec_Size		6
static inline void
ptp_unpack_Nikon_EC (PTPParams *params, unsigned char* data, unsigned int len, PTPContainer **ec, int *cnt)
{
	int i;

	*ec = NULL;
	if (data == NULL)
		return;
	if (len < PTP_nikon_ec_Code)
		return;
	*cnt = dtoh16a(&data[PTP_nikon_ec_Length]);
	if (*cnt > (len-PTP_nikon_ec_Code)/PTP_nikon_ec_Size) /* broken cnt? */
		return;
	if (!*cnt)
		return;

	*ec = malloc(sizeof(PTPContainer)*(*cnt));
	
	for (i=0;i<*cnt;i++) {
		memset(&(*ec)[i],0,sizeof(PTPContainer));
		(*ec)[i].Code	= dtoh16a(&data[PTP_nikon_ec_Code+PTP_nikon_ec_Size*i]);
		(*ec)[i].Param1	= dtoh32a(&data[PTP_nikon_ec_Param1+PTP_nikon_ec_Size*i]);
		(*ec)[i].Nparam	= 1;
	}
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
	handles->Handler = calloc(sizeof(handles->Handler[0]),nrofobs);
	if (!handles->Handler) return PTP_RC_GeneralError;
	*oinfos = calloc(sizeof((*oinfos)[0]),nrofobs);
	if (!*oinfos) return PTP_RC_GeneralError;
	*flags  = calloc(sizeof((*flags)[0]),nrofobs);
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
		oi->ObjectCompressedSize= dtoh32a(cur + ptp_canon_dir_size);
		oi->ThumbCompressedSize	= dtoh32a(cur + ptp_canon_dir_thumbsize);
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
		int changed = 0;
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
