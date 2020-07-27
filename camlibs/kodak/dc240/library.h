/* library.h
 *
 * Copyright (C) 2001,2002 Hubert Figuiere <hfiguiere@teaser.fr>
 * Copyright (C) 2000,2001,2002 Scott Fritzinger
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

/*
  Kodak DC 240/280/3400/5000 driver.
 */

#ifndef __DC240_LIBRARY_H__
#define __DC240_LIBRARY_H__

#define DC240_ACTION_PREVIEW    (uint8_t)0x93
#define DC240_ACTION_IMAGE      (uint8_t)0x9A
#define DC240_ACTION_DELETE     (uint8_t)0x9D

#include "gphoto2-endian.h"


const char *dc240_convert_type_to_camera (uint16_t type);
const char *dc240_get_battery_status_str (uint8_t status);
const char *dc240_get_ac_status_str (uint8_t status);
const char *dc240_get_memcard_status_str(uint8_t status);

/* Define the status table for DC240 and DC280. */
typedef struct {
    uint8_t cameraType; /* 1 */
    uint8_t fwVersInt; /* 2 */
    uint8_t fwVersDec; /* 3 */
    uint8_t romVers32Int; /* 4 */
    uint8_t romVers32Dec; /* 5 */
    uint8_t romVers8Int; /* 6 */
    uint8_t romVers8Dec; /* 7 */
    uint8_t battStatus; /* 8 */
    uint8_t acAdapter; /* 9 */
    uint8_t strobeStatus; /* 10 */
    uint8_t memCardStatus; /* 11 */
    uint8_t videoFormat; /* 12 */
    uint8_t quickViewMode; /* 13 DC280 */
    uint16_t numPict; /* 14-15 BigEndian */
    char volumeID[11]; /* 16-26 */
    uint8_t powerSave; /* 27 DC280 */
    char cameraID[32]; /* 28-59 */
    uint16_t remPictLow; /* 60-61 BE */
    uint16_t remPictMed; /* 62-63 BE */
    uint16_t remPictHigh; /* 64-65 BE */
    uint16_t totalPictTaken; /* 66-67 BE */
    uint16_t totalStrobeFired; /* 68-69 BE */
    uint8_t langType; /* 70 DC280 */
    uint8_t beep; /* 71 */

    uint8_t fileType; /* 78 */
    uint8_t pictSize; /* 79 */
    uint8_t imgQuality; /* 80 */
    uint8_t ipChainDisable; /* 81 */ /* ????? what does that mean reserved on DC280 */
    uint8_t imageIncomplete; /* 82 */
    uint8_t timerMode; /* 83 */

    uint16_t year; /* 88-89 BE */
    uint8_t month; /* 90 */
    uint8_t day; /* 91 */
    uint8_t hour; /* 92 */
    uint8_t minute; /* 93 */
    uint8_t second; /* 94 */
    uint8_t tenmSec; /* 95 */

    uint8_t strobeMode; /* 97 */
    uint16_t exposureComp; /* 98-99 BE see note */
    uint8_t aeMode; /* 100 */
    uint8_t focusMode; /* 101 */
    uint8_t afMode; /* 102 */
    uint8_t awbMode; /* 103 */
    uint32_t zoomMag; /* 104-107 BE see doc */

    uint8_t exposureMode; /* 129 */

    uint8_t sharpControl; /* 131 */
    uint32_t expTime; /* 132-135 BE */
    uint16_t fValue; /* 136-137 BE */
    uint8_t imageEffect; /* 138 */
    uint8_t dateTimeStamp; /* 139 */
    char borderFileName [11]; /* 140-151 */
    uint8_t exposureLock; /* 152 */
    uint8_t isoMode; /* 153 DC280 */
} DC240StatusTable;


int   dc240_open         (Camera *camera);
int   dc240_close        (Camera *camera, GPContext *context);

int   dc240_set_speed    (Camera *camera, int speed);

int   dc240_get_status (Camera *camera, DC240StatusTable *table,
			GPContext *context);

int   dc240_get_directory_list (Camera *camera, CameraList *list, const char *folder,
     unsigned char attrib, GPContext *context);


int   dc240_file_action	 (Camera *camera, int action, CameraFile *file,
                          const char *folder, const char *filename,
			  GPContext *context);

int   dc240_capture	 (Camera *camera, CameraFilePath *path,
			  GPContext *context);
int   dc240_packet_set_size (Camera *camera, short int size);

#endif /*__DC240_LIBRARY_H__*/
