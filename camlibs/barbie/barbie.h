/* Protocol Notes
   ==========================================================

* PC is the host (always)

* 6 images MAXIMUM on the camera, 128k of memory

* Camera has an image counter that designates current image to do something with.
  (yes, there is better wording, but hey... :)

* ACK = 0x06, NAK = 0x15
	- No ack/nak means bad connection/faulty cable/no camera

* Camera always answers ACK to sent command

* Most COMMAND's have a matching RESPONSE

* Command Packet structure is:
	[0x02] [COMMAND] [DATA] [DATA] [...] [0x03]

* Response Packet structure is:
	[0x02] [RESPONSE] [DATA] [DATA] [...] [0x03]

* Image transfer Packet Structure is:
	[0x02] [RESPONSE] [N1] [N2] [N3] [N4] [DATA] [DATA] [...] [0x03]

	N1 = number of columns in the image
	N2 = number of black lines
	N3 = number of visible lines
	N4 = number of status bytes

	Number of data bytes = N1 * (N2 + N3) + N4

* Firmware version transfer Packet Structure is:
	[0x02] [RESPONSE] [SIZE] [CHAR] [CHAR] [...] [NULL] [0x03]

	- On error:
	[0x02] [RESPONSE] [0] [0x03]

* Camera sends response packets to host after it sends ACK.
  (NAK gets no response of course)

	- Command/Response codes
	  (note: COMMAND is upper case, RESPONSE is lower case of same letter)

	CMD	CMD DATA BYTES			RESP DATA BYTES		Description
	---     -----------------------         ---------------------   -----------------------
	A	1 (index #)			1 (0)			move to image index #
	B	2 (duration, num times)		1 (0)			beep (duration*50ms)
	E	1 (test char)			1 (test char)		echo (test or wake up camera)
	G	1 (delay & retry flags)		1 (0, use grab image	grab image - increments counter,
						  result for error	lower nibble = self timer delay,
									upper nibble = retry numbers
										0,16,32,64(force),112
	I	1 (0)				1 (image index)		get image counter
									(use reset first to get # pics)
	J	2 (value: LSB, MSB)		2 (error, value)	increment counter
									(0 lets current ram be read)
	K	1 (value)			1 (error)		write rambyte
									(counter incremented after)
	L	1 (0)				1 (0)			go idle
	M	1 (0)				hdr+pixel data+info	upload thumbnail (increments
									image counter & image index)
	P	1 (new port value)		1 (old port value)	set port value
	Q	1 (video mode)			1 (0)			video mode (!!)
									(requires capture card)
	R	1 (register index)		1 (register value)	read register value
						  (error = 0xFF)
	T	2 (min, max)			1 (0)			set exposure threshold
	U	1 (0)				hdr+pixel data+info	upload image
	V	1 (0)				version string		get version information
	W	2 (register index, value)	1 (0 = OK, 0xFF = ERR)	write register value
	X	1 (0)				2 (min, max)		get exposure threshold
	Y	1 (0)				1 (grab result code)	get grab result code
						0x00 = successful
						0x80 = memory full
						0x81 = failed exposure
	Z	1 (0)				1 (0)			quit self test

* If camera is busy when command is issued, camera responds with a BUSY packet
	[0x02] [BUSY] [BUSYCODE] [0x03]

	- BUSY = '!'
	- Busy codes:
		0x20	BUSY_TIMER 	camera waiting
		0x21	BUSY_EXPOSURE	waiting for exposure after grab started
		0x22	BUSY_VIDEO	camera in continuous video mode (self test)(yummy!!!!) :)
		0x23	BUSY_SHUTTER	waiting for shutter to come up
		0x24	BUSY_COMMAND	waiting for command completion
		0x25	BUSY_COUNTER	counter being reset (to nonzero value)

*/

#define MAX_PICTURES	6

#define ACK		0x06
#define NAK		0x15

#define COMMAND_BYTE	1
#define RESPONSE_BYTE	1
#define DATA1_BYTE	2
#define DATA2_BYTE	3

#define BARBIE_DATA_FIRMWARE	0
#define BARBIE_DATA_THUMBNAIL	1
#define BARBIE_DATA_PICTURE	2

#define PICTURE_SIZE(n1, n2, n3, n4)	(n1*(n2+n3)+n4)

/* Utility functions */
void	barbie_packet_dump  (CameraPort *port, int direction, char *buf, int size);
int	barbie_write_command(CameraPort *port, char *command, int size);
int	barbie_read_response(CameraPort *port, char *response, int size);
char*	barbie_read_data    (CameraPort *port, char *cmd, int cmd_size, int get_firmware, int *size);

char*	barbie_read_firmware(CameraPort *port);
char*	barbie_read_picture (CameraPort *port, int picture_number, int get_thumbnail, int *size);
int	barbie_exchange     (CameraPort *port, char *cmd, int cmd_size, char *resp, int resp_size);
int	barbie_ping         (CameraPort *port);
int     barbie_file_count (CameraPort *port);
