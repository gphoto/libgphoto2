/****************************************************************/
/* library.c - Gphoto2 library for the Creative PC-CAM600      */
/*                                                              */
/*                                                              */
/* Author: Peter Kajberg <pbk@odense.kollegienet.dk>            */
/*                                                              */
/* This library is free software; you can redistribute it       */
/* and/or modify it under the terms of the GNU Library General  */
/* Public License as published by the Free Software Foundation; */
/* either version 2 of the License, or (at your option) any     */
/* later version.                                               */
/*                                                              */
/* This library is distributed in the hope that it will be      */
/* useful, but WITHOUT ANY WARRANTY; without even the implied   */
/* warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR      */
/* PURPOSE.  See the GNU Library General Public License for     */
/* more details.                                                */
/*                                                              */
/* Please notice that camera commands was sniffed by use of a   */
/* usbsniffer under windows. This is an experimental driver and */
/* a work in progess(I hope :))                                 */
/*                                                              */
/* You should have received a copy of the GNU Library General   */
/* Public License along with this library; if not, write to the */
/* Free Software Foundation, Inc., 59 Temple Place - Suite 330, */
/* Boston, MA 02111-1307, USA.                                  */
/****************************************************************/

#include <config.h>

#include "pccam600.h"

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>

#include <gphoto2.h>
#include <gphoto2-port.h>

#define GP_MODULE "pccam600"

#ifdef ENABLE_NLS
#  include <libintl.h>
#  undef _
#  define _(String) dgettext (GETTEXT_PACKAGE, String)
#  ifdef gettext_noop
#    define N_(String) gettext_noop (String)
#  else
#    define N_(String) (String)
#  endif
#else
#  define _(String) (String)
#  define N_(String) (String)
#endif



#define QUALITY_LO   0x56
#define QUALITY_ME   0x58
#define QUALITY_HI   0x45

struct models{
  char *name;
  unsigned short idVendor;
  unsigned short idProduct;
  char serial;
} models[] = {
  {"Creative PC-CAM600",0x041e,0x400b,0},
  {NULL,0,0,0}
};

int camera_id(CameraText *id)
{
  strcpy(id->text, "Creative PC-CAM600");
  return (GP_OK);
}

int camera_abilities(CameraAbilitiesList *list)
{        
  int i;
  CameraAbilities a;
  for(i=0; models[i].name; i++) {
    memset(&a , 0, sizeof(CameraAbilities));
    strcpy(a.model, models[i].name);
    a.status      = GP_DRIVER_STATUS_EXPERIMENTAL;
    a.port        = GP_PORT_USB;
    a.speed[0]    = 0;
    a.usb_vendor  = models[i].idVendor;
    a.usb_product = models[i].idProduct;
    a.operations        = 	GP_OPERATION_NONE;
    a.folder_operations = 	GP_FOLDER_OPERATION_NONE;
    a.file_operations   =     GP_FILE_OPERATION_DELETE;	
    gp_abilities_list_append(list, a);
  }
  return GP_OK;
}
		     
static int camera_exit(Camera *camera, GPContext *context){  
  int ret;
  ret = pccam600_close(camera->port, context);
  return ret;
}

static int file_list_func (CameraFilesystem *fs, const char *folder,
			   CameraList *list, void *data, GPContext *context){
  
  Camera *camera = data;
  CameraFileInfo info;
  int ret,n,i,nr_of_blocks;
  int offset = 64;
  char *temp;
  char buffer[512];
  FileEntry *file_entry;
  file_entry = (malloc(sizeof(FileEntry)));
  if ( (nr_of_blocks = pccam600_get_file_list(camera->port, context)) < 0 ){
    gp_log(GP_LOG_DEBUG,"pccam600","pccam600->get_file_list return <0");
    return GP_ERROR;
  }
  for (n = 0; n != nr_of_blocks; n++)
    {
      ret = pccam600_read_data(camera->port, buffer);
      for (i = offset; i <= 512-32; i=i+32)
	{
	  memcpy(file_entry,&(buffer)[i],32);
	  /*Fileentry valid? */
	  if( !((file_entry->state & 0x02) != 2)  && 
	      !((file_entry->state & 0x08) == 8) )
	    {
	      info.file.fields = 0;
	      temp = &(file_entry->name)[5];
	      if (strncmp(temp,"JPG",3) == 0)
		{
		  memcpy(&(file_entry->name)[5],".jpg",4);
		  strcpy(info.file.type,GP_MIME_JPEG);
		  info.file.fields = GP_FILE_INFO_WIDTH | GP_FILE_INFO_HEIGHT;
		  if (file_entry->quality == QUALITY_LO)
		    {
		      info.file.width = 640;
		      info.file.height = 480;
		    }
		  else if(file_entry->quality == QUALITY_ME)
		    {
		      info.file.width = 1024;
		      info.file.height = 768;
		    }
		}
	      else if (strncmp(temp,"AVI",3) == 0)
		{
		  memcpy(&(file_entry->name)[5],".avi",4);
		  info.file.fields = GP_FILE_INFO_WIDTH | GP_FILE_INFO_HEIGHT;
		  info.file.height = 352;
		  info.file.width = 288;
		  strcpy(info.file.type, GP_MIME_AVI);
		}
	      else if (strncmp(temp,"WAV",3) == 0)
		{
		  memcpy(&(file_entry->name)[5],".wav",4);
		  strcpy(info.file.type, GP_MIME_WAV);
		  info.file.height = 0;
		}
	      else if (strncmp(temp,"RAW",3) == 0)
		{
		  memcpy(&(file_entry->name)[5],".raw",4);
		  info.file.width = 1280;
		  info.file.height = 960;
		  info.file.fields = GP_FILE_INFO_WIDTH | GP_FILE_INFO_HEIGHT;
		  strcpy(info.file.type, GP_MIME_RAW);
		}
	      strcpy(info.file.name,file_entry->name);
	      gp_filesystem_append(fs,folder,info.file.name,context);
	      info.preview.fields = 0;
	      info.file.size = (file_entry->size[1]*256+
				file_entry->size[0]) * 256;
	      info.file.permissions = GP_FILE_PERM_READ | GP_FILE_PERM_DELETE;
	      info.file.fields |= GP_FILE_INFO_SIZE | GP_FILE_INFO_PERMISSIONS 
		|GP_FILE_INFO_TYPE | GP_FILE_INFO_NAME ;
	      ret = gp_filesystem_set_info_noop(fs, folder, info, context);
	    }
	}
	offset = 0;
    }
  return GP_OK;
}

static int camera_get_file (Camera *camera, GPContext *context, int index,  
			    unsigned char **data, int *size) 
{
  unsigned char buffer[512];
  int nr_of_blocks = 0;
  int n,id,canceled=0;
  int picturebuffersize = 0;
  int offset = 0;  
  nr_of_blocks = pccam600_get_file(camera->port,context,index);
  if (nr_of_blocks < 0) 
    return GP_ERROR_FILE_NOT_FOUND;
  picturebuffersize = nr_of_blocks * 512;
  id = gp_context_progress_start(context,nr_of_blocks,_("Downloading file..."));
  *data= malloc(picturebuffersize+1);
  memset(*data,0,picturebuffersize+1);
  for (n = 0; n != nr_of_blocks; n++){
    pccam600_read_data(camera->port, buffer);
    memmove(&(*data)[offset],buffer,512);
    offset = offset + 512;
    gp_context_progress_update(context,id,n);
    if (gp_context_cancel(context) == GP_CONTEXT_FEEDBACK_CANCEL)
      {
	/* Keep reading data or else data will be invalid next time*/
	canceled = 1;
      }
  }
  *size = offset;
  gp_context_progress_stop(context,id);
  if (canceled) return GP_ERROR_CANCEL;
  return GP_OK;
}

static int get_file_func (CameraFilesystem *fs, const char *folder,
			  const char *filename, CameraFileType type,
			  CameraFile *file, void *user_data, 
			  GPContext *context)
{
  Camera *camera =  user_data;
  unsigned char *data = NULL;
  int size,ret,index;
  size = 0;
  index = gp_filesystem_number(fs, folder, filename, context);
  if (index < 0)
    return index;
  switch(type)
    {
    case GP_FILE_TYPE_NORMAL:
      ret=camera_get_file(camera, context, index, &data, &size);
      break;
    default:
      return GP_ERROR_NOT_SUPPORTED;
    }
  gp_file_set_data_and_size(file, data, size);
  gp_file_set_name (file, filename);
  return GP_OK;
}

static int camera_summary(Camera *camera, CameraText *summary, GPContext *context)
{
  int totalmem;
  int  freemem;
  int ret;
  char summary_text[256];  
  ret = pccam600_get_mem_info(camera->port,context,&totalmem,&freemem);
  snprintf(summary_text,sizeof(summary_text),
  	   (" Total memory is %8d bytes.\n Free memory is  %8d bytes."),
	   totalmem,freemem );
  strcat(summary->text,summary_text);
  return GP_OK;
}

static int camera_manual(Camera *camera, CameraText *manual, GPContext *context){
  strcpy(manual->text, _("Manual Not Implemented Yet"));
  return GP_OK;
}

static int camera_about(Camera *camera, CameraText *about, GPContext *context)
{
  strcpy(about->text, 
	 _("Creative PC-CAM600\n Author: Peter Kajberg\n email: <pbk@odense.kollegienet.dk>\n"));
  return GP_OK;
}
static int
get_info_func (CameraFilesystem *fs, const char *folder, const char *filename,
               CameraFileInfo *info, void *data, GPContext *context)
{

  return GP_OK;
}

static int delete_file_func(CameraFilesystem *fs, const char *folder,
			    const char *filename, void *data, GPContext *context){
  int index,ret;

  Camera *camera = data;
  index = gp_filesystem_number(fs, folder, filename, context);
  gp_log(GP_LOG_DEBUG,"pccam","deleting '%s' in '%s'.. index:%d",filename, folder,index);
  ret = pccam600_delete_file(camera->port, context, index);
  return GP_OK;
}

int camera_init(Camera *camera, GPContext *context){
  GPPortSettings settings;
  int ret = 0;
  camera->functions->exit         = camera_exit;
  camera->functions->summary      = camera_summary;
  camera->functions->manual       = camera_manual;
  camera->functions->about        = camera_about;
  gp_log (GP_LOG_DEBUG, "pccam", "Initializing the camera\n");
  switch (camera->port->type) 
    {
    case GP_PORT_USB:
      ret = gp_port_get_settings(camera->port,&settings);
      if (ret < 0) return ret;
      settings.usb.inep = 0x82;
      settings.usb.outep = 0x03;
      settings.usb.config = 1;
      settings.usb.interface = 1;
      settings.usb.altsetting = 0;
      ret=gp_port_set_settings(camera->port,settings);
      if (ret<0) return ret;
      break;
    case GP_PORT_SERIAL:
      return GP_ERROR_IO_SUPPORTED_SERIAL;
    default: 
      return GP_ERROR_NOT_SUPPORTED;
    }    
  ret = pccam600_init(camera->port, context);
  if (ret < 0) return ret;
  gp_filesystem_set_info_funcs (camera->fs, get_info_func,NULL, camera);
  gp_filesystem_set_list_funcs (camera->fs, file_list_func, NULL, camera);
  gp_filesystem_set_file_funcs (camera->fs, get_file_func, delete_file_func,
				camera);
  return GP_OK;
}
