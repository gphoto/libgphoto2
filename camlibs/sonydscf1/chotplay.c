#include "config.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <gphoto2.h>
extern int errno;

#define BINARYFILEMODE

#ifdef BINARYFILEMODE
#include <fcntl.h>  /* for setmode() */
#endif
#include <stdlib.h>
#include <time.h>
#if HAVE_UNISTD_H
# include <sys/types.h>
# include <unistd.h>
#endif
#if HAVE_SYS_PARAM_H
# include <sys/param.h>
#else
#define MAXPATHLEN 256
#endif
#include "common.h"
#include "command.h"
#include <termios.h>
#include "pmp.h"
//#include <io.h>

#define MAX_PICTURE_NUM 200

u_char picture_index[MAX_PICTURE_NUM];
u_short picture_thumbnail_index[MAX_PICTURE_NUM];
u_char picture_protect[MAX_PICTURE_NUM];
u_char picture_rotate[MAX_PICTURE_NUM];

#ifdef BINARYFILEMODE
#define WMODE "wb"
#define RMODE "rb"
#else
#define WMODE "w"
#define RMODE "r"
#endif
extern int      optind, opterr;
extern char     *optarg;
static int      verbose = 0;
static  int     errflg = 0;

#if 0
void Exit(code)
     int code;
{
  if (!(F1getfd() < 0)){
    F1reset();
    closetty(F1getfd());
  }
  exit(code);
}
#endif

int write_file(u_char *buf, int len, FILE *outfp)
{
  int i, l;
  int result;

  i = 0;
  while( len > i) {
    l = ( (len - i) < BUFSIZ) ? (len -i) : BUFSIZ;
    result = fwrite(&buf[i], sizeof(u_char), l, outfp);
    if(result != len){
      perror("chotplay");
      if(outfp != stdout);
      fclose(outfp);
      Exit(2);
    }

    i = i + l;
  }
  return(i);
}


int make_jpeg_comment(u_char *buf, u_char *jpeg_comment)
{
  int i, cur = 0;
  int reso, shutter;

  struct resolution {
    int reso_val;
    char *reso_conv;
  } reso_tab[] = {
    {PMP_FIN, "fine"},
    {PMP_STD, "standard"},
    {PMP_ECM, "economy"},
    {0,       "unknown"},
  };

  struct sh_speed {
    int spd_val;
    char *spd_conv;
  } sh_speed_tab[] = {
    {0x0123, "1/7.5"},
    {0x0187, "1/15"},
    {0x01eb, "1/30"},
    {0x024f, "1/60"},
    {0x0298, "1/100"},
    {0x031d, "1/250"},
    {0x0381, "1/500"},
    {0x03e5, "1/1000"},
    {0,      "unknown"},
  };

  jpeg_comment[0] = 0xff;
  jpeg_comment[1] = 0xd8;
  jpeg_comment[2] = 0xff;
  jpeg_comment[3] = 0xfe;

  /* resolution */
  reso = *(buf+PMP_RESOLUTION);

  i = 0;
  while (1) {
    if ((reso == reso_tab[i].reso_val) || (reso_tab[i].reso_val == 0)) {
      cur = 6 + sprintf(&jpeg_comment[6], "Resolution: %s\n",
                          reso_tab[i].reso_conv);
      break;
    }
    i++;
  }

  /* shutter speed */
  shutter = (int)get_u_short(buf+PMP_SPEED);

  i = 0;
  while (1) {
    if ((shutter == sh_speed_tab[i].spd_val) ||
        (sh_speed_tab[i].spd_val == 0)) {
      cur = cur + sprintf(&jpeg_comment[cur], "Shutter-speed: %s\n",
                          sh_speed_tab[i].spd_conv);
      break;
    }
    i++;
  }

  /* PMP comment */
  if (*(buf+PMP_COMMENT)) {
    cur = cur + sprintf(&jpeg_comment[cur], "Comment: %s\n",
                        (char *)(buf+PMP_COMMENT));
  }

  /* taken date */
  if (*(buf+PMP_TAKE_YEAR) == 0xff) {
    cur = cur + sprintf(&jpeg_comment[cur],
                        "Date-Taken: ----/--/-- --:--:--\n");
  }
  else {
    cur = cur + sprintf(&jpeg_comment[cur],
                        "Date-Taken: %d/%02d/%02d %02d:%02d:%02d\n",
    2000+(*(buf+PMP_TAKE_YEAR)), *(buf+PMP_TAKE_MONTH),
    *(buf+PMP_TAKE_DATE), *(buf+PMP_TAKE_HOUR), *(buf+PMP_TAKE_MINUTE),
    *(buf+PMP_TAKE_SECOND));
  }

  /* edited date */
  if (*(buf+PMP_EDIT_YEAR) == 0xff) {
    cur = cur + sprintf(&jpeg_comment[cur],
                        "Date-Edited: ----/--/-- --:--:--\n");
  }
  else {
    cur = cur + sprintf(&jpeg_comment[cur],
                        "Date-Edited: %d/%02d/%02d %02d:%02d:%02d\n",
    2000+(*(buf+PMP_EDIT_YEAR)), *(buf+PMP_EDIT_MONTH),
    *(buf+PMP_EDIT_DATE), *(buf+PMP_EDIT_HOUR), *(buf+PMP_EDIT_MINUTE),
    *(buf+PMP_EDIT_SECOND));
  }

  /* use flash? */
  if (*(buf+PMP_FLASH) != 0) {
    cur = cur + sprintf(&jpeg_comment[cur], "Flash: on\n");
  }

  /* insert total jpeg comment length */
  jpeg_comment[4] = (u_char)((cur - 4) >> 8);
  jpeg_comment[5] = (u_char)(cur - 4);

  return cur;
}

int get_picture_information(int *pmx_num, int outit)
{
  u_char buforg[PMF_MAXSIZ];
  char name[64];
  long len;
  int i,n;
  int j, k;
  FILE  *outfp;
  int fh;
  int offset=0;
  char *buf = &buforg;

  sprintf(name, "/PIC_CAM/PIC00000/PIC_INF.PMF");
  F1ok();
  len = F1getdata(name, buf, 0);

  n = buf[26] * 256 + buf[27]; /* how many files */
  *pmx_num = buf[31];  /* ??? */

  if(n ==10)
     buf++;

  k = 0;
  for(i = 0 ; i < (int) *pmx_num ; i++){
    for(j = 0 ; j < buforg[0x20 + 4 * i + 3]; j++){
      picture_thumbnail_index[k] = (j << 8) | buforg[0x20 + 4 * i] ;
      k++;
    }
  }
  for(i = 0 ; i < n ; i++){
    picture_index[i] = buf[0x420 + 0x10 * i + 3];
    picture_rotate[i] = buf[0x420 + 0x10 * i + 5];
    picture_protect[i] = buf[0x420 + 0x10 * i + 14];
  }

  if(outit == 2){
      fprintf(stdout," No:Internal name:Thumbnail name(Nth):Rotate:Protect\n");
    for(i = 0 ; i < n ; i++){
      fprintf(stdout,"%03d:", i + 1);
      fprintf(stdout," PSN%05d.PMP:", picture_index[i]);
      fprintf(stdout,"PIDX%03d.PMX(%02d)    :",
              0xff & picture_thumbnail_index[i],
              0xff & (picture_thumbnail_index[i] >> 8));
      switch(picture_rotate[i]){
      case 0x00:
        fprintf(stdout,"     0:");
        break;
      case 0x04:
        fprintf(stdout,"   270:");
        break;
      case 0x08:
        fprintf(stdout,"   180:");
        break;
      case 0x0c:
        fprintf(stdout,"    90:");
        break;
      default:
        fprintf(stdout,"   ???:");
        break;
      }
      if(picture_protect[i])
        fprintf(stdout,"on");
      else
        fprintf(stdout,"off");
      fprintf(stdout,"\n");
    }
  }
  return(n);
}

long get_file(char *name, char **data, int format, int verbose)
{
  u_long filelen;
  u_long total = 0;
  long len,memcpylen;
  char *ptr;
  u_char buf[0x400];
  u_char jpeg_comment[256];

  //verbose=1;
  F1ok();

  F1status(0);

  filelen = F1finfo(name);
  if(filelen == 0)
    return(0);

  if(F1fopen(name) != 0)
    return(0);
  //printf("camfile: %s\n",name);
  if(format == JPEG){
    len = F1fread(buf, 126);
    if( len < 126){
      F1fclose();
      return(0);
    }
    /*write_file(jpeg_comment, make_jpeg_comment(buf, jpeg_comment), fp);*/
    memcpylen=make_jpeg_comment(buf, jpeg_comment);
    ptr = malloc(memcpylen+filelen);
    *data=ptr;
    ptr = memcpy(ptr,jpeg_comment,memcpylen);
    total = 126;
    ptr +=memcpylen;
  }

  while((len = F1fread(buf, 0x0400)) != 0){
    if(len < 0)
      return(0);
    total = total + len;
    if(verbose){
      fprintf(stderr, "%6u/", total);
      fprintf(stderr, "%6u", filelen);
      fprintf(stderr, "\b\b\b\b\b\b\b\b\b\b\b\b\b");
    }
    /*result = fwrite(buf, sizeof(u_char), (size_t) len, fp);*/
    memcpylen=len;
    ptr = memcpy(ptr,buf,memcpylen);
    ptr +=memcpylen;

  }

  F1fclose();
  if(verbose)
    fprintf(stderr, "\n");
  return(total);
}

long get_thumbnail(char *name, char **data, int format, int verbose, int n)
{
  u_long filelen;
  u_long total = 0;
  long len;
  int i;
  u_char buf[0x1000];
  u_char *p;
  char *ptr;
  printf("name %s,%d\n",name,n);
  p = buf;

  F1ok();
  F1status(0);

  filelen = F1finfo(name);
  if(filelen == 0)
    return(0);

  if(F1fopen(name) != 0)
    return(0);

  for( i = 0 ; i < n ; i++)
    len = F1fseek(0x1000 , 1);

  while((len = F1fread(p, 0x0400)) != 0){
    if(len < 0){
      F1fclose();
      return(0);
    }
    total = total + len;
    if(verbose){
      fprintf(stderr, "%4u/", total);
      fprintf(stderr, "%4u", 0x1000);
          fprintf(stderr, "\b\b\b\b\b\b\b\b\b");
    }
    p = p + len;
    if(total >= 0x1000)
      break;
  }
  F1fclose();
  if(verbose)
    fprintf(stderr, "\n");

  filelen = buf[12] * 0x1000000 + buf[13] * 0x10000 +
    buf[14] * 0x100 + buf[15];

  ptr = malloc(filelen);
  *data=ptr;
  ptr = memcpy(ptr,&buf[256],filelen);

  //write_file(&buf[256], (int) filelen, fp);
  return(total);
}

void get_date_info(char *name, char *outfilename ,char *newfilename)
{
  char *p, *q;
  int year = 0;
  int month = 0;
  int date = 0;
  int hour = 0;
  int minute = 0;
  int second = 0;
  u_char buf[128];

  F1ok();
  F1status(0);

  (void) F1finfo(name);
  if(F1fopen(name) ==0){
    if(F1fread(buf, 126) == 126){
      if(*(buf+PMP_TAKE_YEAR) != 0xff){
        year = (int) *(buf+PMP_TAKE_YEAR);
        month = (int) *(buf+PMP_TAKE_MONTH);
        date = (int) *(buf+PMP_TAKE_DATE);
        hour = (int) *(buf+PMP_TAKE_HOUR);
        minute = (int) *(buf+PMP_TAKE_MINUTE);
        second = (int) *(buf+PMP_TAKE_SECOND);
      }
    }
    F1fclose();
  }

  p = outfilename;
  q = newfilename;
  while(*p){
    if(*p == '%'){
      p++;
      switch(*p){
      case '%':
        *q = '%';
        break;
      case 'H':
        q = q + sprintf(q, "%02d", hour);
        break;
      case 'M':
        q = q + sprintf(q, "%02d", minute);
        break;
       case 'S':
        q = q + sprintf(q, "%02d", second);
        break;
      case 'T':
#ifdef BINARYFILEMODE
        q = q + sprintf(q, "%02d%02d%02d", hour, minute, date);
#else
        q = q + sprintf(q, "%02d:%02d:%02d", hour, minute, date);
#endif
        break;
      case 'y':
        q = q + sprintf(q, "%02d", year);
        break;
      case 'm':
        q = q + sprintf(q, "%02d", month);
        break;
      case 'd':
        q = q + sprintf(q, "%02d", date);
        break;
      case 'D':
#ifdef BINARYFILEMODE
        q = q + sprintf(q, "%02d%02d%02d", year, month, date);
#else
        q = q + sprintf(q, "%02d_%02d_%02d", year, month, date);
#endif
        break;
      default:
        q = q + sprintf(q, "%%%c", *p);
        break;
      }
      p++;
    }else
      *q++ = *p++;
  }
  *q = (char) NULL;

}

long get_picture(int n, char **data, int format, int ignore, int all_pic_num)
{
  long  len;
  char name[64];
  char name2[64];
  int i;

  all_pic_num = get_picture_information(&i,0);

retry:

  if (all_pic_num < n) {
    fprintf(stderr, "picture number %d is too large. %d\n",all_pic_num,n);
    errflg ++;
    return(GP_ERROR);
   }

  switch(format){
  case PMX:
    sprintf(name, "/PIC_CAM/PIC00000/PIDX%03d.PMX", n - 1);
    break;
  case JPEG_T:
    sprintf(name, "/PIC_CAM/PIC00000/PIDX%03d.PMX",
            (picture_thumbnail_index[n] & 0xff));
    break;
  case JPEG:
  case PMP:
  default:
    if(ignore)
      sprintf(name, "/PIC_CAM/PIC00000/PSN%05d.PMP", n);
    else
      sprintf(name, "/PIC_CAM/PIC00000/PSN%05d.PMP", picture_index[n]);
    break;
  }
  if(ignore)
    sprintf(name2, "/PIC_CAM/PIC00000/PSN%05d.PMP", n );
  else
    sprintf(name2, "/PIC_CAM/PIC00000/PSN%05d.PMP", picture_index[n]);

  printf("name %s, name2 %s, %d\n",name,name2,n);

  if(verbose)
    switch(format){
    case PMX:
      fprintf(stdout, "pidx%03d.pmx: ", n -1 );
      break;
    case JPEG_T:
      fprintf(stderr, "Thumbnail %03d: ", n);
      break;
    case PMP:
    case JPEG:
    default:
      fprintf(stdout, "Picture %03d: ", n);
      break;
    }

  if(format == JPEG_T)
    len = get_thumbnail(name, data, format, verbose,
                        0xff & (picture_thumbnail_index[n] >> 8));
  else
    len = get_file(name, data, format, verbose);
  if(len == 0 ) {
    if(verbose)
                fprintf(stderr, "\n");
    goto retry;
  }

  if (len < 0)
    errflg ++;

   return(len);
}

void delete_picture(int n, int all_pic_num)
{
  if (all_pic_num < n) {
    fprintf(stderr, "picture number %d is too large. %d\n",all_pic_num,n);
    errflg ++;
    return;
  }

  if(picture_protect[n -1] != 0x00){
    fprintf(stderr, "picture %d is protected.\n", n);
    errflg ++;
    return;
  }

  if (F1deletepicture(picture_index[n]) < 0)
    errflg ++;
}

