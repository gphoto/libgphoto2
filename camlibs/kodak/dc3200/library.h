/****************************************************
 * kodak dc3200 digital camera driver library       *
 * for gphoto2                                      *
 *                                                  *
 * author: donn morrison - dmorriso@gulf.uvic.ca    *
 * date: dec 2000 - feb 2001                        *
 * license: gpl                                     *
 * version: 1.5                                     *
 *                                                  *
 ****************************************************/

#ifndef LIBRARY_H_INCLUDED

#include "dc3200.h"

/* retries */
#define READ_RETRIES 2
#define ACK_RETRIES  2
#define SEND_RETRIES 4

/* packet sizes */
#define DEF_PACKET_LEN 256
#define ACK_PACKET_LEN 2
#define INI_PACKET_LEN 3

/* command types */
#define SETBAUD1 0xAF
#define SETBAUD2 0x9F
#define PING     0xCF
#define COMMAND  0x01

/* dc3200 baud rate values
 *
 * baud    baudcode(ask)  baudcode2(confirm)
 * 9600        0x00             --
 * 19200       0x01            0x01
 * 38400       0x03            0x02
 * 57600       0x07            0x04
 * 115200      0x0F            0x08
 *
 * except for 9600, baudcode = (baudcode2 + 1) / 2
 *
 */
#define C9600   0x00
#define C19200  0x01
#define C38400  0x03
#define C57600  0x07
#define C115200 0x0F

int dc3200_set_speed(DC3200Data *dd, int baudrate);
int dc3200_setup(DC3200Data *dd);

int dc3200_get_data(DC3200Data *dd, u_char **data, u_long *data_len, int command, char *folder, char *filename);

int dc3200_send_command(DC3200Data *dd, u_char *cmd, int cmd_len, u_char *ack, int *ack_len);
int dc3200_recv_response(DC3200Data *dd, u_char *resp, int *resp_len);

int dc3200_send_packet(DC3200Data *dd, u_char *data, int data_len);
int dc3200_recv_packet(DC3200Data *dd, u_char *data, int *data_len);

int dc3200_compile_packet(DC3200Data *dd, u_char **data, int *data_len);
int dc3200_process_packet(DC3200Data *dd, u_char *data, int *data_len);

int dc3200_send_ack(DC3200Data *dd, int seqnum);
int dc3200_check_ack(DC3200Data *dd, u_char *ack, int ack_len);

int dc3200_calc_checksum(DC3200Data *dd, u_char *data, int data_len);
int dc3200_calc_seqnum(DC3200Data *dd);

int dc3200_keep_alive(DC3200Data *dd);
int dc3200_clear_read_buffer(DC3200Data *dd);

int dump_buffer(unsigned char * buffer, int len, char *title, int bytesperline);
ulong bytes_to_l(int a, int b, int c, int d);

#define LIBRARY_H_INCLUDED
#endif
