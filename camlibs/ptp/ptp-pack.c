// curently this file is included into ptp.c

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

#define htod8a(a,x)	*(uint8_t*)(a) = x
#define htod16a(a,x)	*(uint16_t*)(a) = htod16p(params,x)
#define htod32a(a,x)	*(uint32_t*)(a) = htod32p(params,x)
#define htod16(x)	htod16p(params,x)
#define htod32(x)	htod32p(params,x)

#define dtoh8a(x)	*(uint8_t*)(x)
#define dtoh16a(x)	dtoh16p(params,*(uint16_t*)(x))
#define dtoh32a(x)	dtoh32p(params,*(uint32_t*)(x))
#define dtoh16(x)	dtoh16p(params,x)
#define dtoh32(x)	dtoh32p(params,x)

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

static inline int
ptp_pack_OI (PTPParams *params, PTPObjectInfo *oi, PTPReq *req)
{
	int i;

	memset (req, 0, sizeof(PTPReq));
	htod32a(&req->data[PTP_oi_StorageID],oi->StorageID);
	htod16a(&req->data[PTP_oi_ObjectFormat],oi->ObjectFormat);
	htod16a(&req->data[PTP_oi_ProtectionStatus],oi->ProtectionStatus);
	htod32a(&req->data[PTP_oi_ObjectCompressedSize],oi->ObjectCompressedSize);
	htod16a(&req->data[PTP_oi_ThumbFormat],oi->ThumbFormat);
	htod32a(&req->data[PTP_oi_ThumbCompressedSize],oi->ThumbCompressedSize);
	htod32a(&req->data[PTP_oi_ThumbPixWidth],oi->ThumbPixWidth);
	htod32a(&req->data[PTP_oi_ThumbPixHeight],oi->ThumbPixHeight);
	htod32a(&req->data[PTP_oi_ImagePixWidth],oi->ImagePixWidth);
	htod32a(&req->data[PTP_oi_ImagePixHeight],oi->ImagePixHeight);
	htod32a(&req->data[PTP_oi_ImageBitDepth],oi->ImageBitDepth);
	htod32a(&req->data[PTP_oi_ParentObject],oi->ParentObject);
	htod16a(&req->data[PTP_oi_AssociationType],oi->AssociationType);
	htod32a(&req->data[PTP_oi_AssociationDesc],oi->AssociationDesc);
	htod32a(&req->data[PTP_oi_SequenceNumber],oi->SequenceNumber);
	
	htod8a(&req->data[PTP_oi_filenamelen],strlen(oi->Filename)+1);
	for (i=0;i<strlen(oi->Filename) && i< MAXFILENAMELEN; i++) {
		req->data[PTP_oi_Filename+i*2]=oi->Filename[i];
	}
	// XXX add other fields, this function should return dataset length

	return 1;
}

static inline void
ptp_unpack_OI (PTPParams *params, PTPReq *req, PTPObjectInfo *oi)
{
	int i;
	uint8_t filenamelen;
	uint8_t capturedatelen;
	char *capture_date;
	char tmp[16];
	struct tm tm;

	oi->StorageID=dtoh32a(&req->data[PTP_oi_StorageID]);
	oi->ObjectFormat=dtoh16a(&req->data[PTP_oi_ObjectFormat]);
	oi->ProtectionStatus=dtoh16a(&req->data[PTP_oi_ProtectionStatus]);
	oi->ObjectCompressedSize=dtoh32a(&req->data[PTP_oi_ObjectCompressedSize]);
	oi->ThumbFormat=dtoh16a(&req->data[PTP_oi_ThumbFormat]);
	oi->ThumbCompressedSize=dtoh32a(&req->data[PTP_oi_ThumbCompressedSize]);
	oi->ThumbPixWidth=dtoh32a(&req->data[PTP_oi_ThumbPixWidth]);
	oi->ThumbPixHeight=dtoh32a(&req->data[PTP_oi_ThumbPixHeight]);
	oi->ImagePixWidth=dtoh32a(&req->data[PTP_oi_ImagePixWidth]);
	oi->ImagePixHeight=dtoh32a(&req->data[PTP_oi_ImagePixHeight]);
	oi->ImageBitDepth=dtoh32a(&req->data[PTP_oi_ImageBitDepth]);
	oi->ParentObject=dtoh32a(&req->data[PTP_oi_ParentObject]);
	oi->AssociationType=dtoh16a(&req->data[PTP_oi_AssociationType]);
	oi->AssociationDesc=dtoh32a(&req->data[PTP_oi_AssociationDesc]);
	oi->SequenceNumber=dtoh32a(&req->data[PTP_oi_SequenceNumber]);

	filenamelen=dtoh8a(&req->data[PTP_oi_filenamelen]);
	oi->Filename=malloc(filenamelen);
	memset(oi->Filename, 0, filenamelen);
	for (i=0;i<filenamelen && i< MAXFILENAMELEN; i++) {
		oi->Filename[i]=req->data[PTP_oi_Filename+i*2];
	}
	// be paranoid! :)
	oi->Filename[filenamelen]=0;


	capturedatelen=dtoh8a(&req->data[PTP_oi_Filename+filenamelen*2]);
	capture_date=malloc(capturedatelen);
	memset(capture_date, 0, capturedatelen);
	for (i=0;i<capturedatelen && i< MAXFILENAMELEN; i++ ) {
		capture_date[i]=req->data[PTP_oi_Filename+(i+filenamelen)*2+1];
	}

	// subset of ISO 8601, without '.s' tenths of second and
	// time zone
	
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
	
	free(capture_date);
}


