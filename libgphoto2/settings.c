#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gphoto2.h>

#include "settings.h"
#include "globals.h"
#include "util.h"

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
                        save_settings(glob_setting, glob_setting_count);
                        return (GP_OK);
                }
	}
        strcpy(glob_setting[glob_setting_count].id, id);
        strcpy(glob_setting[glob_setting_count].key, key);
        strcpy(glob_setting[glob_setting_count++].value, value);
        save_settings(glob_setting, glob_setting_count);

        return (GP_OK);
}

int verify_settings (char *settings_file) {

	FILE *f;
	char buf[1024];
	int x, equals;

	if ((f=fopen(settings_file, "r"))==NULL) {
		perror("Loading Settings");
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
				printf("Incorrect settings format. resetting\n");
				unlink(settings_file);
				return (GP_ERROR);
			}
		}
	}
	fclose (f);

	return (GP_OK);
}

int load_settings () {

	FILE *f;
	char buf[1024], *id, *key, *value;

	glob_setting_count = 0;
#ifdef WIN32
	GetWindowsDirectory(buf, 1024);
	strcat(buf, "\\gphoto\\settings");
#else
	sprintf(buf, "%s/.gphoto/settings", getenv("HOME"));
#endif

	if (verify_settings(buf) == GP_ERROR)
		/* verify_settings will unlink and recreate the settings file */
		return (GP_OK);
	if (glob_debug)
		printf("core: Loading settings from file \"%s\"\n", buf);

	if ((f=fopen(buf, "r"))==NULL) {
		perror("Loading Settings");
		return(0);
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
		dump_settings();

	return (GP_OK);
}


int save_settings () {

	FILE *f;
	char buf[1024];
	int x=0;

	sprintf(buf, "%s/.gphoto/settings", getenv("HOME"));

	if (glob_debug)
		printf("core: Saving settings to file \"%s\"\n", buf);

	if ((f=fopen(buf, "w+"))==NULL) {
		perror("Loading Settings");
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

int dump_settings () {

	int x;
	printf("core: All settings:\n");
	for (x=0; x<glob_setting_count; x++)
		printf("core:\t (%s) \"%s\" = \"%s\"\n", glob_setting[x].id,
			glob_setting[x].key,glob_setting[x].value);
	if (glob_setting_count == 0)
		printf("core:\tNone\n");
	printf("core: Total settings: %i\n", glob_setting_count);

	return (GP_OK);
}
