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
#include <gpio.h>

#include "util.h"
#include "serial.h"
#include "psa50.h"
#include "canon.h"

/* Global variable to set debug level.                        */
/* Defined levels so far :                                    */
/*   0 : no debug output                                      */
/*   1 : debug actions                                        */
/*   9 : debug everything, including hex dumps of the packets */

int canon_debug_driver = 0;
int gphoto2_debug = 0;

/* DO NOT FORGET to update the NUM_ constants after adding a camera */

#define NUM_SERIAL      4
#define NUM_USB         2
#define NUM_SERIAL_USB  2

char *models_serial[] = {
        "Canon PowerShot A5",
        "Canon PowerShot A5 Zoom",
        "Canon PowerShot A50",
        "Canon PowerShot Pro70",
        NULL
};
char *models_USB[] = {
        "Canon Digital IXUS",
        "Canon PowerShot S100",
        NULL
};
char *models_serialandUSB[] = {
        "Canon PowerShot S10",
        "Canon PowerShot S20",
        NULL
};

int camera_id(CameraText *id)
{
        strcpy(id->text, "canon");

        return GP_OK;
}

int camera_manual(Camera *camera, CameraText *manual)
{
        strcpy(manual->text, "For the A50, 115200 may not be faster than 57600");

        return GP_OK;
}

int camera_abilities(CameraAbilitiesList *list)
{
    int i;
    CameraAbilities *a;

    

    i=0;
    while(models_serialandUSB[i]) {
        a = gp_abilities_new();
        strcpy(a->model, models_serialandUSB[i]);
        a->port = GP_PORT_SERIAL | GP_PORT_USB;
        a->speed[0]   = 9600;
        a->speed[1]   = 19200;
        a->speed[2]   = 38400;
        a->speed[3]   = 57600;
        a->speed[4]   = 115200;
        a->speed[5]   = 0;
        a->capture    = 0;
        a->config     = 0;
        a->file_delete = 1;
        a->file_preview = 1;
        a->file_put = 1;
        gp_abilities_list_append(list, a);
        i++;
    }

    i=0;
    while(models_USB[i]) {
        a = gp_abilities_new();
        strcpy(a->model, models_USB[i]);
        a->port = GP_PORT_USB;
        a->capture    = 0;
        a->config     = 0;
        a->file_delete = 1;
        a->file_preview = 1;
        a->file_put = 0;
        gp_abilities_list_append(list, a);
        i++;
    }

    i=0;
    while(models_serial[i]) {
        a = gp_abilities_new();
        strcpy(a->model, models_serial[i]);
        a->port = GP_PORT_SERIAL;
        a->speed[0]   = 9600;
        a->speed[1]   = 19200;
        a->speed[2]   = 38400;
        a->speed[3]   = 57600;
        a->speed[4]   = 115200;
        a->speed[5]   = 0;
        a->capture    = 0;
        a->config     = 0;
        a->file_delete = 1;
        a->file_preview = 1;
        a->file_put = 1;
        gp_abilities_list_append(list, a);
        i++;
    }

    return GP_OK;
}

int camera_folder_list(Camera *camera, CameraList *list, char *folder)
{
    return GP_OK;
}

static void clear_readiness(Camera *camera)
{
	struct canon_info *cs = (struct canon_info*)camera->camlib_data;

    cs->cached_ready = 0;
}

static int check_readiness(Camera *camera)
{
	struct canon_info *cs = (struct canon_info*)camera->camlib_data;
	
    if (cs->cached_ready) return 1;
    if (psa50_ready(camera)) {
		gp_debug_printf(GP_DEBUG_LOW,"canon","Camera type:  %d\n",cs->model);
		cs->cached_ready = 1;
		return 1;
    }
    gp_frontend_status(NULL, "Camera unavailable");
    return 0;
}

void switch_camera_off(Camera *camera)
{
	gp_frontend_status(NULL, "Switching Camera Off");
	psa50_off(camera);
	clear_readiness(camera);
}

int camera_exit(Camera *camera)
{
	struct canon_info *cs = (struct canon_info*)camera->camlib_data;
	
	switch_camera_off(camera);
	
	canon_serial_close(cs->gdev);
	free(cs);
	return GP_OK;
}

int canon_get_batt_status(Camera *camera, int *pwr_status, int *pwr_source)
{
	if (!check_readiness(camera))
	  return -1;
	
	return psa50_get_battery(camera, pwr_status, pwr_source);
}

char *camera_model_string(Camera *camera)
{
	struct canon_info *cs = (struct canon_info*)camera->camlib_data;
	
	if (!check_readiness(camera))
	  return "Camera unavailable";

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
	 case CANON_PS_S100:
		return "Powershot S100 / Digital IXUS";
	 default:
		return "Unknown model !";
	}
}


static int update_disk_cache(Camera *camera)
{
	struct canon_info *cs = (struct canon_info*)camera->camlib_data;
	char root[10]; /* D:\ or such */
	char *disk;
	
	if (cs->cached_disk) return 1;
	if (!check_readiness(camera)) return 0;
	disk = psa50_get_disk(camera);
	if (!disk) {
		gp_frontend_status(NULL, "No response");
		return 0;
	}
	strcpy(cs->cached_drive,disk);
	sprintf(root,"%s\\",disk);
	if (!psa50_disk_info(camera, root,&cs->cached_capacity,&cs->cached_available)) {
		gp_frontend_status(NULL, "No response");
		return 0;
	}
	cs->cached_disk = 1;
	
	return 1;
}


static int is_image(const char *name)
{
        const char *pos;

        pos = strchr(name,'.');
        if (!pos) return 0;
        return !strcmp(pos,".JPG");
}

/* This function is only used by A5 */

static int recurse(Camera *camera, const char *name)
{
	struct canon_info *cs = (struct canon_info*)camera->camlib_data;
    struct psa50_dir *dir,*walk;
    char buffer[300]; /* longest path, etc. */
    int count,curr;

    dir = psa50_list_directory(camera, name);
    if (!dir) return 1; /* assume it's empty @@@ */
    count = 0;
    for (walk = dir; walk->name; walk++)
        if (walk->size && is_image(walk->name)) count++;
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
            if (!is_image(walk->name)) continue;
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
    return strcmp(((const struct psa50_dir *) a)->name,
      ((const struct psa50_dir *) b)->name);
}


/* This function is only used by A50 */

static struct psa50_dir *dir_tree(Camera *camera, const char *path)
{
	struct canon_info *cs = (struct canon_info*)camera->camlib_data;
    struct psa50_dir *dir,*walk;
    char buffer[300]; /* longest path, etc. */

    dir = psa50_list_directory(camera, path);
    if (!dir) return NULL; /* assume it's empty @@@ */
    for (walk = dir; walk->name; walk++) {
        if (walk->is_file) {
            if (is_image(walk->name)) cs->cached_images++;
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
	
    psa50_free_dir(camera, cs->cached_tree);
}


/* A5 only: sort THB_ and AUT_ into their proper arrangement. */
static int compare_a5_paths (const void * p1, const void * p2)
{
	const char * s1 = *((const char **)p1);
	const char * s2 = *((const char **)p2);
	const char * ptr, * base1, * base2;
	int n1 = 0, n2 = 0;
	
	printf("Comparing %s to %s\n", s1, s2);
	
	ptr = strrchr(s1, '_');
	if (ptr)
	  n1 = strtol(ptr+1, 0, 10);
	ptr = strrchr(s2, '_');
	if (ptr)
	  n2 = strtol(ptr+1, 0, 10);
	
	printf("Numbers are %d and %d\n", n1, n2);
	
	if (n1 < n2)
	  return -1;
	else if (n1 > n2)
	  return 1;
	else {
		base1 = strrchr(s1, '\\');
		base2 = strrchr(s2, '\\');
		printf("Base 1 is %s and base 2 is %s\n", base1, base2);
		return strcmp(base1, base2);
	}
}


static int update_dir_cache(Camera *camera)
{
	struct canon_info *cs = (struct canon_info*)camera->camlib_data;
    int i;
	
    if (cs->cached_dir) return 1;
    if (!update_disk_cache(camera)) return 0;
    if (!check_readiness(camera)) return 0;
    cs->cached_images = 0;
    switch (cs->model) {
    case CANON_PS_A5:
    case CANON_PS_A5_ZOOM:
      if (recurse(camera, cs->cached_drive)) {
        printf("Before sort:\n");
        for (i = 1; i < cs->cached_images; i++) {
          printf("%d: %s\n", i, cs->cached_paths[i]);
        }
        qsort(cs->cached_paths+1, cs->cached_images, sizeof(char *), compare_a5_paths);
        printf("After sort:\n");
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

static int _canon_file_list(struct psa50_dir *tree, CameraList *list, char *folder)
{

    if (!tree) {
//        debug_message(camera,"no directory listing available!\n");
        return GP_ERROR;
	}

    while (tree->name) {
        if(is_image(tree->name))
			gp_list_append(list,(char*)tree->name,GP_LIST_FILE);
        else if (!tree->is_file) { 
            _canon_file_list(tree->user, list, folder);      
        }
        tree++;
    }

    return GP_OK;
}

static int canon_file_list(Camera *camera, CameraList *list, char *folder)
{
	struct canon_info *cs = (struct canon_info*)camera->camlib_data;
	
    if(!update_dir_cache(camera)) {
        gp_frontend_status(NULL, "Could not obtain directory listing");
        return GP_ERROR;
    }

    _canon_file_list(cs->cached_tree, list, folder);
    return GP_OK;
}


int camera_file_list(Camera *camera, CameraList *list, char *folder)
{
    
    return canon_file_list(camera, list, folder);

}

/****************************************************************************
 *
 * gphoto library interface calls
 *
 ****************************************************************************/


#define JPEG_END        0xFFFFFFD9
#define JPEG_ESC        0xFFFFFFFF

static char *canon_get_picture(Camera *camera, char *filename, char *path, int thumbnail, int *size)
{
	struct canon_info *cs = (struct canon_info*)camera->camlib_data;
    char *image;
    char attribs;
    char file[300];
    void *ptr;
	int j;
	
    if (!check_readiness(camera)) {
		return NULL;
    }
    switch (cs->model) {
	 case CANON_PS_A5:
	 case CANON_PS_A5_ZOOM:
#if 0
		picture_number=picture_number*2-1;
		if (thumbnail) picture_number+=1;
		gp_debug_printf(GP_DEBUG_LOW,"canon","Picture number %d\n",picture_number);
		
		image = malloc(sizeof(*image));
		if (!image) {
			perror("malloc");
			return NULL;
		}
		memset(image,0,sizeof(*image));
		if (!picture_number || picture_number > cached_images) {
			gp_frontend_status(NULL, "Invalid index");
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
			gp_frontend_status(NULL, "Could not obtain directory listing");
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
			if ( (image = psa50_get_thumbnail(camera, file,size)) == NULL) {
				free(ptr);
				return NULL;
			}
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

static int _get_file_path(struct psa50_dir *tree, char *filename, char *path)
{

  if (tree==NULL) return GP_ERROR;    
  
	if (canon_comm_method !=  CANON_USB) {
		path = strchr(path,0);
		*path = '\\';
	}

	while(tree->name) {
		if (!is_image(tree->name)) { 
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
		if (is_image(tree->name) && strcmp(tree->name,filename)==0) {
			return GP_OK;
		}
		else if (!tree->is_file) {
			if (_get_file_path(tree->user,filename, path) == GP_OK)
			  return GP_OK;
		}
		tree++;
	}
	
	return GP_ERROR;	
}

static int get_file_path(Camera *camera, char *filename, char *path)
{
	struct canon_info *cs = (struct canon_info*)camera->camlib_data;
	
    return _get_file_path(cs->cached_tree, filename, path);

}

int camera_file_get(Camera *camera, CameraFile *file, char *folder, char *filename)
{
	struct canon_info *cs = (struct canon_info*)camera->camlib_data;
    char *data;
    int buflen,j;
    char path[300];


    strcpy(path, cs->cached_drive);

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

    data = canon_get_picture(camera, filename, path, 0, &buflen);
    if (!data)
        return GP_ERROR;

    file->data = data;
    strcpy(file->type, "image/jpeg");
    file->size = buflen;

    snprintf(file->name, sizeof(file->name), "%s",
        filename);

    return GP_OK;
}

int camera_file_get_preview(Camera *camera, CameraFile *preview, char *folder, char *filename)
{
	struct canon_info *cs = (struct canon_info*)camera->camlib_data;
    char *data;
    int buflen,i,j,size;
    char path[300], tempfilename[300];

    strcpy(path, cs->cached_drive);

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

    data = canon_get_picture(camera, filename, path, 1, &buflen);
    if (!data)
        return GP_ERROR;

    preview->data = data;
    strcpy(preview->type, "image/jpeg");

    /* we count the byte returned until the end of the jpeg data
       which is FF D9 */
    /* It would be prettier to get that info from the exif tags     */

    for(size=1;size<buflen;size++)
    if(data[size]==JPEG_END) {
        if(data[size-1]==JPEG_ESC) break;
    }
    buflen = size+1;

    preview->size = buflen;

    i=0;
    if (A5==0) {
        tempfilename[i++]='t';
        tempfilename[i++]='h';
        tempfilename[i++]='u';
        tempfilename[i++]='m';
        tempfilename[i++]='b';
        tempfilename[i++]='_';
    }
    while(filename[i+1] != '\0') {
        tempfilename[i] = tolower(filename[i]);
        i++;
    }

    tempfilename[i]='\0';

    snprintf(preview->name, sizeof(preview->name), "%s",
            tempfilename);

    return GP_OK;
}
/****************************************************************************/


/**
 * This routine initializes the serial port and also load the
 * camera settings. Right now it is only the speed that is
 * saved.
 */
int camera_init(Camera *camera, CameraInit *init)
{
	struct canon_info *cs;
	char buf[8];
	
	/* First, set up all the function pointers */
	camera->functions->id                = camera_id;
	camera->functions->abilities         = camera_abilities;
	camera->functions->init      = camera_init;
	camera->functions->exit      = camera_exit;
	camera->functions->folder_list = camera_folder_list;
	camera->functions->file_list = camera_file_list;
	camera->functions->file_get  = camera_file_get;
	camera->functions->file_get_preview =  camera_file_get_preview;
	camera->functions->file_put  = camera_file_put;
	camera->functions->file_delete = camera_file_delete;
	camera->functions->config    = camera_config;
	camera->functions->capture   = camera_capture;
	camera->functions->summary   = camera_summary;
	camera->functions->manual    = camera_manual;
	camera->functions->about     = camera_about;
	
	cs = (struct canon_info*)malloc(sizeof(struct canon_info));
	camera->camlib_data = cs;

	cs->first_init = 1;
	cs->uploading = 0;
	cs->slow_send = 0;
	cs->cached_ready = 0;
	cs->cached_disk = 0;
	cs->cached_dir = 0;
	cs->dump_packets = 0;
	
	gphoto2_debug = init->debug;
	fprintf(stderr,"canon_initialize()\n");
	
	cs->speed = init->port.speed;
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

	switch (init->port.type) { 
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
	
	return !canon_serial_init(camera,init->port.path);
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
	 case CANON_PS_S100:    model = "Canon Powershot S100 / Digital IXUS"; break;
    }

	canon_get_batt_status(camera, &pwr_status, &pwr_source);
	switch (pwr_source) {
	 case CAMERA_ON_AC:
		strcpy(power_stats, "AC adapter ");
		break;
	 case CAMERA_ON_BATTERY:
		strcpy(power_stats, "on battery ");
		break;
	 default:
		sprintf(power_stats,"unknown (%i",pwr_source);
		break;
	}
	
	switch (pwr_status) {
	 case CAMERA_POWER_OK:
		strcat(power_stats, "(power OK)");
		break;
	 case CAMERA_POWER_BAD:
		strcat(power_stats, "(power low)");
		break;
	 default:
		strcat(power_stats,cde);
		sprintf(cde," - %i)",pwr_status);
		break;
	}
	
    sprintf(summary->text,"%s\n%s\nDrive %s\n%11s bytes total\n%11s bytes available\n",
      model, power_stats,cs->cached_drive,a,b);
    return GP_OK;
}

/****************************************************************************/

int camera_about(Camera *camera, CameraText *about)
{
        strcpy(about->text,
                "Canon PowerShot series driver by\n"
                "Wolfgang G. Reissnegger,\n"
                "Werner Almesberger,\n"
                "Edouard Lafargue,\n"
                "Philippe Marzouk,\n"
                "A5 additions by Ole W. Saastad\n"
        );

        return GP_OK;
}

/****************************************************************************/

int camera_file_delete(Camera *camera, char *folder, char *filename)
{
	struct canon_info *cs = (struct canon_info*)camera->camlib_data;
    char path[300];
    int j;

        /*clear_readiness();*/
    if (!check_readiness(camera)) {
        return 0;
    }

    if (!(cs->model == CANON_PS_A5 ||
        cs->model == CANON_PS_A5_ZOOM)) { /* this is tested only on powershot A50 */

        if (!update_dir_cache(camera)) {
            gp_frontend_status(NULL, "Could not obtain directory listing");
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
            gp_frontend_status(NULL, "error deleting file");
            return GP_ERROR;
        }
        else {
			return GP_OK;
        }
    }
    return GP_ERROR;
}

int camera_file_put(Camera *camera, CameraFile *file, char *folder)
{
	struct canon_info *cs = (struct canon_info*)camera->camlib_data;
    char destpath[300], destname[300];
	int j;

	if (cs->speed>57600 && 
		(strcmp(camera->model,"Canon PowerShot A50") == 0
		 || strcmp(camera->model, "Canon PowerShot Pro70") == 0)) {
		gp_frontend_message(camera,
  "Speeds greater than 57600 are not supported for uploading to this camera");
		return GP_ERROR;
	}
		
	if (!check_readiness(camera)) {
		return GP_ERROR;
	}
	
	strcpy(destname,file->name);

    if(!update_dir_cache(camera)) {
        gp_frontend_status(NULL, "Could not obtain directory listing");
        return GP_ERROR;
    }
	
	sprintf(destpath,"D:\\DCIM\\100CANON\\");
	
	j = strrchr(destpath,'\\') - destpath;
	destpath[j] = '\0';
	
	if(!psa50_directory_operations(camera,destpath, DIR_CREATE)) 
	  return GP_ERROR;
	
	psa50_delete_file(camera,destname,destpath);
	
	destpath[j] = '\\';
	
    return psa50_put_file(camera,file, destname, destpath);     
}

int camera_config(Camera *camera)
{
	struct canon_info *cs = (struct canon_info*)camera->camlib_data;
	CameraWidget *window, *w, *t, *section;
	char *wvalue;
	char power_stats[48], buf[8];
	int pwr_status, pwr_source;
	struct tm *camtm;
	time_t camtime;
	struct canon_info *fd = (struct canon_info*)camera->camlib_data;
	
	if (cs->cached_ready) {
		camtime = psa50_get_time(camera);
		camtm = gmtime(&camtime);
	}

	window = gp_widget_new(GP_WIDGET_WINDOW, "Canon Configuration");

	/* set the window label to something more specific */
	strcpy(window->label, "Canon PowerShot series");
	
	section = gp_widget_new(GP_WIDGET_SECTION, "Configure");
	gp_widget_append(window,section);
	
	t = gp_widget_new(GP_WIDGET_TEXT,"Camera Model");
	strcpy(t->value,fd->ident);
	gp_widget_append(section,t);

	t = gp_widget_new(GP_WIDGET_TEXT,"Owner name");
	strcpy(t->value,fd->owner);
	gp_widget_append(section,t);

	t = gp_widget_new(GP_WIDGET_TEXT, "date");
	if (cs->cached_ready)
	  strcpy(t->value,asctime(camtm));
	else
	  sprintf(t->value,"Unavailable");
	gp_widget_append(section,t);
	
	t = gp_widget_new(GP_WIDGET_TOGGLE, "Set camera date to PC date");
	gp_widget_append(section,t);
	
	t = gp_widget_new(GP_WIDGET_TEXT,"Firmware revision");
	sprintf(t->value,"%i.%i.%i.%i",fd->firmwrev[3], 
			fd->firmwrev[2],fd->firmwrev[1],
			fd->firmwrev[0]);
	gp_widget_append(section,t);
	
	if (cs->cached_ready) {
		canon_get_batt_status(camera, &pwr_status,&pwr_source);
		switch (pwr_source) {
		 case CAMERA_ON_AC:
			strcpy(power_stats, "AC adapter ");
			break;
		 case CAMERA_ON_BATTERY:
			strcpy(power_stats, "on battery ");
			break;
		 default:
			sprintf(power_stats,"unknown (%i",pwr_source);
			break;
		}
		switch (pwr_status) {
			char cde[16];
		 case CAMERA_POWER_OK:
			strcat(power_stats, "(power OK)");
			break;
		 case CAMERA_POWER_BAD:
			strcat(power_stats, "(power low)");
			break;
		 default:
			strcat(power_stats,cde);
			sprintf(cde," - %i)",pwr_status);
			break;
		}
	}
	else
	  strcpy(power_stats,"Power: camera unavailable");
	
	t = gp_widget_new(GP_WIDGET_TEXT,"Power");
	strcpy(t->value, power_stats);
	gp_widget_append(section,t);
	
	section = gp_widget_new(GP_WIDGET_SECTION, "Debug");
	gp_widget_append(window,section);
	
	t = gp_widget_new(GP_WIDGET_MENU, "Debug level");
	gp_widget_choice_add (t, "none");
	gp_widget_choice_add (t, "functions");
	gp_widget_choice_add (t, "complete");
	switch (cs->debug) {
	 case 0:
	 default:
		gp_widget_value_set(t, "none");
		break;
	 case 1:
		gp_widget_value_set(t, "functions");
		break;
	 case 9:
		gp_widget_value_set(t, "complete");
		break;
	}
	gp_widget_append(section,t);

	t = gp_widget_new(GP_WIDGET_TOGGLE, 
					  "Dump data by packets to stderr");
	if(cs->dump_packets == 1)
	  gp_widget_value_set(t, "1");
	else
	  gp_widget_value_set(t, "0");
	gp_widget_append(section,t);

	/* Prompt the user with the config window */	
	if (gp_frontend_prompt (camera, window) == GP_PROMPT_CANCEL) {
		gp_widget_free(window);
		return GP_OK;
	}
	
    if(!window) {
		fprintf(stderr,"no window");
		return GP_ERROR;
	}
	w = gp_widget_child_by_label(window,"Debug Level");
	if (gp_widget_changed(w)) {
		wvalue = gp_widget_value_get(w);
		if(wvalue) {
			if(strcmp(wvalue,"none") == 0)
			  cs->debug = 0;
			else if (strcmp(wvalue,"functions") == 0)
			  cs->debug = 1;
			else if (strcmp(wvalue,"complete") == 0)
			  cs->debug = 9;
			
			sprintf(buf,"%i",cs->debug);
			gp_setting_set("canon", "debug", buf);
		}
	}

	w = gp_widget_child_by_label(window,"Dump data by packets to stderr");
	if (gp_widget_changed(w)) {
		wvalue = gp_widget_value_get(w);
		if(wvalue) {
			if(atoi(wvalue) == 0)
			  cs->dump_packets = 0;
			else if (atoi(wvalue) == 1)
			  cs->dump_packets = 1;
			
			sprintf(buf,"%i",cs->dump_packets);
			gp_setting_set("canon", "dump_packets", buf);
		}
	}
	
	w = gp_widget_child_by_label(window,"Owner name");
	if (gp_widget_changed(w)) {
		wvalue = gp_widget_value_get(w);
		if(wvalue) {
			if(!check_readiness(camera)) {
				gp_frontend_status(camera,"Camera unavailable");
			} else {
				if(psa50_set_owner_name(camera, wvalue))
				  gp_frontend_status(camera, "Owner name changed");
				else
				  gp_frontend_status(camera, "could not change owner name");	
			}
		}
	}
	
	w = gp_widget_child_by_label(window,"Set camera date to PC date");
	if (gp_widget_changed(w)) {
		wvalue = gp_widget_value_get(w);
		if(wvalue) {			   
			if(!check_readiness(camera)) {
				gp_frontend_status(camera,"Camera unavailable");
			} else {
				if(psa50_set_time(camera)) {
					gp_frontend_status(camera,"time set");
				} else {
					gp_frontend_status(camera,"could not set time");
				}
			}
		}
	}

    return GP_OK;
}

#if 0
int camera_config_set(Camera *camera, CameraSetting *setting, int count)
{
	struct canon_info *cs = (struct canon_info*)camera->camlib_data;
	int i=0;
	int new_debug = 0;
	char buf[8];
	
	for (i=0;i<count;i++) {
		gp_debug_printf(GP_DEBUG_LOW,"canon","settings[%i] : %s = %s\n",i,setting[i].name,
					  setting[i].value);
		if (strcmp(setting[i].name,"Debug level") == 0) {
			if(strcmp(setting[i].value,"none") == 0)
			  new_debug = 0;
			else if (strcmp(setting[i].value,"functions") == 0)
			  new_debug = 1;
			else if (strcmp(setting[i].value,"complete") == 0)
			  new_debug = 9;
			
			if (cs->debug != new_debug) {
				cs->debug = new_debug;
				sprintf(buf,"%i",new_debug);
				gp_setting_set("canon", "debug", buf);
			}
		}
		
		if (strcmp(setting[i].name,"Dump data by packets to stderr") == 0) {
			if(atoi(setting[i].value) == 1)
			  cs->dump_packets = 1;
			else
			  cs->dump_packets = 0;
			
			sprintf(buf,"%i", cs->dump_packets);
			gp_setting_set("canon", "dump_packets", buf);
		}
		
		if (strcmp(setting[i].name,"Owner name") == 0) {
			if(strcmp(setting[i].value,cs->owner) != 0) {
				if(!check_readiness(camera)) {
					gp_frontend_status(camera,"Camera unavailable");
				} else {
					if(psa50_set_owner_name(camera, setting[i].value)) {
						gp_frontend_status(camera, "Owner name changed");
					} else {
						gp_frontend_status(camera, "could not change owner name");
					}
				}
			}
		}
		
		if (strcmp(setting[i].name,"Set camera date to PC date") == 0) {
			if(atoi(setting[i].value) == 1) {
				if(!check_readiness(camera)) {
					gp_frontend_status(camera,"Camera unavailable");
				} else {
					if(psa50_set_time(camera)) {
						gp_frontend_status(camera,"time set");
					} else {
						gp_frontend_status(camera,"could not set time");
					}
				}
			}
		}
	}
			
	gp_frontend_status(camera,"settings saved");
	
    return GP_OK;
}
#endif

int camera_capture(Camera *camera, CameraFile *file, CameraCaptureInfo *info)
{
    return GP_ERROR;
}
