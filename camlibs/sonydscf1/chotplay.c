#include "config.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>
extern int errno;

#define BINARYFILEMODE
#define DONTCAREUID

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

#define OUTFILENAME_JPG "%s_%03d.jpg"
#define OUTFILENAME_JPG0 "F1_%03d.jpg"

#define OUTFILENAME_PMP "%s_%03d.pmp"
#define OUTFILENAME_PMP0 "psn%05d.pmp"

#define OUTFILENAME_PMX "pidx%03d.pmx"

#define OUTFILENAME_PMF "pic_inf.pmf"

static int     format = JPEG;
static int      verbose = 0;
#ifdef DOS
static unsigned short  speed = 0;
#else
static int     speed = 0;
#endif

static  int     errflg = 0;
static  int     all_pic_num = -1;

#ifndef DONTCAREUID
static  uid_t   uid, euid;
static  gid_t   gid, egid;
static  int     uidswapped = 0;
#endif

void usage()
{
  static        char    *usagestr[] =  {
    "chotplay (Ver 0.06) (c)1996,1997 Gadget (Palmtop PC) mailing list\n",
    "                    Programmed by Ken-ichi HAYASHI\n",
    "\t -h             : show this usage.\n",
    "\t -n             : print how many pictures in DSC-F1.\n",
    "\t -o filename    : set output filename.\n",
    "\t -g num         : get a picture data in DSC-F1.\n",
    "\t -a             : get all picture data in DSC-F1.\n",
    "\t -A             : get all picture data in DSC-F1. but,\n",
    "\t                  don't care index information.\n",
    "\t -F format      : picture format.[JPEG PMP pmx jpeg]\n",
    "\t                  (use with -a or -g).\n",
    "\t -f             : get 'pic_inf.pmf' file.\n",
    "\t -s num         : start picture number.(use with -a)\n",
    "\t -e num         : end picture number.(use with -a)\n",
    "\t -I             : show informations about all picture\n",
    "\t -z             : show informations about DSC-F1\n",
    "\t -v             : verbose mode(use with -a or -g)\n",
    "\t -d num         : delete picture in DSC-F1.\n",
    "\t -S speed       : serial speed. [normal middle high]\n",
#ifndef X68
    "\t -D ttydevice   : set tty(cua) device.\n",
#endif
    (char *)NULL,
  };
  char  **p;

  p = usagestr;
  while (*p)
    fprintf(stderr, *p++);
}

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

int
write_file(buf, len, outfp)
     u_char     *buf;
     int        len;
     FILE       *outfp;
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


int
make_jpeg_comment(buf, jpeg_comment)
     u_char *buf;
     u_char *jpeg_comment;
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
    1900+(*(buf+PMP_TAKE_YEAR)), *(buf+PMP_TAKE_MONTH),
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
    1900+(*(buf+PMP_EDIT_YEAR)), *(buf+PMP_EDIT_MONTH),
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

int
get_picture_information(pmx_num, outit)
     int *pmx_num;
     int outit;
{
  u_char buf[PMF_MAXSIZ];
  char name[64];
  long len;
  int i,n;
  int j, k;
  FILE  *outfp;

  sprintf(name, "/PIC_CAM/PIC00000/PIC_INF.PMF");
  F1ok();
  len = F1getdata(name, buf, 0);

  n = buf[24] * 256 + buf[25];  /* max file number + 1 */
  n = buf[26] * 256 + buf[27]; /* how many files */
  *pmx_num = buf[30];
  *pmx_num = buf[31];  /* ??? */

  k = 0;
  for(i = 0 ; i < (int) *pmx_num ; i++){
    for(j = 0 ; j < buf[0x20 + 4 * i + 3]; j++){
      picture_thumbnail_index[k] = (j << 8) | buf[0x20 + 4 * i] ;
      k++;
    }
  }
  for(i = 0 ; i < n ; i++){
    picture_index[i] = buf[0x420 + 0x10 * i + 3];
    picture_rotate[i] = buf[0x420 + 0x10 * i + 5];
    picture_protect[i] = buf[0x420 + 0x10 * i + 14];
  }

  if(outit == 1){
    outfp = fopen(OUTFILENAME_PMF, WMODE);
    if (outfp == NULL){
      fprintf(stderr, "can't open outfile(%s).\n", OUTFILENAME_PMF);
      errflg ++;
    }else{
      write_file(buf, len, outfp);
      fclose(outfp);
    }
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

long
get_file(name, data /*fp*/, format, verbose)
     char *name;
/*     FILE *fp;   for output  */
     char **data;
     int format;
     int verbose;
{
  u_long filelen;
  u_long total = 0;
  long len,memcpylen,t;
  long result;
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

    /*if(result != len){
      perror("chotplay");
      F1fclose();
      if(fp != stdout);
       fclose(fp);
      Exit(2);
    } */
  }

  F1fclose();
  if(verbose)
    fprintf(stderr, "\n");
  return(total);
}

long
get_thumbnail(name, data, format, verbose, n)
     char *name;
     char **data;
     //FILE *fp;   for output
     int format;
     int verbose;
     int n;
{
  u_long filelen;
  u_long total = 0;
  long len;
  int i;
  u_char buf[0x1000];
  u_char *p;
  char *ptr;

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

void
get_date_info(name, outfilename ,newfilename)
     char *name;
     char *outfilename;
     char *newfilename;
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

/*void
get_picture(n, outfilename, format, ignore, all_pic_num)
     int        n;
     char *outfilename;
     int  format;
     int ignore;
     int all_pic_num;*/
long
get_picture(n, data, format, ignore, all_pic_num)
     int        n;
     char **data;
     int  format;
     int ignore;
     int all_pic_num;
{
  long  len;
  char name[64];
  char name2[64];
  FILE  *outfp;
  char filename[MAXPATHLEN];
  int i;

  all_pic_num = get_picture_information(&i, 0);

retry:

  if (all_pic_num < n) {
    fprintf(stderr, "picture number %d is too large. %d\n",all_pic_num,n);
    errflg ++;
    return;
  }

  switch(format){
  case PMX:
    sprintf(name, "/PIC_CAM/PIC00000/PIDX%03d.PMX", n - 1);
    break;
  case JPEG_T:
    sprintf(name, "/PIC_CAM/PIC00000/PIDX%03d.PMX",
            (picture_thumbnail_index[n - 1] & 0xff));
    break;
  case JPEG:
  case PMP:
  default:
    if(ignore)
      sprintf(name, "/PIC_CAM/PIC00000/PSN%05d.PMP", n - 1);
    else
      sprintf(name, "/PIC_CAM/PIC00000/PSN%05d.PMP", picture_index[n - 1]);
    break;
  }
  if(ignore)
    sprintf(name2, "/PIC_CAM/PIC00000/PSN%05d.PMP", n - 1);
  else
    sprintf(name2, "/PIC_CAM/PIC00000/PSN%05d.PMP", picture_index[n - 1]);

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

/*  outfp = stdout;*/

  /*if (outfilename) {
    if(((format == JPEG) || (format == PMP) || (format == JPEG_T))
       &&  strchr(outfilename, '%')){
      get_date_info(name2, outfilename, filename);
      outfp = fopen(filename, WMODE);
      if (outfp == NULL){
        fprintf(stderr, "can't open outfile(%s).\n", filename);
        errflg ++;
        return;
      }
    }else{
      outfp = fopen(outfilename, WMODE);
      if (outfp == NULL){
        fprintf(stderr, "can't open outfile(%s).\n", outfilename);
        errflg ++;
        return;
      }
    }
  }
#ifdef BINARYFILEMODE
  if(outfp == stdout){
#ifdef WIN32
        _setmode(_fileno(stdout), _O_BINARY);
#else
        setmode(fileno(stdout), O_BINARY);
#endif
  }
#endif
*/

  if(format == JPEG_T)
    len = get_thumbnail(name, data, format, verbose,
                        0xff & (picture_thumbnail_index[n -1] >> 8));
  else
    len = get_file(name, data, format, verbose);
  if(len == 0 ) {
    if(verbose)
                fprintf(stderr, "\n");
    goto retry;
  }

  if (len < 0)
    errflg ++;

  /*if (outfp != stdout)
    fclose(outfp);*/
   return(len);
}

void
get_all_pictures(start, end, outfilename, format, ignore)
     int        start;
     int        end;
     char       *outfilename;
     int format;
     int ignore;
{
  int   i;
  char  fname[MAXPATHLEN];

  if (all_pic_num < start || all_pic_num < end) {
    fprintf(stderr, "picture number %d is too large. %d,%d\n",all_pic_num,start,end);
    errflg ++;
    return;
  }
  if (start > end) {
    int tmp;
    tmp = end;
    end = start;
    start = tmp;
  }

  for (i = start; i <= end; i++) {
    switch(format){
    case PMX:
      sprintf(fname, OUTFILENAME_PMX, i-1);
      break;
    case PMP:
      if (outfilename)
        sprintf(fname, OUTFILENAME_PMP, outfilename, picture_index[i-1]);
      else
        sprintf(fname, OUTFILENAME_PMP0, picture_index[i-1]);
      break;
    case JPEG:
    case JPEG_T:
    default:
      if (outfilename)
        sprintf(fname, OUTFILENAME_JPG, outfilename, i);
      else
        sprintf(fname, OUTFILENAME_JPG0, i);
      break;
    }
    fprintf(stderr, "Fetching picture number %d, storing it as: %s\n",i,fname);
    get_picture(i, fname, format, ignore);
  }
}

void
delete_picture(n, all_pic_num)
     int        n;
     int        all_pic_num;
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

  if (F1deletepicture(picture_index[n -1]) < 0)
    errflg ++;
}

#ifndef DONTCAREUID
void
daemonuid()
{
  if (uidswapped) {
#ifdef HAVE_SETREUID
    setreuid(uid, euid);
    setregid(gid, egid);
#else
//    setuid(uid);
//    seteuid(euid);
//    setgid(gid);
//    setegid(egid);
#endif
    uidswapped = 0;
  }
}

void
useruid()
{
  if (!uidswapped) {
#ifdef HAVE_SETREUID
    setregid(egid, gid);
    setreuid(euid, uid);
#else
    setgid(egid);
    setegid(gid);
    setuid(euid);
    seteuid(uid);
#endif
    uidswapped = 1;
  }
}
#endif

#if 0
int
main(argc, argv)
     int        argc;
     char       **argv;
{
  char  *devpath = NULL;
  char  *outfilename = NULL;
  int   start_picture = 1;
  int   end_picture = MAX_PICTURE_NUM;
  int   c;
  int i;

#ifndef DONTCAREUID
  uid = getuid();
  euid = geteuid();
  gid = getgid();
  egid = getegid();
  useruid();
#endif

  devpath = getenv("CHOTPLAYTTY");

  if(devpath == NULL){
    devpath = malloc(sizeof(char) * (strlen(RSPORT) +1));
    if(devpath == NULL) {
      fprintf(stderr, "can't malloc\n");
      exit(1);
    }
    strcpy(devpath, RSPORT);
  }

  for(i = 0 ; i < argc; i++){
    if(strcmp("-D", argv[i]) == 0){
      devpath = argv[i+1];
      break;
    }
    if(strcmp("-h", argv[i]) == 0){
      usage();
      exit(-1);
    }
  }

  if (devpath) {
#ifndef DONTCAREUID
    daemonuid();
#endif
    F1setfd(opentty(devpath));
#ifndef DONTCAREUID
    useruid();
#endif
  }
  if (F1getfd() < 0)
    Exit(1);

  while ((c = getopt(argc, argv, "D:ro:g:naAs:e:d:vF:fS:Izh")) != EOF) {
    switch(c) {
    case 'S':
      switch(optarg[0]){
      case 'l':
      case '5':
#if defined(WIN32) || defined(OS2) || defined(BSD) || defined(DOS)
        speed = B38400;
#else
        speed = B38400;
#endif
        break;
      case 't':
      case '4':
#if defined(WIN32) || defined(OS2) || defined(BSD) || defined(DOS)
        speed = B38400;
#else
        speed = B38400;
#endif
        break;
      case 'h':
      case '3':
        speed = B38400;
        break;
      case 'm':
      case '2':
        speed = B19200;
        break;
      case 'n':
      case '1':
        speed = B9600;
        break;
      default:
        speed = DEFAULTBAUD;
        break;
      }
      changespeed(F1getfd(), speed);
#ifdef WIN32    /* for IrDA.. */
        Sleep(1500);
#endif
      break;
    case 'o':
      outfilename = optarg;
      break;
    case 'g':
      all_pic_num = get_picture_information(&i, 0);
      if(format == PMX)
        all_pic_num = i;
      get_picture(atoi(optarg), outfilename, format, 0);
      break;
    case 'r':
      F1reset();
      exit(0);
      break;
    case 'n':
      F1ok();
      all_pic_num = F1howmany();
      printf("pictures = %d\n", all_pic_num);
      break;
    case 'z':
      F1ok();
      F1status(1);
      break;
    case 'I':
      F1ok();
      all_pic_num = get_picture_information(&i, 2);
      break;
    case 'a':
      all_pic_num = get_picture_information(&i, 0);
      if(format == PMX)
        all_pic_num = i;
      if(all_pic_num < end_picture)
        end_picture = all_pic_num;
      get_all_pictures(start_picture, end_picture, outfilename, format, 0);
      end_picture = MAX_PICTURE_NUM;
      break;
    case 'A':
      all_pic_num = get_picture_information(&i, 0);
      if(format == PMX)
        all_pic_num = i;
      if(all_pic_num < end_picture)
        end_picture = all_pic_num;
      get_all_pictures(start_picture, end_picture, outfilename, format, 1);
      end_picture = MAX_PICTURE_NUM;
      break;
    case 's':
      start_picture = atoi(optarg);
      break;
    case 'e':
      end_picture = atoi(optarg);
      break;
    case 'v':
      verbose = 1;
      break;
    case 'd':
      F1ok();
      all_pic_num = get_picture_information(&i, 0);
      delete_picture(atoi(optarg));
      all_pic_num = get_picture_information(&i, 0);
      break;
    case 'f':
      F1ok();
      all_pic_num = get_picture_information(&i, 1);
      break;
/*    case 'Z':
      F1ok();
      all_pic_num = get_picture_information(&i, 0);
      F1test(atoi(optarg));
      all_pic_num = get_picture_information(&i, 0);
      break;
*/
    case 'D':
      break; /* do nothing */
    case 'F':
      {
        switch(optarg[0]){
        case 'j':
          format = JPEG_T;
          break;
        case 'J':
          format = JPEG;
          break;
        case 'p':
          format = PMX;
          break;
        case 'P':
          format = PMP;
          break;
        default:
          format = JPEG;
          break;
        }
      }
      break;
    default:
      usage();
      Exit(-1);
    }
  }

  Exit (errflg ? 1 : 0);
  return(255);
}
#endif
