#include <gphoto2.h>

/* Make it easy to add command-line options */

#define OPTION_CALLBACK(_a)		int _a (char *arg)
#define	SHORT_OPTION			"-"
#define LONG_OPTION			"--"
#define MAX_IMAGE_NUMBER                1024

#ifdef WIN32
#include <io.h>
#define VERSION "2"
#endif

typedef struct {
	char	short_id[3];
	char	long_id[20];
	char	argument[32];
	char	description[35];
	OPTION_CALLBACK((*execute));
	int	required;
} Option;

void 	cli_debug_print(char *format, ...);
void 	cli_error_print(char *format, ...);
int	save_picture_to_file(char *folder, char *filename, int thumbnail);
int 	get_picture_common(char *arg, int thumbnail);
int 	capture_generic (int type, char *capture_choice);
