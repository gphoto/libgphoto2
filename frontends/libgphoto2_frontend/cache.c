#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <gphoto2-frontend.h>


int gpfe_cache_put (int camera_number, int folder_number, int file_number, CameraFile *file) {

	FILE *fp;
	char buf[1024];

	sprintf(buf, "%s/.gphoto/cache/%i-%i-%i.%i", getenv("HOME"), 
		camera_number, folder_number, file_number, file->type);
#ifdef DEBUG
	printf("cache: putting \"%s\"... ", buf);
	fflush(stdout);
#endif
	fp = fopen(buf, "w");
	fwrite(file->data, (size_t)sizeof(char), (size_t)file->size, fp);
	fclose(fp);

#ifdef DEBUG
	printf("done!\n");
#endif 
	return (GP_OK);
}

int gpfe_cache_get (int camera_number, int folder_number, int file_number, CameraFile *file) {

	FILE *fp;
	DIR  *d;
	struct dirent *de;
	long int s;
	int camn, foldn, filen, filet;
	char buf[1024], cachedir[1024];

	sprintf(cachedir, "%s/.gphoto/cache", getenv("HOME"));
	d = opendir(cachedir);
#ifdef DEBUG
	printf("cache: getting (%i, %i, %i)... ", 
		camera_number, folder_number, file_number);
	fflush(stdout);
#endif

	do {
		camn  = -1;
		foldn = -1;
		filen = -1;
		/* Read each entry */
		de = readdir(d);
		if (de) {
			sscanf(de->d_name, "%i-%i-%i.%i", 
			       &camn, &foldn, &filen, &filet);
			if ((camn  == camera_number) &&
			    (foldn == folder_number) && 
			    (filen == file_number)) {
				sprintf(buf, "%s/%s", cachedir, de->d_name);
#ifdef DEBUG
	printf("found! (%s)\n", de->d_name);
#endif
				fp = fopen(buf, "w");
				fseek(fp, 0, SEEK_END);
				s = ftell(fp);
				rewind(fp);
				file->data = (char*)malloc(sizeof(char)*s);
				fread(file->data, (size_t)sizeof(char), 
					(size_t)file->size, fp);
				fclose(fp);
				file->size = s;
				file->type = filet;
				return (GP_OK);
			}
		}
	} while (de);
#ifdef DEBUG
	printf("error! didn't exist!\n");
#endif 
	return (GP_ERROR);
}

int gpfe_cache_delete (int camera_number, int folder_number, int file_number) {

	/* if file_number   == -1, all the files in the folder get deleted */
	/* if folder_number == -1, all the folders for camera get deleted */
	/* if camera_number == -1, ALL the camera's files get delete */

	DIR  *d;
	struct dirent *de;
	int camn, foldn, filen, filet;
	char buf[1024], cachedir[1024];

#ifdef DEBUG
	printf("cache: Deleting (%i, %i, %i)...\n", 
		camera_number, folder_number, file_number);
	fflush(stdout);
#endif
	sprintf(cachedir, "%s/.gphoto/cache", getenv("HOME"));
	d = opendir(cachedir);
	do {
		camn  = -2;
		foldn = -2;
		filen = -2;
		/* Read each entry */
		de = readdir(d);
		if (de) {
			sscanf(de->d_name, "%i-%i-%i.%i", 
			       &camn, &foldn, &filen, &filet);
			if ((camn>=0)&&(foldn>=0)&&(filen>=0)) {
				sprintf(buf, "%s/%s", cachedir, de->d_name);
				if (((camn  == camera_number) &&
				     (foldn == folder_number) &&
				     (filen == file_number))
					||
				    ((camn  == camera_number) &&
				     (foldn == folder_number) &&
				     (file_number == -1))
					||
				    ((camn  == camera_number) &&
				     (folder_number == -1))
					||
				     (camera_number == -1)) {
#ifdef DEBUG
					printf("cache: Removing %s\n", buf);
#endif
					unlink(buf);
				} else {
#ifdef DEBUG
					printf("cache: Leaving %s\n", buf);
#endif
				}

			}
			
		}
	} while (de);

	return (GP_OK);
}

int gpfe_cache_exists (int camera_number, int folder_number, int file_number) {

	DIR  *d;
	struct dirent *de;
	int camn, foldn, filen, filet;
	char cachedir[1024];

#ifdef DEBUG
	printf("cache: looking for (%i, %i, %i)... ", 
		camera_number, folder_number, file_number);
	fflush(stdout);
#endif

	sprintf(cachedir, "%s/.gphoto/cache", getenv("HOME"));
	d = opendir(cachedir);
	do {
		camn  = -1;
		foldn = -1;
		filen = -1;
		/* Read each entry */
		de = readdir(d);
		if (de) {
			sscanf(de->d_name, "%i-%i-%i.%i", 
			       &camn, &foldn, &filen, &filet);
			if ((camn == camera_number) &&
			    (foldn == folder_number) && 
			    (filen == file_number)) {
#ifdef DEBUG
	printf("found! (%s)\n", de->d_name);
#endif
				return (GP_OK);
			}
		}
	} while (de);

#ifdef DEBUG
	printf("missing!\n");
#endif

	return (GP_ERROR);
}
