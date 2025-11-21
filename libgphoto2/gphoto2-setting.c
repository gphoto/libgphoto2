/** \file
 *
 * \author Copyright 2000 Scott Fritzinger
 *
 * \par License
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * \par
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * \par
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

#define _DEFAULT_SOURCE

#include "config.h"
#include <gphoto2/gphoto2-setting.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gphoto2/gphoto2-result.h>
#include <gphoto2/gphoto2-port-log.h>
#include <gphoto2/gphoto2-port-portability.h>

#ifdef WIN32
/* Win32 headers may use interface as a define; temporarily correct this */
#define interface struct
#include <shlobj.h>
#undef interface
#endif

/**
 * Internal struct to store settings.
 */
typedef struct {
	/* key = value settings */
	char id[256];
	char key[256];
	char value[256];
} Setting;

/* Currently loaded settings */
static int             glob_setting_count = 0;
static Setting         glob_setting[512];

static int save_settings (void);

#define CHECK_RESULT(result)       {int r = (result); if (r < 0) return (r);}

static int load_settings (void);

static void *get_func_userdata = NULL;
static void *set_func_userdata = NULL;
static gp_settings_func custom_get_func = NULL;
static gp_settings_func custom_set_func = NULL;

/**
 * \brief Set user defined function to get a gphoto setting.
 * \param func The function to get the settings
 * \param userdata userdata passed to func
 *
 * This function is expected to behave like gp_settings_get.
 * To clear set func to NULL.
 */
void gp_setting_set_get_func (gp_settings_func func, void *userdata)
{
	custom_get_func = func;
	get_func_userdata = userdata;
}

/**
 * \brief Set user defined function to set a gphoto setting.
 * \param func The function to set the settings
 * \param userdata userdata passed to func
 *
 * This function is expected to behave like gp_settings_set.
 * To clear set func to NULL.
 */
void gp_setting_set_set_func (gp_settings_func func, void *userdata)
{
	custom_set_func = func;
	set_func_userdata = userdata;
}

/**
 * \brief Retrieve a specific gphoto setting.
 * \param id the frontend id of the caller
 * \param key the key the frontend queries
 * \param value changed value
 * \return GPhoto error code
 *
 * This function retrieves the setting key for a specific frontend
 * id and copies the value into the passed value pointer.
 */
int
gp_setting_get (char *id, char *key, char *value)
{
	if (custom_get_func != NULL)
		return custom_get_func(id, key, value, get_func_userdata);

	int x;

	C_PARAMS (id && key);

	if (!glob_setting_count)
		load_settings ();

	for (x=0; x<glob_setting_count; x++) {
		if ((strcmp(glob_setting[x].id, id)==0) &&
		    (strcmp(glob_setting[x].key, key)==0)) {
			strcpy(value, glob_setting[x].value);
			return (GP_OK);
		}
	}
	strcpy(value, "");
	return(GP_ERROR);
}

/**
 * \brief Set a specific gphoto setting.
 *
 * \param id the frontend id of the caller
 * \param key the key the frontend queries
 * \param value new value
 * \return GPhoto error code
 *
 * This function sets the setting key for a specific frontend
 * id to the value.
 */
int
gp_setting_set (char *id, char *key, char *value)
{
	if (custom_set_func != NULL)
		return custom_set_func(id, key, value, set_func_userdata);

	int x;

	C_PARAMS (id && key);

	if (!glob_setting_count)
		load_settings ();

	GP_LOG_D ("Setting key '%s' to value '%s' (%s)", key, value, id);

	for (x=0; x<glob_setting_count; x++) {
		if ((strcmp(glob_setting[x].id, id)==0) &&
		    (strcmp(glob_setting[x].key, key)==0)) {
			strcpy(glob_setting[x].value, value);
			save_settings ();
			return (GP_OK);
		}
	}
	strcpy(glob_setting[glob_setting_count].id, id);
	strcpy(glob_setting[glob_setting_count].key, key);
	strcpy(glob_setting[glob_setting_count++].value, value);
	save_settings ();

	return (GP_OK);
}

#define GP_PATH_MAX 1024

static int
gp_settings_path (char (*out)[GP_PATH_MAX])
{
#ifdef WIN32

	/* TODO: improve robustness */
	/* TODO: respect system-defined folders of Windows as well (AppData etc.) */
	SHGetFolderPath (NULL, CSIDL_PROFILE, NULL, 0, *out);
	strcat (*out, "\\.gphoto");
	GP_LOG_D ("Creating gphoto config directory ('%s')", *out);
	(void)gp_system_mkdir (*out);
	strcat (*out, "\\settings");

#else

	char dir[GP_PATH_MAX];
	char path[GP_PATH_MAX];
	const char *home = getenv("HOME");
	char xdg_config_home[GP_PATH_MAX];
	FILE *tmp;

# ifdef __APPLE__
	/* TODO: use more native approach */
	const char *xdg_fallback = "Library/Application Support";
# else
	const char *xdg_fallback = ".config";
# endif

	const char *xdg_tmpvar = getenv ("XDG_CONFIG_HOME");
	if (xdg_tmpvar != NULL && xdg_tmpvar[0] != '\0') {
		snprintf (xdg_config_home, GP_PATH_MAX, "%s", xdg_tmpvar);
	} else {
		snprintf (xdg_config_home, GP_PATH_MAX, "%s/%s", home, xdg_fallback);
	}

	snprintf (dir, GP_PATH_MAX, "%s/gphoto", xdg_config_home);
	snprintf (path, GP_PATH_MAX, "%s/settings", dir);

	tmp = fopen (path, "r");
	if (tmp) {
		fclose (tmp);
	} else {
		char legacy[GP_PATH_MAX];
		snprintf (legacy, GP_PATH_MAX, "%s/.gphoto/settings", home);
		tmp = fopen (legacy, "r");
		if (tmp) {
			fclose (tmp);
			snprintf (dir, GP_PATH_MAX, "%s/.gphoto", home);
			snprintf (path, GP_PATH_MAX, "%s", legacy);
			GP_LOG_D ("Using legacy settings path ('%s')", path);
		}
	}

	GP_LOG_D ("Creating gphoto config directory ('%s')", dir);
	(void)gp_system_mkdir (dir);

	snprintf (*out, GP_PATH_MAX, "%s", path);

#endif

	return (GP_OK);
}

static int
verify_settings (char *settings_file)
{
	FILE *f;
	char buf[1024];
	unsigned int x, equals;

	if ((f=fopen(settings_file, "r"))==NULL) {
		GP_LOG_D ("Can't open settings file '%s' for reading.", settings_file);
		return(0);
	}

	rewind(f);
	while (!feof(f)) {
		strcpy(buf, "");
		if (!fgets(buf, 1023, f))
			break;
		buf[strlen(buf)] = 0;
		if (strlen(buf)>2) {
			equals = 0;
			for (x=0; x<strlen(buf); x++)
				if (buf[x] == '=')
					equals++;

			if (equals < 2) {
				fclose (f);
				GP_LOG_E ("Incorrect settings format. Resetting.");
				unlink(settings_file);
				return (GP_ERROR);
			}
		}
	}
	fclose (f);

	return (GP_OK);
}

static int
load_settings (void)
{
	FILE *f;
	char buf[GP_PATH_MAX], *id, *key, *value;

	gp_settings_path (&buf);
	glob_setting_count = 0;

	if (verify_settings(buf) != GP_OK)
		/* verify_settings will unlink and recreate the settings file */
		return (GP_OK);
	GP_LOG_D ("Loading settings from file '%s'.", buf);

	if ((f=fopen(buf, "r"))==NULL) {
		GP_LOG_D ("Can't open settings file '%s' for reading.", buf);
		return(GP_ERROR);
	}

	rewind(f);
	while (!feof(f)) {
		strcpy(buf, "");
		if (!fgets(buf, 1023, f))
			break;
		if (strlen(buf)>2) {
			buf[strlen(buf)-1] = '\0';
			id = strtok(buf, "=");
			strcpy(glob_setting[glob_setting_count].id,id);
			key = strtok(NULL, "=");
			strcpy(glob_setting[glob_setting_count].key,key);
			value = strtok(NULL, "\0");
			if (value)
				strcpy(glob_setting[glob_setting_count++].value, value);
			else
				strcpy(glob_setting[glob_setting_count++].value, "");
		}
	}
	fclose (f);
	return (GP_OK);
}


static int
save_settings (void)
{
	FILE *f;
	char buf[GP_PATH_MAX];
	int x=0;

	gp_settings_path (&buf);

	GP_LOG_D ("Saving %i setting(s) to file \"%s\"", glob_setting_count, buf);

	if ((f=fopen(buf, "w+"))==NULL) {
		GP_LOG_E ("Can't open settings file for writing.");
		return(0);
	}
	rewind(f);
	while (x < glob_setting_count) {
		fwrite(glob_setting[x].id, strlen(glob_setting[x].id),1,f);
		fputc('=', f);
		fwrite(glob_setting[x].key, strlen(glob_setting[x].key),1,f);
		fputc('=', f);
		fwrite(glob_setting[x].value, strlen(glob_setting[x].value),1,f);
		fputc('\n', f);
		x++;
	}
	fclose(f);

	return (GP_OK);
}

#if 0
static int dump_settings (void)
{
	int x;

	gp_debug_printf(GP_DEBUG_LOW, "core", "All settings:");
	for (x=0; x<glob_setting_count; x++)
		gp_debug_printf(GP_DEBUG_LOW, "core", "\t (%s) \"%s\" = \"%s\"", glob_setting[x].id,
			glob_setting[x].key,glob_setting[x].value);
	if (glob_setting_count == 0)
		gp_debug_printf(GP_DEBUG_LOW, "core", "\tNone");
	gp_debug_printf(GP_DEBUG_LOW, "core", "Total settings: %i", glob_setting_count);

	return (GP_OK);
}
#endif
