#include <stdlib.h>
#include <stdio.h>
#include <gphoto2-datatypes.h>
#include <gphoto2-frontend.h>

int gpfe_script (char *script_line) {

        printf("scripting: got %s\n", script_line);


        return (GP_OK);
}
