#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <gphoto2.h>
#include "file.h"


CameraFile* gp_file_new () {

        /*
        Allocates a new CameraFile
        */

        CameraFile *f;

        f = (CameraFile*)malloc(sizeof(CameraFile));
        strcpy(f->type, "unknown/unknown");
        strcpy(f->name, "");
        f->data = NULL;
        f->size = 0;
        f->bytes_read = 0;

        return(f);
}

int gp_file_free (CameraFile *file) {

        /*
        frees a CameraFile from memory
        */

        gp_file_clean(file);
        free(file);
        return(GP_OK);
}

int gp_file_append (CameraFile *file, char *data, int size) {

        if (size < 0)
                return (GP_ERROR);

	if (!file->data)
	        file->data = (char*)malloc(sizeof(char) * (size));
	   else
	        file->data = (char*)realloc(file->data, sizeof(char) * (file->size + size));
        memcpy(&file->data[file->size], data, size);

        file->bytes_read = size;
        file->size += size;

        return (GP_OK);
}

int gp_file_get_last_chunk (CameraFile *file, char **data, int *size) {

        if (file->bytes_read == 0) {
                /* chunk_add was never called. return safely. */
                *data = NULL;
                *size = 0;
                return (GP_ERROR);
        }

        /* They must free the returned data data! */
        *data = (char*)malloc(file->bytes_read);
        memcpy(*data, &file->data[file->size - file->bytes_read], file->bytes_read);
        *size = file->bytes_read;

        return (GP_OK);
}

int gp_file_save (CameraFile *file, char *filename) {

        FILE *fp;

        if ((fp = fopen(filename, "wb"))==NULL)
                return (GP_ERROR);
        fwrite(file->data, (size_t)sizeof(char), (size_t)file->size, fp);
        fclose(fp);

        return (GP_OK);
}

int gp_file_open (CameraFile *file, char *filename) {

        FILE *fp;
        char *name, *dot;
        long size, size_read;

        gp_file_clean(file);

        fp = fopen(filename, "r");
        if (!fp)
                return (GP_ERROR);
        fseek(fp, 0, SEEK_END);
        size = ftell(fp);
        rewind(fp);

        file->data = (char*)malloc(sizeof(char)*(size + 1));
        if (!file->data)
                return (GP_ERROR);
        size_read = fread(file->data, (size_t)sizeof(char), (size_t)size, fp);
        if (ferror(fp)) {
                gp_file_clean(file);
                return (GP_ERROR);
        }
        fclose(fp);

        file->size = size_read;
        file->data[size_read] = 0;

        name = strrchr(filename, '/');
        if (name)
                strcpy(file->name, name + 1);
           else
                strcpy(file->name, filename);

/* needs MIME lookup!! */
        dot = strrchr(filename, '.');
        if (dot)
                sprintf(file->type, "image/%s", dot);
           else
                strcpy(file->type, "image/unknown");

        return (GP_OK);
}

int gp_file_clean (CameraFile *file) {

        /*
        frees a CameraFile's components, not the CameraFile itself.
        this is used to prep a CameraFile struct to be filled.
        */

        if (file->data != NULL)
                free(file->data);
        strcpy(file->name, "");
        file->data = NULL;
        file->size = 0;
        file->bytes_read = 0;
        return(GP_OK);
}
