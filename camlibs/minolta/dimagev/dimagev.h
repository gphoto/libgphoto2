#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gphoto2.h>
#include <gpio.h>

/* These are the repsonse codes. */
#define DIMAGEV_STX 0x02
#define DIMAGEV_ETX 0x03
#define DIMAGEV_EOT 0x04
#define DIMAGEV_ACK 0x06
#define DIMAGEV_NAK 0x15
#define DIMAGEV_CAN 0x18

/* These are the commands. */
#define DIMAGEV_INQUIRY "\x01"
#define DIMAGEV_DRIVE_READY "\x02"
#define DIMAGEV_GET_DIR "\x03"
#define DIMAGEV_GET_IMAGE "\x04"
#define DIMAGEV_ERASE_IMAGE "\x05"
#define DIMAGEV_ERASE_ALL "\x06"
#define DIMAGEV_GET_STATUS "\x07"
#define DIMAGEV_SET_DATA "\x08"
#define DIMAGEV_GET_DATA "\x09"
#define DIMAGEV_SHUTTER "\x0a"
#define DIMAGEV_FORMAT "\x0b"
#define DIMAGEV_DIAG "\x0c"
#define DIMAGEV_GET_THUMB "\x0d"
#define DIMAGEV_SET_IMAGE "\x0e"

#define DIMAGEV_FILENAME_FMT "dv%05i.jpg"

typedef struct {
	unsigned int length;
	unsigned char buffer[1024];
} dimagev_packet;

typedef struct {
	unsigned char vendor[8];
	unsigned char model[8];
	unsigned char hardware_rev[4];
	unsigned char firmware_rev[4];
	unsigned char have_storage;
} dimagev_info_t;

typedef struct {
	unsigned char battery_level;
	unsigned int number_images;
	unsigned int minimum_images_can_take;
	unsigned char busy;
	unsigned char flash_charging;
	unsigned char lens_status;
	unsigned char card_status;
	unsigned char id_number;
} dimagev_status_t;

typedef struct {
	unsigned char host_mode;
	unsigned char exposure_valid;
	unsigned char date_valid;
	unsigned char self_timer_mode;
	unsigned char flash_mode;
	unsigned char quality_setting;
	unsigned char play_rec_mode;
	unsigned char year;
	unsigned char month;
	unsigned char day;
	unsigned char hour;
	unsigned char minute;
	unsigned char second;
	unsigned char exposure_correction;
	unsigned char valid;
	unsigned char id_number;
} dimagev_data_t;

typedef struct {
	int size;   
	int debug;
	gpio_device *dev;
	gpio_device_settings settings;
	dimagev_data_t *data;
	dimagev_status_t *status;
	dimagev_info_t *info;
	CameraFilesystem *fs;
} dimagev_t;

/* These functions are in packet.c */
dimagev_packet *dimagev_make_packet(unsigned char *buffer, unsigned int length, unsigned int seq);
dimagev_packet *dimagev_read_packet(dimagev_t *dimagev);
int dimagev_verify_packet(dimagev_packet *p);
dimagev_packet *dimagev_strip_packet(dimagev_packet *p);
void dimagev_dump_packet(dimagev_packet *p);
unsigned char dimagev_decimal_to_bcd(unsigned char decimal);
unsigned char dimagev_bcd_to_decimal(unsigned char bcd);
int dimagev_packet_sequence(dimagev_packet *p);

/* These functions are in data.c */
int dimagev_get_camera_data(dimagev_t *dimagev);
dimagev_data_t *dimagev_import_camera_data(unsigned char *raw_data);
unsigned char *dimagev_export_camera_data(dimagev_data_t *good_data);
void dimagev_dump_camera_data(dimagev_data_t *data);

/* These functions are in status.c */
int dimagev_get_camera_status(dimagev_t *dimagev);
dimagev_status_t *dimagev_import_camera_status(unsigned char *raw_data);
void dimagev_dump_camera_status(dimagev_status_t *status);

/* These functions are in info.c */
int dimagev_get_camera_info(dimagev_t *dimagev);
dimagev_info_t *dimagev_import_camera_info(unsigned char *raw_data);
void dimagev_dump_camera_info(dimagev_info_t *info);

/* These functions are in capture.c */
int dimagev_shutter(dimagev_t *dimagev);

/* These functions are in util.c */
int dimagev_host_mode(dimagev_t *dimagev, int newmode);
int dimagev_get_picture(dimagev_t *dimagev, int file_number, CameraFile *file);
int dimagev_delete_picture(dimagev_t *dimagev, int file_number);
int dimagev_delete_all(dimagev_t *dimagev);
