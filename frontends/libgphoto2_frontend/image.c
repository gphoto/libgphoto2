#include <gphoto2.h>
#include <gphoto2-frontend.h>

int gp_file_image_rotate (CameraFile *old_file, CameraFile *new_file,
			  int degrees) {

	return(GP_OK);
}

int gp_file_image_scale  (CameraFile *old_file, CameraFile *new_file,
			  int width, int height) {

	return(GP_OK);
}

int gp_file_image_flip_h (CameraFile *old_file, CameraFile *new_file) {

	return(GP_OK);
}

int gp_file_image_flip_v (CameraFile *old_file, CameraFile *new_file) {

	return(GP_OK);
}
