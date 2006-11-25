#ifndef commands_H
#define commands_H
 

int32_t soundvision_send_command(uint32_t command, uint32_t argument,
	                                 CameraPrivateLibrary *dev);

int32_t soundvision_send_file_command(const char *filename,
                                              CameraPrivateLibrary *dev);
    
int32_t soundvision_read(CameraPrivateLibrary *dev, void *buffer, int len);

#endif
