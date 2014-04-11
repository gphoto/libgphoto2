#define _BSD_SOURCE
#include "config.h"

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include <gphoto2/gphoto2.h>

#include "command.h"

static unsigned char address = 0;
static const unsigned char sendaddr[8] = { 0x00, 0x22, 0x44, 0x66, 0x88, 0xaa, 0xcc, 0xee };
static const unsigned char recvaddr[8] = { 0x0e, 0x20, 0x42, 0x64, 0x86, 0xa8, 0xca, 0xec };
static int sw_mode = 0;
static int pic_num = 0;
static int pic_num2 = 0;
static int year, month, date;
static int hour, minutes;

static const unsigned char BOFRAME = 0xC0;
static const unsigned char EOFRAME = 0xC1;
#define CESCAPE 0x7D

static int F1reset(GPPort *port);

static int
wbyte(GPPort *port,unsigned char c)
{
  unsigned char temp = c;
  return gp_port_write(port, (char*)&temp, 1);
}

static unsigned char
checksum(unsigned char addr, unsigned char *cp, int len)
{
  int ret = addr;
  while(len --)
    ret = ret + (*cp++);
  return(0x100 -(ret & 0xff) );
}

static void
sendcommand(GPPort *port,unsigned char *p, int len)
{
  wbyte(port,BOFRAME);
  wbyte(port,sendaddr[address]);
  gp_port_write (port, (char*)p, len);
  wbyte(port,checksum(sendaddr[address], p, len));
  wbyte(port,EOFRAME);
  address ++;
  if(address >7 ) address = 0;
}

static void
Abort(GPPort *port)
{
  unsigned char buf[4];
  buf[0] = BOFRAME;
  buf[1] = 0x85;
  buf[2] = 0x7B;
  buf[3] = EOFRAME;
  gp_port_write (port, (char*)buf, 4);
}

static int recvdata(GPPort *port, unsigned char *p, int len)
{
  unsigned char s, t;
  int sum;
  int i;

  gp_log (GP_LOG_DEBUG, "recvdata", "reading %d bytes", len);
  gp_port_read(port, (char *)&s, 1);  /* BOFL */
  gp_port_read(port, (char *)&t, 1);  /* recvaddr */

  if(t != recvaddr[address]){
    gp_log (GP_LOG_ERROR, "recvdata", "address %02x does not match %02x, draining 3 bytes", t, recvaddr[address]);
    gp_port_read(port, (char *)&s, 1);  /* drain */
    gp_port_read(port, (char *)&s, 1);  /* drain */
    gp_port_read(port, (char *)&s, 1);  /* drain */
    Abort(port);
    return(-1);
  }
  i = len;
  sum = (int) t;
  while ((GP_OK <= gp_port_read(port, (char *)&s, 1)) && (s != EOFRAME)) {
    sum = sum + s;
    if(i > 0) {
      if(s == CESCAPE){
        gp_port_read(port, (char *)&s, 1);
        if(0x20 & s)
          s = 0xDF & s;
        else
          s = 0x20 | s;
      }
      *p = s;
      p++;
      i--;
    }
    t = s;
  }
  gp_log (GP_LOG_DEBUG, "recvdata", "checksum expected %02x (have %02x)", t, sum );
  gp_log (GP_LOG_DEBUG, "recvdata", "EOFL %02x (%d)", s, len - i);
  if(sum & 0xff){
    gp_log (GP_LOG_ERROR, "recvdata" ,"Checksum error.(%02x)\n", sum);
    return(-1);
  }
  return(len - i);
}

/*------------------------------------------------------------*/


char F1newstatus(GPPort *port, int verbose, char *return_buf)
{
  unsigned char buf[34];
  char status_buf[1000]="";
  char tmp_buf[150]="";
  buf[0] = 0x03;
  buf[1] = 0x02;
  sendcommand(port,buf, 2);
  recvdata(port, buf, 33);
#ifdef DEBUG
  fprintf(stderr,"Status: %02x%02x:%02x(len = %d)\n", buf[0], buf[1], buf[2], i);
#endif
  if((buf[0] != 0x03) || (buf[1] != 0x02) ||(buf[2] != 0)){
    Abort(port);
    return(-1);
  }
  sw_mode = buf[3];
  pic_num = buf[4] * 0x100 + buf[5];
  pic_num2 = buf[6] * 0x100 + buf[7];
  year = (buf[10] >> 4 ) * 10 + (buf[10] & 0x0f);
  month = (buf[11] >> 4 ) * 10 + (buf[11] & 0x0f);
  date = (buf[12] >> 4 ) * 10 + (buf[12] & 0x0f);
  hour = (buf[13] >> 4 ) * 10 + (buf[13] & 0x0f);
  minutes = (buf[14] >> 4 ) * 10 + (buf[14] & 0x0f);

  if(verbose){
    strcat(status_buf, "Current camera statistics\n\n");
    strcat(status_buf, "Mode: ");
    switch (sw_mode){
    case 1:
      strcat(status_buf, "Playback\n");
      break;
    case 2:
      strcat(status_buf, "Record[Auto]\n");
      break;
    case 3:
      strcat(status_buf, "Record[Manual]\n");
      break;
    default:
      strcat(status_buf, "Huh?\n");
      break;
    }
    sprintf(tmp_buf, "Total Pictures: %02d\n", pic_num);
    strncat(status_buf, tmp_buf, sizeof(tmp_buf));
    sprintf(tmp_buf, "Date: %02d/%02d/%02d\n", month, date, year);
    strncat(status_buf, tmp_buf, sizeof(tmp_buf));
    sprintf(tmp_buf, "Time: %02d:%02d\n",hour, minutes);
    strncat(status_buf, tmp_buf, sizeof(tmp_buf));
  }

    strcpy(return_buf, status_buf);
    return (buf[2]);   /*ok*/
}


int F1status(GPPort *port)
{

  unsigned char buf[34];

  buf[0] = 0x03;
  buf[1] = 0x02;
  sendcommand(port,buf, 2);
  recvdata(port, buf, 33);
#ifdef DEBUG
  fprintf(stderr,"Status: %02x%02x:%02x(len = %d)\n", buf[0], buf[1], buf[2], i);
#endif
  if((buf[0] != 0x03) || (buf[1] != 0x02) ||(buf[2] != 0)){
    Abort(port);
    return(-1);
  }
  sw_mode = buf[3];
  pic_num = buf[4] * 0x100 + buf[5];
  pic_num2 = buf[6] * 0x100 + buf[7];
  year = (buf[10] >> 4 ) * 10 + (buf[10] & 0x0f);
  month = (buf[11] >> 4 ) * 10 + (buf[11] & 0x0f);
  date = (buf[12] >> 4 ) * 10 + (buf[12] & 0x0f);
  hour = (buf[13] >> 4 ) * 10 + (buf[13] & 0x0f);
  minutes = (buf[14] >> 4 ) * 10 + (buf[14] & 0x0f);

  if(0){
    fprintf(stdout, "FnDial: ");
    switch (sw_mode){
    case 1:
      fprintf(stdout, "play\n");
      break;
    case 2:
      fprintf(stdout, "rec[A]\n");
      break;
    case 3:
      fprintf(stdout, "rec[M]\n");
      break;
    default:
      fprintf(stdout, "unknown?\n");
      break;
    }
    fprintf(stdout, "Picture: %3d\n", pic_num);
    fprintf(stdout,"Date: %02d/%02d/%02d\nTime: %02d:%02d\n",
            year,month,date, hour, minutes);
  }
  return (buf[2]);              /*ok*/
}

int F1howmany(GPPort *port)
{
  F1status(port);
  return(pic_num);
}

int F1fopen(GPPort *port, char *name)
{
  unsigned char buf[64];
  int len;
  buf[0] = 0x02;
  buf[1] = 0x0A;
  buf[2] = 0x00;
  buf[3] = 0x00;
  snprintf((char*)&buf[4], sizeof(buf)-4, "%s", name);
  len = strlen(name) + 5;
  sendcommand(port,buf, len);
  recvdata(port, buf, 6);
  if((buf[0] != 0x02) || (buf[1] != 0x0A) || (buf[2] != 0x00)){
    Abort(port);
    fprintf(stderr,"F1fopen fail\n");
    return(-1);
  }

  return(buf[3]);
}

int F1fclose(GPPort*port)
{
  unsigned char buf[4];

  buf[0] = 0x02;
  buf[1] = 0x0B;
  buf[2] = 0x00;
  buf[3] = 0x00;
  sendcommand(port,buf, 4);
  recvdata(port, buf, 3);
#ifdef DEBUG
  fprintf(stderr,"Fclose: %02x%02x:%02x\n", buf[0], buf[1], buf[2]);
#endif
  if((buf[0] != 0x02) || (buf[1] != 0x0B) || (buf[2] != 0x00)){
    fprintf(stderr,"F1fclose fail\n");
    Abort(port);
    return(-1);
  }
  return (buf[2]);              /* ok == 0 */
}

long F1fread(GPPort *port, unsigned char *data, long len)
{

  long len2;
  long i = 0;
  unsigned char s;

  unsigned char buf[10];

  buf[0] = 0x02;
  buf[1] = 0x0C;
  buf[2] = 0x00;
  buf[3] = 0x00;

  buf[4] = 0; /* data block size */
  buf[5] = 0;

  buf[6] = (len >> 8) & 0xff;
  buf[7] = 0xff & len;
  sendcommand(port,buf, 8);
  gp_port_read(port, (char *)buf, 9);
  if((buf[2] != 0x02) || (buf[3] != 0x0C) || (buf[4] != 0x00)){
    Abort(port);
    fprintf(stderr,"F1fread fail\n");
    return(-1);
  }

  len2 = buf[7] * 0x100 + buf[8]; /* data size */
  if(len2 == 0) {
    gp_port_read(port, (char *)&s, 1); /* last block checksum */
    gp_port_read(port, (char *)&s, 1); /* last block EOFL */
    return(0);
  }
  while((GP_OK <= gp_port_read(port, (char *)&s, 1)) && (s != EOFRAME)){
    if(s == CESCAPE){
      gp_port_read(port, (char *)&s, 1);
      if(0x20 & s)
        s = 0xDF & s;
      else
        s = 0x20 | s;
    }
    if(i < len)
      data[i] = s;
    i++;

  }
  i--; /* checksum */
  return(i);
}

long F1fseek(GPPort *port,long offset, int base)
{
  unsigned char buf[10];

  buf[0] = 0x02;
  buf[1] = 0x0E;
  buf[2] = 0x00;
  buf[3] = 0x00;

  buf[4] = (offset >> 24) & 0xff;
  buf[5] = (offset >> 16) & 0xff;
  buf[6] = (offset >> 8) & 0xff;
  buf[7] = 0xff & offset;

  buf[8] = (base >> 8) & 0xff;
  buf[9] = 0xff & base;

  sendcommand(port,buf, 10);
  recvdata(port, buf, 3);
  if((buf[0] != 0x02) || (buf[1] != 0x0E) || (buf[2] != 0x00)){
    Abort(port);
    return(-1);
  }

  return(buf[2]);
}

long F1fwrite(GPPort *port,unsigned char *data, long len, unsigned char b) /* this function not work well */
{

  long i = 0;
  unsigned char *p;
  unsigned char s;
  unsigned char buf[10];

  int checksum;

  p = data;
  wbyte(port,BOFRAME);
  wbyte(port,sendaddr[address]);
  wbyte(port,0x02);
  wbyte(port,0x14);
  wbyte(port,b);
  wbyte(port,0x00);
  wbyte(port,0x00);

  wbyte(port,(len >> 8) & 0xff);
  wbyte(port,0xff & len);

  checksum = sendaddr[address] +
    0x02 + 0x14 + b + ((len >> 8) & 0xff) + (0xff & len);

  while(i < len){
    s = *p;
    if((s == 0x7D) || (s == 0xC1) || (s == 0xC0)){
      wbyte(port,CESCAPE);
      if(0x20 & s)
        s = 0xDF & s;
      else
        s = 0x20 | s;
      checksum = checksum + CESCAPE;
      i++;
    }
    wbyte(port,s);
    checksum = checksum + s;
    i++;
    p++;
  }
  wbyte(port,0x100 -(checksum & 0xff) );
  wbyte(port,EOFRAME);
  address ++;
  if(address >7 ) address = 0;

  gp_port_read(port, (char *)buf, 7);
  if((buf[2] != 0x02) || (buf[3] != 0x14) || (buf[4] != 0x00)){
    Abort(port);
    fprintf(stderr,"F1fwrite fail\n");
    return(-1);
  }

  return(i);
}

unsigned long F1finfo(GPPort *port,char *name)
{
  unsigned char buf[64];
  int len;
  unsigned long flen;

  buf[0] = 0x02;
  buf[1] = 0x0F;
  snprintf((char*)&buf[2], sizeof(buf)-2, "%s", name);
  len = strlen(name) + 3;

  sendcommand(port,buf, len);
  len = recvdata(port, buf, 37);
  if((buf[0] != 0x02) || (buf[1] != 0x0F) || (buf[2] != 00)){
    Abort(port);
    return(0);
  }

#ifdef DEBUG
  fprintf(stderr,"info:");
  for(i = 0; i < len ; i++)
    fprintf(stderr,"%02x ", buf[i]);
  fprintf(stderr,"len = %d\n", len);
#endif

  flen = buf[33] * 0x1000000 + buf[34] * 0x10000 +
    buf[35] * 0x100 + buf[36];
#ifdef DEBUG
  fprintf(stderr,"inf len = %ld %02x %02x %02x %02x\n", flen,
          buf[33], buf[34], buf[35], buf[36]);
#endif

  if(buf[2] != 0) return(0);
  return(flen);
}

long F1getdata(GPPort*port,char *name, unsigned char *data)
{
  long filelen;
  long total = 0;
  long len;
  unsigned char *p;

  F1status(port);
  p = data;
  filelen = F1finfo(port,name);
  if(filelen < 0)
    return(0);

  if(F1fopen(port,name) != 0)
    return(0);

  while((len = F1fread(port, p, 0x0400)) != 0){
    if(len < 0){
      F1fclose(port);
      return(0);
    }
    p = p + len;
    total = total + len;
  }
  F1fclose(port);
  return(total);
}

int F1deletepicture(GPPort *port,int n)
{
  unsigned char buf[4];

  gp_log (GP_LOG_DEBUG, "F1deletepicture", "Deleting picture %d...", n);
  buf[0] = 0x02;
  buf[1] = 0x15;
  buf[2] = 0x00;
  buf[3] = 0xff & n;
  sendcommand(port,buf, 4);
  recvdata(port, buf, 3);
  if((buf[0] != 0x02) || (buf[1] != 0x15) || (buf[2] != 0)){
    Abort(port);
    return GP_ERROR;
  }
  return GP_OK;
}

int F1ok(GPPort*port)
{
  int retrycount = 100;
  unsigned char buf[64];

  gp_log (GP_LOG_DEBUG, "F1ok", "Asking for OK...");

  buf[0] = 0x01;
  buf[1] = 0x01;
  sprintf((char*)&buf[2],"SONY     MKY-1001         1.00");

  while(retrycount--){
    sendcommand(port,buf, 32);
    recvdata(port, buf, 32);
#ifdef DEBUG
    fprintf(stderr,"OK:%02x%02x:%c%c%c%c\n", buf[0], buf[1],
            buf[3],buf[4],buf[5],buf[6]);
#endif
    if((buf[0] != 0x01) || (buf[1] != 0x01) || (buf[2] != 0x00) ){
      Abort(port);
      F1reset(port);
   } else
      return 1;
  }
  return 0;                     /*ng*/
}

static int
F1reset(GPPort *port)
{
  unsigned char buf[3];
  gp_log (GP_LOG_DEBUG, "F1reset", "Resetting camera...");
 retryreset:
  buf[0] = 0x01;
  buf[1] = 0x02;
  sendcommand(port,buf, 2);
  recvdata(port, buf, 3);
#ifdef DEBUG
  fprintf(stderr,"Reset: %02x%02x:%02x\n", buf[0], buf[1], buf[2]);
#endif
  if(!((buf[0] == 0x01 ) && (buf[1] == 0x02) && buf[2] == 0x00))
    goto retryreset;
  return (int) buf[2];          /*ok*/
}
