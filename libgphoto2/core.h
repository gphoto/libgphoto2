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

typedef int (*c_abilities)	(CameraAbilities*);
typedef int (*c_init)		();
typedef int (*c_exit)		();
typedef int (*c_open)		();
typedef int (*c_close)		();
typedef int (*c_folder_count)	();
typedef int (*c_folder_name)	(int, char*);
typedef int (*c_folder_set)	(int);
typedef int (*c_file_count)	();
typedef int (*c_file_get)	(int, CameraFile*);
typedef int (*c_file_get_preview)(int, CameraFile*);
typedef int (*c_file_delete)	(int);
typedef int (*c_file_lock)	(int);
typedef int (*c_file_unlock)	(int);
typedef int (*c_config_set)	(char*);
typedef int (*c_capture)	(int);
typedef int (*c_summary)	(char*);
typedef int (*c_manual)		(char*);
typedef int (*c_about)		(char*);


/* Function pointers to the current library functions */
typedef struct {
	c_abilities		abilities;
	c_init			init;
	c_exit			exit;
	c_open			open;
	c_close			close;
	c_folder_count		folder_count;
	c_folder_name		folder_name;
	c_folder_set		folder_set;
	c_file_count		file_count;
	c_file_get		file_get;
	c_file_get_preview	file_get_preview;
	c_file_delete		file_delete;
	c_file_lock		file_lock;
	c_file_unlock		file_unlock;
	c_config_set		config_set;
	c_capture		capture;
	c_summary		summary;
	c_manual		manual;
	c_about			about;
} Camera;
