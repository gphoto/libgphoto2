/*
  Kodak DC 240/280/3400/5000 driver.
 */

#ifndef __DC240_LIBRARY_H__
#define __DC240_LIBRARY_H__

#define DC240_ACTION_PREVIEW    (unsigned char)0x93
#define DC240_ACTION_IMAGE      (unsigned char)0x9A
#define DC240_ACTION_DELETE     (unsigned char)0x9D



const char *dc240_convert_type_to_camera (int type);
const char *dc240_get_battery_status_str (char status);
const char *dc240_get_ac_status_str (char status);

/* Define the status table for DC240 and DC280. */
typedef struct {
    char cameraType; /* 1 */
    char fwVersInt; /* 2 */
    char fwVersDec; /* 3 */
    char romVers32Int; /* 4 */
    char romVers32Dec; /* 5 */
    char romVers8Int; /* 6 */
    char romVers8Dec; /* 7 */
    char battStatus; /* 8 */
    char acAdapter; /* 9 */
    char strobeStatus; /* 10 */
    char memCardStatus; /* 11 */
    char videoFormat; /* 12 */
    char quickViewMode; /* 13 DC280 */
    short numPict; /* 14-15 BigEndian */
    char volumeID[11]; /* 16-26 */
    char powerSave; /* 27 DC280 */
    char cameraID[32]; /* 28-59 */
    short remPictLow; /* 60-61 BE */
    short remPictMed; /* 62-63 BE */
    short remPictHigh; /* 64-65 BE */
    short totalPictTaken; /* 66-67 BE */
    short totalStrobeFired; /* 68-69 BE */
    char langType; /* 70 DC280 */
    char beep; /* 71 */

    char fileType; /* 78 */
    char pictSize; /* 79 */
    char imgQuality; /* 80 */
    char ipChainDisable; /* 81 */ /* ????? what does that mean reserved on DC280 */
    char imageIncomplete; /* 82 */
    char timerMode; /* 83 */
    
    short year; /* 88-89 BE */
    char month; /* 90 */
    char day; /* 91 */
    char hour; /* 92 */
    char minute; /* 93 */
    char second; /* 94 */
    char tenmSec; /* 95 */
    
    char strobeMode; /* 97 */
    short exposureComp; /* 98-99 BE see note */
    char aeMode; /* 100 */
    char focusMode; /* 101 */
    char afMode; /* 102 */
    char awbMode; /* 103 */
    long zoomMag; /* 104-107 BE see doc */
    
    char exposureMode; /* 129 */

    char sharpControl; /* 131 */
    long expTime; /* 132-135 BE */
    short fValue; /* 136-137 BE */
    char imageEffect; /* 138 */
    char dateTimeStamp; /* 139 */
    char borderFileName [11]; /* 140-151 */
    char exposureLock; /* 152 */
    char isoMode; /* 153 DC280 */
} DC240StatusTable;


int   dc240_open         (Camera *camera);
int   dc240_close        (Camera *camera);

int   dc240_set_speed    (Camera *camera, int speed);

int   dc240_get_status (Camera *camera, DC240StatusTable *table);

int   dc240_get_folders  (Camera *camera, CameraList *list, const char *folder);
int   dc240_get_filenames(Camera *camera, CameraList *list, const char *folder);

int   dc240_file_action	 (Camera *camera, int action, CameraFile *file,
                          const char *folder, const char *filename);

int   dc240_capture	 (Camera *camera, CameraFilePath *path);
int   dc240_packet_set_size (Camera *camera, short int size);

#endif /*__DC240_LIBRARY_H__*/
