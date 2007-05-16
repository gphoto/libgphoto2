/****************************************************
 * kodak dc3200 digital camera driver library       *
 * for gphoto2                                      *
 *                                                  *
 * author: donn morrison - dmorriso@gulf.uvic.ca    *
 * date: dec 2000 - jan 2002                        *
 * license: gpl                                     *
 * version: 1.6                                     *
 *                                                  *
 ****************************************************/

#ifndef __DC3200_H__
#define __DC3200_H__

#include <gphoto2/gphoto2-camera.h>

#define CMD_LIST_FILES		0
#define CMD_GET_PREVIEW		1
#define CMD_GET_FILE		2

#define TIMEOUT	        	750

struct _CameraPrivateLibrary {
	int			pkt_seqnum;	/* sequence number */
	int			cmd_seqnum;	/* command seqnum */
	int			rec_seqnum;	/* last received seqnum */
	int			debug;
	time_t			last;		/* remember last recv time */
	GPContext		*context;	/* for progress updates */
};

int check_last_use(Camera *camera);

#endif
