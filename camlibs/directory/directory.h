typedef struct {
	int  debug;
	char directory[1024];
	char images[1024][1024];
	int  num_images;
	int  get_index;
	CameraFilesystem *fs;
} DirectoryStruct;
