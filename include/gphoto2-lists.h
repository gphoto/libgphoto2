/* 	Header file for gPhoto2

	Author: Scott Fritzinger <scottf@unr.edu>

	This library is covered by the LGPL.
*/

CameraList*		gp_list_new	();
int			gp_list_free	(CameraList *list);

int			gp_list_count	(CameraList *list);
	
int			gp_list_append	(CameraList *list, char *name, CameraListType type);

CameraListEntry*	gp_list_entry	(CameraList *list, int entrynum);

CameraAbilities*	gp_abilities_new  ();
int 			gp_abilities_free (CameraAbilities *abilities);

CameraAbilitiesList*	gp_abilities_list_new  ();
int 			gp_abilities_list_free (CameraAbilitiesList *list);

int 			gp_abilities_list_dump (CameraAbilitiesList *list);

int 			gp_abilities_list_append (CameraAbilitiesList *list, 
						  CameraAbilities *abilities);
