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

#ifndef DC3200_H_INCLUDED

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <gphoto2.h>
#include <gphoto2.h>
#include <gphoto2-port.h>

#define CMD_LIST_FILES		0
#define CMD_GET_PREVIEW		1
#define CMD_GET_FILE		2

#define TIMEOUT	        	750

typedef struct {
        gp_port			*dev;
        gp_port_settings	settings;
	CameraFilesystem	*fs;
	int			pkt_seqnum;	/* sequence number */
	int			cmd_seqnum;	/* command seqnum */
	int			debug;
	time_t			last;		/* remember last recv time */
} DC3200Data;

int init(Camera *camera);
int check_last_use(Camera *camera);

#define DC3200_H_INCLUDED
#endif
