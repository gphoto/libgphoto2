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

#define _BSD_SOURCE

#include "config.h"
#include <gphoto2/gphoto2-setting.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <gphoto2/gphoto2-result.h>
#include <gphoto2/gphoto2-port-log.h>
#include <gphoto2/gphoto2-port-portability.h>

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

/**
 * \brief Retrieve a specific gphoto setting.
 * \param id the frontend id of the caller
 * \param key the key the frontend queries
 * \param value retrieved value
 * \return GPhoto error code
 *
 * This function retrieves the setting key for a specific frontend
 * id and copies the value into the passed value pointer.
 */
int
gp_setting_get (char *id, char *key, char *value)
{
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
 * \param value retrieved value
 * \return GPhoto error code
 *
 * This function sets the setting key for a specific frontend
 * id to the value.
 */
int
gp_setting_set (char *id, char *key, char *value)
{
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

static int
verify_settings (char *settings_file)
{
	FILE *f;
	char buf[1024];
	unsigned int x, equals;

	if ((f=fopen(settings_file, "r"))==NULL) {
		GP_LOG_E ("Can't open settings file for reading.");
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
	char buf[1024], *id, *key, *value;

	/* Make sure the directories are created */
	GP_LOG_D ("Creating $HOME/.gphoto");
#ifdef WIN32
	GetWindowsDirectory (buf, sizeof(buf));
	strcat (buf, "\\gphoto");
#else
	snprintf (buf, sizeof(buf), "%s/.gphoto", getenv ("HOME"));
#endif
	(void)gp_system_mkdir (buf);

	glob_setting_count = 0;
#ifdef WIN32
	GetWindowsDirectory(buf, sizeof(buf));
	strcat(buf, "\\gphoto\\settings");
#else
	snprintf(buf, sizeof(buf), "%s/.gphoto/settings", getenv("HOME"));
#endif

	if (verify_settings(buf) != GP_OK)
		/* verify_settings will unlink and recreate the settings file */
		return (GP_OK);
	GP_LOG_D ("Loading settings from file '%s'.", buf);

	if ((f=fopen(buf, "r"))==NULL) {
		GP_LOG_D ("Can't open settings file for reading.");
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
	char buf[1024];
	int x=0;

	snprintf (buf, sizeof(buf), "%s/.gphoto/settings", getenv ("HOME"));

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
