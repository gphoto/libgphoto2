/* gphoto2-setting.c
 *
 * Copyright (C) 2000 Scott Fritzinger
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
#include "gphoto2-setting.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "gphoto2-result.h"
#include "gphoto2-port-log.h"
#include "gphoto2-port-portability.h"

#define GP_MODULE "setting"

typedef struct {
	/* key = value settings */
	char id[256];
	char key[256];
	char value[256];
} Setting;

/* Currently loaded settings */
int             glob_setting_count = 0;
Setting         glob_setting[512];

static int save_settings (void);

#define CHECK_NULL(r)              {if (!(r)) return (GP_ERROR_BAD_PARAMETERS);}
#define CHECK_RESULT(result)       {int r = (result); if (r < 0) return (r);}

static int load_settings (void);

int gp_setting_get (char *id, char *key, char *value)
{
        int x;

	CHECK_NULL (id && key);

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

int
gp_setting_set (char *id, char *key, char *value)
{
        int x;

	CHECK_NULL (id && key);

	if (!glob_setting_count)
		load_settings ();

	gp_log (GP_LOG_DEBUG, "gphoto2-setting",
		"Setting key '%s' to value '%s' (%s)", key, value, id);

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
	int x, equals;

	if ((f=fopen(settings_file, "r"))==NULL) {
		GP_DEBUG ("Can't open settings file for reading");
		return(0);
	}

	rewind(f);
	while (!feof(f)) {
		strcpy(buf, "");
		fgets(buf, 1023, f);
		buf[strlen(buf)] = 0;
		if (strlen(buf)>2) {
			equals = 0;
			for (x=0; x<strlen(buf); x++)
				if (buf[x] == '=')
					equals++;

			if (equals < 2) {
				fclose (f);
				GP_DEBUG ("Incorrect settings format. resetting\n");
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
	GP_DEBUG ("Creating $HOME/.gphoto");
#ifdef WIN32
	GetWindowsDirectory (buf, 1024);
	strcat (buf, "\\gphoto");
#else
	sprintf (buf, "%s/.gphoto", getenv ("HOME"));
#endif
	(void)GP_SYSTEM_MKDIR (buf);

	glob_setting_count = 0;
#ifdef WIN32
	GetWindowsDirectory(buf, 1024);
	strcat(buf, "\\gphoto\\settings");
#else
	sprintf(buf, "%s/.gphoto/settings", getenv("HOME"));
#endif

	if (verify_settings(buf) != GP_OK)
		/* verify_settings will unlink and recreate the settings file */
		return (GP_OK);
	GP_DEBUG ("Loading settings from file \"%s\"", buf);

	if ((f=fopen(buf, "r"))==NULL) {
		GP_DEBUG ("Can't open settings for reading");
		return(GP_ERROR);
	}

	rewind(f);
	while (!feof(f)) {
		strcpy(buf, "");
		fgets(buf, 1023, f);
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

	return (GP_OK);
}


static int
save_settings (void)
{
	FILE *f;
	char buf[1024];
	int x=0;

	sprintf (buf, "%s/.gphoto/settings", getenv ("HOME"));

	gp_log (GP_LOG_DEBUG, "gphoto2-setting",
		"Saving %i setting(s) to file \"%s\"",
		glob_setting_count, buf);

	if ((f=fopen(buf, "w+"))==NULL) {
		GP_DEBUG ("Can't open settings file for writing");
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
