/* Make it easy to add command-line options */

#define OPTION_CALLBACK(_a)		void _a (char *arg)

typedef struct {
	char	short_id;
	char	long_id[20];
	char	argument[16];
	char	description[35];
	OPTION_CALLBACK((*execute));
} Option;
