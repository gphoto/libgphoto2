#include <config.h>
#include <gphoto2.h>

/* Make it easy to add command-line options */

#define OPTION_CALLBACK(_a)		int _a (char *arg)
#define	SHORT_OPTION			"-"
#define LONG_OPTION			"--"
#define MAX_IMAGE_NUMBER                1024

#define GP_USB_HOTPLUG_SCRIPT "usbcam"
#define GP_USB_HOTPLUG_MATCH_VENDOR_ID  0x0001
#define GP_USB_HOTPLUG_MATCH_PRODUCT_ID 0x0002

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

#ifndef DISABLE_DEBUGGING
void 	cli_debug_print(char *format, ...);
#else

#ifdef __GNUC__
#define cli_debug_print(format, args...) /**/
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
#define cli_debug_print(...) /**/
#else
#define cli_debug_print (void)
#endif

#endif
void 	cli_error_print(char *format, ...);

int	save_file_to_file (const char *folder, const char *filename,
			      CameraFileType type);

int 	get_file_common (char *arg, CameraFileType type);

/*
 * Local Variables:
 * c-file-style:"linux"
 * indent-tabs-mode:t
 * End:
 */
