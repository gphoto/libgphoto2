/* Data types
---------------------------------------------------------------- */
typedef struct {
	  /* The name of the camera ("Olympus D-220L") */
	  char name[128];
	  /* The filename of the camera library. */
	  char library[128];
} CameraChoice;

typedef struct {
	/* key = value settings */
	char key[256];
	char value[256];
} Setting;

typedef int (*c_id)		(char *);
typedef int (*c_abilities)	(CameraAbilities*,int*);
typedef int (*c_init)		();
typedef int (*c_exit)		();
typedef int (*c_folder_list)	(char*,CameraFolderInfo*);
typedef int (*c_folder_set)	(char*);
typedef int (*c_file_count)	();
typedef int (*c_file_get)	(int, CameraFile*);
typedef int (*c_file_get_preview)(int, CameraFile*);
typedef int (*c_file_put)	(CameraFile*);
typedef int (*c_file_delete)	(int);
typedef int (*c_file_lock)	(int);
typedef int (*c_file_unlock)	(int);
typedef int (*c_config_get)	(CameraWidget*);
typedef int (*c_config_set)	(CameraSetting*, int);
typedef int (*c_capture)	(CameraFile*, CameraCaptureInfo *);
typedef int (*c_summary)	(char*);
typedef int (*c_manual)		(char*);
typedef int (*c_about)		(char*);


/* Function pointers to the current library functions */
typedef struct {
	c_id			id;
	c_abilities		abilities;
	c_init			init;
	c_exit			exit;
	c_folder_list		folder_list;
	c_folder_set		folder_set;
	c_file_count		file_count;
	c_file_get		file_get;
	c_file_get_preview	file_get_preview;
	c_file_put		file_put;
	c_file_delete		file_delete;
	c_file_lock		file_lock;
	c_file_unlock		file_unlock;
	c_config_get		config_get;
	c_config_set		config_set;
	c_capture		capture;
	c_summary		summary;
	c_manual		manual;
	c_about			about;
} Camera;
