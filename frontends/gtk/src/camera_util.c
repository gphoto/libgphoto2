/* 
   file : camera_util.c 
   author : jae gangemi (jgangemi@yahoo.com)

   contains all functions to create and manipulate camera settings through
   the gtk interface. 

*/

#include <stdio.h>
#include <stdlib.h>
#include <gphoto2.h>
#include <gtk/gtk.h>

#include "globals.h"
#include "camera_util.h"

int gp_gtk_camera_init;
Camera *gp_gtk_camera;

int camera_set() {

  gp_gtk_camera_init = 0;

} /* end camera_set */
