#include <gphoto2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "core.h"
#include "settings.h"

extern Setting	glob_setting[];
extern int	glob_setting_count;

int load_settings () {

	FILE *f;
	char buf[1024], *c;

	glob_setting_count = 0;

	sprintf(buf, "%s/.gphoto/settings", getenv("HOME"));
#ifdef DEBUG
	printf("core: Loading settings from file \"%s\"\n", buf);
#endif
	if ((f=fopen(buf, "a+"))==NULL) {
		perror("Loading Settings");
		return(0);
	}
	rewind(f);
	while (!feof(f)) {
		strcpy(buf, "");
		fgets(buf, 1023, f);
		if (strlen(buf)>3) {
			buf[strlen(buf)-1] = '\0';
			c = strtok(buf, "=");
			strcpy(glob_setting[glob_setting_count].key, c);
			c = strtok(NULL, "=");
			strcpy(glob_setting[glob_setting_count++].value, c);
		}
	}
#ifdef DEBUG
	dump_settings();
#endif

	return (GP_OK);
}


int save_settings () {

	FILE *f;
	char buf[1024];
	int x=0;

	sprintf(buf, "%s/.gphoto/settings", getenv("HOME"));
#ifdef DEBUG
	printf("core: Saving settings to file \"%s\"\n", buf);
#endif
	if ((f=fopen(buf, "w+"))==NULL) {
		perror("Loading Settings");
		return(0);
	}
	rewind(f);
	while (x < glob_setting_count) {
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
		printf("core:\t\"%s\" = \"%s\"\n",
			glob_setting[x].key,glob_setting[x].value);
	if (glob_setting_count == 0)
		printf("core:\tNone\n");
	printf("core: Total settings: %i\n", glob_setting_count);

	return (GP_OK);
}
