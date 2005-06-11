
/* Windows Portability
   ------------------------------------------------------------------ */

#ifdef WIN32

#include <windows.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <stdio.h>
#include <direct.h>

#ifdef IOLIBS
#undef IOLIBS
#endif
#define IOLIBS			"."
#define strcasecmp		_stricmp
#define snprintf		_snprintf

/* Work-around for readdir() */
typedef struct {
	HANDLE handle;
	int got_first;
	WIN32_FIND_DATA search;
	char dir[1024];
	char drive[32][2];
	int  drive_count;
	int  drive_index;
} GPPORTWINDIR;

/* Sleep functionality */
#define	GP_SYSTEM_SLEEP(_ms)		Sleep(_ms)

/* Directory-oriented functions */
#define gp_system_dir		        GPPORTWINDIR *
#define gp_system_dirent		WIN32_FIND_DATA *
#define gp_system_dir_delim		'\\'


#else

/* POSIX Portability
   ------------------------------------------------------------------ */

/* yummy. :) */

#include <sys/types.h>
#include <dirent.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <unistd.h>

/* Sleep functionality */
#define	GP_SYSTEM_SLEEP(_ms)		usleep((_ms)*1000)

/* Directory-oriented functions */
#define gp_system_dir                   DIR *
#define gp_system_dirent		struct dirent *
#ifdef OS2
#define gp_system_dir_delim		'\\'
#else
#define gp_system_dir_delim		'/'
#endif /* OS2 */

#endif /* else */

int		 gp_system_mkdir	(const char *dirname);
int              gp_system_rmdir        (const char *dirname);
gp_system_dir	 gp_system_opendir	(const char *dirname);
gp_system_dirent gp_system_readdir	(gp_system_dir d);
const char*	 gp_system_filename	(gp_system_dirent de);
int		 gp_system_closedir	(gp_system_dir dir);
int		 gp_system_is_file	(const char *filename);
int		 gp_system_is_dir	(const char *dirname);

/* end of file */
