struct agfa_device {
	gp_port *gpdev;
	
	int num_pictures;
	char *file_list;
	
	/* These parameters are only significant for serial support */
	int portspeed;
	
	int deviceframesize;
};


int agfa_reset          (struct agfa_device *dev);
int agfa_get_status     (struct agfa_device *dev, int *taken,
		         int *available, int *rawcount);
int agfa_photos_taken   (struct agfa_device *dev,int *taken);
int agfa_get_file_list  (struct agfa_device *dev);
int agfa_delete_picture (struct agfa_device *dev, const char *filename);

int agfa_get_thumb_size (struct agfa_device *dev, const char *filename);
int agfa_get_thumb      (struct agfa_device *dev, const char *filename,
			 unsigned char *data,int size);

int agfa_get_pic_size   (struct agfa_device *dev, const char *filename);
int agfa_get_pic        (struct agfa_device *dev, const char *filename,
		         unsigned char *data,int size);

int agfa_capture        (struct agfa_device *dev, CameraFilePath *path);
