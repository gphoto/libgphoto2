#include "config.h"

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <termios.h>
#if HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <gphoto2/gphoto2.h>

#include "command.h"
#include "common.h"

#ifdef DEBUG
#define dprintf(x) printf x
#else
#define dprintf(x)
#endif

/* static int      F1fd = -1; */
static u_char address = 0;
static u_char sendaddr[8] = { 0x00, 0x22, 0x44, 0x66, 0x88, 0xaa, 0xcc, 0xee };
static u_char recvaddr[8] = { 0x0e, 0x20, 0x42, 0x64, 0x86, 0xa8, 0xca, 0xec };
static int sw_mode = 0;
static int pic_num = 0;
static int pic_num2 = 0;
static int year, month, date;
static int hour, minutes;

static void wbyte(GPPort *port,u_char c)
{
  char temp[2];
  dprintf((stderr, "> %02x\n", c));
  temp[0]=c;
  temp[1]='\0';
  /* if (writetty(F1fd, &c, 1) < 0) { */
  if( gp_port_write(port, (char*)temp, 1) <0) {
    perror("wbyte");
    /* Exit(1); */
  }
}

static u_char rbyte(GPPort *port)
{
  u_char        c[2];
  /* if (readtty(F1fd, &c, 1) < 0) { */
  if (gp_port_read(port,c, 1) <0) {
    perror("rbtyte");
    /* Exit(1); */
  }
  dprintf((stderr, "< %02x\n", c));

  return c[0];
}

static void
wstr(GPPort *port,u_char *p, int len)
{
  dprintf((stderr, "> len=%d\n", len));
  /* if (writetty(F1fd, p, len) < 0) { */
  if( gp_port_write(port, p, len) <0) {
    perror("wstr");
    /* Exit(1); */
  }
}

static void rstr(GPPort *port,u_char *p, int len)
{

  dprintf((stderr, "< len=%d\n", len));
  /* if (readtty(F1fd, p, len) < 0) { */
  if (gp_port_read(port,p, len) <0) {
    perror("rstr");
    /* Exit(1); */
  }
}

u_char checksum(u_char addr, u_char *cp, int len)
{
  int ret = addr;
  while(len --)
    ret = ret + (*cp++);
  return(0x100 -(ret & 0xff) );
}

static void sendcommand(GPPort *port,u_char *p, int len)
{
  wbyte(port,BOFRAME);
  wbyte(port,sendaddr[address]);
  wstr(port,p, len);
  wbyte(port,checksum(sendaddr[address], p, len));
  wbyte(port,EOFRAME);
  address ++;
  if(address >7 ) address = 0;
}

static void Abort(GPPort *port)
{
  u_char buf[4];
  buf[0] = BOFRAME;
  buf[1] = 0x85;
  buf[2] = 0x7B;
  buf[3] = EOFRAME;
  wstr(port, buf, 4);
}

static int recvdata(GPPort *port, u_char *p, int len)
{
  u_char s, t;
  int sum;
  int i;

  s = rbyte(port);  /* BOFL */
  t= rbyte(port);  /* recvaddr */
#ifdef DEBUG
  fprintf(stderr,"BOFL %02x ", s);
  fprintf(stderr,"Raddr %02x %02x \n", t, recvaddr[address]);
#endif

  if(t != recvaddr[address]){
    s = rbyte(port);  /* drain */
    s = rbyte(port);  /* drain */
    s = rbyte(port);  /* drain */
#ifdef DEBUG
    fprintf(stderr," abort \n");
#endif
    Abort(port);
    return(-1);
  }
  i = len;
  sum = (int) t;
  while ((s = rbyte(port)) != EOFRAME){
    sum = sum + s;
    if(i > 0){
      if(s == CESCAPE){
        s = rbyte(port);
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
#ifdef DEBUG
  fprintf(stderr,"checksum %02x (%x)", t, sum );
  fprintf(stderr,"EOFL %02x (%d) \n", s, len - i);
#endif
  if(sum & 0xff){
#ifdef DEBUG
    fprintf(stderr,"check sum error.(%02x)\n", sum);
#endif
    return(-1);
  }
  return(len - i);
}

/*------------------------------------------------------------*/


char F1newstatus(GPPort *port, int verbose, char *return_buf)
{
  u_char buf[34];
  int i;
  char status_buf[1000]="";
  char tmp_buf[150]="";
  buf[0] = 0x03;
  buf[1] = 0x02;
  sendcommand(port,buf, 2);
  i = recvdata(port, buf, 33);
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


int F1status(GPPort *port, int verbose)
{

  u_char buf[34];
  int i;

  buf[0] = 0x03;
  buf[1] = 0x02;
  sendcommand(port,buf, 2);
  i = recvdata(port, buf, 33);
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
  F1status(port, 0);
  return(pic_num);
}

int F1fopen(GPPort *port, char *name)
{
  u_char buf[64];
  int len;
  buf[0] = 0x02;
  buf[1] = 0x0A;
  buf[2] = 0x00;
  buf[3] = 0x00;
  snprintf(&buf[4], sizeof(name), "%s", name);
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
  u_char buf[4];

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

long F1fread(GPPort *port, u_char *data, long len)
{

  long len2;
  long i = 0;
  u_char s;

  u_char buf[10];

  buf[0] = 0x02;
  buf[1] = 0x0C;
  buf[2] = 0x00;
  buf[3] = 0x00;

  buf[4] = 0; /* data block size */
  buf[5] = 0;

  buf[6] = (len >> 8) & 0xff;
  buf[7] = 0xff & len;
  sendcommand(port,buf, 8);
  rstr(port, buf, 9);
  if((buf[2] != 0x02) || (buf[3] != 0x0C) || (buf[4] != 0x00)){
    Abort(port);
    fprintf(stderr,"F1fread fail\n");
    return(-1);
  }

  len2 = buf[7] * 0x100 + buf[8]; /* data size */
  if(len2 == 0) {
    s = rbyte(port); /* last block checksum */
    s = rbyte(port); /* last block EOFL */
    return(0);
  }
  while((s = rbyte(port)) != EOFRAME){
    if(s == CESCAPE){
      s = rbyte(port);
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
  u_char buf[10];

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

long F1fwrite(GPPort *port,u_char *data, long len, u_char b) /* this function not work well */
{

  long i = 0;
  u_char *p;
  u_char s;
  u_char buf[10];

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

  rstr(port, buf, 7);
  if((buf[2] != 0x02) || (buf[3] != 0x14) || (buf[4] != 0x00)){
    Abort(port);
    fprintf(stderr,"F1fwrite fail\n");
    return(-1);
  }

  return(i);
}

u_long F1finfo(GPPort *port,char *name)
{
  u_char buf[64];
  int len;
  u_long flen;

  buf[0] = 0x02;
  buf[1] = 0x0F;
  snprintf(&buf[2], sizeof(name), "%s", name);
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

long F1getdata(GPPort*port,char *name, u_char *data, int verbose)
{
  long filelen;
  long total = 0;
  long len;
  u_char *p;

  F1status(port,0);
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
    if(verbose){
      fprintf(stderr, "%6ld/", total);
      fprintf(stderr, "%6ld", filelen);
      fprintf(stderr, "\b\b\b\b\b\b\b\b\b\b\b\b\b");
    }
  }
  F1fclose(port);
  if(verbose)
    fprintf(stderr, "\n");
  return(total);
}

int F1deletepicture(GPPort *port,int n)
{
  u_char buf[4];
  buf[0] = 0x02;
  buf[1] = 0x15;
  buf[2] = 0x00;
  buf[3] = 0xff & n;
  sendcommand(port,buf, 4);
  recvdata(port, buf, 3);
  if((buf[0] != 0x02) || (buf[1] != 0x15) || (buf[2] != 0)){
    Abort(port);
    return(-1);
  }
  return(0);
}

int F1ok(GPPort*port)
{
  int retrycount = RETRY;
  u_char buf[64];

  buf[0] = 0x01;
  buf[1] = 0x01;
  sprintf(&buf[2],"SONY     MKY-1001         1.00");

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

int F1reset(GPPort *port)
{
  u_char buf[3];
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
