#include <string.h>
#include <stdio.h>
#include <gphoto2.h>

CameraFilesystemFile *gp_filesystem_entry_new (char *filename) {

        CameraFilesystemFile *fse;

        fse = (CameraFilesystemFile*)malloc(sizeof(CameraFilesystemFile));

        if (!fse)
                return (NULL);

        strcpy(fse->name, filename);

        return (fse);
}

CameraFilesystem *gp_filesystem_new() {

        CameraFilesystem *fs;

        fs = (CameraFilesystem*)malloc(sizeof(CameraFilesystem));

        if (!fs)
                return (NULL);

        fs->folder = NULL;
        fs->count = 0;

        return (fs);
}

int gp_filesystem_free(CameraFilesystem *fs) {

        gp_filesystem_format(fs);

        free(fs);

        return (GP_OK);
}

int gp_filesystem_append (CameraFilesystem *fs, char *folder, char *filename) {

	int x, y, folder_num=-1, file_num;

	for (x=0; x<fs->count; x++) {
                if (strcmp(fs->folder[x]->name, folder) == 0) {
			folder_num = x;
                        /* If file exists, return error */
                        for (y=0; y<fs->folder[x]->count; y++)
				if (strcmp(fs->folder[x]->file[y]->name, filename)==0)
					return (GP_ERROR);
                }
	}

	if (folder_num >= 0) {
	        /* Allocate a new file in that folder */
	        fs->folder[folder_num]->file = (CameraFilesystemFile**)realloc(
			fs->folder[folder_num]->file,
			sizeof(CameraFilesystemFile*)*(fs->folder[folder_num]->count+1));
	        if (!fs->folder[folder_num]->file)
	                return (GP_ERROR);
		file_num = fs->folder[folder_num]->count;
	} else {
                /* Allocate the folder pointer */
                fs->folder = (CameraFilesystemFolder**)realloc(fs->folder,
               		sizeof(CameraFilesystemFolder*)*(fs->count+1));
                if (!fs->folder)
                        return (GP_ERROR);

	        /* Allocate the actual folder */
	        fs->folder[fs->count] = (CameraFilesystemFolder*)malloc(
        	        sizeof(CameraFilesystemFolder));

	        if (!fs->folder[fs->count])
	                return (GP_ERROR);

		/* Initialize the folder */
	        strcpy(fs->folder[fs->count]->name, folder);
		fs->folder[fs->count]->count = 0;

		folder_num = fs->count;
		fs->count += 1;
		file_num = 0;

	        /* Allocate a file in that folder */
	        fs->folder[folder_num]->file = (CameraFilesystemFile**)malloc(
			sizeof(CameraFilesystemFile*));
	
	        if (!fs->folder[folder_num]->file)
	                return (GP_ERROR);
	} 

	/* Append the file */
	fs->folder[folder_num]->file[file_num] = gp_filesystem_entry_new(filename);
	fs->folder[folder_num]->count += 1;

	return (GP_OK);
}

int gp_filesystem_populate (CameraFilesystem *fs, char *folder, char *format, int count) {

        int x, y, new_folder=-1;
        char buf[1024];

        for (x=0; x<fs->count; x++) {
                if (strcmp(fs->folder[x]->name, folder) == 0) {
                        /* If folder already populated, free it */
                        for (y=0; y<fs->folder[x]->count; y++)
                                free (fs->folder[x]->file[y]);
                        free(fs->folder[x]);
                        new_folder = x;
                }
        }

        if (new_folder == -1) {
                /* Allocate the folder pointer */
                fs->folder = (CameraFilesystemFolder**)realloc(fs->folder,
               		sizeof(CameraFilesystemFolder*)*(fs->count+count));
                if (!fs->folder)
                        return (GP_ERROR);
                new_folder = fs->count;
                fs->count += 1;
        }

        /* Allocate the actual folder */
        fs->folder[new_folder] = (CameraFilesystemFolder*)malloc(
                sizeof(CameraFilesystemFolder));

        if (!fs->folder[new_folder])
                return (GP_ERROR);

        strcpy(fs->folder[new_folder]->name, folder);

        /* Allocate the files in that folder */
        fs->folder[new_folder]->file = (CameraFilesystemFile**)malloc(
                sizeof(CameraFilesystemFile*)*count);

        if (!fs->folder[new_folder]->file)
                return (GP_ERROR);

        /* Populate the folder with files */
        for (x=0; x<count; x++) {
                sprintf(buf, format, x+1);
                fs->folder[new_folder]->file[x] = gp_filesystem_entry_new(buf);
        }
        fs->folder[new_folder]->count = count;

        return (GP_OK);
}

int gp_filesystem_count (CameraFilesystem *fs, char *folder) {

        int x;
        for (x=0; x<fs->count; x++) {
           if (strcmp(fs->folder[x]->name, folder)==0)
                return (fs->folder[x]->count);
        }

        return (GP_ERROR);
}

int gp_filesystem_delete (CameraFilesystem *fs, char *folder, char *filename) {

        int x,y, shift=0;

        for (x=0; x<fs->count; x++) {
           if (strcmp(fs->folder[x]->name, folder)==0) {
                for (y=0; y<fs->folder[x]->count; y++) {
                        if (strcmp(fs->folder[x]->file[y]->name, filename)==0)
                                shift = 1;
                        if ((shift)&&(y<fs->folder[x]->count-1))
                                memcpy( &fs->folder[x]->file[y],
                                        &fs->folder[x]->file[y+1],
                                        sizeof(fs->folder[x]->file[y]));
                }
                if (shift)
                        fs->folder[x]->count -= 1;
           }
        }

        if (!shift)
                return (GP_ERROR);

        return (GP_OK);

}

int gp_filesystem_format (CameraFilesystem *fs) {

        int x, y;

        if (fs->folder) {
                for (x=0; x<fs->count; x++) {
                        for (y=0; y<fs->folder[x]->count; y++)
                                free (fs->folder[x]->file[y]);
                        free(fs->folder[x]);
                }
                free(fs->folder);
        }
        fs->count = 0;

        return (GP_OK);
}

char *gp_filesystem_name (CameraFilesystem *fs, char *folder, int filenumber) {

        int x;

        for (x=0; x<fs->count; x++) {
                if (strcmp(fs->folder[x]->name, folder)==0) {
                        if (filenumber > fs->folder[x]->count)
                                return (NULL);
                        return (fs->folder[x]->file[filenumber]->name);
                }
        }
        return (NULL);
}

int gp_filesystem_number (CameraFilesystem *fs, char *folder, char *filename) {

        int x, y;

        for (x=0; x<fs->count; x++) {
                if (strcmp(fs->folder[x]->name, folder)==0) {
                        for (y=0; y<fs->folder[x]->count; y++) {
                                if (strcmp(fs->folder[x]->file[y]->name, filename)==0)
                                        return (y);
                        }

                }
        }

        return (GP_ERROR);
}

int gp_filesystem_dump (CameraFilesystem *fs) {
        /* debug dump the filesystem */

        int x, y;

        printf("core: CameraFilesystem dump:\n");

        for (x=0; x<fs->count; x++) {
                printf("core:\t%s\n",fs->folder[x]->name);
                for (y=0; y<fs->folder[x]->count; y++)
                        printf("core:\t\t%i %s\n", y, fs->folder[x]->file[y]->name);
        }

        return (GP_OK);
}
