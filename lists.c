#include <gpio.h>
#include <gphoto2.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "util.h"

CameraList *gp_list_new()
{
	CameraList *list;

	list = (CameraList*)malloc(sizeof(CameraList));
	if (!list)
		return NULL;

	memset(list, 0, sizeof(CameraList));
	return (list);
}

int gp_list_free(CameraList *list) {

	if (list)
		free(list);
	return (GP_OK);
}

int gp_list_append(CameraList *list, char *name, CameraListType type)
{
        strcpy(list->entry[list->count].name, name);
        list->entry[list->count].type = type;

        list->count += 1;

        return (GP_OK);
}

int gp_list_count(CameraList *list)
{
        return (list->count);
}

CameraListEntry *gp_list_entry(CameraList *list, int entrynum)
{
        return (&list->entry[entrynum]);
}

CameraAbilities *gp_abilities_new()
{
        CameraAbilities *abilities;

        abilities = (CameraAbilities*)malloc(sizeof(CameraAbilities));
        if (!abilities)
                return (NULL);

        return (abilities);
}

int gp_abilities_free(CameraAbilities *abilities)
{
        free(abilities);

        return (GP_OK);
}

CameraAbilitiesList *gp_abilities_list_new ()
{

        CameraAbilitiesList *list;

        list = (CameraAbilitiesList*)malloc(sizeof(CameraAbilitiesList));
        if (!list)
                return (NULL);
        list->count = 0;
        list->abilities = NULL;

        return (list);
}

int gp_abilities_list_free (CameraAbilitiesList *list)
{

        int x;

        for (x=0; x<list->count; x++)
                free (list->abilities[x]);
        free(list);

        return (GP_OK);
}

int gp_abilities_list_dump (CameraAbilitiesList *list)
{
        int x;

        for (x=0; x<list->count; x++) {
                gp_debug_printf(GP_DEBUG_LOW, "core", "Camera #%i:", x);
                gp_abilities_dump(list->abilities[x]);
        }

        return (GP_OK);
}

int gp_abilities_list_append (CameraAbilitiesList *list, CameraAbilities *abilities)
{
	if (!list->abilities)
	        list->abilities = (CameraAbilities**)malloc(
			sizeof(CameraAbilities*));
	   else
	        list->abilities = (CameraAbilities**)realloc(list->abilities,
	                sizeof(CameraAbilities*)*(list->count+1));
        if (!list->abilities)
                return (GP_ERROR_NO_MEMORY);

        list->abilities[list->count] = abilities;
        list->count += 1;

        return (GP_OK);
}
