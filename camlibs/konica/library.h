/********************/
/* Type definitions */
/********************/
typedef struct {
        gpio_device*		device;
	gboolean 		image_id_long;
        CameraFilesystem*	filesystem;
} konica_data_t;


/***************/
/* Definitions */
/***************/
#define KONICA_ERROR_FOCUSING_ERROR                             -100
#define KONICA_ERROR_IRIS_ERROR                                 -101
#define KONICA_ERROR_STROBE_ERROR                               -102
#define KONICA_ERROR_EEPROM_CHECKSUM_ERROR                      -103
#define KONICA_ERROR_INTERNAL_ERROR1                            -104
#define KONICA_ERROR_INTERNAL_ERROR2                            -105
#define KONICA_ERROR_NO_CARD_PRESENT                            -106
#define KONICA_ERROR_CARD_NOT_SUPPORTED                         -107
#define KONICA_ERROR_CARD_REMOVED_DURING_ACCESS                 -108
#define KONICA_ERROR_IMAGE_NUMBER_NOT_VALID                     -109
#define KONICA_ERROR_CARD_CAN_NOT_BE_WRITTEN                    -110
#define KONICA_ERROR_CARD_IS_WRITE_PROTECTED                    -111
#define KONICA_ERROR_NO_SPACE_LEFT_ON_CARD                      -112
#define KONICA_ERROR_NO_IMAGE_ERASED_AS_IMAGE_PROTECTED         -113
#define KONICA_ERROR_LIGHT_TOO_DARK                             -114
#define KONICA_ERROR_AUTOFOCUS_ERROR                            -115
#define KONICA_ERROR_SYSTEM_ERROR                               -116
#define KONICA_ERROR_ILLEGAL_PARAMETER                          -117
#define KONICA_ERROR_COMMAND_CANNOT_BE_CANCELLED                -118
#define KONICA_ERROR_LOCALIZATION_DATA_EXCESS                   -119
#define KONICA_ERROR_LOCALIZATION_DATA_CORRUPT                  -120
#define KONICA_ERROR_UNSUPPORTED_COMMAND                        -121
#define KONICA_ERROR_OTHER_COMMAND_EXECUTING                    -122
#define KONICA_ERROR_COMMAND_ORDER_ERROR                        -123
#define KONICA_ERROR_UNKNOWN_ERROR                              -124

#define KONICA_NUM_ERRORS       25

