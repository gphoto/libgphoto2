#include <string.h>
#include <gphoto2.h>
#include <gpio.h>

int
camera_id (CameraText *id) 
{
	strcpy(id->text, "REPLACE WITH UNIQUE LIBRARY ID");

	return (GP_OK);
}

int
camera_abilities (CameraAbilitiesList *list) 
{
	CameraAbilities *a;

	a = gp_abilities_new();

	strcpy(a->model, "CAMERA MODEL");
	a->port     = GP_PORT_SERIAL | GP_PORT_USB;
	a->speed[0] = 0;
	a->operations        = 	GP_OPERATION_CAPTURE_PREVIEW | 
				GP_CAPTURE_VIDEO | GP_CAPTURE_IMAGE;
	a->file_operations   = 	GP_FILE_OPERATION_DELETE | 
				GP_FILE_OPERATION_PREVIEW;
	a->folder_operations = 	GP_FOLDER_OPERATION_NONE;

	gp_abilities_list_append(list, a);

	return (GP_OK);
}

static int
camera_exit (Camera *camera) 
{
	return (GP_OK);
}

static int
camera_file_get (Camera *camera, const char *folder, const char *filename, 
		 CameraFile *file)
{ 
	return (GP_OK);
}

int camera_file_get_preview (Camera *camera, const char *folder, 
			     const char *filename, CameraFile *file) 
{
	return (GP_OK);
}

static int
camera_folder_put_file (Camera *camera, const char *folder, CameraFile *file)
{
	return (GP_OK);
}

static int
camera_file_delete (Camera *camera, const char *folder, const char *filename) 
{
	return (GP_OK);
}

static int
camera_config_get (Camera *camera, CameraWidget **window) 
{
	*window = gp_widget_new (GP_WIDGET_WINDOW, "Camera Configuration");

	// Append your sections and widgets here.

	return (GP_OK);
}

static int
camera_config_set (Camera *camera, CameraWidget *window) 
{
	// Check, if the widget's value have changed.

	return (GP_OK);
}

static int
camera_capture (Camera *camera, int capture_type, CameraFile *file)
{
	// Capture image/preview/video and return it in 'file'. Don't store it
	// anywhere on your camera! If your camera does store the image, 
	// delete it manually here.

	return (GP_OK);
}

static int
camera_summary (Camera *camera, CameraText *summary) 
{
	return (GP_OK);
}

static int
camera_manual (Camera *camera, CameraText *manual) 
{
	return (GP_OK);
}

static int
camera_about (Camera *camera, CameraText *about) 
{
	strcpy(about->text, 
"Library Name
YOUR NAME <email@somewhere.com>
Quick description of the library.
No more than 5 lines if possible.");

	return (GP_OK);
}

static int
get_info_func (CameraFilesystem *fs, const char *folder, const char *filename,
	       CameraFileInfo *info, void *data)
{
	Camera *camera = data;

	/* Get the file info here and write it into <info> */

	return (GP_OK);
}

static int
folder_list_func (CameraFilesystem *fs, const char *folder, CameraList *list,
		  void *data)
{
	Camera *camera = data;

	/* List your folders here */

	return (GP_OK);
}

static int
file_list_func (CameraFilesystem *fs, const char *folder, CameraList *list,
		void *data)
{
	Camera *camera = data;

	/* List your files here */

	return (GP_OK);
}

static char *
camera_result_as_string (Camera *camera, int result) 
{
	if (result >= 0) return ("This is not an error...");
	if (-result < 100) return gp_result_as_string (result);
	return ("This is a template specific error.");
}

int
camera_init (Camera *camera) 
{
        /* First, set up all the function pointers */
        camera->functions->exit                 = camera_exit;
        camera->functions->file_get             = camera_file_get;
        camera->functions->file_get_preview     =  camera_file_get_preview;
        camera->functions->file_delete          = camera_file_delete;
        camera->functions->config_get           = camera_config_get;
        camera->functions->config_set           = camera_config_set;
        camera->functions->capture              = camera_capture;
        camera->functions->summary              = camera_summary;
        camera->functions->manual               = camera_manual;
        camera->functions->about                = camera_about;
        camera->functions->result_as_string     = camera_result_as_string;

	/* Now, tell the filesystem where to get lists and info */
	gp_filesystem_set_list_funcs (camera->fs, file_list_func,
				      folder_list_func, camera);
	gp_filesystem_set_info_funcs (camera->fs, get_info_func, set_info_func,
				      camera);

	/* Open the port and check if the camera is there */
	gp_port_open (camera->port);
	// Do something here.
	gp_port_close (camera->port);

	return (GP_OK);
}
