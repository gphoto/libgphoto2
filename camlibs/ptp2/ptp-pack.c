/* curently this file is included into ptp.c */

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
		htole16a(a,val); else 
		htobe16a(a,val);
}

static inline void
htod32ap (PTPParams *params, unsigned char *a, uint32_t val)
{
	if (params->byteorder==PTP_DL_LE)
		htole32a(a,val); else 
		htobe32a(a,val);
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

static inline uint16_t
dtoh16ap (PTPParams *params, unsigned char *a)
{
	return ((params->byteorder==PTP_DL_LE)?le16atoh(a):be16atoh(a));
}

static inline uint32_t
dtoh32ap (PTPParams *params, unsigned char *a)
{
	return ((params->byteorder==PTP_DL_LE)?le32atoh(a):be32atoh(a));
}

#define htod8a(a,x)	*(uint8_t*)(a) = x
#define htod16a(a,x)	htod16ap(params,a,x)
#define htod32a(a,x)	htod32ap(params,a,x)
#define htod16(x)	htod16p(params,x)
#define htod32(x)	htod32p(params,x)

#define dtoh8a(x)	(*(uint8_t*)(x))
#define dtoh16a(a)	dtoh16ap(params,a)
#define dtoh32a(a)	dtoh32ap(params,a)
#define dtoh16(x)	dtoh16p(params,x)
#define dtoh32(x)	dtoh32p(params,x)


static inline char*
ptp_unpack_string(PTPParams *params, char* data, uint16_t offset, uint8_t *len)
{
	int i;
	char *string=NULL;

	*len=dtoh8a(&data[offset]);
	if (*len) {
		string=malloc(*len);
		memset(string, 0, *len);
		for (i=0;i<*len && i< PTP_MAXSTRLEN; i++) {
			string[i]=(char)dtoh16a(&data[offset+i*2+1]);
		}
		/* be paranoid! :( */
		string[*len-1]=0;
	}
	return (string);
}

static inline void
ptp_pack_string(PTPParams *params, char *string, char* data, uint16_t offset, uint8_t *len)
{
	int i;
	*len = (uint8_t)strlen(string);
	
	/* XXX: check strlen! */
	htod8a(&data[offset],*len+1);
	for (i=0;i<*len && i< PTP_MAXSTRLEN; i++) {
		htod16a(&data[offset+i*2+1],(uint16_t)string[i]);
	}
}

static inline uint32_t
ptp_unpack_uint32_t_array(PTPParams *params, char* data, uint16_t offset, uint32_t **array)
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
ptp_unpack_uint16_t_array(PTPParams *params, char* data, uint16_t offset, uint16_t **array)
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
ptp_unpack_DI (PTPParams *params, char* data, PTPDeviceInfo *di)
{
	uint8_t len;
	unsigned int totallen;
	
	di->StaqndardVersion = dtoh16a(&data[PTP_di_StandardVersion]);
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
	
/* ObjectHandles array pack/unpack */

#define PTP_oh				 0

static inline void
ptp_unpack_OH (PTPParams *params, char* data, PTPObjectHandles *oh)
{
	oh->n = ptp_unpack_uint32_t_array(params, data, PTP_oh, &oh->Handler);
}

/* StoreIDs array pack/unpack */

#define PTP_sids			 0

static inline void
ptp_unpack_SIDs (PTPParams *params, char* data, PTPStorageIDs *sids)
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
ptp_unpack_SI (PTPParams *params, char* data, PTPStorageInfo *si)
{
	uint8_t storagedescriptionlen;

	si->StorageType=dtoh16a(&data[PTP_si_StorageType]);
	si->FilesystemType=dtoh16a(&data[PTP_si_FilesystemType]);
	si->AccessCapability=dtoh16a(&data[PTP_si_AccessCapability]);
	/* XXX no dtoh64a !!! skiping next two */
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

static inline uint32_t
ptp_pack_OI (PTPParams *params, PTPObjectInfo *oi, char** oidataptr)
{
	char* oidata;
	uint8_t filenamelen;
	uint8_t capturedatelen=0;
	/* let's allocate some memory first; XXX i'm sure it's wrong */
	oidata=malloc(PTP_oi_Filename+(strlen(oi->Filename)+1)*2+4);
	/* the caller should free it after use! */
#if 0
	char *capture_date="20020101T010101"; /* XXX Fake date */
#endif
	memset (oidata, 0, (PTP_oi_Filename+(strlen(oi->Filename)+1)*2+4));
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
	return (PTP_oi_Filename+(filenamelen+1)*2+(capturedatelen+1)*4);
}

static inline void
ptp_unpack_OI (PTPParams *params, char* data, PTPObjectInfo *oi)
{
	uint8_t filenamelen;
	uint8_t capturedatelen;
	char *capture_date;
	char tmp[16];
	struct tm tm;

	memset(&tm,0,sizeof(tm));

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
	if (capturedatelen>15)
	{
		strncpy (tmp, capture_date, 4);
		tmp[4] = 0;
		tm.tm_year=atoi (tmp) - 1900;
		strncpy (tmp, capture_date + 4, 2);
		tmp[2] = 0;
		tm.tm_mon = atoi (tmp) - 1;
		strncpy (tmp, capture_date + 6, 2);
		tmp[2] = 0;
		tm.tm_mday = atoi (tmp);
		strncpy (tmp, capture_date + 9, 2);
		tmp[2] = 0;
		tm.tm_hour = atoi (tmp);
		strncpy (tmp, capture_date + 11, 2);
		tmp[2] = 0;
		tm.tm_min = atoi (tmp);
		strncpy (tmp, capture_date + 13, 2);
		tmp[2] = 0;
		tm.tm_sec = atoi (tmp);
		oi->CaptureDate=mktime (&tm);
	}
	free(capture_date);

	/* now it's modification date ;) */
	capture_date = ptp_unpack_string(params, data,
		PTP_oi_filenamelen+filenamelen*2
		+capturedatelen*2+2,&capturedatelen);
	if (capturedatelen>15)
	{
		strncpy (tmp, capture_date, 4);
		tmp[4] = 0;
		tm.tm_year=atoi (tmp) - 1900;
		strncpy (tmp, capture_date + 4, 2);
		tmp[2] = 0;
		tm.tm_mon = atoi (tmp) - 1;
		strncpy (tmp, capture_date + 6, 2);
		tmp[2] = 0;
		tm.tm_mday = atoi (tmp);
		strncpy (tmp, capture_date + 9, 2);
		tmp[2] = 0;
		tm.tm_hour = atoi (tmp);
		strncpy (tmp, capture_date + 11, 2);
		tmp[2] = 0;
		tm.tm_min = atoi (tmp);
		strncpy (tmp, capture_date + 13, 2);
		tmp[2] = 0;
		tm.tm_sec = atoi (tmp);
		oi->ModificationDate=mktime (&tm);
	}
	free(capture_date);
}

/* Custom Type Value Assignement (without Length) macro frequently used below */
#define CTVAL(type,func,target)  {					\
		*target = malloc(sizeof(type));				\
		**(type **)target =					\
			func(data);\
}

static inline void
ptp_unpack_DPV (PTPParams *params, char* data, void** value, uint16_t datatype)
{

	switch (datatype) {
		case PTP_DTC_INT8:
			CTVAL(int8_t,dtoh8a,value);
			break;
		case PTP_DTC_UINT8:
			CTVAL(uint8_t,dtoh8a,value);
			break;
		case PTP_DTC_INT16:
			CTVAL(int16_t,dtoh16a,value);
			break;
		case PTP_DTC_UINT16:
			CTVAL(uint16_t,dtoh16a,value);
			break;
		case PTP_DTC_INT32:
			CTVAL(int32_t,dtoh32a,value);
			break;
		case PTP_DTC_UINT32:
			CTVAL(uint32_t,dtoh32a,value);
			break;
		/* XXX: other int types are unimplemented */
		/* XXX: int arrays are unimplemented also */
		case PTP_DTC_STR:
		{
			uint8_t len;
			(char *)(*value)=ptp_unpack_string(params,data,0,&len);
			break;
		}
	}
}


/* Device Property pack/unpack */

#define PTP_dpd_DevicePropertyCode	0
#define PTP_dpd_DataType		2
#define PTP_dpd_GetSet			4
#define PTP_dpd_FactoryDefaultValue	5

/* Custom Type Value Assignement macro frequently used below */
#define CTVA(type,func,target)  {					\
		target = malloc(sizeof(type));				\
		*(type *)target =					\
			func(&data[PTP_dpd_FactoryDefaultValue+totallen]);\
			totallen+=sizeof(type);				\
}

/* Many Custom Types Vale Assignement macro frequently used below */

#define MCTVA(type,func,target,n) {					\
		uint16_t i;						\
		for (i=0;i<n;i++) {					\
			target[i] = malloc(sizeof(type));		\
			*(type *)target[i] =				\
			func(&data[PTP_dpd_FactoryDefaultValue+totallen]);\
			totallen+=sizeof(type);				\
		}							\
}

static inline void
ptp_unpack_DPD (PTPParams *params, char* data, PTPDevicePropDesc *dpd)
{
	uint8_t len;
	int totallen=0;

	dpd->DevicePropertyCode=dtoh16a(&data[PTP_dpd_DevicePropertyCode]);
	dpd->DataType=dtoh16a(&data[PTP_dpd_DataType]);
	dpd->GetSet=dtoh8a(&data[PTP_dpd_GetSet]);
	dpd->FactoryDefaultValue = NULL;
	dpd->CurrentValue = NULL;
	switch (dpd->DataType) {
		case PTP_DTC_INT8:
			CTVA(int8_t,dtoh8a,dpd->FactoryDefaultValue);
			CTVA(int8_t,dtoh8a,dpd->CurrentValue);
			break;
		case PTP_DTC_UINT8:
			CTVA(uint8_t,dtoh8a,dpd->FactoryDefaultValue);
			CTVA(uint8_t,dtoh8a,dpd->CurrentValue);
			break;
		case PTP_DTC_INT16:
			CTVA(int16_t,dtoh16a,dpd->FactoryDefaultValue);
			CTVA(int16_t,dtoh16a,dpd->CurrentValue);
			break;
		case PTP_DTC_UINT16:
			CTVA(uint16_t,dtoh16a,dpd->FactoryDefaultValue);
			CTVA(uint16_t,dtoh16a,dpd->CurrentValue);
			break;
		case PTP_DTC_INT32:
			CTVA(int32_t,dtoh32a,dpd->FactoryDefaultValue);
			CTVA(int32_t,dtoh32a,dpd->CurrentValue);
			break;
		case PTP_DTC_UINT32:
			CTVA(uint32_t,dtoh32a,dpd->FactoryDefaultValue);
			CTVA(uint32_t,dtoh32a,dpd->CurrentValue);
			break;
		/* XXX: other int types are unimplemented */
		/* XXX: int arrays are unimplemented also */
		case PTP_DTC_STR:
			(char *)dpd->FactoryDefaultValue = ptp_unpack_string
				(params,data,PTP_dpd_FactoryDefaultValue,&len);
			totallen=len*2+1;
			(char *)dpd->CurrentValue = ptp_unpack_string
				(params, data, PTP_dpd_FactoryDefaultValue + 
				totallen, &len);
			totallen+=len*2+1;
			break;
	}
	/* if totallen==0 then Data Type format is not supported by this
	code or the Data Type is a string (with two empty strings as
	values). In both cases Form Flag should be set to 0x00 and FORM is
	not present. */
	dpd->FormFlag=PTP_DPFF_None;
	if (totallen==0) return;

	dpd->FormFlag=dtoh8a(&data[PTP_dpd_FactoryDefaultValue+totallen]);
	totallen+=sizeof(uint8_t);
	switch (dpd->FormFlag) {
		case PTP_DPFF_Range:
		switch (dpd->DataType) {
			case PTP_DTC_INT8:
			CTVA(int8_t,dtoh8a,dpd->FORM.Range.MinimumValue);
			CTVA(int8_t,dtoh8a,dpd->FORM.Range.MaximumValue);
			CTVA(int8_t,dtoh8a,dpd->FORM.Range.StepSize);
			break;
			case PTP_DTC_UINT8:
			CTVA(uint8_t,dtoh8a,dpd->FORM.Range.MinimumValue);
			CTVA(uint8_t,dtoh8a,dpd->FORM.Range.MaximumValue);
			CTVA(uint8_t,dtoh8a,dpd->FORM.Range.StepSize);
			break;
			case PTP_DTC_INT16:
			CTVA(int16_t,dtoh16a,dpd->FORM.Range.MinimumValue);
			CTVA(int16_t,dtoh16a,dpd->FORM.Range.MaximumValue);
			CTVA(int16_t,dtoh16a,dpd->FORM.Range.StepSize);
			break;
			case PTP_DTC_UINT16:
			CTVA(uint16_t,dtoh16a,dpd->FORM.Range.MinimumValue);
			CTVA(uint16_t,dtoh16a,dpd->FORM.Range.MaximumValue);
			CTVA(uint16_t,dtoh16a,dpd->FORM.Range.StepSize);
			break;
			case PTP_DTC_INT32:
			CTVA(int32_t,dtoh32a,dpd->FORM.Range.MinimumValue);
			CTVA(int32_t,dtoh32a,dpd->FORM.Range.MaximumValue);
			CTVA(int32_t,dtoh32a,dpd->FORM.Range.StepSize);
			break;
			case PTP_DTC_UINT32:
			CTVA(uint32_t,dtoh32a,dpd->FORM.Range.MinimumValue);
			CTVA(uint32_t,dtoh32a,dpd->FORM.Range.MaximumValue);
			CTVA(uint32_t,dtoh32a,dpd->FORM.Range.StepSize);
			break;
		/* XXX: other int types are unimplemented */
		/* XXX: int arrays are unimplemented also */
		/* XXX: does it make any sense: "a range of strings"? */
		}
		break;
		case PTP_DPFF_Enumeration:
#define N	dpd->FORM.Enum.NumberOfValues
		N = dtoh16a(&data[PTP_dpd_FactoryDefaultValue+totallen]);
		totallen+=sizeof(uint16_t);
		dpd->FORM.Enum.SupportedValue = malloc(N*sizeof(void *));
		switch (dpd->DataType) {
			case PTP_DTC_INT8:
			MCTVA(int8_t,dtoh8a,dpd->FORM.Enum.SupportedValue,N);
			break;
			case PTP_DTC_UINT8:
			MCTVA(uint8_t,dtoh8a,dpd->FORM.Enum.SupportedValue,N);
			break;
			case PTP_DTC_INT16:
			MCTVA(int16_t,dtoh16a,dpd->FORM.Enum.SupportedValue,N);
			break;
			case PTP_DTC_UINT16:
			MCTVA(uint16_t,dtoh16a,dpd->FORM.Enum.SupportedValue,N);
			break;
			case PTP_DTC_INT32:
			MCTVA(int32_t,dtoh16a,dpd->FORM.Enum.SupportedValue,N);
			break;
			case PTP_DTC_UINT32:
			MCTVA(uint32_t,dtoh16a,dpd->FORM.Enum.SupportedValue,N);
			break;
			case PTP_DTC_STR:
			{
			int i;
			for(i=0;i<N;i++)
			{
				(char *)dpd->FORM.Enum.SupportedValue[i]=
					ptp_unpack_string
					(params,data,PTP_dpd_FactoryDefaultValue
					+totallen,&len);
				totallen+=len*2+1;
			}
			}
			break;
		}
	}
}

static inline uint32_t
ptp_pack_DPV (PTPParams *params, void* value, char** dpvptr, uint16_t datatype)
{
	char* dpv=NULL;
	uint32_t size=0;

	switch (datatype) {
		case PTP_DTC_INT8:
			size=sizeof(int8_t);
			dpv=malloc(size);
			htod8a(dpv,*(int8_t*)value);
			break;
		case PTP_DTC_UINT8:
			size=sizeof(uint8_t);
			dpv=malloc(size);
			htod8a(dpv,*(uint8_t*)value);
			break;
		case PTP_DTC_INT16:
			size=sizeof(int16_t);
			dpv=malloc(size);
			htod16a(dpv,*(int16_t*)value);
			break;
		case PTP_DTC_UINT16:
			size=sizeof(uint16_t);
			dpv=malloc(size);
			htod16a(dpv,*(uint16_t*)value);
			break;
		case PTP_DTC_INT32:
			size=sizeof(int32_t);
			dpv=malloc(size);
			htod32a(dpv,*(int32_t*)value);
			break;
		case PTP_DTC_UINT32:
			size=sizeof(uint32_t);
			dpv=malloc(size);
			htod32a(dpv,*(uint32_t*)value);
			break;
		/* XXX: other int types are unimplemented */
		/* XXX: int arrays are unimplemented also */
		case PTP_DTC_STR:
		{
		uint8_t len;
			size=strlen((char*)value)*2+3;
			dpv=malloc(size);
			memset(dpv,0,size);
			ptp_pack_string(params, (char *)value, dpv, 0, &len);
		}
		break;
	}
	*dpvptr=dpv;
	return size;
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
ptp_unpack_EC (PTPParams *params, char* data, PTPUSBEventContainer *ec)
{
	if (data==NULL)
		return;
	ec->length=dtoh32a(&data[PTP_ec_Length]);
	ec->type=dtoh16a(&data[PTP_ec_Type]);
	ec->code=dtoh16a(&data[PTP_ec_Code]);
	ec->trans_id=dtoh32a(&data[PTP_ec_TransId]);
	if (ec->length>=(PTP_ec_Param1+4))
		ec->param1=dtoh32a(&data[PTP_ec_Param1]);
	else
		ec->param1=0;
	if (ec->length>=(PTP_ec_Param2+4))
		ec->param2=dtoh32a(&data[PTP_ec_Param2]);
	else
		ec->param2=0;
	if (ec->length>=(PTP_ec_Param3+4))
		ec->param3=dtoh32a(&data[PTP_ec_Param3]);
	else
		ec->param3=0;
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
ptp_unpack_Canon_FE (PTPParams *params, char* data, PTPCANONFolderEntry *fe)
{
	int i;
	if (data==NULL)
		return;
	fe->ObjectHandle=dtoh32a(&data[PTP_cfe_ObjectHandle]);
	fe->ObjectFormatCode=dtoh16a(&data[PTP_cfe_ObjectFormatCode]);
	fe->Flags=dtoh8a(&data[PTP_cfe_Flags]);
	fe->ObjectSize=dtoh32a(&data[PTP_cfe_ObjectSize]);
	fe->Time=(time_t)dtoh32a(&data[PTP_cfe_Time]);
	for (i=0; i<PTP_CANON_FilenameBufferLen; i++)
	fe->Filename[i]=(char)dtoh8a(&data[PTP_cfe_Filename+i]);
}


