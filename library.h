#ifdef WIN32
#define GP_DLOPEN(_handle, _filename)
#define GP_DLSYM(_pointer, _handle, _funcname)
#define GP_DLCLOSE(_handle)
#define GP_DLERROR()
#else
#include <dirent.h>
#include <dlfcn.h>
#define GP_DLOPEN(_handle, _filename)		(_handle = dlopen(_filename, RTLD_LAZY))
#define GP_DLSYM(_pointer, _handle, _funcname)	(_pointer = dlsym(_handle, _funcname))
#define GP_DLCLOSE(_handle)			(dlclose(_handle))
#define GP_DLERROR()				(dlerror())
#endif

int load_cameras();
int is_library (char *library_filename);
int load_library (Camera *camera, char *camera_name);
int close_library (Camera *camera);
