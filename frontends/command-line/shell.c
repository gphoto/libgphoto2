/* shell.c:
 *
 * Copyright (C) 2002 Lutz Müller <lutz@users.sourceforge.net>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, 
 * but WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details. 
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <config.h>
#include "shell.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef HAVE_RL
#  include <readline/readline.h>
#  include <readline/history.h>
#endif

#include "main.h"
#include "globals.h"
#include "actions.h"

#ifdef ENABLE_NLS
#  include <libintl.h>
#  undef _
#  define _(String) dgettext (PACKAGE, String)
#  ifdef gettext_noop
#    define N_(String) gettext_noop (String)
#  else
#    define N_(String) (String)
#  endif
#else
#  define textdomain(String) (String)
#  define gettext(String) (String)
#  define dgettext(Domain,Message) (Message)
#  define dcgettext(Domain,Message,Type) (Message)
#  define bindtextdomain(Domain,Directory) (Domain)
#  define _(String) (String)
#  define N_(String) (String)
#endif

#ifndef MAX
# define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif
#ifndef MIN
# define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

#define CHECK(result) {int r=(result);if(r<0) return(r);}
#define CHECK_CONT(result)					\
{								\
	int r = (result);					\
								\
	if (r < 0) {						\
		if (r == GP_ERROR_CANCEL) {			\
			glob_cancel = 0;			\
		} else {					\
			printf (_("*** Error (%i: '%s') ***"),	\
				r, gp_result_as_string (r));	\
			printf ("\n");				\
		}						\
	}							\
}


static Camera *camera = NULL;

/* Forward declarations */
static int shell_cd            (Camera *, const char *);
static int shell_lcd           (Camera *, const char *);
static int shell_exit          (Camera *, const char *);
static int shell_get           (Camera *, const char *);
static int shell_get_thumbnail (Camera *, const char *);
static int shell_get_raw       (Camera *, const char *);
static int shell_del           (Camera *, const char *);
static int shell_help          (Camera *, const char *);
static int shell_ls            (Camera *, const char *);
static int shell_exit          (Camera *, const char *);
static int shell_show_info     (Camera *, const char *);
#ifdef HAVE_EXIF
static int shell_show_exif     (Camera *, const char *);
#endif

#define MAX_FOLDER_LEN 1024
#define MAX_FILE_LEN 1024

static int shell_construct_path (const char *folder_orig, const char *rel_path,
				 char *dest_folder, char *dest_filename);

typedef int (* ShellFunction) (Camera *camera, const char *arg);

typedef struct _ShellFunctionTable ShellFunctionTable;
struct _ShellFunctionTable {
	const char *command;
	ShellFunction function;
	const char *description;
	const char *description_arg;
	unsigned char arg_required;
} func[] = {
	{"cd", shell_cd, N_("Change to a directory on the camera"),
	 N_("directory"), 1},
	{"lcd", shell_lcd, N_("Change to a directory on the local drive"),
	 N_("directory"), 1},
	{"exit", shell_exit, N_("Exit the gPhoto shell"), NULL, 0},
	{"get", shell_get, N_("Download a file"), N_("[directory/]filename"),
	 1},
	{"get-thumbnail", shell_get_thumbnail, N_("Download a thumbnail"),
	 N_("[directory/]filename"), 1},
	{"get-raw", shell_get_raw, N_("Download raw data"),
	 N_("[directory/]filename"), 1},
	{"show-info", shell_show_info, N_("Show info"),
	 N_("[directory/]filename"), 1},
	{"delete", shell_del, N_("Delete"), N_("[directory/]filename"), 1},
#ifdef HAVE_EXIF
	{"show-exif", shell_show_exif, N_("Show EXIF information"),
	 N_("[directory/]filename"), 1},
#endif
	{"help", shell_help, N_("Displays command usage"),
	 N_("[command]"), 0},
	{"ls", shell_ls, N_("List the contents of the current directory"),
	 N_("[directory/]"), 0},
	{"q", shell_exit, N_("Exit the gPhoto shell"), NULL, 0},
	{"quit", shell_exit, N_("Exit the gPhoto shell"), NULL, 0},
	{"?", shell_help, N_("Displays command usage"), N_("[command]"), 0},
	{"", NULL, NULL, NULL}
};

/* Local globals */
#define SHELL_PROMPT "gphoto2: {%s} %s> "
int 	shell_done		= 0;

static int
shell_arg_count (const char *args)
{
	int x=0, in_arg=0, count=0;
	
	while (x < strlen (args)) {
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

static char *
shell_read_line (void)
{
	char prompt[70], buf[1024], *line;

	if (glob_quiet)
		snprintf (prompt, sizeof (prompt), SHELL_PROMPT, "\0", "\0");
	else {
		if (strlen (glob_cwd) > 25) {
			strncpy (buf, "...", sizeof (buf));
			strncat (buf, &glob_cwd[strlen (glob_cwd) - 22],
				 sizeof (buf));
			snprintf (prompt, sizeof (prompt), SHELL_PROMPT, buf,
				  glob_folder);
		} else
			snprintf (prompt, sizeof (prompt), SHELL_PROMPT,
				  glob_cwd, glob_folder);
	}
#ifdef HAVE_RL
	line = readline (prompt);
	if (line)
		add_history (line);
#else
	line = malloc (1024);
	if (!line)
		return (NULL);
	printf (SHELL_PROMPT, prompt, glob_folder);
	fflush(stdout);
	fgets (line, 1023, stdin);
	line[strlen (line) - 1] = '\0'; 
#endif
	return (line);
}

static int
shell_arg (const char *args, int arg_num, char *arg)
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

#ifdef HAVE_RL

static char *
shell_command_generator (const char *text, int state)
{
	static int x, len;

	/* If this is a new command to complete, reinitialize */
	if (!state) {
		x = 0;
		len = strlen (text);
	}

	/* Search 'text' */
	for (; func[x].command; x++)
		if (!strncmp (func[x].command, text, len))
			break;
	if (func[x].command)
		return (strdup (func[x++].command));

	return (NULL);
}

static char *
shell_path_generator (const char *text, int state)
{
	static int x;
	const char *slash, *name;
	CameraList list;
	int file_count, folder_count, r, len;
	char folder[MAX_FOLDER_LEN], basename[MAX_FILE_LEN], *path;

#if 0
	printf ("shell_path_generator ('%s', %i)\n", text, state);
#endif

	r = shell_construct_path (glob_folder, text, folder, basename);
	if (r < 0)
		return (NULL);
	len = strlen (basename);

#if 0
	printf ("Searching for '%s' in '%s'...\n", basename, folder);
#endif

	/* If this is a new path to complete, reinitialize */
	if (!state)
		x = 0;

	/* First search for matching file */
	r = gp_camera_folder_list_files (camera, folder, &list, glob_context);
	if (r < 0)
		return (NULL);
	file_count = gp_list_count (&list);
	if (file_count < 0)
		return (NULL);
	if (x < file_count) {
		for (; x < file_count; x++) {
			r = gp_list_get_name (&list, x, &name);
			if (r < 0)
				return (NULL);
			if (!strncmp (name, basename, len)) {
				x++; 
				slash = strrchr (text, '/');
				if (!slash) {
					path = malloc (strlen (name) + 2);
					if (!path)
						return (NULL);
					strcpy (path, name);
					strcat (path, " ");
				} else {
					path = malloc (slash - text + 1 + strlen (name) + 2);
					if (!path)
						return (NULL);
					memset (path, 0, slash - text + 1 + strlen (name) + 2);
					strncpy (path, text, slash - text);
					strcat (path, "/");
					strcat (path, name);
					strcat (path, " ");
				}
				return (path);
			}
		}
	}

	/* Ok, we listed all matching files. Now, list matching folders. */
	r = gp_camera_folder_list_folders (camera, folder, &list, glob_context);
	if (r < 0)
		return (NULL);
	folder_count = gp_list_count (&list);
	if (folder_count < 0)
		return (NULL);
	if (x - file_count < folder_count) {
		for (; x - file_count < folder_count; x++) {
			r = gp_list_get_name (&list, x - file_count, &name);
			if (r < 0)
				return (NULL);
			if (!strncmp (name, basename, len)) {
				x++;
				slash = strrchr (text, '/');
				if (!slash) {
					path = malloc (strlen (name) + 2);
					if (!path)
						return (NULL);
					strcpy (path, name);
					strcat (path, "/");
				} else {
					path = malloc (slash - text + 1 + strlen (name) + 2);
					if (!path)
						return (NULL);
					memset (path, 0, slash - text + 1 + strlen (name) + 2);
					strncpy (path, text, slash - text);
					strcat (path, "/");
					strcat (path, name);
					strcat (path, "/");
				}
				return (path);
			}
		}
		return (NULL);
	}

	return (NULL);
}

static char **
shell_completion_function (const char *text, int start, int end)
{
	char **matches = NULL;
	char *current;

	if (!text)
		return (NULL);

	if (!start) {
		/* Complete command */
		matches = rl_completion_matches (text, shell_command_generator);
	} else {
		current = strdup (rl_copy_text (0, end));

		/* Complete local path? */
		if (!strncmp (current, "lcd", strlen ("lcd"))) {
			free (current);
			return (NULL);
		}
		free (current);

		/* Complete remote path */
		matches = rl_completion_matches (text, shell_path_generator);
	}

	return (matches);
}
#endif /* HAVE_RL */

int
shell_prompt (Camera *c)
{
	int x;
	char cmd[1024], arg[1024], *line;

	/* The stupid readline functions need that global variable. */
	camera = c;

#ifdef HAVE_RL
	rl_attempted_completion_function = shell_completion_function;
	rl_completion_append_character = '\0';
#endif

	while (!shell_done && !glob_cancel) {
		line = shell_read_line ();
		if (!line)
			continue;

		/* If we don't have any command, start from the beginning */
		if (shell_arg_count (line) <= 0) {
			free (line);
			continue;
		}
		
		shell_arg (line, 0, cmd);
		strcpy (arg, &line[strlen (cmd)]);
		free (line);

		/* Search the command */
		for (x = 0; func[x].function; x++)
			if (!strcmp (cmd, func[x].command))
				break;
		if (!func[x].function) {
			cli_error_print (_("Invalid command."));
			continue;
		}

		/*
		 * If the command requires an argument, complain if this
		 * argument is not given.
		 */
		if (func[x].arg_required && !shell_arg_count (arg)) {
			printf (_("The command '%s' requires "
				  "an argument."), cmd);
			printf ("\n");
			continue;
		}

		/* Execute the command */
		CHECK_CONT (func[x].function (camera, arg));
	}

	return (GP_OK);
}

static int
shell_construct_path (const char *folder_orig, const char *rel_path,
                      char *dest_folder, char *dest_filename)
{
        const char *slash;

        if (!folder_orig || !rel_path || !dest_folder)
                return (GP_ERROR);

        memset (dest_folder, 0, MAX_FOLDER_LEN);
	if (dest_filename)
	        memset (dest_filename, 0, MAX_FILE_LEN);

	/* Skip leading spaces */
	while (rel_path[0] == ' ')
		rel_path++;

        /*
         * Consider folder_orig only if we are really given a relative
         * path.
         */
        if (rel_path[0] != '/')
                strncpy (dest_folder, folder_orig, MAX_FOLDER_LEN);
	else {
		while (rel_path[0] == '/')
			rel_path++;
		strncpy (dest_folder, "/", MAX_FOLDER_LEN);
	}

        while (rel_path) {
		if (!strncmp (rel_path, "./", 2)) {
                        rel_path += MIN (strlen (rel_path), 2);
			continue;
		}
		if (!strncmp (rel_path, "../", 3)) {
                        rel_path += MIN (3, strlen (rel_path));

                        /* Go up one folder */
                        slash = strrchr (dest_folder, '/');
                        if (!slash) {
                                cli_error_print (_("Invalid path."));
                                return (GP_ERROR);
                        }
			dest_folder[slash - dest_folder] = '\0';
			if (!strlen (dest_folder))
				strcpy (dest_folder, "/");
			continue;
                }

                slash = strchr (rel_path, '/');
		if (strcmp (rel_path, "") && (slash || !dest_filename)) {

			/*
			 * We need to go down one folder. Append a 
			 * trailing slash
			 */
			if (dest_folder[strlen (dest_folder) - 1] != '/')
				strncat (dest_folder, "/", MAX_FOLDER_LEN);
		}
                if (slash) {
                        strncat (dest_folder, rel_path,
                                 MIN (MAX_FOLDER_LEN, slash - rel_path));
                        rel_path = slash + 1;
                } else {

                        /* Done */
                        if (dest_filename)
                                strncpy (dest_filename, rel_path,
                                         MAX_FILE_LEN);
                        else
                                strncat (dest_folder, rel_path, MAX_FILE_LEN);
                        break;
                }
        }

        return (GP_OK);
}

static int
shell_lcd (Camera *camera, const char *arg)
{
	char new_cwd[MAX_FOLDER_LEN];
	int arg_count = shell_arg_count (arg);

	if (!arg_count) {
		if (!getenv ("HOME")) {
			cli_error_print (_("Could not find home directory."));
			return (GP_OK); 
		}
		strcpy (new_cwd, getenv ("HOME"));
	} else
		shell_construct_path (glob_cwd, arg, new_cwd, NULL);

	if (chdir (new_cwd) < 0) {
		cli_error_print (_("Could not change to "
				   "local directory '%s'."), new_cwd);
	} else {
		printf (_("Local directory now '%s'."), new_cwd);
		printf ("\n");
		strcpy (glob_cwd, new_cwd);
	}

	return (GP_OK);
}

static int
shell_cd (Camera *camera, const char *arg)
{
	char folder[MAX_FOLDER_LEN];
	CameraList list;
	int arg_count = shell_arg_count (arg);

	if (!arg_count)
		return (GP_OK);

	/* shell_arg(arg, 0, arg_dir); */
	
	if (strlen (arg) > 1023) {
		cli_error_print ("Folder value is too long");
		return (GP_ERROR);
	}

	/* Get the new folder value */
	shell_construct_path (glob_folder, arg, folder, NULL);

	CHECK (gp_camera_folder_list_folders (camera, folder, &list,
					      glob_context));
	strcpy (glob_folder, folder);
	printf (_("Remote directory now '%s'."), glob_folder);
	printf ("\n");

	return (GP_OK);
}

static int
shell_ls (Camera *camera, const char *arg)
{
	CameraList list;
	char buf[1024], folder[MAX_FOLDER_LEN];
	int x, y=1;
	int arg_count = shell_arg_count(arg);
	const char *name;

	if (arg_count) {
		shell_construct_path (glob_folder, arg, folder, NULL);
	} else {
		strcpy (folder, glob_folder);
	}

	CHECK (gp_camera_folder_list_folders (camera, folder, &list,
					      glob_context));

	if (glob_quiet)
		printf ("%i\n", gp_list_count (&list));

	for (x = 1; x <= gp_list_count (&list); x++) {
		CHECK (gp_list_get_name (&list, x - 1, &name));
		if (glob_quiet)
			printf ("%s\n", name);
		else {
			sprintf (buf, "%s/", name);
			printf ("%-20s", buf);
			if (y++ % 4 == 0)
				printf("\n");
		}
	}

	CHECK (gp_camera_folder_list_files (camera, folder, &list,
					    glob_context));

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
shell_file_action (Camera *camera, GPContext *context, const char *folder,
		   const char *args, FileAction action)
{
	char arg[1024];
	unsigned int x;
	char dest_folder[MAX_FOLDER_LEN], dest_filename[MAX_FILE_LEN];
	ActionParams ap;

	memset (&ap, 0, sizeof (ActionParams));
	ap.camera = camera;
	ap.context = context;
	ap.folder = dest_folder; 

	for (x = 0; x < shell_arg_count (args); x++) {
		CHECK (shell_arg (args, x, arg));
		CHECK (shell_construct_path (folder, arg,
					     dest_folder, dest_filename));
		CHECK (action (&ap, dest_filename));
	}
	
	return (GP_OK);      
}

static int
shell_get_thumbnail (Camera *camera, const char *arg)
{
	CHECK (shell_file_action (camera, glob_context, glob_folder, arg,
				  save_thumbnail_action));

	return (GP_OK);
}

static int
shell_get (Camera *camera, const char *arg)
{
	CHECK (shell_file_action (camera, glob_context, glob_folder, arg,
				  save_file_action));

	return (GP_OK);
}

static int
shell_get_raw (Camera *camera, const char *arg)
{
	CHECK (shell_file_action (camera, glob_context, glob_folder, arg,
				  save_raw_action));

	return (GP_OK);
}

static int
shell_del (Camera *camera, const char *arg)
{
	CHECK (shell_file_action (camera, glob_context, glob_folder, arg,
				  delete_file_action));

	return (GP_OK);
}

#ifdef HAVE_EXIF
static int
shell_show_exif (Camera *camera, const char *arg)
{
	CHECK (shell_file_action (camera, glob_context, glob_folder, arg,
				  print_exif_action));

	return (GP_OK);
}
#endif

static int
shell_show_info (Camera *camera, const char *arg)
{
	CHECK (shell_file_action (camera, glob_context, glob_folder, arg,
				  print_info_action));

	return (GP_OK);
}

int
shell_exit (Camera *camera, const char *arg)
{
	shell_done = 1;
	return (GP_OK);
}

static int
shell_help_command (Camera *camera, const char *arg)
{
	char arg_cmd[1024];
	unsigned int x;

	shell_arg (arg, 0, arg_cmd);

	/* Search this command */
	for (x = 0; func[x].function; x++)
		if (!strcmp (arg_cmd, func[x].command))
			break;
	if (!func[x].function) {
		printf (_("Command '%s' not found. Use 'help' to get a "
			"list of available commands."), arg_cmd);
		printf ("\n");
		return (GP_OK);
	}

	/* Print the command's syntax. */
	printf (_("Help on \"%s\":"), func[x].command);
	printf ("\n\n");
	printf (_("Usage:"));
	printf (" %s %s", func[x].command,
		func[x].description_arg ? _(func[x].description_arg) : "");
	printf ("\n");
	printf (_("Description:"));
	printf ("\n\t%s\n\n", _(func[x].description));
	printf (_("* Arguments in brackets [] are optional"));
	printf ("\n");
	
	return (GP_OK);
}

static int
shell_help (Camera *camera, const char *arg)
{
	unsigned int x;
	int arg_count = shell_arg_count (arg);

	/*
	 * If help on a command is requested, print the syntax of the command.
	 */
	if (arg_count > 0) {
		CHECK (shell_help_command (camera, arg));
		return (GP_OK);
	}

	/* No command specified. Print command listing. */
	printf (_("Available commands:"));
	printf ("\n");
	for (x = 0; func[x].function; x++)
		printf ("\t%-16s%s\n", func[x].command, _(func[x].description));
	printf ("\n");
	printf (_("To get help on a particular command, type in "
		"'help command-name'."));
	printf ("\n\n");

	return (GP_OK);
}
