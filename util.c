#include <stdlib.h>
#include <gphoto2.h>
#include "util.h"

int gp_free_abilities (CameraAbilities *abilities) {

	int x=0;

	while (abilities->model[x])
		free(abilities->model[x++]);

	return GP_OK;
}
