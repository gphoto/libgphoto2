/* ADC-65(s) camera driver
 * Released under the GPL version 2   
 * 
 * Copyright 2001
 * Benjamin Moos
 * <benjamin@psnw.com>
 * http://www.psnw.com/~smokeserpent/code/
 */
/* Protocol Notes
   ==========================================================

* 15 images on the camera, 1M of memory

* Command Packet structure is:
	[COMMAND]
	
	Known commands:
	0x30 = ping (This isn't working in the gPhoto driver yet, hmmm...)
	0x00 - 0x15 = retrieve a data block (0x00 always exists and the
	Win32 client always retrieves it but it isn't used as an image. It seems
	to be calibration data for the CMOS, but I don't use this yet.)

* Ping Response Packet structure is:
	[0x15] [0x30] [0x00]

* Data transfer Packet Structure is:
	[0x15] [N1] [DATA]

	N1 = number of images + 1
	Number of data bytes = 64K

*/

#define MAX_PICTURES	6

#define ACK		0x15

#define ADC65_DATA_FIRMWARE	0
#define ADC65_DATA_THUMBNAIL	1
#define ADC65_DATA_PICTURE	2

#define PICTURE_SIZE	65535

char*	adc65_read_picture	(Camera *camera, int picture_number, int *size);
int	adc65_ping		(Camera *camera);
int     adc65_file_count	(Camera *);
