#include <string.h>
#include <gphoto2.h>

/*#include <gphoto2-port.h>*/

#include "sonydscf1.h"

/*gpio_device *dev=NULL;
gpio_device_settings settings;*/

gp_port *dev;

extern int glob_debug;
char glob_camera_model[64];
static int all_pic_num = -1;

int camera_id (CameraText *id) {
        printf("->camera id");
        strcpy(id->text, "sonydscf1-bvl");

        return (GP_OK);
}

int camera_debug_set (int onoff) {
        printf("->camera debug");
        if(onoff)
          printf("Setting debugging to on\n");
        glob_debug=onoff;
        return (GP_OK);
}

int camera_abilities (CameraAbilitiesList *list) {

        //*count = 1;
        CameraAbilities *a;
        // printf("->camera_abilities\n");
        a = gp_abilities_new();
        /* Fill in each camera model's abilities */
        /* Make separate entries for each conneciton type (usb, serial, etc...)
           if a camera supported multiple ways. */
        strcpy(a->model, "Sony DSC-F1");
        a->port=GP_PORT_SERIAL;
        a->speed[0] = 0;
        a->capture[0].type = GP_CAPTURE_NONE;
        a->config   = 1;
        a->file_operations = GP_FILE_OPERATION_DELETE | GP_FILE_OPERATION_PREVIEW;
        a->folder_operations = GP_FOLDER_OPERATION_PUT_FILE;
        gp_abilities_list_append(list, a);
        // printf("<-camera_abilities\n");
        return (GP_OK);
}

int camera_init (Camera *camera) {
        gp_port_settings settings;
        SonyStruct *b;
        int ret;
        printf("->camera init\n");

        if(glob_debug)
        {
         printf("sony dscf1: Initializing the camera\n");
         printf("port: %s\n",camera->port->path);
        }

        camera->functions->id           = camera_id;
        camera->functions->abilities    = camera_abilities;
        camera->functions->init         = camera_init;
        camera->functions->exit         = camera_exit;
        camera->functions->folder_list  = camera_folder_list;
//	camera->functions->folder_set   = camera_folder_set;
//	camera->functions->file_count   = camera_file_count;
        camera->functions->file_get     = camera_file_get;
        camera->functions->file_get_preview =  camera_file_get_preview;
        camera->functions->folder_put_file = camera_folder_put_file;
        camera->functions->file_list    = camera_file_list;
        camera->functions->file_delete  = camera_file_delete;
        camera->functions->config_get   = camera_config_get;
        camera->functions->config_set   = camera_config_set;
        camera->functions->summary      = camera_summary;
        camera->functions->manual       = camera_manual;
        camera->functions->about        = camera_about;

        b = (SonyStruct*)malloc(sizeof(SonyStruct));
        camera->camlib_data = b;

        if ((ret = gp_port_new(&(b->dev), GP_PORT_SERIAL)) < 0) {
            return (ret);
        }

        /*
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
        strcpy(settings.serial.port, camera->port->path);
        printf("5\n");
        settings.serial.speed   = 38400;
        settings.serial.bits    = 8;
        settings.serial.parity  = 0;
        settings.serial.stopbits= 1;

        gpio_set_settings(dev, settings);

        gpio_open(dev);
        printf("6\n");
        strcpy(glob_camera_model, camera->model);
	*/

        gp_port_timeout_set(b->dev, 5000);
        strcpy(settings.serial.port, camera->port->path);

        settings.serial.speed   = 38400;
        settings.serial.bits    = 8;
        settings.serial.parity  = 0;
        settings.serial.stopbits= 1;

        gp_port_settings_set(b->dev, settings);
        gp_port_open(b->dev);

        /* Create the filesystem */
        b->fs = gp_filesystem_new();
        dev = b->dev;
        printf("<-camera init\n");
        return (GP_OK);
}

int camera_exit (Camera *camera) {
        printf("->camera exit");
        if(F1ok())
           return(GP_ERROR);
        printf("<-camera exit");
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
        printf("sony dscf1: file count\n");
        if(!F1ok())
           return (GP_ERROR);

        return (F1howmany());
}

int camera_file_get (Camera *camera, CameraFile *file, char *folder, char *filename) {

        /**********************************/
        /* file_number now starts at 0!!! */
        /**********************************/
        /*printf("sony dscf1: file get\n");
        if(!F1ok())
           return (GP_ERROR);
        file->size = get_picture(file_number,&file->data,JPEG,0,camera_file_count(camera));
        sprintf(file->name,"PSN%05d.jpg",file_number);
        return (GP_OK);*/

        int size, num;
        SonyStruct *b = (SonyStruct*)camera->camlib_data;
        printf("->dscf1 camera_file_get\n");
        /*gp_frontend_progress(camera, NULL, 0.00);*/
        if(!F1ok())
           return (GP_ERROR);

        strcpy(file->name, filename);
        strcpy(file->type, "image/jpg");

        /* Retrieve the number of the photo on the camera */
        num = gp_filesystem_number(b->fs, "/", filename);

        /*file->data = barbie_read_picture(b, num, 0, &size);*/
        size=get_picture(num,&file->data,JPEG,0,camera_file_count(camera));
        if (!file->data)
                return GP_ERROR;
        file->size = size;
        printf("<-dscf1 camera_file_get\n");
        return GP_OK;

}


int camera_file_get_preview (Camera *camera, CameraFile *preview, char *folder, char *filename) {

        /**********************************/
        /* file_number now starts at 0!!! */
        /**********************************/
        printf("sony dscf1: file get preview\n");
        if(!F1ok())
           return (GP_ERROR);
        /*preview->size = get_picture(file_number,&preview->data,JPEG_T,0,camera_file_count(camera));
        sprintf(preview->name,"PIDX%04d.jpg",file_number);*/
        return (GP_OK);
}

int camera_folder_file_put (Camera *camera, CameraFile *file,char *folder) {
                printf("sony dscf1: file put\n");
        return (GP_ERROR);
}

int camera_file_delete (Camera *camera,char *folder , char *filename) {
        printf("sony dscf1: file delete\n");
        if(!F1ok())
           return (GP_ERROR);
        /*return (F1deletepicture(file_number));*/
}

int camera_config_get (Camera *camera, CameraWidget **window) {
        printf("sony dscf1: config get\n");
        return GP_ERROR;
}

int camera_config_set (Camera *camera, CameraWidget *window) {
                printf("sony dscf1: config set\n");
        return (GP_ERROR);
}

int camera_summary (Camera *camera, CameraText *summary) {
        printf("->camera summary");
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

int camera_file_list (Camera *camera, CameraList *list, char *folder) {

        int count, x;
        SonyStruct *b = (SonyStruct*)camera->camlib_data;
        F1ok();
        /*if(F1ok())
           return(GP_ERROR);*/
        count = F1howmany();
        /* Populate the filesystem */
        gp_filesystem_populate(b->fs, "/", "dscf1%03i.jpg", count);

        for (x=0; x<gp_filesystem_count(b->fs, folder); x++)
                gp_list_append(list, gp_filesystem_name(b->fs, folder, x), GP_LIST_FILE);

        return GP_OK;
}
