#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "main.h"
#include "globals.h"
#include "shell.h"


/* Valid shell commands */
shell_function func[] = {
{"cd", 			shell_cd,		"Change to a directory on the camera",			"directory"},
{"lcd", 		shell_lcd,		"Change to a directory on the local drive"		"directory"},
{"exit", 		shell_exit,		"Exit the gPhoto shell",				""},
{"get",			shell_get,		"Download the file to the current directory",		"[directory/]filename"},
{"get-thumbnail",	shell_get_thumbnail,	"Download the thumbnail to the current directory",	"[directory/]filename"},
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

int shell_prompt () {

	int x, i, len, found;
	char buf[1024];
	char *cmd, *arg;

	while (!shell_done) {
		cmd = NULL; arg = NULL; x=-1; found=0, i=0;
		if (strlen(glob_cwd) > 25) {
			strcpy(buf, "....");
			strcat(buf, &glob_cwd[strlen(glob_cwd)-20]);
		} else {
			strcpy(buf, glob_cwd);
		}
		printf(shell_prompt_text, buf, glob_folder); /* Display prompt text */
		fflush(stdout);

		fgets(buf, 1023, stdin);		/* Get the command/args */

		len = strlen(buf);
		buf[len-1]=0;				/* Chop the newline */

							/* Extract command and argument */
		while ((isspace(buf[++x]))&&(x < len)) { }
		if (x==len) goto shell_prompt_error;
		cmd = &buf[x];
		while ((!isspace(buf[++x]))&&(x < len)) { }
		if (x==len) goto shell_prompt_process;
		buf[x] = 0;
		while (isspace(buf[++x])) { }
		if (x < len)
			arg = &buf[x];
		   else
			arg = NULL;
shell_prompt_process:
		x=-1;
		while (func[++x].function) {		/* Execute the associated function */
			if (strcmp(cmd, func[x].command)==0) {
				found = 1;
				func[x].function(arg);
				break;
			}
		}
		if (!found)
shell_prompt_error:
			cli_error_print("Invalid command");
		
	}

	return (GP_OK);
}

int shell_get_new_folder (char *relative_path, char *cd_arg, char *new_folder) {

	/*
	   Find what the new folder will be based on the "cd" or "ls" argument.
	   This can take paths like:
		"/blah", "./blah", "../blah", "/blah/hey/../doh/blah", etc...
	*/

	char *dir = NULL, *slash = NULL;
	char tmp_folder[1024];
	int append = 0;

	if (cd_arg[0] == '/')				/* Check if path is relative to root */
		strcpy(tmp_folder, "/");
	   else
		strcpy(tmp_folder, relative_path);

	do {
		append = 1;
		if (!dir)				/* Get each directory individually */
			dir = strtok(cd_arg, "/");
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

int shell_lcd (char *arg) {

	char tmp_dir[1024];

	if (!arg) {
		if (getenv("HOME")==NULL) {
			cli_error_print("Could not find home directory");
			return (GP_OK);
		}
		strcpy(tmp_dir, getenv("HOME"));
	} else {
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

int shell_cd (char *arg) {

	char tmp_folder[1024];
	CameraList list;

	if (!arg)
		return (GP_OK);

	if (strlen(arg) > 1023) {
		cli_error_print("Folder value is too long");
		return (GP_ERROR);
	}

	shell_get_new_folder(glob_folder, arg, tmp_folder);	/* Get the new folder value */

	if (gp_camera_folder_list(glob_camera, &list, tmp_folder) == GP_OK)
		strcpy(glob_folder, tmp_folder);
	   else
		cli_error_print("Folder %s does not exist", tmp_folder);

	return (GP_OK);
}

int shell_ls (char *arg) {

	CameraList list;
	CameraListEntry *entry;
	char buf[1024], tmp_folder[1024];
	int x, y=1;

	if (arg)
		shell_get_new_folder(glob_folder, arg, tmp_folder);
	   else
		strcpy(tmp_folder, glob_folder);

	if (gp_camera_folder_list(glob_camera, &list, tmp_folder)==GP_ERROR) {
		if (arg)
			cli_error_print("Folder %s does not exist", tmp_folder);
		  else
			cli_error_print("Could not retrieve the folder list");
		return (GP_ERROR);
	}

	if (glob_quiet)
		printf("%i\n", gp_list_count(&list));

	for (x=1; x<=gp_list_count(&list); x++) {
		entry = gp_list_entry(&list, x-1);
		if (glob_quiet)
			printf("%s\n", entry->name);
		   else {
			sprintf(buf, "%s/", entry->name);
			printf("%-20s", buf);
			if (y++%4==0)
				printf("\n");
		}
	}

	if (gp_camera_file_list(glob_camera, &list, tmp_folder)==GP_ERROR) {
		cli_error_print("Could not retrieve the file list");
		return (GP_ERROR);
	}

	if (glob_quiet)
		printf("%i\n", gp_list_count(&list));

	for (x=1; x<=gp_list_count(&list); x++) {
		entry = gp_list_entry(&list, x-1);
		if (glob_quiet)
			printf("%s\n", entry->name);
		   else {
			printf("%-20s", entry->name);
			if (y++%4==0)
				printf("\n");
		}
	}
	if ((!glob_quiet)&&(y%4!=1))
		printf("\n");

	return (GP_OK);
}

int shell_get_common (char *arg, int thumbnail) {

	char tmp_folder[1024];
	char *slash, *tmp_filename;

	if (!arg) {
		cli_error_print("No filename specified");
		return (GP_ERROR);
	}

	if (strchr(arg, '/')) {
		slash = strrchr(arg, '/');
		tmp_filename = slash + 1;
		*slash = 0;
		if (strlen(arg)==0)
			strcpy(tmp_folder, "/");
		   else
			strcpy(tmp_folder, arg);
	} else {
		tmp_filename = arg;
		strcpy(tmp_folder, glob_folder);
	}

	save_picture_to_file(tmp_folder, tmp_filename, thumbnail);

	return (GP_OK);		
}

int shell_get_thumbnail (char *arg) {

	shell_get_common (arg, 1);

	return (GP_OK);
}	

int shell_get (char *arg) {

	shell_get_common (arg, 0);

	return (GP_OK);
}	

int shell_exit (char *arg) {

	shell_done = 1;
	return (GP_OK);
}

int shell_help (char *arg) {
	int x=0;

	if (arg) {
		while (func[x].function) {
			if (strcmp(arg, func[x].command)==0) {
				printf("Help on \"%s\":\n\n", arg);
				printf("Usage: %s %s\n", arg, func[x].description_arg);
				printf("Description: \n\t%s\n\n", func[x].description);
				printf("* Arguments in brackets [] are optional\n");
				return (GP_OK);
			}
			x++;
		}
		cli_error_print("Command not found in 'help'");
		return (GP_OK);
	}

	printf("Available commands:\n");

	while (func[x].function) {
		printf("\t%-16s%s\n", func[x].command, func[x].description);
		x++;
	}

	printf("\nTo get help on a particular command, type in 'help command-name'\n\n");

	return (GP_OK);
}
