#include <string.h>
#include <gphoto2.h>

#include <gpio.h>

#include "sonydscf1.h"

gpio_device *dev=NULL;
gpio_device_settings settings;

extern int glob_debug;
char glob_camera_model[64];
static int all_pic_num = -1;

int camera_id (CameraText *id) {

        strcpy(id->text, "sonydscf1-bvl");

        return (GP_OK);
}

int camera_debug_set (int onoff) {

        if(onoff)
          printf("Setting debugging to on\n");
        glob_debug=onoff;
        return (GP_OK);
}

int camera_abilities (CameraAbilitiesList *list) {

        //*count = 1;
        CameraAbilities *a;

        a = gp_abilities_new();
        /* Fill in each camera model's abilities */
        /* Make separate entries for each conneciton type (usb, serial, etc...)
           if a camera supported multiple ways. */
        strcpy(a->model, "Sony DSC-F1");
        a->port=GP_PORT_SERIAL;
        a->speed[0] = 0;
        a->capture  = GP_CAPTURE_NONE;
        a->config   = 1;
        a->file_delete  = 1;
        a->file_preview = 1;
        a->file_put = 1;

        printf("bla1");
        gp_abilities_list_append(list, a);
        printf("bla2");
        return (GP_OK);
}

int camera_init (Camera *camera, CameraInit *init) {

        /*if(glob_debug)
        {*/
         printf("sony dscf1: Initializing the camera\n");
         printf("port: %s\n",init->port_settings.path);
        /*}*/

        camera->functions->id           = camera_id;
        camera->functions->abilities    = camera_abilities;
        camera->functions->init         = camera_init;
        camera->functions->exit         = camera_exit;
        camera->functions->folder_list  = camera_folder_list;
        //camera->functions->folder_set   = camera_folder_set;
        //camera->functions->file_count   = camera_file_count;
        camera->functions->file_get     = camera_file_get;
        camera->functions->file_get_preview =  camera_file_get_preview;
        camera->functions->file_put     = camera_file_put;
        camera->functions->file_delete  = camera_file_delete;
        camera->functions->config_get   = camera_config_get;
        camera->functions->config_set   = camera_config_set;
        camera->functions->capture      = camera_capture;
        camera->functions->summary      = camera_summary;
        camera->functions->manual       = camera_manual;
        camera->functions->about        = camera_about;
        printf("1\n");
        if (dev) {
                gpio_close(dev);
                gpio_free(dev);
        }
        printf("2\n");
        dev = gpio_new(GPIO_DEVICE_SERIAL);
        printf("3\n");
        gpio_set_timeout(dev, 5000);
        printf("4\n");
        strcpy(settings.serial.port, init->port_settings.path);
        printf("5\n");
        settings.serial.speed   = 38400;
        settings.serial.bits    = 8;
        settings.serial.parity  = 0;
        settings.serial.stopbits= 1;

        gpio_set_settings(dev, settings);

        gpio_open(dev);
        printf("6\n");
        strcpy(glob_camera_model, init->model);
        return (GP_OK);
}

int camera_exit (Camera *camera) {
        if(F1ok())
           return(GP_ERROR);
        return (F1fclose());
}

int camera_folder_list(Camera *camera, CameraList *list, char *folder_name) {
                printf("sony dscf1: folderlist\n");
        return (GP_OK);
}

int camera_folder_set(Camera *camera, char *folder_name) {
/*                printf("sony dscf1: folder set\n");*/
        printf("folder: %s\n",folder_name);
        return (GP_OK);
}

int camera_file_count (Camera *camera) {
        //printf("sony dscf1: file count\n");
        if(!F1ok())
           return (GP_ERROR);

        return (F1howmany());
}

int camera_file_get (Camera *camera, CameraFile *file, char *folder, char *filename) {

        /**********************************/
        /* file_number now starts at 0!!! */
        /**********************************/
        printf("sony dscf1: file get\n");
        if(!F1ok())
           return (GP_ERROR);
        /*file->size = get_picture(file_number,&file->data,JPEG,0,camera_file_count(camera));
        sprintf(file->name,"PSN%05d.jpg",file_number);*/
        return (GP_OK);
}

int camera_file_get_preview (Camera *camera, CameraFile *preview, char *folder, char *filename) {

        /**********************************/
        /* file_number now starts at 0!!! */
        /**********************************/
        //printf("sony dscf1: file get preview\n");
        if(!F1ok())
           return (GP_ERROR);
        /*preview->size = get_picture(file_number,&preview->data,JPEG_T,0,camera_file_count(camera));
        sprintf(preview->name,"PIDX%04d.jpg",file_number);*/
        return (GP_OK);
}

int camera_file_put (Camera *camera, CameraFile *file,char *folder) {
                printf("sony dscf1: file put\n");
        return (GP_ERROR);
}

int camera_file_delete (Camera *camera,char *folder , char *filename) {
        //printf("sony dscf1: file delete\n");
        if(!F1ok())
           return (GP_ERROR);
        /*return (F1deletepicture(file_number));*/
}

int camera_config_get (Camera *camera, CameraWidget *window) {
        printf("sony dscf1: config get\n");
        return GP_ERROR;
}

int camera_config_set (Camera *camera, CameraSetting *setting, int count) {
                printf("sony dscf1: config set\n");
        return (GP_ERROR);
}

int camera_capture (Camera *camera, CameraFile *file, CameraCaptureInfo *info) {
        printf("sony dscf1: camera does not support capture !!\n");
        return (GP_ERROR);
}

int camera_summary (Camera *camera, CameraText *summary) {
        if(!F1ok())
           return (GP_ERROR);
        return (F1newstatus(1, summary->text));
}

int camera_manual (Camera *camera, CameraText *manual) {
                printf("sony dscf1: manual\n");
        strcpy(manual->text, "Manual Not Available");

        return (GP_OK);
}

int camera_about (Camera *camera, CameraText *about) {

        strcpy(about->text,
"Sony DSC-F1 Digital Camera Support\n
        M. Adam Kendall <joker@penguinpub.com>\n
        Based on the chotplay CLI interface from\n
        Ken-ichi Hayashi\n
        Gphoto2 port by
        Bart van Leeuwen <bart@netage.nl>");

        return (GP_OK);
}

