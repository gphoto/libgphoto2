#include <sys/types.h>

#define JPEG 0
#define JPEG_T 1
#define PMP 2
#define PMX 3

typedef struct {
        gp_port *dev;
        gp_port_settings settings;
        CameraFilesystem *fs;
} SonyStruct;
