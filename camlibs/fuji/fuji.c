/*
 
 Fuji Camera library for the gphoto project, 
 (C) 2001 Matthew G. Martin <matt.martin@ieee.org>
  This routine works for Fuji DS-7 and DX-5,7,10 and 
  MX-500,600,700,1200,1700,2700,2900,  Apple QuickTake 200,
  Samsung Kenox SSC-350N,Leica Digilux Zoom cameras and possibly others.

   Preview and take_picture fixes and preview conversion integrated 
   by Michael Smith <michael@csuite.ns.ca>.

   This driver was reworked from the "fujiplay" package:
      * A program to control Fujifilm digital cameras, like
      * the DS-7 and MX-700, and their clones.
      * Written by Thierry Bousch <bousch@topo.math.u-psud.fr>
      * and released in the public domain.

    Portions of this code were adapted from
    GDS7 v0.1 interactive digital image transfer software for DS-7 camera
    Copyright (C) 1998 Matthew G. Martin

    Some of which was derived from get_ds7 , a Perl Language library
    Copyright (C) 1997 Mamoru Ohno

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

 */
#include <string.h>
#include <gphoto2.h>

#include <stdlib.h>
#include <termios.h>
#include <errno.h>

#include "fuji.h"
#include "../../libgphoto2/exif.h"
#include <gphoto2.h>
#define serial_port 0

extern unsigned char *fuji_exif_convert(exifparser *exifdat,CameraFile *cfile);

#warning Do not use globals - they break pretty much every GUI for gphoto2,
#warning including Konqueror (KDE) and Nautilus (GNOME).
static gp_port *thedev;

static int pictures;
static int interrupted = 0;
static int pending_input = 0;
static struct pict_info *pinfo = NULL;


unsigned char answer[5000];
static char *fuji_buffer;
static long fuji_maxbuf=FUJI_MAXBUF_DEFAULT;
static int answer_len = 0;
static Camera *curcam=NULL;
static CameraFile *curcamfile=NULL;

static char *models_serial[] = {
        "Apple QuickTake 200",
        "Fuji DS-7",
        "Fuji DX-5",
        "Fuji DX-7",
        "Fuji DX-10",
        "Fuji MX-500",
        "Fuji MX-600",
        "Fuji MX-700",
        "Fuji MX-1200",
        "Fuji MX-1700",
        "Fuji MX-2700",
        "Fuji MX-2900",
        "Leica Digilux Zoom",
        "Samsung Kenox SSC-350N",
        "Toshiba PDR-M1",
        NULL
};


static int fuji_initialized=0; 
static int fuji_count;
static int fuji_size;
static int devfd = -1;
static int maxnum;


struct pict_info {
	char *name;
	int number;
	int size;
	short ondisk;
	short transferred;
};

static int get_raw_byte (void)
{
	static unsigned char buffer[128];
	static unsigned char *bufstart;
	int ret;

	while (!pending_input) {
		/* Refill the buffer */
	        ret=gp_port_read(thedev,buffer,1);
		/*ret = read(devfd, buffer, 128);*/
		DBG2("GOT %d",ret);
		if (ret == GP_ERROR_IO_TIMEOUT)
			return -1;  /* timeout */
		if (ret == GP_ERROR) {
		  return -1;  /* error */
		}
		if (ret <0 ) {
		  return -1;  /* undocumented error */
		}
		pending_input = ret;
		bufstart = buffer;
	}
	pending_input--;
	return *bufstart++;
}

static int wait_for_input (int seconds)
{
	fd_set rfds;
	struct timeval tv;
	DBG2("Wait for input,devfd=%d",devfd);
	if (pending_input)
		return 1;
	if (!seconds)
		return 0;
	DBG("Wait for input, running cmds");
	FD_ZERO(&rfds);
	FD_SET(devfd, &rfds);
	DBG("Wait for input,cmds complete");
	tv.tv_sec = seconds;
	tv.tv_usec = 0;

	return select(1+devfd, &rfds, NULL, NULL, &tv);
}

static int get_byte (void)
{
	int c;

	c = get_raw_byte();
	if (c < 255)
		return c;
	c = get_raw_byte();
	if (c == 255)
		return c;	/* escaped '\377' */
	if (c != 0)
		DBG("get_byte: impossible escape sequence following 0xFF\n");
	/* Otherwise, it's a parity or framing error */
	get_raw_byte();
	return -1;
}

static int put_bytes (int n, unsigned char* buff)
{
	int ret;

	ret=gp_port_write(thedev,buff,n);
	if (ret==GP_OK) {
	  buff+=n;
	  return -1;
	};
	return 0;
	while (n > 0) {
	  /*ret = write(devfd, buff, n);*/
	  ret=gp_port_write(thedev,buff,n); 
	  if (ret < 0) {
	    if (errno == EINTR)/* this doesn't fit gpio, does it ?*/
	      continue;
	    return -1;
	  }
	  n -= ret;
	  buff += ret;
	}
	return 0;
}

static int put_byte (int c)
{
	unsigned char buff[1];

	buff[0] = c;
	return put_bytes(1, buff);
}

static int attention (void)
{
	int i;

	/* drain input */
	while (get_byte() >= 0)
		continue;
	for (i = 0; i < 3; i++) {
		put_byte(ENQ);
		if (get_byte() == ACK)
			return 0;
	}
	//gp_frontend_status(NULL, "The camera does not respond.");
	DBG("The camera does not respond.");
	return(-1);
}

static void send_packet (int len, unsigned char *data, int last)
{
	unsigned char *p, *end, buff[3];
	int check;

	last = last ? ETX : ETB;
	check = last;
	end = data + len;
	for (p = data; p < end; p++)
		check ^= *p;

	/* Start of frame */
	buff[0] = ESC;
	buff[1] = STX;
	put_bytes(2, buff);

	/* Data */
	// This is not so good if we have more than one esc. char
	for (p = data; p < end; p++)
		if (*p == ESC) {
			/* Escape the last character */
		        put_bytes(p-data+1, data); /* send up to the ESC*/
			data = p+1;
			put_byte(ESC); /* send another esc*/
		}
	put_bytes(end-data, data);

	/* End of frame */
	buff[1] = last;
	buff[2] = check;
	put_bytes(3, buff);
}

static int read_packet (void)
{
	unsigned char *p = answer;
	int c, check, incomplete;

	if (get_byte() != ESC || get_byte() != STX) {
bad_frame:
		/* drain input */
		while (get_byte() >= 0)
			continue;
		return -1;
	}
	check = 0;
	while(1) {
		if ((c = get_byte()) < 0)
			goto bad_frame;
		if (c == ESC) {
			if ((c = get_byte()) < 0)
				goto bad_frame;
			if (c == ETX || c == ETB) {
				incomplete = (c == ETB);
				break;
			}
		}
		*p++ = c;
		check ^= c;
	}
	/* Append a sentry '\0' at the end of the buffer, for the convenience
	   of C programmers */
	*p = '\0';
	answer_len = p - answer;
	check ^= c;
	c = get_byte();
	if (c != check)
		return -1;
	if (answer[2] + (answer[3]<<8) != answer_len - 4)
		return -1;
	/* Return 0 for the last packet, 1 otherwise */
	return incomplete;
}


static int cmd (int len, unsigned char *data)
{
	int i, c, timeout=50;
	fuji_count=0;

	DBG2("CMD $%X called ",data[1]);	

	/* Some commands require large timeouts */
	switch (data[1]) {
	  case FUJI_TAKE:	/* Take picture */
	  case FUJI_CHARGE_FLASH:	/* Recharge the flash */
	  case FUJI_PREVIEW:	/* Take preview */
	    timeout = 12;
	    break;
	  case FUJI_DELETE:	/* Erase a picture */
	    timeout = 1;
	    break;
	}

	for (i = 0; i < 3; i++) { /* Three tries */
		send_packet(len, data, 1);
		c = get_byte();
		if (c == ACK)
			goto rd_pkt;
		if (c != NAK){
			if(attention()) return(-1);
		};
	}
	DBG2("Command error, aborting. %d bytes recd",answer_len);
	return(-1);

rd_pkt:
	/*wait_for_input(timeout);*/

	for (i = 0; i < 3; i++) {
		if (i) put_byte(NAK);
		c = read_packet();
		if (c < 0)
			continue;
		if (c && interrupted) {
		        DBG("interrupted");
			exit(1);
		}

		put_byte(ACK);

		if (fuji_buffer != NULL) {
		  if ((fuji_count+answer_len-4)<fuji_maxbuf){
		    memcpy(fuji_buffer+fuji_count,answer+4,answer_len-4);
		    fuji_count+=answer_len-4;
		  } else DBG("buffer overflow");
		    
		  DBG3("Recd %d of %d\n",fuji_count,fuji_size);

		  //if (curcamfile)
		  //  gp_frontend_progress(curcam,curcamfile,
		  //		 (1.0*fuji_count/fuji_size>1.0)?1.0:1.0*fuji_count/fuji_size);

		};
		/* More packets ? */
		if (c != 0)
			goto rd_pkt;
		//gp_frontend_progress("","",0); /* Clean up the indicator */
		return 0;
	}		
	DBG("Cannot receive answer, aborting.");
	return(-1);
}

/* Zero byte command */
static int cmd0 (int c0, int c1)
{
	unsigned char b[4];

	b[0] = c0; b[1] = c1;
	b[2] = b[3] = 0;
	return cmd(4, b);
}


/* One byte command */
static int cmd1 (int c0, int c1, int arg)
{
	unsigned char b[5];

	b[0] = c0; b[1] = c1;
	b[2] =  1; b[3] =  0;
	b[4] = arg;
	return cmd(5, b);
}

/* Two byte command */
static int cmd2 (int c0, int c1, int arg)
{
	unsigned char b[6];

	b[0] = c0; b[1] = c1;
	b[2] =  2; b[3] =  0;
	b[4] = arg; b[5] = arg>>8;
	return cmd(6, b);
}

static char* fuji_version_info (void)
{
	cmd0 (0, FUJI_VERSION);
	return answer+4;
}

static char* fuji_camera_type (void)
{
	cmd0 (0, 0x29);
	return answer+4;
}

static char* fuji_camera_id (void)
{
	cmd0 (0, 0x80);
	return answer+4;
}

static int fuji_set_camera_id (const char *id)
{
	unsigned char b[14];
	int n = strlen(id);

	if (n > 10)
		n = 10;
	b[0] = 0;
	b[1] = 0x82;
	b[2] = n;
	b[3] = 0;
	memcpy(b+4, id, n);
	return cmd(n+4, b);
}

static char* fuji_get_date (void)
{
	char *fmtdate = answer+50;
	
	cmd0 (0, 0x84);
	strcpy(fmtdate, "YYYY/MM/DD HH:MM:SS");
	memcpy(fmtdate,    answer+4,   4);	/* year */
	memcpy(fmtdate+5,  answer+8,   2);	/* month */
	memcpy(fmtdate+8,  answer+10,  2);	/* day */
	memcpy(fmtdate+11, answer+12,  2);	/* hour */
	memcpy(fmtdate+14, answer+14,  2);	/* minutes */
	memcpy(fmtdate+17, answer+16,  2);	/* seconds */

	return fmtdate;
}

static int fuji_set_date (struct tm *pt)
{
	unsigned char b[50];

	sprintf(b+4, "%04d%02d%02d%02d%02d%02d", 1900 + pt->tm_year, pt->tm_mon+1, pt->tm_mday,
		pt->tm_hour, pt->tm_min, pt->tm_sec);
	b[0] = 0;
	b[1] = 0x86;
	b[2] = strlen(b+4);	/* should be 14 */
	b[3] = 0;
	return cmd(4+b[2], b);
}

static int fuji_get_flash_mode (void)
{
	cmd0 (0, 0x30);
	return answer[4];
}

static int fuji_set_flash_mode (int mode)
{
	cmd1 (0, 0x32, mode);
	return answer[4];
}

static int fuji_nb_pictures (void)
{
	if (cmd0 (0, 0x0b)) return(-1);
	return answer[4] + (answer[5]<<8);
}

static char *fuji_picture_name (int i)
{
	cmd2 (0, 0x0a, i);
	return answer+4;
}

static int fuji_picture_size (int i)
{
	cmd2 (0, FUJI_SIZE, i);
	return answer[4] + (answer[5] << 8) + (answer[6] << 16) + (answer[7] << 24);
}

static int charge_flash (void)
{
	cmd2 (0, FUJI_CHARGE_FLASH, 200);
	return answer[4];
}

static int take_picture (void)
{
	cmd0 (0, FUJI_TAKE);
	return answer[4] + (answer[5] << 8) + (answer[6] << 16) + (answer[7] << 24);
}

static int del_frame (int i)
{
	cmd2 (0, FUJI_DELETE, i);
	return answer[4];
}

static void get_command_list (CameraPrivateLibrary* fjd)
{
	int i;
	DBG("Get command list");
	memset(fjd->has_cmd, 0, 256);
	if (cmd0 (0, FUJI_CMDS_VALID)) return ;
	for (i = 4; i < answer_len; i++)
	  fjd->has_cmd[answer[i]] = 1;
}

static int get_picture_info(int num,char *name,CameraPrivateLibrary *camdata){

          DBG("Getting name...");

	  strncpy(name,fuji_picture_name(num),100);

	  DBG2("%s\n",name);

	  /*
	   * To find the picture number, go to the first digit. According to
	   * recent Exif specs, n_off can be either 3 or 4.
	   */
	  if (camdata->has_cmd[FUJI_SIZE])   fuji_size=fuji_picture_size(num);
	  else {
	    fuji_size=70000;  /* this is an overestimation for DS7 */
	    DBG2("Image size not obtained, guessing %d",fuji_size);
	  };
	  return (fuji_size);
};

static void get_picture_list (FujiData *fjd)
{
	int i, n_off;
	char *name;
	struct stat st;

	pictures = fuji_nb_pictures();
	maxnum = 100;
	free(pinfo);
	pinfo = calloc(pictures+1, sizeof(struct pict_info));
	if (pinfo==NULL) {
	  DBG("Allocation error");
	};

	for (i = 1; i <= pictures; i++) {
	        DBG("Getting name...");

	        name = strdup(fuji_picture_name(i));
	        pinfo[i].name = name;

		DBG2("%s\n",name);

		/*
		 * To find the picture number, go to the first digit. According to
		 * recent Exif specs, n_off can be either 3 or 4.
		 */
		n_off = strcspn(name, "0123456789");
		if ((pinfo[i].number = atoi(name+n_off)) > maxnum)
			maxnum = pinfo[i].number;
		if (fjd->has_cmd[FUJI_SIZE])	pinfo[i].size = fuji_picture_size(i);
		else pinfo[i].size=65000;
		pinfo[i].ondisk = !stat(name, &st);
	}
}
/*
void list_pictures (void)
{
	int i;
	struct pict_info* pi;
	char ex;

	for (i = 1; i <= pictures; i++) {
		pi = &pinfo[i];
		ex = pi->ondisk ? '*' : ' ';
		DBG4(stderr,"%3d%c  %12s  %7d\n", i, ex,pi->name, pi->size);
	}
}
*/
static void fuji_close_connection (void)
{
	put_byte(0x04);
	usleep(50000);
}

static void reset_serial (void)
{
	if (devfd >= 0) {
	  fuji_close_connection();
		/*tcsetattr(devfd, TCSANOW, &oldt);*/
	}
	devfd = -1; /* shouldn't be needed*/
}

static int fix_serial ()
{
        struct termios newt;

	gp_port_settings settings;

	gp_port_settings_get(thedev, &settings);

        devfd=thedev->device_fd;

	DBG2("Devfd is %d",devfd);

	if (devfd < 0) {
		DBG("Cannot open device");
		return(-1);
	}

	if (tcgetattr(devfd, &newt) < 0) {
		DBG("tcgetattr");
		return(-1);
	}

	DBG2("Speed is %d",cfgetispeed(&newt));
	newt.c_iflag |= (PARMRK|INPCK);
	newt.c_iflag &= ~(BRKINT|IGNBRK|IGNPAR|ISTRIP|INLCR|IGNCR|ICRNL|IXON|IXOFF);
	newt.c_oflag &= ~(OPOST);
	newt.c_cflag |= (CLOCAL|CREAD|CS8|PARENB);
	newt.c_cflag &= ~(CSTOPB|HUPCL|PARODD);
	newt.c_lflag &= ~(ECHO|ECHOE|ECHOK|ECHONL|ICANON|ISIG|NOFLSH|TOSTOP);
	newt.c_cc[VMIN] = 0;
	newt.c_cc[VTIME] = 1;

	if (tcsetattr(devfd, TCSANOW, &newt) < 0) {
	        perror("tcsetattr");
		DBG2("fix_serial tcsetattr error for %s\n",settings.serial.port);
		exit(1);
	}
       return(attention());
}

static int fuji_set_max_speed (int newspeed)
{
  int speed,i,res;
  int speedlist[]={115200,57600,38400,19200,9600,0};  /* Serial Speeds */
  int numlist[]={8,7,6,5,0,0}; /* Rate codes for camera */
  gp_port_settings settings;

  gp_port_settings_get(thedev, &settings);
  
  DBG2("Speed currently set to %d",settings.serial.speed);

  DBG("Begin set_max_speed");

  speed=9600; /* The default */

  /* Search for speed =< that specified which is supported by the camera*/
  for (i=0;speedlist[i]>0;i+=1) {

    if (speedlist[i]>newspeed) continue;

    res=cmd1(1,7,numlist[i]); /* Check if supported */

    DBG4("Speed change to %d answer was %X, res=%d",speedlist[i],answer[4],res);

    if (answer[4]==0 && res>-1) {
      speed=speedlist[i];
      break; /* Use this one !!! */
    };
  };

  fuji_close_connection();
  settings.serial.speed=speed;

  DBG2("Setting speed to %d",speed);
  gp_port_settings_set(thedev, settings);

  if (fix_serial()) return (GP_ERROR);

  gp_port_settings_get(thedev, &settings);
  DBG2("Speed set to %d",settings.serial.speed);
  return(attention());
}

static int download_picture(int n,int thumb,CameraFile *file,CameraPrivateLibrary *fjd)
{
	clock_t t1, t2;
	char name[100];
	long int file_size;
	char *data;

	DBG3("download_picture: %d,%s",n,thumb?"thumb":"pic");

	if (!thumb) {
	        fuji_size=get_picture_info(n,name,fjd);
		DBG3("Info %3d   %12s ", n, name);
	}
	else fuji_size=10500;  /* Probly not same for all cams, better way ? */
	
	DBG2("calling download for %d bytes",fuji_size);

	t1 = times(0);
	if (cmd2(0, (thumb?0:0x02), n)==-1) return(GP_ERROR);

	DBG3("Download :%4d actual bytes vs expected %4d bytes\n", 
	     fuji_count ,fuji_size);

	//file->bytes_read=fuji_count;
	//	file->size=fuji_count+(thumb?128:0);/*add room for thumb-decode*/
	file_size=fuji_count+(thumb?128:0);

	data=malloc(file_size);

	if (data==NULL) return(GP_ERROR);

	memcpy(data,fuji_buffer,fuji_count);

	gp_file_set_data_and_size(file,data,file_size);

	t2 = times(0);

	DBG2("%3d seconds, ", (int)(t2-t1) / CLK_TCK);
	DBG2("%4d bytes/s\n", 
		fuji_count * CLK_TCK / (int)(t2-t1));

	if (fjd->has_cmd[17]&&!thumb){
	if (!thumb&&(fuji_count != fuji_size)){
	  //gp_frontend_status(NULL,"Short picture file--disk full or quota exceeded\n");
	    return(GP_ERROR);
	  };
	};
	return(GP_OK);

}

static int fuji_free_memory (void)
{
	cmd0 (0, 0x1B);
	return answer[5] + (answer[6]<<8) + (answer[7]<<16) + (answer[8]<<24);
}


static int delete_pic (const char *picname,FujiData *fjd)
{
	int i, ret;

	for (i = 1; i <= pictures; i++)
	        if (!strcmp(pinfo[i].name, picname)) {
	                 if ((ret = del_frame(i)) == 0)
			          get_picture_list(fjd);
			 return ret;
		}
	return -1;
}


static char* auto_rename (void)
{
	static char buffer[13];
	
	if (maxnum < 99999)
		maxnum++;

	sprintf(buffer, "DSC%05d.JPG", maxnum);
	return buffer;
}


static int upload_pic (const char *picname)
{
	unsigned char buffer[516];
	const char *p;
	struct stat st;
	FILE *fd;
	int c, last, len, free_space;

	fd = fopen(picname, "r");
	if (fd == NULL) {
	  //update_status("Cannot open file for upload");
		return 0;
	}
	if (fstat(fileno(fd), &st) < 0) {
		DBG("fstat");
		return 0;
	}
	free_space = fuji_free_memory();
	fprintf(stderr, "Uploading %s (size %d, available %d bytes)\n",
		picname, (int) st.st_size, free_space);
	if (st.st_size > free_space) {
		fprintf(stderr, "  not enough space\n");
		return 0;
	}
	if ((p = strrchr(picname, '/')) != NULL)
		picname = p+1;
	if (strlen(picname) != 12 || memcmp(picname,"DSC",3) || memcmp(picname+8,".JPG",4)) {
		picname = auto_rename();
		fprintf(stderr, "  file renamed %s\n", picname);
	}
	buffer[0] = 0;
	buffer[1] = 0x0F;
	buffer[2] = 12;
	buffer[3] = 0;
	memcpy(buffer+4, picname, 12);
	cmd(16, buffer);
	if (answer[4] != 0) {
		fprintf(stderr, "  rejected by the camera\n");
		return 0;
	}
	buffer[1] = 0x0E;
	while(1) {
		len = fread(buffer+4, 1, 512, fd);
		if (!len) break;
		buffer[2] = len;
		buffer[3] = (len>>8);
		last = 1;
		if ((c = getc(fd)) != EOF) {
			last = 0;
			ungetc(c, fd);
		}
		if (!last && interrupted) {
			fprintf(stderr, "Interrupted!\n");
			exit(1);
		}
again:
		send_packet(4+len, buffer, last);
		/*wait_for_input(1);*/
		if (get_byte() == 0x15)
			goto again;
	}
	fclose(fd);
	fprintf(stderr, "  looks ok\n");
	return 1;
}


/***********************************************************************
     gphoto lib calls
***********************************************************************/


static int fuji_configure(){
  /* 
     DS7 can't be configured, looks like other Fuji cams can...
     expansion needed here.
  */
  // open_fuji_config_dialog();

  return(1);
};

static int fuji_get_picture (int picture_number,int thumbnail,CameraFile *cfile,CameraPrivateLibrary *fjd){

  const char *buffer;
  long int file_size;

  exifparser exifdat;

  DBG3("fuji_get_picture called for #%d %s\n",
       picture_number, thumbnail?"thumb":"photo");

  //if (fuji_init()) return(GP_ERROR);
  DBG2("buffer is %x",fuji_buffer);

  //  if (thumbnail)
  //  sprintf(tmpfname, "%s/gphoto-thumbnail-%i-%i.tiff",

  DBG("test 1");

  if (download_picture(picture_number,thumbnail,cfile,fjd)!=GP_OK) {
    return(GP_ERROR);
  };

  DBG("test 2");

  //buffer=cfile->data;
  gp_file_get_data_and_size(cfile,&buffer,&file_size);

  /* Work on the tags...*/
  exifdat.header=buffer;
  exifdat.data=buffer+12;

  if (exif_parse_data(&exifdat)>0){
    stat_exif(&exifdat);  /* pre-parse the exif data */

    //newimage->image_info_size=exifdat.ifdtags[thumbnail?1:0]*2;

    //if (thumbnail) newimage->image_info_size+=6; /* just a little bigger...*/

    //DBG2("Image info size is %d",cfile->size);

    //newimage->image_info=malloc(sizeof(char*)*infosize);

    if (buffer==NULL) {
      DBG("BIG TROUBLE!!! Bad image memory allocation");
      return(GP_ERROR);
    };

    if (thumbnail) {/* This SHOULD be done by tag id */
      //      togphotostr(&exifdat,0,0,newimage->image_info,newimage->image_info+1);
      //      togphotostr(&exifdat,0,1,newimage->image_info+2,newimage->image_info+3);
      //      togphotostr(&exifdat,0,2,newimage->image_info+4,newimage->image_info+5);
      //      newimage->image_info_size+=6;
    };

    //exif_add_all(&exifdat,thumbnail?1:0,newimage->image_info+(thumbnail?6:0));

    //DBG("====================EXIF TAGS================");
    //for(i=0;i<cfile->size/2;i++)
    // DBG3("%12s = %12s \n",cfile->data[i*2],cfile->data[i*2+1]);
    //DBG("=============================================");

    /* Tiff info in thumbnail must be converted to be viewable */
    if (thumbnail) {
      fuji_exif_convert(&exifdat,cfile);
      gp_file_get_data_and_size(cfile,&buffer,&file_size);
      
      //  if (cfile->data==NULL) {
      if (buffer==NULL) {
	DBG("Thumbnail conversion error");
	return(GP_ERROR);
      };
    }
    else  { /* Real image ... */
      //cfile->data=buffer;  /* ok, use original data...*/
      //strcpy(cfile->type,"image/jpg");
    };
  } else    { /* Wasn't recognizable exif data */
	DBG("Exif data parsing error");
  };

  return(GP_OK/*newimage*/);

};

static int fuji_delete_image (FujiData *fjd, int picture_number){
  int err_status;

  if (!fjd->has_cmd[FUJI_DELETE]) {
    return(0);
  };

  err_status = del_frame(picture_number);
  reset_serial();

  /* Warning, does the index need to be re-counted after each delete?*/
  return(!err_status);
};

static int fuji_number_of_pictures (){
  int numpix;

  numpix=fuji_nb_pictures();

  reset_serial(); /* Wish this wasn't necessary...*/
  return(numpix);

};

int camera_id (CameraText *id) {

	strcpy(id->text, "Fuji");

	return (GP_OK);
}

int camera_abilities (CameraAbilitiesList *list) {
	CameraAbilities a;
	int i;

	DBG("Camera abilities");

	i=0;
	
	while (models_serial[i]) {

	  strcpy(a.model, models_serial[i]);
	  a.port     = GP_PORT_SERIAL;
	  a.speed[0] = 9600;
	  a.speed[1] = 19200;
	  a.speed[2] = 38400;
	  a.speed[3] = 56700;
	  a.speed[4] = 115200;
	  a.speed[5] = 0;

	  a.file_operations   =    GP_FILE_OPERATION_PREVIEW;
	  a.operations        =  	GP_OPERATION_CAPTURE_IMAGE |
					GP_OPERATION_CAPTURE_PREVIEW |
	    GP_OPERATION_CONFIG;

	  gp_abilities_list_append(list, a);
	  i=i+1;
	};

	return GP_OK;
}

static int camera_exit (Camera *camera) {
  DBG("Camera exit");
  fuji_close_connection();
  if (camera->port) gp_port_close(camera->port);
  DBG("Done");
  return (GP_OK);
}

static int camera_folder_list	(Camera *camera, char *folder, CameraList *list) {
  DBG2("Camera folder list,%s",folder);
  if (folder==NULL) gp_list_append(list,"/",NULL);
  return (GP_OK);
}

static int camera_file_list (Camera *camera, const char *folder, CameraList *list) {
  int i,n;
  char* fn=NULL;
  //FujiData *cs;
  CameraPrivateLibrary *cs;

  //cs=(FujiData*)camera->camlib_data;
  cs=(CameraPrivateLibrary *)camera->pl;

  DBG("Camera file list");
  fix_serial();
  n=fuji_nb_pictures();
  DBG3("Getting file list for %s, %d files",folder,n);
  for (i=0;i<n;i++) {
    fn=strdup(fuji_picture_name(i+1));
    DBG3("File %s is number %d",fn,i+1);
    //gp_filesystem_append(camera->fs,"/",fn);
    gp_list_append (list, fn, NULL);
  };
  return (GP_OK);
  }


static int
file_list_func (CameraFilesystem *fs, const char *folder, CameraList *list,
	       void *data)
{
	Camera *camera = data;
	DBG("file_list_func");
	return (camera_file_list (camera, folder, list));
}

static int
folder_list_func (CameraFilesystem *fs, const char *folder,
		      CameraList *list, void *data)
{
  //	Camera *camera = data;
	DBG("folder_list_func");

	return (GP_OK);
}

static int get_info_func (CameraFilesystem *fs, const char *folder, const char *filename,CameraFileInfo *info, void *data)
{
  DBG2("Info Func %s",folder);

  info->file.fields = GP_FILE_INFO_SIZE | GP_FILE_INFO_TYPE |
    GP_FILE_INFO_NAME;
  info->preview.fields = GP_FILE_INFO_SIZE | GP_FILE_INFO_TYPE;

  /* Name of image */
  strncpy (info->file.name, "test\0", 5);
  
  /* Get the size of the current image */
  info->file.size = 1;
  
  /* Type of image? */
  /* if (strstr (filename, ".MOV") != NULL) {
     strcpy (info->file.type, GP_MIME_QUICKTIME);
     strcpy (info->preview.type, GP_MIME_JPEG);
     } else if (strstr (filename, ".TIF") != NULL) {
     strcpy (info->file.type, GP_MIME_TIFF);
     strcpy (info->preview.type, GP_MIME_TIFF);
     } else {*/
     strcpy (info->file.type, GP_MIME_JPEG);
     strcpy (info->preview.type, GP_MIME_JPEG);
     //     }


  return(GP_OK);
};

static int
set_info_func (CameraFilesystem *fs, const char *folder, const char *filename,
	       CameraFileInfo *info, void *data)
{
	/* Implement for example renaming of files */

	return (GP_ERROR_NOT_SUPPORTED);
}

static int
get_file_func (CameraFilesystem *fs, const char *folder,
	       const char *filename, CameraFileType type, CameraFile *file,
	       void *data) { 

  Camera *camera = data;
  int num;
  //FujiData *fjd;
  CameraPrivateLibrary *fjd;

  fjd=camera->pl;


  curcam=camera;
  curcamfile=file;

  DBG3("Camera file type %d get %s",type,filename);

  num=gp_filesystem_number   (camera->fs, "/" ,filename)+1;


  DBG3("File %s,%d\n",filename,num);

	return (fuji_get_picture(num,type==0?1:0,file,fjd));
}

static int camera_file_put (Camera *camera, CameraFile *file, char *folder) {

	return (GP_OK);
}

static int camera_file_delete (Camera *camera, const char *folder, const char *filename) {

	return (GP_OK);
}

static int camera_config_get (Camera *camera, CameraWidget *window) {

	return (GP_OK);
}

static int camera_config_set (Camera *camera, CameraWidget *window) {

	return (GP_OK);
}

//static int camera__set_config (Camera *camera, CameraSetting *conf, int count) {
//
//	return (GP_OK);
//}

static int camera_capture (Camera *camera, int capture_type, CameraFilePath *file) {

	return (GP_OK);
}

static int camera_summary (Camera *camera, CameraText *summary) {

	return (GP_OK);
}

static int camera_manual (Camera *camera, CameraText *manual) {

	return (GP_OK);
}

static int camera_about (Camera *camera, CameraText *about) {

	strcpy(about->text, 
"Fuji DS-7\n
Matthew G. Martin\n
Based on fujiplay by
Thierry Bousch<bousch@topo.math.u-psud.fr>

Known to work with Fuji DX-5,DS-7,MX-10,MX-600,MX-1700,MX-2700,Apple QuickTake 200,Samsung Kenox SSC-350N cameras.\n");

	return (GP_OK);
}

int camera_init (Camera *camera) {
        char idstring[256];
	CameraPrivateLibrary *fjd;

	gp_port_settings settings;

	DBG("Camera init");

	/* First, set up all the function pointers */
	camera->functions->exit 	= camera_exit;
	//camera->functions->file_get 	= camera_file_get;
	//	camera->functions->file_get_preview =  camera_file_get_preview;
	//	camera->functions->file_put 	= camera_file_put;
	//camera->functions->file_delete 	= camera_file_delete;
	/*	camera->functions->config_get   = camera_config_get;
		camera->functions->config_set   = camera_config_set;*/
	camera->functions->capture 	= camera_capture;
	camera->functions->summary	= camera_summary;
	camera->functions->manual 	= camera_manual;
	camera->functions->about 	= camera_about;

	//camera->camlib_data=malloc(sizeof( FujiData));
	camera->pl=malloc(sizeof( CameraPrivateLibrary ));
	fjd=camera->pl;//(FujiData*)camera->camlib_data;

	/* Set up the CameraFilesystem */
        gp_filesystem_set_list_funcs (camera->fs,
                                        file_list_func, folder_list_func,
                                        camera);
        gp_filesystem_set_info_funcs (camera->fs,
	                                        get_info_func, set_info_func,
	                              camera);
	gp_filesystem_set_file_funcs (camera->fs, get_file_func, NULL, camera);

	/* Set up the port */
	thedev=camera->port;
	gp_port_settings_get (camera->port, &settings);
	gp_port_timeout_set(thedev,1000);
	settings.serial.speed 	 = 9600;
	settings.serial.bits 	 = 8;
	settings.serial.parity 	 = 0;
	settings.serial.stopbits = 1;

	gp_port_settings_set(camera->port,settings);
	//gp_port_open(thedev);

	//DBG2("Initialized port %s",settings.serial.port);

	fuji_maxbuf=FUJI_MAXBUF_DEFAULT; /* This should be camera dependent */

	if (fix_serial()) return (GP_ERROR);

	//DBG("Port fixed, setting max speed");

	//fuji_set_max_speed(57600);

	//DBG("Max speed set");

	if (!fuji_initialized){
	  fuji_buffer=malloc(fuji_maxbuf);
	  if (fuji_buffer==NULL) {
	    DBG("Memory allocation error");
	    return(-1);
	  } else {
	    DBG2("Allocated %ld bytes to main buffer",fuji_maxbuf);
	  };
	  
	  /* could ID camera here and set the relevent variables... */
	  get_command_list(camera->pl);
	  /* For now assume that the user wil not use 2 different fuji cams
	     in one session */
	  strcpy(idstring,"Identified ");
	  strncat(idstring,fuji_version_info(),100);
	  //gp_frontend_status(NULL,idstring);

	  /* load_fuji_options() */
	  fuji_initialized=1;  
	};

	return (GP_OK);
}
