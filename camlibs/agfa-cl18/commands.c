/*
 * commands.c
 *
 *  Command set for the agfa cameras
 *
 * Copyright 2001 Vince Weaver <vince@deater.net>
 */

#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#ifdef OS2
#include <db.h>
#endif
#include <netinet/in.h>
#include <gphoto2.h>

#include "commands.h"

#define AGFA_RESET          0x0001
#define AGFA_TAKEPIC1       0x0004
#define AGFA_TAKEPIC2       0x0094
#define AGFA_TAKEPIC3       0x0092
#define AGFA_DELETE         0x0100
#define AGFA_GET_PIC        0x0101
#define AGFA_GET_PIC_SIZE   0x0102
#define AGFA_GET_NUM_PICS   0x0103
#define AGFA_DELETE_ALL2    0x0105
#define AGFA_END_OF_THUMB   0x0106   /* delete all one? */
#define AGFA_GET_NAMES      0x0108
#define AGFA_GET_THUMB_SIZE 0x010A
#define AGFA_GET_THUMB      0x010B
#define AGFA_DONE_PIC       0x01FF
#define AGFA_STATUS         0x0114

/* agfa protocol primitives */
struct agfa_command {
        unsigned int length;
        unsigned int command;
        unsigned int argument;
};

struct agfa_file_command {
        unsigned int length;
        char filename[12];
};

#define CHECK(result) {int r = (result); if (r < 0) return (r);}

/* Regular commands always 8 bytes long */
static void
build_command (struct agfa_command *cmd, int command, int argument)
{
	cmd->length = 8;
	cmd->command = command;
	cmd->argument = argument;
}

/* Filenames are always 12 bytes long */
static void
build_file_command (struct agfa_file_command *cmd, const char *filename)
{
	cmd->length = 0x0c;
	strncpy (cmd->filename,filename,12);
}

int
agfa_reset (struct agfa_device *dev)
{
	struct agfa_command cmd;
	
	build_command (&cmd, AGFA_RESET, 0);
	CHECK (gp_port_write (dev->gpdev, (char*)&cmd, sizeof (cmd)));

	return (GP_OK);
}

/* Status is a 60 byte array.  I have no clue what it does */
int
agfa_get_status (struct agfa_device *dev, int *taken,
		 int *available, int *rawcount)
{
	struct agfa_command cmd;
	unsigned char ss[0x60];

	build_command (&cmd, AGFA_STATUS, 0);
	CHECK (gp_port_write (dev->gpdev, (char*)&cmd, sizeof (cmd)));
	CHECK (gp_port_read (dev->gpdev, (unsigned char *)&ss, sizeof (ss)));

	CHECK (agfa_reset (dev));

	return (GP_OK);
}

/* Below contributed by Ben Hague <benhague@btinternet.com> */
int
agfa_capture (struct agfa_device *dev, CameraFilePath *path)
{
	/*
	 * FIXME: Not fully implemented according to the gphoto2 spec.
	 * Should also save taken picture and return the location in path,
	 * but when I try to do that it just hangs
	 */ 
	
	struct agfa_command cmd;
	int ret, taken;

	gp_debug_printf (GP_DEBUG_HIGH, "agfa", "agfa_capture");
	
	/*
	 * 2001/08/30 Lutz Müller <urc8@rz.uni-karlsruhe.de:
	 * You don't check for results here?
	 */
	build_command (&cmd, AGFA_TAKEPIC2, 0);
	ret = gp_port_write (dev->gpdev, (char*)&cmd, sizeof (cmd));
	build_command (&cmd, AGFA_TAKEPIC1, 0);
	ret = gp_port_write (dev->gpdev, (char*)&cmd, sizeof (cmd));
	build_command (&cmd, AGFA_TAKEPIC3, 0);
	ret = gp_port_write (dev->gpdev, (char*)&cmd, sizeof (cmd));
	build_command (&cmd, AGFA_TAKEPIC2, 0);
	ret = gp_port_write (dev->gpdev, (char*)&cmd, sizeof(cmd));
	
	/* 
	 * Not sure if this delay is necessary, but it was used in the windows
	 * driver
	 */
	sleep(20);
	
	/* Again, three times in windows driver */
	taken = agfa_photos_taken (dev);
	taken = agfa_photos_taken (dev);
	taken = agfa_photos_taken (dev);
	
	/* 
	 * This seems to do some kind of reset, but does cause the camera to
	 * start responding again
	 */
	build_command (&cmd, AGFA_GET_NAMES, 0);
	ret = gp_port_write (dev->gpdev, (char*)&cmd, sizeof(cmd));
	
	return GP_OK;
}


int
agfa_photos_taken (struct agfa_device *dev)
{
	struct agfa_command cmd;
	int numpics;
	
	build_command (&cmd, AGFA_GET_NUM_PICS, 0);
	CHECK (gp_port_write (dev->gpdev, (char*)&cmd, sizeof (cmd)));
	CHECK (gp_port_read (dev->gpdev, (char*)&numpics, sizeof (numpics)));

	return (numpics);
}


int
agfa_get_file_list (struct agfa_device *dev)
{
	struct agfa_command cmd;
	char *buffer;
	int ret, taken, buflen;

	/* It seems we need to do a "reset" packet before reading names?? */
	agfa_reset (dev);

	CHECK (taken = agfa_photos_taken (dev));

	dev->num_pictures = taken;

	buflen = (taken * 13)+1;  /* 12 char filenames and space for each */
                              /* plus trailing NULL */
	buffer = malloc (buflen);
	if (!buffer) {
		gp_debug_printf (GP_DEBUG_HIGH, "agfa", "Could not allocate "
				 "%i bytes!", buflen);
		return (GP_ERROR_NO_MEMORY);
	}

	build_command (&cmd, AGFA_GET_NAMES, buflen);
	ret = gp_port_write (dev->gpdev, (char*)&cmd, sizeof(cmd));
	if (ret < 0) {
		free (buffer);
		return (ret);
	}

	ret = gp_port_read (dev->gpdev, (void *)buffer, buflen);
	if (ret < 0) {
		free (buffer);
		return (ret);
	}
	
	if (dev->file_list)
		free (dev->file_list);

	dev->file_list = malloc (taken * 13);
	if (!dev->file_list) {
		gp_debug_printf (GP_DEBUG_HIGH, "agfa", "Could not allocate "
				 "%i bytes!", taken * 13);
		free (buffer);
		return (GP_ERROR_NO_MEMORY);
	}
	
	memcpy (dev->file_list, buffer, taken * 13);
	free (buffer);

#if 0
	agfa_photos_taken (dev, &taken);
	agfa_get_thumb_size (dev, dev->file_list);
#endif

	return (GP_OK);
}

int
agfa_get_thumb_size (struct agfa_device *dev, const char *filename)
{
	struct agfa_command cmd;
	struct agfa_file_command file_cmd;
	int temp, size;

	build_command (&cmd, AGFA_GET_THUMB_SIZE, 0);
	CHECK (gp_port_write (dev->gpdev, (char*)&cmd, sizeof(cmd)));

	/* always returns ff 0f 00 00 ??? */
	CHECK (gp_port_read (dev->gpdev, (char*)&temp, sizeof(temp)));

	build_file_command (&file_cmd, filename);
	CHECK (gp_port_write (dev->gpdev, (char*)&file_cmd, sizeof(file_cmd)));

	CHECK (gp_port_read (dev->gpdev, (char*)&size, sizeof(size)));

	return size;
}

int
agfa_get_thumb (struct agfa_device *dev, const char *filename,
		unsigned char *data,int size)
{
	struct agfa_command cmd;
	struct agfa_file_command file_cmd;
	int temp; 
#if 0
	unsigned char temp_string[8];
#endif
	
	build_command (&cmd, AGFA_GET_THUMB, 0);
	CHECK (gp_port_write (dev->gpdev, (char*)&cmd, sizeof(cmd)));

	/* always returns ff 0f 00 00 ??? */
	CHECK (gp_port_read (dev->gpdev, (char*)&temp, sizeof(temp)));

	build_file_command (&file_cmd, filename);
	CHECK (gp_port_write (dev->gpdev,(char*)&file_cmd, sizeof(file_cmd)));

	CHECK (gp_port_read (dev->gpdev, data, size));

#if 0
	/* Is this needed? */
	agfa_photos_taken (dev, &ret);
   
        build_command (&cmd, AGFA_END_OF_THUMB, 0);
	CHECK (gp_port_write (dev->gpdev,&cmd, sizeof(cmd)));

	CHECK (gp_port_read (dev->gpdev, temp_string, 8));
#endif

	return (GP_OK);
}

int
agfa_get_pic_size (struct agfa_device *dev, const char *filename)
{
	struct agfa_command cmd;
	struct agfa_file_command file_cmd;
	int temp, size;
	
	build_command (&cmd, AGFA_GET_PIC_SIZE, 0);
	CHECK (gp_port_write (dev->gpdev, (char*)&cmd, sizeof(cmd)));

	/* always returns ff 0f 00 00 ??? */
	CHECK (gp_port_read (dev->gpdev, (char*)&temp, sizeof(temp)));

	build_file_command (&file_cmd, filename);
	CHECK (gp_port_write (dev->gpdev,(char*)&file_cmd, sizeof(file_cmd)));

	CHECK (gp_port_read (dev->gpdev, (char*)&size, sizeof(size)));

	return size;
}

int
agfa_get_pic (struct agfa_device *dev, const char *filename,
	      unsigned char *data,int size)
{
	struct agfa_command cmd;
	struct agfa_file_command file_cmd;
	int temp;
#if 0
	int taken;
#endif

#if 0
	taken = agfa_photos_taken (dev);
	agfa_get_pic_size (dev, filename);
#endif
	build_command (&cmd, AGFA_GET_PIC, 0);
	CHECK (gp_port_write (dev->gpdev, (char*)&cmd, sizeof(cmd)));

	/* always returns ff 0f 00 00 ??? */
	CHECK (gp_port_read (dev->gpdev, (char*)&temp, sizeof(temp)));

	build_file_command (&file_cmd, filename);
	CHECK (gp_port_write (dev->gpdev,(char*)&file_cmd, sizeof(file_cmd)));

	CHECK (gp_port_read (dev->gpdev, data, size));

#if 0
	/* Have to do this after getting pic */
	build_command (&cmd, AGFA_DONE_PIC, 0);
	CHECK (gp_port_write (dev->gpdev, (char*)&cmd, sizeof(cmd)));
#endif

	return (GP_OK);
}

/* thanks to heathhey3@hotmail.com for sending me the trace */
/* to implement this */
int
agfa_delete_picture (struct agfa_device *dev, const char *filename)
{
	struct agfa_command cmd;
	struct agfa_file_command file_cmd;
	int ret, temp,taken;
	char data[4], *buffer;
	int size=4, buflen;
	
	/* yes, we do this twice?? */
	taken = agfa_photos_taken (dev);
	taken = agfa_photos_taken (dev);
	
	build_command (&cmd, AGFA_GET_PIC_SIZE,0);
	CHECK (gp_port_write (dev->gpdev, (char*)&cmd, sizeof(cmd)));

	/* always returns ff 0f 00 00 ??? */
	CHECK (gp_port_read (dev->gpdev, (char*)&temp, sizeof(temp)));

	/* Some traces show sending other than the file we want deleted? */
	build_file_command (&file_cmd,filename);
	CHECK (gp_port_write (dev->gpdev, (char*)&file_cmd, sizeof(file_cmd)));

	CHECK (gp_port_read (dev->gpdev, data, size));

	/* Check num taken AGAIN */
	taken = agfa_photos_taken (dev);
	
	build_command (&cmd, AGFA_GET_PIC_SIZE, 0);
	CHECK (gp_port_write (dev->gpdev, (char*)&cmd, sizeof(cmd)));

	/* always returns ff 0f 00 00 ??? */
	CHECK (gp_port_read (dev->gpdev, (char*)&temp, sizeof(temp)));

	build_file_command (&file_cmd, filename);
	CHECK (gp_port_write (dev->gpdev, (char*)&file_cmd, sizeof(file_cmd)));

	CHECK (gp_port_read (dev->gpdev, data, size));

	/* Check num taken AGAIN */
	taken = agfa_photos_taken (dev);

	build_command (&cmd, AGFA_DELETE, 0);
	CHECK (gp_port_write (dev->gpdev, (char*)&cmd, sizeof(cmd)));

        /* read ff 0f ??? */
	CHECK (gp_port_read (dev->gpdev, data, size));

	build_file_command (&file_cmd, filename);
	CHECK (gp_port_write (dev->gpdev, (char*)&file_cmd, sizeof(file_cmd)));

        /* This is the point we notices that in fact a pic is missing */
        /* Why do it 4 times??? Timing?? Who knows */
	taken = agfa_photos_taken (dev);
	taken = agfa_photos_taken (dev);
	taken = agfa_photos_taken (dev);
	taken = agfa_photos_taken (dev);
	
	/* Why +1 ? */
	buflen = ((taken+1) * 13)+1;  /* 12 char filenames and space for each */
                              /* plus trailing NULL */
	buffer = malloc (buflen);
	if (!buffer) {
		gp_debug_printf (GP_DEBUG_HIGH, "agfa", "Could not allocate "
				 "%i bytes!", buflen);
		return (GP_ERROR_NO_MEMORY);
	}
	
	build_command (&cmd, AGFA_GET_NAMES, buflen);
	ret = gp_port_write (dev->gpdev, (char*)&cmd, sizeof(cmd));
	if (ret < 0) {
		free (buffer);
		return (ret);
	}
	
	ret = gp_port_read (dev->gpdev, (void *)buffer, buflen);
	if (ret < 0) {
		free (buffer);
		return (ret);
	}

	if (dev->file_list)
		free (dev->file_list);
	dev->file_list = buffer;
	
	build_command (&cmd, AGFA_GET_PIC_SIZE, 0);
	CHECK (gp_port_write (dev->gpdev, (char*)&cmd, sizeof(cmd)));

	/* always returns ff 0f 00 00 ??? */
	CHECK (gp_port_read (dev->gpdev, (char*)&temp, sizeof(temp)));

	build_file_command (&file_cmd, filename);
	CHECK (gp_port_write (dev->gpdev, (char*)&file_cmd, sizeof(file_cmd)));

	CHECK (gp_port_read (dev->gpdev, data, size));

	return (GP_OK);
}

