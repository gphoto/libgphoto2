/* Data types
---------------------------------------------------------------- */
typedef struct {
	  /* The name of the camera model */
	  char name[128];
	  /* The filename of the camera library. */
	  char library[128];
} CameraChoice;

typedef struct {
	/* key = value settings */
	char key[256];
	char value[256];
} Setting;
