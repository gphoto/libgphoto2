#include "pdrm65.h"
#define GP_MODULE "setting"

int pdrm65_init(GPPort *port)
{
	unsigned char out_buf[1024];
	unsigned char in_buf[1024];
	//int timeout = 50;
	int ret = GP_OK;
	char ok = 0x06;
	gp_port_set_timeout(port,1000);
	memset(out_buf, 0, sizeof(out_buf));
	memset(in_buf, 0, sizeof(in_buf));
	/* exactly what windows driver does */
	//gp_port_usb_msg_read (port, 0x01, PDRM11_CMD_READY, 0, buf, 4);
	out_buf[0] = 0x1b;
	out_buf[1] = 0x43;
	out_buf[2] = 0x02;
	out_buf[3] = 0x00;
	out_buf[4] = 0x04;
	out_buf[5] = 0x1b;
	out_buf[6] = 0x1f;
	out_buf[7] = 0x00;
	gp_port_write(port, out_buf, 8);
	gp_port_read (port, in_buf, 11);
	gp_port_write(port, &ok, 1);
	
	//out_buf[0-3]=0x1b 43 02 00
	out_buf[4]=0x01;
	out_buf[5]=0x02;
	out_buf[6]=0x03;
	//out_buf[7]=0x00
	gp_port_write(port, out_buf, 8);
	gp_port_read (port, in_buf, 10);
	gp_port_write(port, &ok, 1);
	
	//out_buf[0-3]=0x1b 43 02 00
	out_buf[4] = 0x04;
	out_buf[5] = 0x1b;
	out_buf[6] = 0x1f;
	//out_buf[7]=0x00
	gp_port_write(port, out_buf, 8);
	gp_port_read (port, in_buf, 11);
	//in_buf[4-7] == "SI15"
	gp_port_write(port, &ok, 1);
	
	//out_buf[0-3]=0x1b 43 02 00
	out_buf[4]=0x04;
	out_buf[5]=0x16;
	out_buf[6]=0x1a;
	//out_buf[7]=0x00
	gp_port_write(port, out_buf, 8);
	gp_port_read (port, in_buf, 14);
	//in_buf[4-10] == "PDR-M65
	gp_port_write(port, &ok, 1);
	
	return(ret);
}
int pdrm65_get_filenames(GPPort *port, CameraList *list)
{

	uint32_t numPics;
	char out_buf[1024];
	char in_buf[1024];
	char ok = 0x06;
	out_buf[0] = 0x1b;
	out_buf[1] = 0x43; 
	out_buf[2] = 0x02;
	out_buf[3] = 0x00;
	out_buf[4] = 0x01;
	out_buf[5] = 0x0a;
	out_buf[6] = 0x0b;
	out_buf[7] = 0x00;
	
	gp_port_set_timeout(port, 10000);
	gp_port_write(port, out_buf, 8);
	gp_port_read (port, in_buf, 9);
	/* trying to remain endian friendly */
	gp_port_write(port, &ok, 1);
	
	numPics = le16atoh(&in_buf[3]) + (le16atoh(&in_buf[4]) * 256);
	GP_DEBUG("found %d pictures", numPics);

	
#if 0
	for(i=1;  i<numPics+1;  i++) {
		CHECK( pdrm11_select_file(port, i) );

		CHECK(gp_port_usb_msg_read(port, 0x01, 0xe600, i, buf, 14));

		/* the filename is 12 chars starting at the third byte */
		CHECK(gp_port_usb_msg_read(port, 0x01, PDRM11_CMD_GET_FILENAME, i, buf, 26));
		for(j=0; j<12; j+=2) {
			name[j] = buf[j+2+1];
			name[j+1] = buf[j+2];
		}
		name[12] = '\0';

		GP_DEBUG(name);
		gp_list_append(list, name, NULL);
	}

#endif
	return(GP_OK);
}
