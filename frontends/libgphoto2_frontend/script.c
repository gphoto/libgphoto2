#include <stdlib.h>
#include <stdio.h>
#include <gphoto2.h>

int gpfe_script (char *script_line) {

        printf("scripting: got %s\n", script_line);


        return (GP_OK);
}
