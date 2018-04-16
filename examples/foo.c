/*

  My Preview  Trying to capture preview images with libgphoto2

*/

// block of includes from preview.c
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <gphoto2/gphoto2.h>
#include "samples.h"
#include <sys/time.h> // there's a gettimeofday somewhere


Camera  *canon;
GPContext *canoncontext;

void get_one(void) {  // capture 1 preview image
  CameraFile *file;
  char jpgname[] = "test2.jpg";
  int rslt;
  rslt  = gp_file_new(&file);
  if (rslt) {
    printf("Error creating new CameraFile %i\n",rslt);
    exit(1);
  }
  // skipping focus for now but do it here
  // skipping zoom also
  rslt = gp_camera_capture_preview(canon, file, canoncontext);
  if (rslt) {
    printf("error capturing preview %i\n",rslt);
    exit(1);
  }
  rslt = gp_file_save(file, jpgname);
  if (rslt) {
    printf("Error saving file (in gp_file_save) %i\n",rslt);
    exit(1);
  }
}

int main(void) {
  int rslt;
  // skip this for now
//  gp_log_add_func(GP_LOG_ERROR, errordumper, NULL);
  gp_camera_new(&canon);
  canoncontext =  sample_create_context();
  printf("Camera init.  Takes about 10 seconds.\n");
  rslt = gp_camera_init(canon, canoncontext);
  if (rslt) {
    printf("Error initializing camera %i\n",rslt);
    exit(1);
  }
  //canon_enable_capture(canon, TRUE, canoncontext);  // in config.c
  get_one();
  gp_camera_exit(canon, canoncontext);
  return 0;
}
