
#ifndef _DC210_LIBRARY_H_
#define _DC210_LIBRARY_H_

#define DC210_CMD_OKAY         0
#define DC210_WRITE_ERROR     -1
#define DC210_READ_ERROR      -2
#define DC210_TIMEOUT_ERROR   -3
#define DC210_NAK_ERROR       -4
#define DC210_GARBAGE_ERROR   -5

static void dc210_cmd_init(char *cmd, unsigned char command_byte);
static int dc210_write_single_char (Camera *camera, unsigned char response);
static int dc210_write_command_packet(Camera * camera, char * data);
static int dc210_execute_command (Camera *camera, char *cmd);
static int dc210_read_single_char (Camera *camera, unsigned char * p);
static int dc210_wait_for_response (Camera *camera, int expect_busy, GPContext *context);
static int dc210_read_to_file (Camera *camera, CameraFile * f, int blocksize, long int expectsize, GPContext *context);
static int dc210_read_single_block (Camera *camera, unsigned char * b, int blocksize);
static int dc210_take_picture (Camera * camera, GPContext *context);
static int dc210_set_option (Camera * camera, char command, unsigned int value, int valuesize);
static int dc210_space_on_card(Camera * camera, int * space);

#ifdef DEBUG
static int dc210_read_dummy_packet(Camera * camera);
static int dc210_test_command(Camera * camera, unsigned char cmdbyte, unsigned char modifier,
			      unsigned int value, int size);
#endif

#endif /* _DC210_LIBRARY_H_ */


