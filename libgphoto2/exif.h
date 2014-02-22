/** \file
 *
 * EXIF file format support library. This API allows to parse, read and
 * modify EXIF data structures. It provides a low-level API which allows
 * to manipulate EXIF tags in a generic way, and a higher-level API which
 * provides more advanced functions such as comment editing, thumbnail
 * extraction, etc.
 *
 * In the future, vendor-proprietary exif extensions might be supported.
 */

#ifndef _gphoto_exif_
#define _gphoto_exif_ 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <fcntl.h>


/*
 * Tag data type information.
 */
typedef enum {
        EXIF_NOTYPE     = 0,    /* placeholder */
        EXIF_BYTE       = 1,    /* 8-bit unsigned integer */
        EXIF_ASCII      = 2,    /* 8-bit bytes w/ last byte null */
        EXIF_SHORT      = 3,    /* 16-bit unsigned integer */
        EXIF_LONG       = 4,    /* 32-bit unsigned integer */
        EXIF_RATIONAL   = 5,    /* 64-bit unsigned fraction */
        EXIF_SBYTE      = 6,    /* !8-bit signed integer */
        EXIF_UNDEFINED  = 7,    /* !8-bit untyped data */
        EXIF_SSHORT     = 8,    /* !16-bit signed integer */
        EXIF_SLONG      = 9,    /* !32-bit signed integer */
        EXIF_SRATIONAL  = 10,   /* !64-bit signed fraction */
        EXIF_FLOAT      = 11,   /* !32-bit IEEE floating point */
        EXIF_DOUBLE     = 12    /* !64-bit IEEE floating point */
} EXIFDataType;


typedef struct exif_parser {
  unsigned char *header,*data,*ifds[10];
  int ifdtags[10];  /* How many tags in each ifd */
  int ifdcnt;       /* Number of IFD's, assumed to be < 10  */
  unsigned int exiflen;
  int preparsed,endian;
} exifparser;

typedef struct {
    int tag;            /* Tag ID, see exif_tags.h */
    EXIFDataType type;  /* Tag data type, see exif_tags.h */
    int size;           /* Length of the data, in bytes. */
    unsigned char *data;         /* The data itself, not an offset */
    int num;     /* When type is (s)rational, we */
    int den;   /* store the value here... */
    int intval;
} ExifData;

/*
 * The same as gpi_exif_get_thumbnail but returns
 * also the thumbnail size
 */
unsigned char *gpi_exif_get_thumbnail_and_size(exifparser *exifdat, long *size);

/*
 * COMMENT ME
 */
int gpi_exif_stat(exifparser *exifdata);

/*
 * Now, all the 'defines'
 */

#define	EXIF_InteroperabilityIndex 0x1
#define	EXIF_InteroperabilityVersion    0x2
#define	EXIF_RelatedImageFileFormat 0x1000
#define	EXIF_RelatedImageWidth 0x1001
#define	EXIF_RelatedImageLength 0x1002

#define EXIF_NewSubFileType              0xFE
#define EXIF_ImageWidth                  0x100
#define EXIF_ImageLength                 0x101
#define EXIF_BitsPerSample               0x102
#define EXIF_Compression                 0x103
#define EXIF_PhotometricInterpretation   0x106
#define EXIF_FillOrder                   0x10A
#define EXIF_DocumentName                0x10D
#define EXIF_ImageDescription            0x10E
#define EXIF_Make                        0x10F
#define EXIF_Model                       0x110
#define EXIF_StripOffsets                0x111
#define EXIF_Orientation                 0x112
#define EXIF_SamplesPerPixel             0x115
#define EXIF_RowsPerStrip                0x116
#define EXIF_StripByteCounts             0x117
#define EXIF_XResolution                 0x11A
#define EXIF_YResolution                 0x11B
#define EXIF_PlanarConfiguration         0x11C
#define EXIF_ResolutionUnit              0x128
#define EXIF_TransferFunction            0x12D
#define EXIF_Software                    0x131
#define EXIF_DateTime                    0x132
#define EXIF_Artist                      0x13B
#define EXIF_WhitePoint                  0x13E
#define EXIF_PrimaryChromaticities       0x13F
#define EXIF_TransferRange               0x156
#define EXIF_JPEGProc                    0x200
#define EXIF_JPEGInterchangeFormat       0x201
#define EXIF_JPEGInterchangeFormatLength 0x202
#define EXIF_YCbCrCoefficients           0x211
#define EXIF_YCbCrSubSampling            0x212
#define EXIF_YCbCrPositioning            0x213
#define EXIF_ReferenceBlackWhite         0x214
#define EXIF_CFARepeatPatternDim        0x828D
#define EXIF_CFAPattern                 0x828E
#define EXIF_BatteryLevel		0x828F
#define EXIF_Copyright                  0x8298
#define EXIF_ExposureTime               0x829A
#define EXIF_FNumber                    0x829D
/* Be careful: the next tag's official name is IPTC/NAA but
   we can't do a define with a "/" */
#define EXIF_IPTC_NAA                   0x83BB
#define EXIF_ExifOffset                 0x8769
#define EXIF_InterColorProfile          0x8773
#define EXIF_ExposureProgram            0x8822
#define EXIF_SpectralSensitivity        0x8824
#define EXIF_GPSInfo                    0x8825
#define EXIF_ISOSpeedRatings            0x8827
#define EXIF_OECF                       0x8828
#define EXIF_ExifVersion                0x9000
#define EXIF_DateTimeOriginal           0x9003
#define EXIF_DateTimeDigitized          0x9004
#define EXIF_ComponentsConfiguration    0x9101
#define EXIF_CompressedBitsPerPixel     0x9102
#define EXIF_ShutterSpeedValue          0x9201
#define EXIF_ApertureValue              0x9202
#define EXIF_BrightnessValue            0x9203
#define EXIF_ExposureBiasValue          0x9204
#define EXIF_MaxApertureValue           0x9205
#define EXIF_SubjectDistance            0x9206
#define EXIF_MeteringMode               0x9207
#define EXIF_LightSource                0x9208
#define EXIF_Flash                      0x9209
#define EXIF_FocalLength                0x920A
#define EXIF_MakerNote                  0x927C
#define EXIF_UserComment                0x9286
#define EXIF_SubSecTime                 0x9290
#define EXIF_SubSecTimeOriginal         0x9291
#define EXIF_SubSecTimeDigitized        0x9292
#define EXIF_FlashPixVersion            0xA000
#define EXIF_ColorSpace                 0xA001
#define EXIF_ExifImageWidth             0xA002
#define EXIF_ExifImageLength            0xA003
#define EXIF_InteroperabilityOffset     0xA005
#define EXIF_FlashEnergy                0xA20B  /* 0x920B in TIFF/EP */
#define EXIF_SpatialFrequencyResponse   0xA20C  /* 0x920C    -  - */
#define EXIF_FocalPlaneXResolution      0xA20E  /* 0x920E    -  - */
#define EXIF_FocalPlaneYResolution      0xA20F  /* 0x920F    -  - */
#define EXIF_FocalPlaneResolutionUnit   0xA210  /* 0x9210    -  - */
#define EXIF_SubjectLocation            0xA214  /* 0x9214    -  - */
#define EXIF_ExposureIndex              0xA215  /* 0x9215    -  - */
#define EXIF_SensingMethod              0xA217  /* 0x9217    -  - */
#define EXIF_FileSource                 0xA300
#define EXIF_SceneType                  0xA301

#endif /* _gphoto_exif_ */
