/***************************************************************************
 *
 * canon.c
 *
 *   Canon Camera library for the gphoto project,
 *   (c) 1999 Wolfgang G. Reissnegger
 *   Developed for the Canon PowerShot A50
 *   Additions for PowerShot A5 by Ole W. Saastad
 *   (c) 2000 : Other additions  by Edouard Lafargue, Philippe Marzouk.
 *
 * $Header$
 ****************************************************************************/


/****************************************************************************
 *
 * include files
 *
 ****************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>
#include <time.h>
#include <ctype.h>

#include <gphoto2.h>

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

#include "util.h"
#include "serial.h"
#include "psa50.h"
#include "canon.h"

/* Global variable to set debug level.                        */
/* Defined levels so far :                                    */
/*   0 : no debug output                                      */
/*   1 : debug actions                                        */
/*   9 : debug everything, including hex dumps of the packets */

int canon_debug_driver = 9;

static struct {
	char *name;
	unsigned short idVendor;
	unsigned short idProduct;
	char serial;
} models[] = {
	{ "Canon PowerShot A5", 	0, 0, 1 },
	{ "Canon PowerShot A5 Zoom", 	0, 0, 1},
	{ "Canon PowerShot A50", 	0, 0, 1},
	{ "Canon PowerShot Pro70", 	0, 0, 1},
	{ "Canon PowerShot S10", 	0x04A9, 0x3041, 1},
	{ "Canon PowerShot S20", 	0x04A9, 0x3043, 1 },
	{ "Canon EOS D30", 		0x04A9, 0x3044, 0 },
	{ "Canon PowerShot S100", 	0x04A9, 0x3045, 0 },
	{ "Canon IXY DIGITAL", 		0x04A9, 0x3046, 0 },
	{ "Canon Digital IXUS", 	0x04A9, 0x3047, 0 },
	{ "Canon PowerShot G1", 	0x04A9, 0x3048, 1 },
	{ "Canon PowerShot G2", 	0x04A9, 0x3055, 0 },
	{ "Canon PowerShot Pro90 IS", 	0x04A9, 0x3049, 1 },
	{ "Canon IXY DIGITAL 300", 	0x04A9, 0x304B, 0 },
	{ "Canon PowerShot S300", 	0x04A9, 0x304C, 0 },
	{ "Canon Digital IXUS 300", 	0x04A9, 0x304D, 0 },
	{ "Canon PowerShot A20", 	0x04A9, 0x304E, 0 },
	{ "Canon PowerShot A10", 	0x04A9, 0x304F, 0 },
	{ "Canon PowerShot S110",	0x04A9, 0x3051, 0 },
	{ "Canon DIGITAL IXUS v",       0x04A9, 0x3052, 0 },
	{ 0, 0, 0}
};

int camera_id(CameraText *id)
{
	gp_debug_printf(GP_DEBUG_LOW,"canon","camera_id()");
	
	strcpy(id->text, "canon");
	
	return GP_OK;
}

int camera_manual(Camera *camera, CameraText *manual)
{
	gp_debug_printf(GP_DEBUG_LOW,"canon","camera_manual()");
	
	strcpy(manual->text, _("For the A50, 115200 may not be faster than 57600\n"
		   "Folders are NOT supported\n"
		   "if you experience a lot of transmissions errors,"
		   " try to have you computer as idle as possible (ie: no disk activity)"));

	return GP_OK;
}

int camera_abilities(CameraAbilitiesList *list)
{
    int i;
    CameraAbilities *a;

    gp_debug_printf(GP_DEBUG_LOW,"canon","camera_abilities()");

    for (i=0; models[i].name; i++) {
        gp_abilities_new(&a);
        strcpy(a->model, models[i].name);
	a->port = 0;
	if (models[i].idProduct) {
		a->port |= GP_PORT_USB;
		a->usb_vendor = models[i].idVendor;
		a->usb_product = models[i].idProduct;
	}
	if (models[i].serial) {
		a->port |= GP_PORT_SERIAL;
		a->speed[0]   = 9600;
		a->speed[1]   = 19200;
		a->speed[2]   = 38400;
		a->speed[3]   = 57600;
		a->speed[4]   = 115200;
		a->speed[5]   = 0;
	}
        a->operations        = 	GP_OPERATION_CONFIG;
	a->folder_operations = 	GP_FOLDER_OPERATION_PUT_FILE;
        a->file_operations   = 	GP_FILE_OPERATION_DELETE | 
				GP_FILE_OPERATION_PREVIEW;
        gp_abilities_list_append(list, a);
    }

    return GP_OK;
}

int camera_folder_list_folders (Camera *camera, const char *folder, 
				CameraList *list)
{
	gp_debug_printf(GP_DEBUG_LOW,"canon","camera_folder_list()");
					
	return GP_OK;
}

void clear_readiness(Camera *camera)
{	
	canon_info *cs = (struct canon_info*)camera->camlib_data;

	gp_debug_printf(GP_DEBUG_LOW,"canon","clear_readiness()");
	
    cs->cached_ready = 0;
}

static int check_readiness(Camera *camera)
{
	struct canon_info *cs = (struct canon_info*)camera->camlib_data;
	
	gp_debug_printf(GP_DEBUG_LOW,"canon","check_readiness()");
	
    if (cs->cached_ready) return 1;
    if (psa50_ready(camera)) {
		gp_debug_printf(GP_DEBUG_LOW,"canon",_("Camera type:  %d\n"),cs->model);
		cs->cached_ready = 1;
		return 1;
    }
    gp_frontend_status(NULL, _("Camera unavailable"));
    return 0;
}

static void switch_camera_off(Camera *camera)
{
	gp_debug_printf(GP_DEBUG_LOW,"canon","switch_camera_off()");

	
	gp_frontend_status(NULL, _("Switching Camera Off"));

	psa50_off(camera);
	clear_readiness(camera);
}

int camera_exit(Camera *camera)
{
	struct canon_info *cs = (struct canon_info*)camera->camlib_data;
	
	gp_debug_printf(GP_DEBUG_LOW,"canon","camera_exit()");
	
        switch (camera->port->type) {
	 case GP_PORT_USB:
                 break;
	 case GP_PORT_SERIAL:
                 switch_camera_off(camera);
                 break;
	 default:
                 break;
	}
	
	free(cs);
	return GP_OK;
}

static int canon_get_batt_status(Camera *camera, int *pwr_status, int *pwr_source)
{
	gp_debug_printf(GP_DEBUG_LOW,"canon","canon_get_batt_status()");
	
	if (!check_readiness(camera))
	  return -1;
	
	return psa50_get_battery(camera, pwr_status, pwr_source);
}

#if 0
char *camera_model_string(Camera *camera)
{
	struct canon_info *cs = (struct canon_info*)camera->camlib_data;
	
	gp_debug_printf(GP_DEBUG_LOW,"canon","camera_model_string()");
	
	if (!check_readiness(camera))
	  return _("Camera unavailable");

	switch (cs->model) {
	 case CANON_PS_A5:
		return "Powershot A5";
	 case CANON_PS_A5_ZOOM:
		return "Powershot A5 Zoom";
	 case CANON_PS_A50:
		return "Powershot A50";
	 case CANON_PS_A70:
		return "Powershot Pro70";
	 case CANON_PS_S10:
		return "Powershot S10";
	 case CANON_PS_S20:
		return "Powershot S20";
	 case CANON_PS_G1:
		return "Powershot G1";
	 case CANON_PS_G2:
		return "Powershot G2";
	 case CANON_PS_S100:
		return "Powershot S100 / Digital IXUS / IXY DIGITAL";
         case CANON_PS_S300:
                return "Powershot S300 / Digital IXUS 300 / IXY DIGITAL 300";
         case CANON_PS_A10:
                return "Powershot A10";
         case CANON_PS_A20:
                return "Powershot A20";
         case CANON_EOS_D30:
                return "EOS D30";
         case CANON_PS_PRO90_IS:
                return "Powershot Pro90 IS";
	 default:
		return _("Unknown model!");
	}
}
#endif


static int update_disk_cache(Camera *camera)
{
	struct canon_info *cs = (struct canon_info*)camera->camlib_data;
	char root[10]; /* D:\ or such */
	char *disk;

	gp_debug_printf(GP_DEBUG_LOW,"canon","update_disk_cache()");
	
	if (cs->cached_disk) return 1;
	if (!check_readiness(camera)) return 0;
	disk = psa50_get_disk(camera);
	if (!disk) {
		gp_frontend_status(NULL, _("No response"));
		return 0;
	}
	strcpy(cs->cached_drive,disk);
	sprintf(root,"%s\\",disk);
	if (!psa50_disk_info(camera, root,&cs->cached_capacity,&cs->cached_available)) {
		gp_frontend_status(NULL, _("No response"));
		return 0;
	}
	cs->cached_disk = 1;
	
	return 1;
}

/*
 * Test whether the name given corresponds
 * to a thumbnail (.THM)
 */
static int is_thumbnail(const char *name)
{
	const char *pos;

	gp_debug_printf(GP_DEBUG_LOW,"canon","is_thumbnail(%s)",name);

	pos = strchr(name, '.');
	if(!pos) return 0;
	return !strcmp(pos,".THM");
}

static int is_image(const char *name)
{
	const char *pos;

	gp_debug_printf(GP_DEBUG_LOW,"canon","is_image(%s)",name);

	pos = strchr(name,'.');
	if (!pos) return 0;
	return (!strcmp(pos,".JPG"));
}

/*
 * Test whether the name given corresponds
 * to a movie (.AVI)
 */
static int is_movie(const char *name)
{
	const char *pos;

	gp_debug_printf(GP_DEBUG_LOW,"canon","is_movie(%s)",name);

	pos = strchr(name, '.');
	if(!pos) return 0;
	return !strcmp(pos,".AVI");
}




/* This function is only used by A5 */

static int recurse(Camera *camera, const char *name)
{
	struct canon_info *cs = (struct canon_info*)camera->camlib_data;
    struct psa50_dir *dir,*walk;
    char buffer[300]; /* longest path, etc. */
    int count,curr;

	gp_debug_printf(GP_DEBUG_LOW,"canon","recurse()");
	
    dir = psa50_list_directory(camera, name);
    if (!dir) return 1; /* assume it's empty @@@ */
    count = 0;
    for (walk = dir; walk->name; walk++)
        if (walk->size && (is_image(walk->name) || is_movie(walk->name) )) count++;
    cs->cached_paths = realloc(cs->cached_paths,sizeof(char *)*(cs->cached_images+count+1));
    memset(cs->cached_paths+cs->cached_images+1,0,sizeof(char *)*count);
    if (!cs->cached_paths) {
        perror("realloc");
        return 0;
    }
    curr = cs->cached_images;
    cs->cached_images += count;
    for (walk = dir; walk->name; walk++) {
        sprintf(buffer,"%s\\%s",name,walk->name);
        if (!walk->size) {
            if (!recurse(camera, buffer)) return 0;
        }
        else {
            if ((!is_image(walk->name)) && (!is_movie(walk->name))) continue;
            curr++;
            cs->cached_paths[curr] = strdup(buffer);
            if (!cs->cached_paths[curr]) {
                perror("strdup");
                return 0;
            }
        }
    }
    free(dir);
    return 1;
}


static int comp_dir(const void *a,const void *b)
{
	gp_debug_printf(GP_DEBUG_LOW,"canon","comp_dir()");
	
    return strcmp(((const struct psa50_dir *) a)->name,
      ((const struct psa50_dir *) b)->name);
}


/* This function is only used by A50 */

static struct psa50_dir *dir_tree(Camera *camera, const char *path)
{
	struct canon_info *cs = (struct canon_info*)camera->camlib_data;
    struct psa50_dir *dir,*walk;
    char buffer[300]; /* longest path, etc. */

	gp_debug_printf(GP_DEBUG_LOW,"canon","dir_tree()");
	
    dir = psa50_list_directory(camera, path);
    if (!dir) return NULL; /* assume it's empty @@@ */
    for (walk = dir; walk->name; walk++) {
        if (walk->is_file) {
            if (is_image(walk->name) || is_movie(walk->name) || is_thumbnail(walk->name)) cs->cached_images++;
        }
        else {
            sprintf(buffer,"%s\\%s",path,walk->name);
            walk->user = dir_tree(camera, buffer);
        }
    }
    qsort(dir,walk-dir,sizeof(*dir),comp_dir);
    return dir;
}


static void clear_dir_cache(Camera *camera)
{
	struct canon_info *cs = (struct canon_info*)camera->camlib_data;
	
	gp_debug_printf(GP_DEBUG_LOW,"canon","clear_dir_cache()");
	
    psa50_free_dir(camera, cs->cached_tree);
}


/* A5 only: sort THB_ and AUT_ into their proper arrangement. */
static int compare_a5_paths (const void * p1, const void * p2)
{
	const char * s1 = *((const char **)p1);
	const char * s2 = *((const char **)p2);
	const char * ptr, * base1, * base2;
	int n1 = 0, n2 = 0;
	
	gp_debug_printf(GP_DEBUG_LOW,"canon","compare_a5_paths()");
	
	printf(_("Comparing %s to %s\n"), s1, s2);
	
	ptr = strrchr(s1, '_');
	if (ptr)
	  n1 = strtol(ptr+1, 0, 10);
	ptr = strrchr(s2, '_');
	if (ptr)
	  n2 = strtol(ptr+1, 0, 10);
	
	printf(_("Numbers are %d and %d\n"), n1, n2);
	
	if (n1 < n2)
	  return -1;
	else if (n1 > n2)
	  return 1;
	else {
		base1 = strrchr(s1, '\\');
		base2 = strrchr(s2, '\\');
		printf(_("Base 1 is %s and base 2 is %s\n"), base1, base2);
		return strcmp(base1, base2);
	}
}


static int update_dir_cache(Camera *camera)
{
	struct canon_info *cs = (struct canon_info*)camera->camlib_data;
    int i;
	
	gp_debug_printf(GP_DEBUG_LOW,"canon","update_dir_cache()");
	
    if (cs->cached_dir) return 1;
    if (!update_disk_cache(camera)) return 0;
    if (!check_readiness(camera)) return 0;
    cs->cached_images = 0;
    switch (cs->model) {
    case CANON_PS_A5:
    case CANON_PS_A5_ZOOM:
      if (recurse(camera, cs->cached_drive)) {
        printf(_("Before sort:\n"));
        for (i = 1; i < cs->cached_images; i++) {
          printf("%d: %s\n", i, cs->cached_paths[i]);
        }
        qsort(cs->cached_paths+1, cs->cached_images, sizeof(char *), compare_a5_paths);
        printf(_("After sort:\n"));
        for (i = 1; i < cs->cached_images; i++) {
          printf("%d: %s\n", i, cs->cached_paths[i]);
        }
        cs->cached_dir = 1;
        return 1;
      }
      clear_dir_cache(camera);
      return 0;
      break;

    default:  /* A50 or S10 or other */
      cs->cached_tree = dir_tree(camera, cs->cached_drive);
      if (!cs->cached_tree) return 0;
      cs->cached_dir = 1;
      return 1;
      break;
    }
}

static int _canon_file_list (struct psa50_dir *tree, const char *folder, 
			     CameraList *list)
{

  if (!tree) {
//        debug_message(camera,"no directory listing available!\n");
    return GP_ERROR;
  }

  while (tree->name) {
    if(is_image(tree->name) || is_movie(tree->name)) {
      gp_list_append(list,(char*)tree->name, NULL);
    } else if (!tree->is_file) { 
      _canon_file_list(tree->user, folder, list);      
    }
    tree++;
  }
  
  return GP_OK;
}

static int canon_file_list (Camera *camera, const char *folder, 
			    CameraList *list)
{
  struct canon_info *cs = (struct canon_info*)camera->camlib_data;
  
  gp_debug_printf(GP_DEBUG_LOW,"canon","canon_file_list()");
  
  if(!update_dir_cache(camera)) {
    gp_frontend_status(NULL, _("Could not obtain directory listing"));
    return GP_ERROR;
  }
  
  _canon_file_list(cs->cached_tree, folder, list);
  return GP_OK;
}


int camera_folder_list_files (Camera *camera, const char *folder, 
			      CameraList *list)
{
  gp_debug_printf(GP_DEBUG_LOW,"canon","camera_file_list()");

  return canon_file_list(camera, folder, list);
}

/****************************************************************************
 *
 * gphoto library interface calls
 *
 ****************************************************************************/


#define JPEG_END        0xFFFFFFD9
#define JPEG_ESC        0xFFFFFFFF

static char *canon_get_picture (Camera *camera, char *filename, 
				char *path, int thumbnail, int *size)
{
	struct canon_info *cs = (struct canon_info*)camera->camlib_data;
    char *image;
    char attribs;
    char file[300];
    void *ptr;
	int j;

	gp_debug_printf(GP_DEBUG_LOW,"canon","canon_get_picture()");
	
    if (!check_readiness(camera)) {
		return NULL;
    }
    switch (cs->model) {
	 case CANON_PS_A5:
	 case CANON_PS_A5_ZOOM:
#if 0
		picture_number=picture_number*2-1;
		if (thumbnail) picture_number+=1;
		gp_debug_printf(GP_DEBUG_LOW,"canon",_("Picture number %d\n"),picture_number);
		
		image = malloc(sizeof(*image));
		if (!image) {
			perror("malloc");
			return NULL;
		}
		memset(image,0,sizeof(*image));
		if (!picture_number || picture_number > cached_images) {
			gp_frontend_status(NULL, _("Invalid index"));
			free(image);
			return NULL;
		}
		gp_frontend_status(NULL, cached_paths[picture_number]);
		if (!check_readiness(camera)) {
			free(image);
			return NULL;
		}
		image = psa50_get_file(cached_paths[picture_number], size);
                        if (image) return image;
		free(image);
#endif
		return NULL;
		break;
	 default:
		/* For A50 or others */
		/* clear_readiness(); */
		if (!update_dir_cache(camera)) {
			gp_frontend_status(NULL, _("Could not obtain directory listing"));
			return 0;
		}
		image = malloc(sizeof(*image));
		if (!image) {
			perror("malloc");
			return NULL;
                }
		memset(image,0,sizeof(*image));
		
		sprintf(file,"%s%s",path,filename);
		gp_debug_printf(GP_DEBUG_LOW,"canon", "canon_get_picture: file = %s\n\tpath=%s, filename=%s\n",
		       file,path,filename);
		attribs = 0;
		if (!check_readiness(camera)) {
			free(image);
			return NULL;
		}
		if (thumbnail) {
			ptr=image;
			/* The thumbnail of a movie in on a file called MVI_XXXX.THM
			 * we replace .AVI by .THM to download the thumbnail (jpeg format)
			 */
			if (is_movie(filename)) {
				char *thumbfile = strdup(filename);
				j = strrchr(thumbfile,'.') - thumbfile;
				thumbfile[j+1]='\0';
				strcat(thumbfile,"THM");
				sprintf(file,"%s%s",path,thumbfile);
				free(thumbfile);
				printf("movie thumbnail: %s\n",file);
				image = psa50_get_file(camera, file, size);
			} else {
				image = psa50_get_thumbnail(camera, file, size);
			}
			
			free(ptr);
			if (image) return image;
			else return NULL;
		}
		else {
			image = psa50_get_file(camera, file,size);
			j = strrchr(path, '\\') - path;
			path[j] = '\0';
			gp_debug_printf(GP_DEBUG_LOW,"canon","We now have to set the \"downloaded\" flag on the  picture\n");
			gp_debug_printf(GP_DEBUG_LOW,"canon","The old file attributes were: %#x\n",attribs);
			attribs = attribs & 0xdf; // 0xdf = !0x20
			psa50_set_file_attributes(camera,filename,path,attribs);
		}
		if (image) return image;
		//if(receive_error==FATAL_ERROR) clear_readiness();
		free(image);
		return NULL;
		break;
    }
}

static int _get_file_path (struct psa50_dir *tree, const char *filename, 
			   char *path)
{

  if (tree==NULL) return GP_ERROR;    
  
	if (canon_comm_method !=  CANON_USB) {
		path = strchr(path,0);
		*path = '\\';
	}

	while(tree->name) {
		if (!is_image(tree->name) && !is_movie(tree->name) && !is_thumbnail(tree->name)) { 
			switch(canon_comm_method) {
			 case CANON_USB:
				strcpy(path,tree->name);
				break;
			 case CANON_SERIAL_RS232:
			 default:
				strcpy(path+1,tree->name);
				break;
			}
		}
		if ((is_image(tree->name) || (is_movie(tree->name) || is_thumbnail(tree->name))) && strcmp(tree->name,filename)==0) {
			return GP_OK;
		}
		else if (!tree->is_file) {
			if (_get_file_path (tree->user,filename, path) == GP_OK)
			  return GP_OK;
		}
		tree++;
	}
	
	return GP_ERROR;	
}

static int get_file_path (Camera *camera, const char *filename, 
			  const char *path)
{
	struct canon_info *cs = (struct canon_info*)camera->camlib_data;

	gp_debug_printf(GP_DEBUG_LOW,"canon","get_file_path()");
	
	return _get_file_path (cs->cached_tree, filename, (char*) path);

}

int camera_file_get (Camera *camera, const char *folder, const char *filename, 
		     CameraFileType type, CameraFile *file)
{
	struct canon_info *cs = (struct canon_info*)camera->camlib_data;
	char *data;
	int buflen,i,j, size;
	char path[300]={0}, tempfilename[300]={0};

	gp_debug_printf(GP_DEBUG_LOW,"canon","camera_file_get()");
	
	if(check_readiness(camera) != 1)
	  return GP_ERROR;

	strcpy(path, cs->cached_drive);

	/* update file cache (if necessary) first */
	if(!update_dir_cache(camera)) {
		gp_frontend_status(NULL, _("Could not obtain directory listing"));
		return GP_ERROR;
	}
	
	if (get_file_path(camera, filename,path) == GP_ERROR) {
		gp_debug_printf(GP_DEBUG_LOW,"canon","Filename not found!\n");
		return GP_ERROR;
	}

	if (canon_comm_method != CANON_USB) {
		j = strrchr(path,'\\') - path;
		path[j+1] = '\0';
	} else {
		j = strchr(path,'\0') - path;
		path[j] = '\\';     
		path[j+1] = '\0';
	}

	switch (type) {
	case GP_FILE_TYPE_NORMAL:
		data = canon_get_picture (camera, (char*) filename,
					  (char*) path, 0, &buflen);
		break;
	case GP_FILE_TYPE_PREVIEW:
		data = canon_get_picture (camera, (char*) filename,
					  (char*) path, 1, &buflen);
		break;
	default:
		return (GP_ERROR_NOT_SUPPORTED);
	}

	if (!data)
		return GP_ERROR;

	switch (type) {
	case GP_FILE_TYPE_PREVIEW:
		/* we count the byte returned until the end of the jpeg data
		   which is FF D9 */
		/* It would be prettier to get that info from the exif tags */
		for(size=1;size<buflen;size++)
		  if((data[size-1]==JPEG_ESC) && (data[size]==JPEG_END)) 
		    break;
		buflen = size+1;
		gp_file_set_data_and_size (file, data, buflen);
		gp_file_set_mime_type (file, "image/jpeg"); /* always */
		strcpy(tempfilename, filename);
		strcat(tempfilename, "\0");
		strcpy(tempfilename+strlen("IMG_XXXX"), ".JPG\0");
		gp_file_set_name (file, tempfilename);
		break;
	case GP_FILE_TYPE_NORMAL:
		if (is_movie(filename))
			gp_file_set_mime_type (file, "video/x-msvideo");
		else
			gp_file_set_mime_type (file, "image/jpeg");
		gp_file_set_data_and_size (file, data, buflen);
		gp_file_set_name (file, filename);
		break;
	default:
		return (GP_ERROR_NOT_SUPPORTED);
	}

	return GP_OK;
}

/****************************************************************************/


static void pretty_number(int number,char *buffer)
{
    int len,tmp,digits;
    char *pos;

    len = 0;
    tmp = number;
    do {
        len++;
        tmp /= 10;
    }
    while (tmp);
    len += (len-1)/3;
    pos = buffer+len;
    *pos = 0;
    digits = 0;
    do {
        *--pos = (number % 10)+'0';
        number /= 10;
        if (++digits == 3) {
            *--pos = '\'';
            digits = 0;
        }
    }
    while (number);
}

int camera_summary(Camera *camera, CameraText *summary)
{
	struct canon_info *cs = (struct canon_info*)camera->camlib_data;
    char a[20],b[20];
    char *model;
	int pwr_source, pwr_status;
	char power_stats[48], cde[16];

	gp_debug_printf(GP_DEBUG_LOW,"canon","camera_summary()");
	
	if(check_readiness(camera) != 1)
	  return GP_ERROR;
	
    /*clear_readiness();*/
    if (!update_disk_cache(camera)) return GP_ERROR;
    
	pretty_number(cs->cached_capacity,a);
    pretty_number(cs->cached_available,b);
    
	model = "Canon Powershot";
    switch (cs->model) {
	 case CANON_PS_A5:      model = "Canon Powershot A5"; break;
	 case CANON_PS_A5_ZOOM: model = "Canon Powershot A5 Zoom"; break;
	 case CANON_PS_A50:     model = "Canon Powershot A50"; break;
	 case CANON_PS_A70:     model = "Canon Powershot A70"; break;
	 case CANON_PS_S10:     model = "Canon Powershot S10"; break;
	 case CANON_PS_S20:     model = "Canon Powershot S20"; break;
	 case CANON_PS_G1:      model = "Canon Powershot G1"; break;
	 case CANON_PS_G2:      model = "Canon Powershot G2"; break;
	 case CANON_PS_S100:    model = "Canon Powershot S100 / Digital IXUS / IXY DIGITAL"; break;
         case CANON_PS_S300:     model = "Canon Powershot S300 / Digital IXUS 300 / IXY DIGITAL 300"; break;
         case CANON_PS_A10:      model = "Canon Powershot A10"; break;
         case CANON_PS_A20:      model = "Canon Powershot A20"; break;
         case CANON_EOS_D30:  model = "Canon EOS D30"; break;
         case CANON_PS_PRO90_IS: model = "Canon Pro90 IS"; break;
    }
		
	canon_get_batt_status(camera, &pwr_status, &pwr_source);
	switch (pwr_source) {
	 case CAMERA_ON_AC:
		strcpy(power_stats, _("AC adapter "));
		break;
	 case CAMERA_ON_BATTERY:
		strcpy(power_stats, _("on battery "));
		break;
	 default:
		sprintf(power_stats,_("unknown (%i"),pwr_source);
		break;
	}
	
	switch (pwr_status) {
	 case CAMERA_POWER_OK:
		strcat(power_stats, _("(power OK)"));
		break;
	 case CAMERA_POWER_BAD:
		strcat(power_stats, _("(power low)"));
		break;
	 default:
		strcat(power_stats,cde);
		sprintf(cde," - %i)",pwr_status);
		break;
	}
	
    sprintf(summary->text,_("%s\n%s\n%s\nDrive %s\n%11s bytes total\n%11s bytes available\n"),
      model, cs->owner,power_stats,cs->cached_drive,a,b);
    return GP_OK;
}

/****************************************************************************/

int camera_about(Camera *camera, CameraText *about)
{
	gp_debug_printf(GP_DEBUG_LOW,"canon","camera_about()");
	
	strcpy(about->text,
		   _("Canon PowerShot series driver by\n"
		   "Wolfgang G. Reissnegger,\n"
		   "Werner Almesberger,\n"
		   "Edouard Lafargue,\n"
		   "Philippe Marzouk,\n"
		   "A5 additions by Ole W. Saastad\n"
                   "Holger Klemm\n")
		   );

	return GP_OK;
}

/****************************************************************************/

int camera_file_delete (Camera *camera, const char *folder, 
			const char *filename)
{
	struct canon_info *cs = (struct canon_info*)camera->camlib_data;
	char path[300], thumbname[300];
	int j;

	gp_debug_printf(GP_DEBUG_LOW,"canon","camera_file_delete()");
	
	// initialize memory to avoid problems later
	for (j=0; j < sizeof(path); j++)
	  path[j] = '\0';
	memset(thumbname, 0, sizeof(thumbname));

	if(check_readiness(camera) != 1)
	  return GP_ERROR;

    if (!(cs->model == CANON_PS_A5 ||
        cs->model == CANON_PS_A5_ZOOM)) { /* Tested only on powershot A50 */

        if (!update_dir_cache(camera)) {
            gp_frontend_status(NULL, _("Could not obtain directory listing"));
            return 0;
        }
        strcpy(path, cs->cached_drive);

        if (get_file_path(camera,filename,path) == GP_ERROR) {
            gp_debug_printf(GP_DEBUG_LOW,"canon","Filename not found!\n");
            return GP_ERROR;
        }
		if (canon_comm_method != CANON_USB) {
			j = strrchr(path,'\\') - path;
			path[j] = '\0';
		} else {
			j = strchr(path,'\0') - path;
			path[j] = '\0';
//			path[j+1] = '\0';
		}

		fprintf(stderr,"filename: %s\n path: %s\n",filename,path);
        if (psa50_delete_file(camera, filename,path)) {
            gp_frontend_status(NULL, _("error deleting file"));
            return GP_ERROR;
        }
        else {
		/* If we have a movie, delete its thumbnail as well */ 
		if (is_movie(filename)) {
			strcpy(thumbname, filename);
			strcpy(thumbname+strlen("MVI_XXXX"), ".THM\0"); 
			if (psa50_delete_file(camera, thumbname, path)) {
				gp_frontend_status(NULL, _("error deleting thumbnail"));
				return GP_ERROR;
			}
		}
		return GP_OK;
        }
    }
    return GP_ERROR;
}

/****************************************************************************/

static int _get_last_dir(struct psa50_dir *tree, char *path, char *temppath)
{

	if (tree==NULL) return GP_ERROR;    
  
	if (canon_comm_method !=  CANON_USB) {
		path = strchr(path,0);
		*path = '\\';
	}

	while(tree->name) {
		if (!is_image(tree->name) && !is_movie(tree->name) && !is_thumbnail(tree->name)) {
			switch(canon_comm_method) {
			 case CANON_USB:
				strcpy(path,tree->name);
				break;
			 case CANON_SERIAL_RS232:
			 default:
				strcpy(path+1,tree->name);
				break;
			}
		}
		
		if (!tree->is_file) {
			if((strcmp(path+4,"CANON")==0) && (strcmp(temppath,path)<0))
			  strcpy(temppath,path);
			_get_last_dir(tree->user,path,temppath);
		}
		tree++;
	}
	
	strcpy(path,temppath);
	
	return GP_OK;	
}

/*
 * get from the cache the name of the highest numbered directory
 * 
 */
static int get_last_dir(Camera *camera, char *path)
{
	struct canon_info *cs = (struct canon_info*)camera->camlib_data;
	char temppath[300];

	gp_debug_printf(GP_DEBUG_LOW,"canon","get_last_dir()");
	
	strncpy(temppath, path, sizeof(temppath));
	
    return _get_last_dir(cs->cached_tree, path, temppath);
}

static int _get_last_picture(struct psa50_dir *tree, char *directory, char *filename)
{

	if (tree==NULL) return GP_ERROR;    

	while(tree->name) {

		if (is_image(tree->name) || is_movie(tree->name) || is_thumbnail(tree->name)) {
			if(strcmp(tree->name,filename)>0)
			  strcpy(filename,tree->name);
		}
		
		if (!tree->is_file) {
			if((strcmp(tree->name,"DCIM")==0) || (strcmp(tree->name,directory)==0)) {
				_get_last_picture(tree->user,directory,filename);
			}
		}
		  
		tree++;
	}
	
	return GP_OK;	
}

/*
 * get from the cache the name of the highest numbered picture
 * 
 */
static int get_last_picture(Camera *camera, char *directory, char *filename)
{
	struct canon_info *cs = (struct canon_info*)camera->camlib_data;

	gp_debug_printf(GP_DEBUG_LOW,"canon","get_last_picture()");
	
    return _get_last_picture(cs->cached_tree, directory, filename);
}


int camera_folder_put_file (Camera *camera, const char *folder, 
			    CameraFile *file)
{
	struct canon_info *cs = (struct canon_info*)camera->camlib_data;
    char destpath[300], destname[300], dir[300], dcf_root_dir[10];
	int j, dirnum=0;
	char buf[10];

	gp_debug_printf(GP_DEBUG_LOW,"canon","camera_folder_put_file()");
	
	if(check_readiness(camera) != 1)
	  return GP_ERROR;
	
	if (cs->speed>57600 && 
		(strcmp(camera->model,"Canon PowerShot A50") == 0
		 || strcmp(camera->model, "Canon PowerShot Pro70") == 0)) {
		gp_frontend_message(camera,
  _("Speeds greater than 57600 are not supported for uploading to this camera"));
		return GP_ERROR;
	}
		
	if (!check_readiness(camera)) {
		return GP_ERROR;
	}
	
	for (j=0; j < sizeof(destpath); j++) {
		destpath[j] = '\0';
		dir[j] = '\0';
		destname [j] = '\0';
	}

    if(!update_dir_cache(camera)) {
        gp_frontend_status(NULL, _("Could not obtain directory listing"));
        return GP_ERROR;
    }

	sprintf(dcf_root_dir,"%s\\DCIM",cs->cached_drive);

	if(get_last_dir(camera, dir) == GP_ERROR)
	  return GP_ERROR;

	if(strlen(dir)==0) {
		sprintf(dir,"\\100CANON");
		sprintf(destname, "AUT_0001.JPG");
	}
	else {
		if (get_last_picture(camera,dir+1,destname)==GP_ERROR)
		  return GP_ERROR;
		
		if(strlen(destname)==0) {
			sprintf(destname,"AUT_%c%c01.JPG",dir[2],dir[3]);
		}
		else {
			sprintf(buf, "%c%c",destname[6],destname[7]);
			j = 1;
			j = atoi(buf);
			if (j==99) {
				j=1;
				sprintf(buf,"%c%c%c",dir[1],dir[2],dir[3]);
				dirnum = atoi(buf);
				if (dirnum == 999) {
					gp_frontend_message(camera,_("Could not upload, no free folder name available!\n"
										"999CANON folder name exists and has an AUT_9999.JPG picture in it."));
					return GP_ERROR;
				}
				else {
					dirnum++;
					sprintf(dir,"\\%03iCANON", dirnum);
				}
			}
			else
			  j++;
			
			sprintf(destname,"AUT_%c%c%02i.JPG",dir[2],dir[3],j);
		}
				
		sprintf(destpath,"%s%s",dcf_root_dir,dir);
		
		gp_debug_printf(GP_DEBUG_LOW,"canon","destpath: %s destname: %s\n",destpath,destname);
	}
	
	if(!psa50_directory_operations(camera, dcf_root_dir, DIR_CREATE)) {
		gp_frontend_message(camera,"could not create \\DCIM directory");
		return GP_ERROR;
	}
	
	if(!psa50_directory_operations(camera,destpath, DIR_CREATE)) {
		gp_frontend_message(camera,"could not create destination directory");
		return GP_ERROR;
	}
	

	j = strlen(destpath);
	destpath[j] = '\\';
	destpath[j+1] = '\0';
	
	clear_readiness(camera);
	
    return psa50_put_file(camera,file, destname, destpath);     
}

/****************************************************************************/
	
int camera_get_config (Camera *camera, CameraWidget **window)
{
	struct canon_info *cs = (struct canon_info*)camera->camlib_data;
	CameraWidget *t, *section;
        char power_stats[48], firm[64];
	int pwr_status, pwr_source;
	struct tm *camtm;
	time_t camtime;

	gp_debug_printf(GP_DEBUG_LOW,"canon","camera_get_config()");
	
	gp_widget_new (GP_WIDGET_WINDOW, "Canon PowerShot Configuration", 
		       window);

	gp_widget_new (GP_WIDGET_SECTION, _("Configure"), &section);
	gp_widget_append (*window,section);
	
	gp_widget_new (GP_WIDGET_TEXT,_("Camera Model"), &t);
	gp_widget_set_value (t, cs->ident);
	gp_widget_append (section,t);

	gp_widget_new (GP_WIDGET_TEXT,_("Owner name"), &t);
	gp_widget_set_value (t, cs->owner);
	gp_widget_append (section,t);

	gp_widget_new (GP_WIDGET_TEXT, "date", &t);
	if (cs->cached_ready == 1) {
	  camtime = psa50_get_time(camera);
	  camtm = gmtime(&camtime);
	  gp_widget_set_value (t, asctime(camtm));
	} else
	  gp_widget_set_value (t, _("Unavailable"));
	gp_widget_append (section,t);
	
	gp_widget_new (GP_WIDGET_TOGGLE, _("Set camera date to PC date"), &t);
	gp_widget_append (section,t);
	
	gp_widget_new (GP_WIDGET_TEXT,_("Firmware revision"), &t);
	sprintf (firm,"%i.%i.%i.%i",cs->firmwrev[3], 
			cs->firmwrev[2],cs->firmwrev[1],
			cs->firmwrev[0]);
	gp_widget_set_value (t, firm);
	gp_widget_append (section,t);
	
	if (cs->cached_ready == 1) {
		canon_get_batt_status(camera, &pwr_status,&pwr_source);
		switch (pwr_source) {
		 case CAMERA_ON_AC:
			strcpy(power_stats, _("AC adapter "));
			break;
		 case CAMERA_ON_BATTERY:
			strcpy(power_stats, _("on battery "));
			break;
		 default:
			sprintf(power_stats,_("unknown (%i"),pwr_source);
			break;
		}
		switch (pwr_status) {
			char cde[16];
		 case CAMERA_POWER_OK:
			strcat(power_stats, _("(power OK)"));
			break;
		 case CAMERA_POWER_BAD:
			strcat(power_stats, _("(power low)"));
			break;
		 default:
			strcat(power_stats,cde);
			sprintf(cde," - %i)",pwr_status);
			break;
		}
	}
	else
	  strcpy (power_stats,_("Power: camera unavailable"));
	
	gp_widget_new (GP_WIDGET_TEXT,_("Power"), &t);
	gp_widget_set_value (t, power_stats);
	gp_widget_append (section,t);
	
	gp_widget_new (GP_WIDGET_SECTION, _("Debug"), &section);
	gp_widget_append (*window, section);
	
	gp_widget_new (GP_WIDGET_MENU, _("Debug level"), &t);
	gp_widget_add_choice (t, "none") ;
	gp_widget_add_choice (t, "functions");
	gp_widget_add_choice (t, "complete");
	switch (cs->debug) {
	 case 0:
	 default:
		gp_widget_set_value (t, "none");
		break;
	 case 1:
		gp_widget_set_value (t, "functions");
		break;
	 case 9:
		gp_widget_set_value (t, "complete");
		break;
	}
	gp_widget_append (section,t);

	gp_widget_new (GP_WIDGET_TOGGLE, _("Dump data by packets to stderr"), &t);
	gp_widget_set_value (t, &(cs->dump_packets));
	gp_widget_append (section,t);

	return GP_OK;
}

#if 0
int camera_config(Camera *camera)
{
	CameraWidget *window;

	camera_get_config (camera, &window);
	
	/* Prompt the user with the config window */	
	if (gp_frontend_prompt (camera, window) == GP_PROMPT_CANCEL) {
		gp_widget_unref(window);
		return GP_OK;
	}

	camera_set_config (camera, window);
	gp_widget_unref (window);
	return GP_OK;
}
#endif

int camera_set_config (Camera *camera, CameraWidget *window)
{
  struct canon_info *cs = (struct canon_info*)camera->camlib_data;
  CameraWidget *w;
  char *wvalue;
  char buf[8];
  
  gp_debug_printf (GP_DEBUG_LOW,"canon","camera_set_config()");
  
  gp_widget_get_child_by_label (window,_("Debug level"), &w);
  if (gp_widget_changed (w)) {
    gp_widget_get_value (w, &wvalue);
    if(strcmp(wvalue,"none") == 0)
      cs->debug = 0;
    else if (strcmp(wvalue,"functions") == 0)
      cs->debug = 1;
    else if (strcmp(wvalue,"complete") == 0)
      cs->debug = 9;
    
    sprintf(buf,"%i",cs->debug);
    gp_setting_set("canon", "debug", buf);
  }
  
  gp_widget_get_child_by_label (window,_("Dump data by packets to stderr"), 
				&w);
  if (gp_widget_changed (w)) {
    gp_widget_get_value (w, &(cs->dump_packets));
    sprintf(buf,"%i",cs->dump_packets);
    gp_setting_set("canon", "dump_packets", buf);
  }
  
  gp_widget_get_child_by_label (window,_("Owner name"), &w);
  if (gp_widget_changed (w)) {
    gp_widget_get_value (w, &wvalue);
    if(!check_readiness(camera)) {
      gp_frontend_status(camera,_("Camera unavailable"));
    } else {
      if(psa50_set_owner_name(camera, wvalue))
	gp_frontend_status(camera, _("Owner name changed"));
      else
	gp_frontend_status (camera, 
			    _("could not change owner name"));
    }
  }
	
  gp_widget_get_child_by_label (window,_("Set camera date to PC date"), &w);
  if (gp_widget_changed (w)) {
    gp_widget_get_value (w, &wvalue);
    if(!check_readiness(camera)) {
      gp_frontend_status(camera,_("Camera unavailable"));
    } else {
      if(psa50_set_time(camera)) {
	gp_frontend_status(camera,_("time set"));
      } else {
	gp_frontend_status(camera,_("could not set time"));
      }
    }
  }
  
  gp_debug_printf (GP_DEBUG_LOW, "canon", _("done configuring camera.\n"));
  
  return GP_OK;
}

int camera_file_get_info (Camera *camera,  const char *folder, 
		     const char *filename, CameraFileInfo *info)
{
  gp_debug_printf(GP_DEBUG_LOW,"canon","camera_file_get_info()");
  
  info->preview.fields = GP_FILE_INFO_TYPE;

  /* thumbnails are always jpeg on Canon Cameras */
  strcpy (info->preview.type, "image/jpeg");

  /* FIXME GP_FILE_INFO_PERMISSIONS to add */
  info->file.fields = GP_FILE_INFO_NAME | GP_FILE_INFO_TYPE;

  if(is_movie(filename))
    strcpy (info->file.type, "video/x-msvideo");
  else if (is_image(filename))
    strcpy (info->file.type, "image/jpeg");
  else
    /* May no be correct behaviour ... */
    strcpy (info->file.type, "unknown");

  strcpy (info->file.name, filename);

  return GP_OK; 
}

int camera_file_set_info (Camera *camera, const char *folder, 
			     const char *filename, CameraFileInfo *info)
{
  gp_debug_printf(GP_DEBUG_LOW,"canon","camera_file_set_info()");
  
  return GP_ERROR_NOT_SUPPORTED;
}

/****************************************************************************/
	
int camera_capture (Camera *camera, int capture_type, CameraFilePath *path)
{
  gp_debug_printf(GP_DEBUG_LOW,"canon","camera_capture()");
  
  return GP_ERROR_NOT_SUPPORTED;
}

int camera_capture_preview(Camera *camera, CameraFile *file)
{
  gp_debug_printf(GP_DEBUG_LOW,"canon","camera_capture_preview()");
  
  return GP_ERROR_NOT_SUPPORTED;
}

int camera_file_get_config (Camera *camera, const char *folder, 
			    const char *filename, CameraWidget **window)
{
  return GP_ERROR_NOT_SUPPORTED;
}

int camera_file_set_config  (Camera *camera, const char *folder, 
			     const char *filename, CameraWidget *window)
{
  return GP_ERROR_NOT_SUPPORTED;
}

int camera_folder_get_config(Camera *camera, const char *folder,
                             CameraWidget **window)
{
  return GP_ERROR_NOT_SUPPORTED;
}

int camera_folder_set_config (Camera *camera, const char *folder,
                              CameraWidget *window)
{
  return GP_ERROR_NOT_SUPPORTED;
}

char *camera_result_as_string (Camera *camera, int result)
{
  return NULL;
}

/**
 * This routine initializes the serial port and also load the
 * camera settings. Right now it is only the speed that is
 * saved.
 */
int camera_init(Camera *camera)
{
  struct canon_info *cs;
  char buf[8];
  
  gp_debug_printf(GP_DEBUG_LOW,"canon","camera_init()");
  
  /* First, set up all the function pointers */
  camera->functions->exit                = camera_exit;
  camera->functions->folder_list_folders = camera_folder_list_folders;
  camera->functions->folder_list_files   = camera_folder_list_files;
  camera->functions->folder_put_file     = camera_folder_put_file;
  camera->functions->file_get            = camera_file_get;
  camera->functions->file_delete         = camera_file_delete;
  camera->functions->file_get_info       = camera_file_get_info;
  camera->functions->file_set_info       = camera_file_set_info;
  camera->functions->get_config          = camera_get_config;
  camera->functions->set_config          = camera_set_config;
  camera->functions->file_get_config     = camera_file_get_config;
  camera->functions->file_set_config     = camera_file_set_config;
  camera->functions->folder_get_config   = camera_folder_get_config;
  camera->functions->folder_set_config   = camera_folder_set_config;
  camera->functions->capture             = camera_capture;
  camera->functions->summary             = camera_summary;
  camera->functions->manual              = camera_manual;
  camera->functions->about               = camera_about;
  camera->functions->result_as_string    = camera_result_as_string;
  
  cs = (struct canon_info*)malloc(sizeof(struct canon_info));
  camera->camlib_data = cs;
  
  cs->first_init = 1;
  cs->uploading = 0;
  cs->slow_send = 0;
  cs->cached_ready = 0;
  cs->cached_disk = 0;
  cs->cached_dir = 0;
  cs->dump_packets = 0;
  cs->cached_tree = NULL;
  
  fprintf(stderr,"canon_initialize()\n");
  
  cs->speed = camera->port_info->speed;
  /* Default speed */
  if (cs->speed == 0)
    cs->speed = 9600;
  
  if (gp_setting_get("canon","debug",buf) != GP_OK)
    cs->debug = 1;
  
  if (strncmp(buf, "0", 1) == 0)
    cs->debug = 0;
  if (strncmp(buf, "1", 1) == 0)
    cs->debug = 1;
  if (strncmp(buf, "1", 1) == 0)
    cs->debug = 1;
  if (strncmp(buf, "2", 1) == 0)
    cs->debug = 2;
  if (strncmp(buf, "3", 1) == 0)
    cs->debug = 3;
  if (strncmp(buf, "4", 1) == 0)
    cs->debug = 4;
  if (strncmp(buf, "5", 1) == 0)
    cs->debug = 5;
  if (strncmp(buf, "6", 1) == 0)
    cs->debug = 6;
  if (strncmp(buf, "7", 1) == 0)
    cs->debug = 7;
  if (strncmp(buf, "8", 1) == 0)
    cs->debug = 8;
  if (strncmp(buf, "9", 1) == 0)
    cs->debug = 9;
  fprintf(stderr,"Debug level: %i\n",cs->debug);
  
  if(gp_setting_get("canon","dump_packets",buf) == GP_OK) {
    if(strncmp(buf, "1", 1) == 0)
      cs->dump_packets = 1;
    if(strncmp(buf, "0", 1) == 0)
      cs->dump_packets = 0;
  }
  
  switch (camera->port->type) { 
  case GP_PORT_USB:
    gp_debug_printf(GP_DEBUG_LOW,"canon","GPhoto tells us that we should use a USB link.\n");
    canon_comm_method = CANON_USB;
    break;
  case GP_PORT_SERIAL:
  default:
    gp_debug_printf(GP_DEBUG_LOW,"canon","GPhoto tells us that we should use a RS232 link.\n");
    canon_comm_method = CANON_SERIAL_RS232;
    break;
  }
  if (canon_comm_method == CANON_SERIAL_RS232)
    gp_debug_printf(GP_DEBUG_LOW,"canon","Camera transmission speed : %i\n", cs->speed);
  
  
  return canon_serial_init(camera,camera->port_info->path);
}

