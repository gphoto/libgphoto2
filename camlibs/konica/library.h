/********************/
/* Type definitions */
/********************/
typedef struct {
        gp_port*		device;
	gboolean 		image_id_long;
        CameraFilesystem*	filesystem;
} konica_data_t;


/***************/
/* Definitions */
/***************/
#define KONICA_ERROR_FOCUSING_ERROR                             -1000
#define KONICA_ERROR_IRIS_ERROR                                 -1001
#define KONICA_ERROR_STROBE_ERROR                               -1002
#define KONICA_ERROR_EEPROM_CHECKSUM_ERROR                      -1003
#define KONICA_ERROR_INTERNAL_ERROR1                            -1004
#define KONICA_ERROR_INTERNAL_ERROR2                            -1005
#define KONICA_ERROR_NO_CARD_PRESENT                            -1006
#define KONICA_ERROR_CARD_NOT_SUPPORTED                         -1007
#define KONICA_ERROR_CARD_REMOVED_DURING_ACCESS                 -1008
#define KONICA_ERROR_IMAGE_NUMBER_NOT_VALID                     -1009
#define KONICA_ERROR_CARD_CAN_NOT_BE_WRITTEN                    -1010
#define KONICA_ERROR_CARD_IS_WRITE_PROTECTED                    -1011
#define KONICA_ERROR_NO_SPACE_LEFT_ON_CARD                      -1012
#define KONICA_ERROR_NO_IMAGE_ERASED_AS_IMAGE_PROTECTED         -1013
#define KONICA_ERROR_LIGHT_TOO_DARK                             -1014
#define KONICA_ERROR_AUTOFOCUS_ERROR                            -1015
#define KONICA_ERROR_SYSTEM_ERROR                               -1016
#define KONICA_ERROR_ILLEGAL_PARAMETER                          -1017
#define KONICA_ERROR_COMMAND_CANNOT_BE_CANCELLED                -1018
#define KONICA_ERROR_LOCALIZATION_DATA_EXCESS                   -1019
#define KONICA_ERROR_LOCALIZATION_DATA_CORRUPT                  -1020
#define KONICA_ERROR_UNSUPPORTED_COMMAND                        -1021
#define KONICA_ERROR_OTHER_COMMAND_EXECUTING                    -1022
#define KONICA_ERROR_COMMAND_ORDER_ERROR                        -1023
#define KONICA_ERROR_UNKNOWN_ERROR                              -1024


