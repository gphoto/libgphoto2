typedef int (*shell_function_call)  (char *arg);
 
typedef struct {
	char			command[16];
	shell_function_call	function;
	char			description[64];
	char			description_arg[64];
} shell_function;

int shell_prompt (void);
int shell_cd (char *arg);
int shell_lcd (char *arg);
int shell_ls (char *arg);
int shell_get (char *arg);
int shell_get_thumbnail (char *arg);
int shell_exit (char *arg);
int shell_help (char *arg);
