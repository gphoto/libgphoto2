/* setting.c
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

#include "gphoto2-setting.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gphoto2-result.h"
#include "gphoto2-core.h"

typedef struct {
	/* key = value settings */
	char id[256];
	char key[256];
	char value[256];
} Setting;

/* Currently loaded settings */
int             glob_setting_count = 0;
Setting         glob_setting[512];

extern int glob_debug;

static int save_settings (void);
static int dump_settings (void);

int gp_setting_get (char *id, char *key, char *value)
{
        int x;

	if (!id || !key)
		return (GP_ERROR);

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

int gp_setting_set (char *id, char *key, char *value)
{
        int x;

	gp_debug_printf(GP_DEBUG_LOW, "core", "(%s) Setting key \"%s\" to value \"%s\"",
                        id,key,value);

	if (!id || !key)
		return (GP_ERROR);

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
		gp_debug_printf(GP_DEBUG_LOW, "core", "Can't open settings file for reading");
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
				gp_debug_printf(GP_DEBUG_LOW, "core", "Incorrect settings format. resetting\n");
				unlink(settings_file);
				return (GP_ERROR);
			}
		}
	}
	fclose (f);

	return (GP_OK);
}

int load_settings (void)
{
	FILE *f;
	char buf[1024], *id, *key, *value;

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
	gp_debug_printf(GP_DEBUG_LOW, "core", "Loading settings from file \"%s\"", buf);

	if ((f=fopen(buf, "r"))==NULL) {
		gp_debug_printf(GP_DEBUG_LOW, "core", "Can't open settings for reading");
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
	if (glob_debug)
		dump_settings ();

	return (GP_OK);
}


static int
save_settings (void)
{
	FILE *f;
	char buf[1024];
	int x=0;

	sprintf(buf, "%s/.gphoto/settings", getenv("HOME"));

	gp_debug_printf(GP_DEBUG_LOW, "core", "Saving settings to file \"%s\"", buf);

	if ((f=fopen(buf, "w+"))==NULL) {
		gp_debug_printf(GP_DEBUG_LOW, "core", "Can't open settings file for writing");
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
