/* Make it easy to add command-line options */

#define OPTION_CALLBACK(_a)		int _a (char *arg)
#define	SHORT_OPTION			"-"
#define LONG_OPTION			"--"

typedef struct {
	char	short_id[3];
	char	long_id[20];
	char	argument[16];
	char	description[35];
	OPTION_CALLBACK((*execute));
	int	required;
} Option;
