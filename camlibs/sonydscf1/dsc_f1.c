#include "config.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>
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

#ifdef ENABLE_NLS
#  include <libintl.h>
#  undef _
#  define _(String) dgettext (PACKAGE, String)
#  ifdef gettext_noop
#    define N_(String) gettext_noop (String)
#  else
#    define _(String) (String)
#    define N_(String) (String)
#  endif
#else
#  define _(String) (String)
#  define N_(String) (String)
#endif

#include "common.h"
#include "command.h"
#include <termios.h>
#include "pmp.h"
#include "chotplay.h"

#define MAX_PICTURE_NUM 200

u_char picture_index[MAX_PICTURE_NUM];
u_short picture_thumbnail_index[MAX_PICTURE_NUM];
u_char picture_protect[MAX_PICTURE_NUM];

int init;
char status_info[1000];
static int all_pic_num = -1;

int dsc_f1_initialize () {
  return(1);
}

int dsc_f1_open_cam () {
  char  *devpath = NULL;
  int   start_picture = 1;
  int   c;
  int i;

  devpath = serial_port;
  if(devpath == NULL){
    devpath = malloc(sizeof(char) * (strlen(serial_port) +1));
    if(devpath == NULL) {
      fprintf(stderr, "can't malloc\n");
      exit(1);
    }
    strcpy(devpath, RSPORT);
  }

  if (devpath) {
//    daemonuid();
    F1setfd(opentty(devpath));
  }
  if (F1getfd() < 0)
      return (0);
  return(1);
}

void dsc_f1_close_cam () {
  if (!(F1getfd() < 0)){
    F1reset();
    closetty(F1getfd());
  }
}

struct Image *dsc_f1_get_picture (int picNum, int thumbnail) {
        static FILE *fp = NULL;
        char fileName[1024], rm[1024];
        char *picData;
        int i;
        int format;
        long size;
        struct Image *im;

        if (picNum !=0) {
            if ((dsc_f1_open_cam() != 1))
                return(0);
        }

        F1ok();
        if (thumbnail) {
           format = JPEG_T;
           sprintf(fileName, "%s/gphoto-thumb-%i.jpg", gphotoDir, picNum);
           all_pic_num = get_picture_information(&i, 0);
           if(format == PMX)
               all_pic_num=i;
           get_picture(picNum, fileName, format, 0, all_pic_num);
        }
        else {
           format = JPEG;
           sprintf(fileName, "%s/gphoto-norm-%i.jpg", gphotoDir, picNum);
           all_pic_num = get_picture_information(&i, 0);
           if(format == PMX)
               all_pic_num=i;
           get_picture(picNum, fileName, format, 0, all_pic_num);
        }
        fp = fopen(fileName, "r");
        fseek(fp, 0, SEEK_END);
        size = ftell(fp);
        rewind(fp);
        im = (struct Image*)malloc(sizeof(struct Image));
        im->image = (char*)malloc(sizeof(char)*size);
        im->image_size = size;
        im->image_info_size = 0;
        fread(im->image, (size_t)sizeof(char), (size_t)size, fp);
        sprintf(rm, "rm %s", fileName);
        system(rm);
        dsc_f1_close_cam();
        return(im);
}

struct Image *dsc_f1_get_preview () {
        update_status(_("Sorry this function not implemented.."));
        return(0);
}

/* Sorry, don't have any config data for DSC-F1 */
int dsc_f1_configure() {
    update_status(_("Sorry, nothing to configure.."));
    return(1);
}

/* Not implemented at this time */
int dsc_f1_take_picture() {
    update_status(_("Sorry, taking pictures is not implemented.."));
    return(0);
}

int dsc_f1_number_of_pictures() {
    if ((dsc_f1_open_cam() != 1)) {
        fprintf(stdout, _("Couldn't open camera.\n"));
        return(0);
    }
    F1ok();
    all_pic_num = F1howmany();
    dsc_f1_close_cam();
    return(all_pic_num);
}

int dsc_f1_delete_image (int picNum) {
    int status;
    int i;
    i = picNum-1;
    if ((dsc_f1_open_cam() != 1))  {
        fprintf(stdout, _("Error opening camera\n"));
        return(1);
    }
    F1ok();
    if (all_pic_num < picNum) {
      fprintf(stderr, _("Picture number is too large.\n"));
      return(1);
    }
    status = F1deletepicture(i);
    dsc_f1_close_cam();
    if (status == 1) return(0);
    return(1);
}

char *dsc_f1_description () {
    return(
        _("Sony DSC-F1 Digital Camera Support\nM. Adam Kendall <joker@penguinpub.com>\n\nBased on the chotplay CLI interface from\nKen-ichi Hayashi\n\nThis lib may not work. YMMV\n"));
}

char *dsc_f1_summary () {
    if((dsc_f1_open_cam() != 1)) {
      fprintf(stdout, _("Error opening camera.\n"));
      return(_("Error opening camera.\n"));
    }
    F1ok();
    F1newstatus(1, status_info);
    dsc_f1_close_cam();
    return(status_info);
}

/* Declare the camera functions here */
/* Note: take_picture and configure are not implemented */
struct _Camera sony_dscf1 = { dsc_f1_initialize,
                        dsc_f1_get_picture,
                        dsc_f1_get_preview,
                        dsc_f1_delete_image,
                        dsc_f1_take_picture,
                        dsc_f1_number_of_pictures,
                        dsc_f1_configure,
                        dsc_f1_summary,
                        dsc_f1_description};
/* End struct */
