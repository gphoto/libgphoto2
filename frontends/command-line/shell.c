#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "main.h"
#include "globals.h"
#include "shell.h"

#define CHECK(result) {int r=(result);if(r<0) return(r);}
#define CHECK_CONT(result) {int r=(result);if(r<0){printf ("*** Error ('%s') ***\n", gp_result_as_string (result));}}

/* Valid shell commands */
shell_function func[] = {
{"cd", 			shell_cd,		"Change to a directory on the camera",			"directory"},
{"lcd", 		shell_lcd,		"Change to a directory on the local drive",		"directory"},
{"exit", 		shell_exit,		"Exit the gPhoto shell",				""},
{"get",			shell_get,		"Download the file to the current directory",		"[directory/]filename"},
{"get-thumbnail",	shell_get_thumbnail,	"Download the thumbnail to the current directory",	"[directory/]filename"},
{"get-raw",		shell_get_raw,		"Download raw data to the current directory",		"[directory/]filename"},
{"help",		shell_help,		"Displays command usage",				"command"},
{"ls", 			shell_ls,		"List the contents of the current directory",		"[directory/]"},
{"q", 			shell_exit,		"Exit the gPhoto shell",				""},
{"quit", 		shell_exit,		"Exit the gPhoto shell",				""},
{"?",			shell_help,		"Displays command usage",				""},
{"", NULL}
};

/* Local globals */
char*	shell_prompt_text	= "gphoto: {%s} %s> ";
int 	shell_done		= 0;

static int
shell_arg_count (char *args)
{
	int x=0, in_arg=0, count=0;
	
	while (x < strlen(args)) {				/* Edge-triggered */
		if ((!isspace(args[x])) && (!in_arg)) {
			in_arg = 1;
			count++;
		}
		if ((isspace(args[x])) && (in_arg))
			in_arg = 0;
		x++;
	}

	return (count);
}

static int
shell_arg (char *args, int arg_num, char *arg)
{
	int x=0, y=0, in_arg=0, count=0, copy=0;

	if (arg_num > shell_arg_count(args)-1)
		return (GP_ERROR);

	while (x < strlen(args)) {				/* Edge-triggered */
		if ((!isspace(args[x])) && (!in_arg)) {
			in_arg = 1;
			if (count == arg_num)
				copy = 1;
			count++;
		}
		if ((isspace(args[x])) && (in_arg)) {
			copy = 0;
			in_arg = 0;
		}

		if (copy)					/* Copy over the chars */
			arg[y++] = args[x];
		x++;
	}

	arg[y] = 0;						/* null-terminate the arg */

	return (GP_OK);
}

int
shell_prompt (void)
{
	int x;
	char buf[1024], cmd[1024], arg[1024];

	while (!shell_done) {
		if (glob_quiet) {
			printf(">\n");
		} else {
			if (strlen(glob_cwd) > 25) {
				strcpy(buf, "...");
				strcat(buf, &glob_cwd[strlen(glob_cwd)-22]);
			} else {
				strcpy(buf, glob_cwd);
			}
			printf(shell_prompt_text, buf, glob_folder); /* Display prompt text */
		}
		fflush(stdout);
		fgets(buf, 1023, stdin);		/* Get the command/args */
		buf[strlen(buf)-1]=0;			/* Chop the newline */

							/* Extract command and argument */

		/* If we don't have any command, start from the beginning */
		if (shell_arg_count (buf) <= 0)
			continue;
		
		shell_arg (buf, 0, cmd);
		strcpy(arg, &buf[strlen(cmd)]);
		for (x = 0; func[x].function; x++) {
			if (!strcmp (cmd, func[x].command)) {
				CHECK_CONT (func[x].function (arg));
				break;
			}
		}
		if (!func[x].function)
			cli_error_print ("Invalid command");
	}

	return (GP_OK);
}

static int
shell_get_new_folder (char *relative_path, char *cd_arg, char *new_folder)
{
	/*
	   Find what the new folder will be based on the "cd" or "ls" argument.
	   This can take paths like:
		"/blah", "./blah", "../blah", "/blah/hey/../doh/blah", etc...
	*/

	char *dir = NULL, *slash = NULL;
	char tmp_folder[1024];
	int append = 0, x=0;

	while (isspace(cd_arg[x])) {x++;}

	if (cd_arg[x] == '/')				/* Check if path is relative to root */
		strcpy(tmp_folder, "/");
	   else
		strcpy(tmp_folder, relative_path);
	do {
		append = 1;
		if (!dir)				/* Get each directory individually */
			dir = strtok(&cd_arg[x], "/");
		   else
			dir = strtok(NULL, "/");
		if (!dir) 
			break;

		if (strcmp(dir, ".")==0)		/* Do nothing for CWD */
			append = 0;

		if (strcmp(dir, "..")==0) {		/* Move up a directory */
			if (strcmp(tmp_folder, "/")!=0) {
				slash = strrchr(tmp_folder, '/');
				*slash = 0;
				if (strlen(tmp_folder)==0)
					strcpy(tmp_folder, "/");
			}
			append = 0;
		}

		if (append) {				/* Append the dir name */
			if (strcmp(tmp_folder, "/")!=0)
				strcat(tmp_folder, "/");
			strcat(tmp_folder, dir);
		}

	} while (1);

	strcpy(new_folder, tmp_folder);

	return (GP_OK);
}

int
shell_lcd (char *arg)
{
	char tmp_dir[1024];
	int arg_count = shell_arg_count(arg);

	if (!arg_count) {
		if (getenv("HOME")==NULL) {
			cli_error_print("Could not find home directory");
			return (GP_OK);
		}
		strcpy(tmp_dir, getenv("HOME"));
	} else {
		/* shell_arg(arg, 0, arg_dir); */
		shell_get_new_folder(glob_cwd, arg, tmp_dir);
	}

	if (chdir(tmp_dir)<0) {
		cli_error_print("Could not change to local directory %s", tmp_dir);
	} else {
		printf("Local directory now %s\n", tmp_dir);
		strcpy(glob_cwd, tmp_dir);
	}

	return (GP_OK);
}

int
shell_cd (char *arg)
{
	char tmp_folder[1024];
	CameraList list;
	int arg_count = shell_arg_count (arg);
	int res;

	if (!arg_count)
		return (GP_OK);

	/* shell_arg(arg, 0, arg_dir); */
	
	if (strlen (arg) > 1023) {
		cli_error_print ("Folder value is too long");
		return (GP_ERROR);
	}

	/* Get the new folder value */
	shell_get_new_folder (glob_folder, arg, tmp_folder);
	printf("tmp_folder=%s\n", tmp_folder);

	res = gp_camera_folder_list_folders (glob_camera, tmp_folder, &list, glob_context);
	if (res == GP_OK)
		strcpy (glob_folder, tmp_folder);
	else
		cli_error_print ("Folder %s does not exist", tmp_folder);

	return (GP_OK);
}

int
shell_ls (char *arg)
{
	CameraList list;
	char buf[1024], tmp_folder[1024];
	int x, y=1, res;
	int arg_count = shell_arg_count(arg);
	const char *name;

	if (arg_count) {
		/* shell_arg(arg, 0, arg_dir); */
		shell_get_new_folder (glob_folder, arg, tmp_folder);
	} else {
		strcpy (tmp_folder, glob_folder);
	}

	res = gp_camera_folder_list_folders (glob_camera, tmp_folder, &list, glob_context);
	if (res != GP_OK) {
		if (arg_count)
			cli_error_print("Folder %s does not exist", tmp_folder);
		else
			cli_error_print("Could not retrieve the folder list");
		return (res);
	}

	if (glob_quiet)
		printf ("%i\n", gp_list_count(&list));

	for (x = 1; x <= gp_list_count (&list); x++) {
		gp_list_get_name (&list, x - 1, &name);
		if (glob_quiet)
			printf ("%s\n", name);
		else {
			sprintf (buf, "%s/", name);
			printf ("%-20s", buf);
			if (y++ % 4 == 0)
				printf("\n");
		}
	}

	res = gp_camera_folder_list_files (glob_camera, tmp_folder, &list, glob_context);
	if (res != GP_OK)
		return (res);

	if (glob_quiet)
		printf("%i\n", gp_list_count(&list));

	for (x = 1; x <= gp_list_count (&list); x++) {
		gp_list_get_name (&list, x - 1, &name);
		if (glob_quiet)
			printf ("%s\n", name);
		   else {
			printf ("%-20s", name);
			if (y++ % 4 == 0)
				printf("\n");
		}
	}
	if ((!glob_quiet) && (y % 4 != 1))
		printf("\n");

	return (GP_OK);
}

static int
shell_get_common (char *args, CameraFileType type)
{
	char tmp_folder[1024], arg[1024];
	char *slash, *tmp_filename;
	int x, arg_count = shell_arg_count (args);

	if (!arg_count) {
		cli_error_print("No filename specified");
		return (GP_ERROR);
	}

	for (x = 0; x < arg_count; x++) {

		shell_arg (args, x, arg);

		if (strchr (arg, '/')) {
			slash = strrchr (arg, '/');
			tmp_filename = slash + 1;
			*slash = 0;
			if (strlen (arg)==0)
				strcpy (tmp_folder, "/");
			   else
				strcpy (tmp_folder, arg);
		} else {
			tmp_filename = arg;
			strcpy (tmp_folder, glob_folder);
		}

		CHECK (save_file_to_file (tmp_folder, tmp_filename, type));
	}

	return (GP_OK);		
}

int
shell_get_thumbnail (char *arg)
{
	CHECK (shell_get_common (arg, GP_FILE_TYPE_PREVIEW));

	return (GP_OK);
}

int
shell_get (char *arg)
{

	CHECK (shell_get_common (arg, GP_FILE_TYPE_NORMAL));

	return (GP_OK);
}

int
shell_get_raw (char *arg)
{
	CHECK (shell_get_common (arg, GP_FILE_TYPE_RAW));

	return (GP_OK);
}

int
shell_exit (char *arg)
{
	shell_done = 1;
	return (GP_OK);
}

int
shell_help (char *arg)
{
	char arg_cmd[1024];
	int x=0;
	int arg_count = shell_arg_count (arg);

	if (arg_count > 0) {
		shell_arg(arg, 0, arg_cmd);
		while (func[x].function) {
			if (strcmp (arg_cmd, func[x].command) == 0) {
				printf ("Help on \"%s\":\n\n", arg_cmd);
				printf ("Usage: %s %s\n", arg_cmd, 
					func[x].description_arg);
				printf ("Description: \n\t%s\n\n", 
					func[x].description);
				printf ("* Arguments in brackets [] are "
					"optional\n");
				return (GP_OK);
			}
			x++;
		}
		cli_error_print ("Command not found in 'help'");
		return (GP_OK);
	}

	printf ("Available commands:\n");

	while (func[x].function) {
		printf ("\t%-16s%s\n", func[x].command, func[x].description);
		x++;
	}

	printf ("\nTo get help on a particular command, type in "
		"'help command-name'\n\n");

	return (GP_OK);
}
