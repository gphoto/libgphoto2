/****************************************************************************
 * 
 * canon.c
 *
 *   Canon Camera library for the gphoto project,
 *   (c) 1999 Wolfgang G. Reissnegger
 *   Developed for the Canon PowerShot A50
 *   Additions for PowerShot A5 by Ole W. Saastad
 *   (c) 2000 : Other additions  by Edouard Lafargue, Philippe Marzouk.
 *
 ****************************************************************************/


/****************************************************************************
 *
 * include files
 *
 ****************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>
#include <time.h>
#include <ctype.h>
#include <gtk/gtk.h>

#include <gphoto2.h>
#include <gpio/gpio.h>

#include "util.h"
#include "serial.h"
#include "psa50.h"
#include "canon.h"

/*
 * Directory access may be rather expensive, so we cache some information.
 * The first variable in each block indicates whether the block is valid.
 */

static int cached_ready = 0;

static int cached_disk = 0;
static char cached_drive[10]; /* usually something like C: */
static int cached_capacity,cached_available;

static int cached_dir = 0;
static struct psa50_dir *cached_tree;
static int cached_images;

static char **cached_paths; /* only used by A5 */

/* Global variable to set debug level.                        */
/* Defined levels so far :                                    */
/*   0 : no debug output                                      */
/*   1 : debug actions                                        */
/*   9 : debug everything, including hex dumps of the packets */

int canon_debug_driver = 0;
int gphoto2_debug = 0;

/* DO NOT FORGET to update the NUM_ constants after adding a camera */

#define NUM_SERIAL      4
#define NUM_USB         2
#define NUM_SERIAL_USB  2

char *models_serial[] = {
	"Canon PowerShot A5",
	"Canon PowerShot A5 Zoom",
	"Canon PowerShot A50",
	"Canon PowerShot Pro70",
	NULL
};
char *models_USB[] = {
	"Canon PowerShot IXUS",
	"Canon PowerShot S100",
	NULL
};
char *models_serialandUSB[] = {
	"Canon PowerShot S10",
	"Canon PowerShot S20",
	NULL
};

int camera_id(char *id)
{
        strcpy(id, "canon");

        return GP_OK;
}

int camera_manual(CameraText *manual)
{
        strcpy(manual->text, "Manual Not Available");

        return GP_OK;
}

int camera_abilities(CameraAbilities *abilities, int *count)
{
	int x=0;
	int i;
	CameraAbilities a;

	strcpy(a.model, "");
	a.serial     = 1;
	a.parallel   = 0;
	a.usb        = 1;
	a.ieee1394   = 0;
	a.speed[0]   = 9600;
	a.speed[1]   = 19200;
	a.speed[2]   = 38400;
	a.speed[3]   = 57600;
	a.speed[4]   = 115200;
	a.speed[5]   = 0;
	a.capture    = 0;
	a.config     = 0;
	a.file_delete = 1;
	a.file_preview = 1;
	a.file_put = 0;

	i=0;
	while (i<NUM_SERIAL_USB) {
		memcpy(&abilities[x], &a, sizeof(a));
		strcpy(abilities[x].model, models_serialandUSB[i]);
		x++;
		i++;
	}
	
	i=0;
	a.serial     = 0;
	while (i<NUM_USB) {
		memcpy(&abilities[x], &a, sizeof(a));
		strcpy(abilities[x].model, models_USB[i]);
		x++;
		i++;
	}
	
	i=0;
	a.serial     = 1;
	a.usb        = 0;
	while (i<NUM_SERIAL) {
		memcpy(&abilities[x], &a, sizeof(a));
		strcpy(abilities[x].model, models_serial[i]);
		x++;
		i++;
	}


	*count = x;

	return GP_OK;
}



int camera_folder_list(char *folder_name, CameraFolderInfo *list)
{
	return GP_OK;
}

int camera_folder_set(char *folder_name)
{
	return GP_OK;
}

static void clear_readiness(void)
{
    cached_ready = 0;
}

static int check_readiness(void)
{
    if (cached_ready) return 1;
    if (psa50_ready()) {
      debug_message("Camera type:  %d\n",camera.model);
      cached_ready = 1;
      return 1;
    }
    gp_interface_status("Camera unavailable");
    return 0;
}

void switch_camera_off(void)
{
  psa50_off();
  clear_readiness();
}

int camera_exit(void)
{
	switch_camera_off();
	return GP_OK;
}

int canon_get_batt_status(int *pwr_status, int *pwr_source) {
  if (!check_readiness())
    return -1;

  return psa50_get_battery(pwr_status, pwr_source);
}

static void canon_set_owner(GtkWidget *win,GtkWidget *owner)
{
    char *entry;

    gp_interface_status("Setting owner name.");
    entry = gtk_entry_get_text(GTK_ENTRY(owner));
    debug_message("New owner name: %s\n",entry);
    psa50_set_owner_name(entry);
}


char *camera_model_string()
{
  if (!check_readiness())
    return "Camera unavailable";

  switch (camera.model) {
  case CANON_PS_A5:
    return "Powershot A5";
  case CANON_PS_A5_ZOOM:
    return "Powershot A5 Zoom";
    case CANON_PS_A50:
    return "Powershot A50";
  case CANON_PS_A70:
    return "Powershot Pro70";
  case CANON_PS_S10:
    return "Powershot S10";
  case CANON_PS_S20:
    return "Powershot S20";
  default:
    return "Unknown model !";
  }

}


static int update_disk_cache(void)
{
    char root[10]; /* D:\ or such */
    char *disk;

    if (cached_disk) return 1;
    if (!check_readiness()) return 0;
    disk = psa50_get_disk();
    if (!disk) {
	gp_interface_status("No response");
	return 0;
    }
    strcpy(cached_drive,disk);
    sprintf(root,"%s\\",disk);
    if (!psa50_disk_info(root,&cached_capacity,&cached_available)) {
	gp_interface_status("No response");
	return 0;
    }
    cached_disk = 1;
    return 1;
}


static int is_image(const char *name)
{
    const char *pos;

    pos = strchr(name,'.');
    if (!pos) return 0;
    return !strcmp(pos,".JPG");
}

/* This function is only used by A5 */

static int recurse(const char *name)
{
    struct psa50_dir *dir,*walk;
    char buffer[300]; /* longest path, etc. */
    int count,curr;

    dir = psa50_list_directory(name);
    if (!dir) return 1; /* assume it's empty @@@ */
    count = 0;
    for (walk = dir; walk->name; walk++)
	if (walk->size && is_image(walk->name)) count++;
    cached_paths = realloc(cached_paths,sizeof(char *)*(cached_images+count+1));
    memset(cached_paths+cached_images+1,0,sizeof(char *)*count);
    if (!cached_paths) {
	perror("realloc");
	return 0;
    }
    curr = cached_images;
    cached_images += count;
    for (walk = dir; walk->name; walk++) {
	sprintf(buffer,"%s\\%s",name,walk->name);
	if (!walk->size) {
	    if (!recurse(buffer)) return 0;
	}
	else {
	    if (!is_image(walk->name)) continue;
	    curr++;
	    cached_paths[curr] = strdup(buffer);
	    if (!cached_paths[curr]) {
		perror("strdup");
		return 0;
	    }
	}
    }
    free(dir);
    return 1;
}


static int comp_dir(const void *a,const void *b)
{
    return strcmp(((const struct psa50_dir *) a)->name,
      ((const struct psa50_dir *) b)->name);
}


/* This function is only used by A50 */

static struct psa50_dir *dir_tree(const char *path)
{
    struct psa50_dir *dir,*walk;
    char buffer[300]; /* longest path, etc. */

    dir = psa50_list_directory(path);
    if (!dir) return NULL; /* assume it's empty @@@ */
    for (walk = dir; walk->name; walk++) {
	if (walk->is_file) {
	    if (is_image(walk->name)) cached_images++;
	}
	else {
	    sprintf(buffer,"%s\\%s",path,walk->name);
	    walk->user = dir_tree(buffer);
	}
    }
    qsort(dir,walk-dir,sizeof(*dir),comp_dir);
    return dir;
}


static void clear_dir_cache(void)
{
    psa50_free_dir(cached_tree);
}


/* A5 only: sort THB_ and AUT_ into their proper arrangement. */
static int compare_a5_paths (const void * p1, const void * p2)
{
  const char * s1 = *((const char **)p1);
  const char * s2 = *((const char **)p2);
  const char * ptr, * base1, * base2;
  int n1 = 0, n2 = 0;

  printf("Comparing %s to %s\n", s1, s2);

  ptr = strrchr(s1, '_');
  if (ptr)
    n1 = strtol(ptr+1, 0, 10);
  ptr = strrchr(s2, '_');
  if (ptr)
    n2 = strtol(ptr+1, 0, 10);

  printf("Numbers are %d and %d\n", n1, n2);
  
  if (n1 < n2)
    return -1;
  else if (n1 > n2)
    return 1;
  else {
    base1 = strrchr(s1, '\\');
    base2 = strrchr(s2, '\\');
    printf("Base 1 is %s and base 2 is %s\n", base1, base2);
    return strcmp(base1, base2);
  }
}


static int update_dir_cache(void)
{
    int i;
    if (cached_dir) return 1;
    if (!update_disk_cache()) return 0;
    if (!check_readiness()) return 0;
    cached_images = 0;
    switch (camera.model) {
    case CANON_PS_A5:
    case CANON_PS_A5_ZOOM:
      if (recurse(cached_drive)) {
	printf("Before sort:\n");
	for (i = 1; i < cached_images; i++) {
	  printf("%d: %s\n", i, cached_paths[i]);
	}
	qsort(cached_paths+1, cached_images, sizeof(char *), compare_a5_paths);
	printf("After sort:\n");
	for (i = 1; i < cached_images; i++) {
	  printf("%d: %s\n", i, cached_paths[i]);
	}
	cached_dir = 1;
	return 1;
      }
      clear_dir_cache();
      return 0;
      break;

    default:  /* A50 or S10 or other */
      cached_tree = dir_tree(cached_drive);
      if (!cached_tree) return 0;
      cached_dir = 1;
      return 1;
      break;
    }
}

/**
 * setPathName was borrowed from the konica driver
 */
static void setPathName(char *fname)
{
  char *sp;
  int   flen;
  sp = getenv("HOME");
  if (!sp)
    sp = ".";
  strcpy(fname, sp);
  flen = strlen(fname);
  while (fname[flen-1] == '/')
    {
      fname[flen-1] = '\0';
      flen--;
    }
  if (!strstr(fname, "/.gphoto"))
      strcat(fname, "/.gphoto");
}
/**
 * setFileName was borrowed from the Konica driver
 */
static void setFileName(char *fname)
{
   setPathName(fname);
   strcat(fname, "/powershotrc");
}



/****************************************************************************
 *
 * gphoto library interface calls
 *
 ****************************************************************************/


static int _entry_path(const struct psa50_dir *tree,
  const struct psa50_dir *entry,char *path)
{
    path = strchr(path,0);
    while (tree->name) {
	*path = '\\';
	strcpy(path+1,tree->name);
	if (tree == entry) return 1;
	if (!tree->is_file && tree->user)
	    if (_entry_path(tree->user,entry,path)) return 1;
	tree++;
    }
    return 0;
}


static char *entry_path(const struct psa50_dir *tree,
  const struct psa50_dir *entry)
{
    static char path[300];

    strcpy(path,cached_drive);
    (void) _entry_path(cached_tree,entry,path);
    return path;
}


static void cb_select(GtkItem *item,struct psa50_dir *entry)
{
    char *path;
    unsigned char *file;
    int length,size;
    int fd;

    if (!entry || !entry->is_file) {
	gtk_item_deselect(item);
	return;
    }
    path = entry_path(cached_tree,entry);
    gp_interface_status(path);
    file = psa50_get_file(path,&length);
    if (!file) return;
    fd = creat(entry->name,0644);
    if (fd < 0) {
	perror("creat");
	free(file);
	return;
    }
    size = write(fd,file,length);
    if (size < 0) perror("write");
    else if (size < length) debug_message("short write: %d/%d\n",size,length);
    if (close(fd) < 0) perror("close");
    free(file);
    gp_interface_status("File saved");
}

static int get_all(struct psa50_dir *entry) {

  if (  entry->is_file) {
    cb_select(NULL, entry);
    return 1;
  }
  entry = entry->user;
  if (!entry) return 1;
  for (; entry->name; entry++)
    if (!get_all(entry)) return 0;
  return 1; 

}

static void cb_get_all(GtkItem *item) {
  
  get_all(cached_tree);

}


static int populate(struct psa50_dir *entry,GtkWidget *branch)
{
    GtkWidget *item,*subtree;

    item = gtk_tree_item_new_with_label(entry ? (char *) entry->name :
      cached_drive);
    if (!item) return 0;
    gtk_tree_append(GTK_TREE(branch),item);
    gtk_widget_show(item);
    gtk_signal_connect(GTK_OBJECT(item),"select",GTK_SIGNAL_FUNC(cb_select),
      entry);
    if (entry && entry->is_file) {
	entry->user = item;
	return 1;
    }
    entry = entry ? entry->user : cached_tree;
    if (!entry) return 1;
    subtree = gtk_tree_new();
    if (!subtree) return 0;
    gtk_tree_item_set_subtree(GTK_TREE_ITEM(item),subtree);
    gtk_tree_item_expand(GTK_TREE_ITEM(item));
    for (; entry->name; entry++)
	if (!populate(entry,subtree)) return 0;
    return 1;
}


static void cb_clear(GtkWidget *widget,GtkWidget *window)
{
  //    gtk_widget_destroy(window);

    cached_ready = 0;
    cached_disk = 0;
    if (cached_dir) clear_dir_cache();
    cached_dir = 0;
}


/**
 * Save current driver setup into a settings file (~/.gphoto/powershotrc)
 */
void save_rcfile(void) {
  char  fname[128];
  FILE *fd;

  setFileName(fname);
  fd = fopen(fname, "w");
  if (!fd) {
    char cmd[140];
    setPathName(fname);
    sprintf(cmd, "mkdir %s", fname);
    system(cmd);
    setFileName(fname);
    fd = fopen(fname, "w");
  }
  if (fd) {
    struct tm *tp;
    time_t    mytime;
    char *speed;
    mytime = time(NULL);
    tp = localtime(&mytime);
    fprintf(fd, "#  Canon Powershot configuration - saved on %4.4d/%2.2d/%2.2d at %2.2d:%2.2d\n\n",
	    tp->tm_year+1900, tp->tm_mon+1, tp->tm_mday,
	    tp->tm_hour, tp->tm_min);
    switch (camera.speed) {
    case 19200:
      speed = "19200"; break;
    case 38400:
      speed = "38400"; break;
    case 57600:
      speed = "57600"; break;
    case 115200:
      speed = "115200"; break;
    default:
      speed = "9600";
    }
    fprintf(fd, "# Serial port speed\n");
    fprintf(fd, "%-12.12s %s\n", "Speed", speed);
    fprintf(fd, "\n\n# Debug level:\n"
	        "# 0: no debug at all\n"
	        "# 1: debug functions\n"
   	        "# 9: debug everything, including hex. dumps of packets\n\n");
    fprintf(fd, "%-12.12s %i\n", "Debug", canon_debug_driver);
    
    fclose(fd);
    gp_interface_status("Saved configuration");
  } else
    debug_message("Unable to open/create %s - configuration not saved\n",
	   fname);
}

/**
 * Configuration dialog for Canon cameras.
 *
 * This dialog tries to get information from the camera, but in case
 * the camera does not answer, it should show anyway.
 *
 * Main settings available there:
 *
 * - Compact Flash card contents with download
 * - Speed select
 * - Date get/set
 * - Owner name get/set
 * - AC status (battery/AC)
 * - Firmware revision
 */

static int canon_configure(void)
{

  //    GtkWidget *window,*box,*scrolled_win,*tree,*clear,*done;
  GtkWidget *dialog, *hbox, *vbox, *label, *tree, *vseparator;
  GtkWidget *file_list, *button, *cbutton, *clear,*set_button;
  GtkWidget *combo, *swoff, *sync, *linkCombo;
  GtkWidget *owner_entry, *ga;
  GList *list;

  char cam_model[80];
  char cam_date[34];
  char firmwrev[48];
  char power_stats[48];
  struct tm *camtm;
  time_t camtime;
  char *csp;
  int pwr_status, pwr_source;



  // First create the dialog window, with title, position and size:
  dialog = gtk_dialog_new();
  gtk_window_set_title(GTK_WINDOW(dialog), "Camera Setup");
  gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER);
  gtk_container_border_width(GTK_CONTAINER(dialog), 5);
  gtk_widget_set_usize(dialog, 500, 350);


  /* Box going across the dialog... */
  hbox = gtk_hbox_new(FALSE, 5);
  gtk_widget_show(hbox);
  gtk_box_pack_start_defaults(GTK_BOX(GTK_DIALOG(dialog)->vbox),
                             hbox);

  /* Vertical box holding the file browser */
  vbox = gtk_vbox_new(FALSE, 0);
  gtk_widget_show(vbox);
  gtk_box_pack_start_defaults(GTK_BOX(hbox), vbox);

  label = gtk_label_new("CF card file browser:");
  gtk_misc_set_alignment (GTK_MISC (label), 0, 0);
  gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);
  gtk_widget_show(label);

  file_list = gtk_scrolled_window_new(NULL,NULL);
  if (!file_list) {
    gtk_widget_destroy(dialog);
    return 0;
  }
  gtk_container_border_width(GTK_CONTAINER(file_list), 5);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(file_list),
                                GTK_POLICY_AUTOMATIC,GTK_POLICY_AUTOMATIC);
  gtk_widget_set_usize(file_list,200,400);
  gtk_widget_show(file_list);
  gtk_box_pack_start(GTK_BOX(vbox),file_list,TRUE,TRUE,FALSE);


  clear = gtk_button_new_with_label("Clear list");
  if (!clear) {
    gtk_widget_destroy(dialog);
    return 0;
  }
  
  gtk_signal_connect(GTK_OBJECT(clear),"clicked",GTK_SIGNAL_FUNC(cb_clear),
                    dialog);
  gtk_widget_show(clear);
  gtk_box_pack_start(GTK_BOX(vbox),clear,FALSE,FALSE,5);

  ga = gtk_button_new_with_label("Get All");
  if (!ga) {
    gtk_widget_destroy(dialog);
    return 0;
  }
  
  gtk_signal_connect(GTK_OBJECT(ga),"clicked",GTK_SIGNAL_FUNC(cb_get_all),
                    dialog);
  gtk_widget_show(ga);
  gtk_box_pack_start(GTK_BOX(vbox),ga,FALSE,FALSE,5);

  /* Create the tree object that will hold the directory
   * contents, and fill it.
   */
  tree = gtk_tree_new();
  if (!tree) {
    gtk_widget_destroy(dialog);
    return 0;
  }
  gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(file_list),
                                       tree);
  gtk_widget_show(tree);
  populate(NULL,tree);


  /* The the second half of the dialog: camera model, date and time,
   * etc...
   */
  vseparator = gtk_vseparator_new();
  gtk_widget_show(vseparator);
  gtk_box_pack_start(GTK_BOX(hbox), vseparator, FALSE, FALSE, 2);

  vbox = gtk_vbox_new(FALSE, 0);
  gtk_widget_show(vbox);
  gtk_box_pack_start(GTK_BOX(hbox), vbox, FALSE, FALSE, 2);

  /* Get the camera model string. This will initiate serial traffic
   * if the user did not get the index before. */
  strcpy(cam_model,"Camera model: ");
  strncat(cam_model, camera_model_string(),60);
  label = gtk_label_new(cam_model);
  gtk_misc_set_alignment (GTK_MISC (label), 0, 0);
  gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);
  gtk_widget_show(label);

  /**
   * Get the owner name, and put it in a text field.
   */
  hbox = gtk_hbox_new(FALSE, 2);
  gtk_widget_show(hbox);
  gtk_box_pack_start(GTK_BOX(vbox),hbox,FALSE,FALSE,0);
  label = gtk_label_new("Owner name: ");
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
  gtk_widget_show(label);
  
  owner_entry = gtk_entry_new_with_max_length(30);
  gtk_entry_set_text(GTK_ENTRY(owner_entry),camera.owner);
  gtk_box_pack_start(GTK_BOX(hbox), owner_entry, FALSE, FALSE, 0);
  gtk_widget_show(owner_entry);

  set_button = gtk_button_new_with_label("Set");
  gtk_box_pack_start(GTK_BOX(hbox), set_button, FALSE, FALSE, 1);
  gtk_widget_show(set_button);
  gtk_signal_connect(GTK_OBJECT(set_button),"clicked",GTK_SIGNAL_FUNC(canon_set_owner),
                    owner_entry);
  gtk_widget_show(label);

  label = gtk_label_new(" ");
  //  gtk_misc_set_alignment (GTK_MISC (label), 0, 0);
  gtk_widget_show(label);
  gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 1);

  if (cached_ready) {
    strcpy(cam_date,"Date: ");
    camtime = psa50_get_time();
    camtm = gmtime(&camtime);
    strncat(cam_date, asctime(camtm),26);
  }
  else
    strcpy(cam_date,"Date: camera unavailable");
  label = gtk_label_new(cam_date);

  gtk_misc_set_alignment (GTK_MISC (label), 0, 0);
  gtk_widget_show(label);
  gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 1);

  sprintf(firmwrev,"Firmware revision: %i.%i.%i.%i",
	  camera.firmwrev[3],camera.firmwrev[2],
	  camera.firmwrev[1],camera.firmwrev[0] );
  label = gtk_label_new(firmwrev);
  gtk_misc_set_alignment (GTK_MISC (label), 0, 0);
  gtk_widget_show(label);
  gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 1);


  if (cached_ready) {
    strcpy(power_stats,"Power: ");
    canon_get_batt_status(&pwr_status,&pwr_source);
    switch (pwr_source) {
    case CAMERA_ON_AC:
      strcat(power_stats, "AC adapter ");
      break;
    case CAMERA_ON_BATTERY:
	strcat(power_stats, "on battery ");
	break;
    default:
	sprintf(power_stats,"Power: unknown (%i",pwr_source);
	break;
    }
    switch (pwr_status) {
	char cde[16];
    case CAMERA_POWER_OK:
      strcat(power_stats, "(power OK)");
      break;
    case CAMERA_POWER_BAD:
	strcat(power_stats, "(power low)");
	break;
    default:
	sprintf(cde," - %i)",pwr_status);
	strcat(power_stats,cde);
	break;
    }
  }
  else
    strcpy(power_stats,"Power: camera unavailable");
  label = gtk_label_new(power_stats);
  gtk_misc_set_alignment (GTK_MISC (label), 0, 0);
  gtk_widget_show(label);
  gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 1);

  label = gtk_label_new(" ");
  gtk_widget_show(label);
  gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 1);

  /**
   * Then, speed settings. We need to save these to a file...
   */

  /* Make a small horizontal box for speed selection: */
  hbox = gtk_hbox_new(FALSE, 2);
  gtk_widget_show(hbox);
  gtk_box_pack_start(GTK_BOX(vbox),hbox,FALSE,FALSE,5);

  label = gtk_label_new("Speed: ");
  gtk_widget_show(label);
  gtk_misc_set_alignment (GTK_MISC (label), 0, 0);
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);

  list = NULL;
  combo = gtk_combo_new();
  gtk_widget_show(combo);
  gtk_box_pack_start(GTK_BOX(hbox), combo, FALSE, FALSE, 1);

  list = g_list_append(list, "9600");
  list = g_list_append(list, "19200");
  list = g_list_append(list, "38400");
  list = g_list_append(list, "57600");
  list = g_list_append(list, "115200");

  gtk_combo_set_popdown_strings(GTK_COMBO(combo), list);
  switch (camera.speed) {
  case 9600: csp = "9600"; break;
  case 19200: csp = "19200"; break;
  case 38400: csp = "38400"; break;
  case 57600: csp = "57600"; break;
  case 115200: csp = "115200"; break;
  default: csp = "Choose speed";
  }
  gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(combo)->entry),
                    csp);
  gtk_entry_set_editable(GTK_ENTRY(GTK_COMBO(combo)->entry),FALSE);


  // SYNCH TIME Button

  sync = gtk_button_new_with_label("Synchronize Camera Time");
  gtk_widget_show(sync);
  gtk_box_pack_start(GTK_BOX(vbox), sync, FALSE, FALSE, 5);
  gtk_signal_connect(GTK_OBJECT(sync),"clicked",GTK_SIGNAL_FUNC(psa50_sync_time),
                    dialog);


  /* Add a last button to switch off the camera if we want: */
  swoff = gtk_button_new_with_label("Switch Camera Off");
  gtk_widget_show(swoff);
  gtk_box_pack_start(GTK_BOX(vbox), swoff, FALSE, FALSE, 5);
  gtk_signal_connect(GTK_OBJECT(swoff),"clicked",GTK_SIGNAL_FUNC(switch_camera_off),
                    dialog);

  /* Buttons at the bottom of the dialog: OK/Save and Cancel */

  button = gtk_button_new_with_label("Save");
  gtk_widget_show(button);
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->action_area),
                    button, TRUE, TRUE, 0);

  cbutton = gtk_button_new_with_label("Cancel");
  gtk_widget_show(cbutton);
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->action_area),
                    cbutton, TRUE, TRUE, 0);

  if (wait_for_hide(dialog, button, cbutton) == 0)
      return 1;

  /* If we're there, it's because the user pressed "OK". We should
   * save the settings now...
   */
  csp = gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(combo)->entry));
  if (strncmp(csp, "115200", 6) == 0)
    camera.speed = 115200;
  else if (strncmp(csp, "57600", 5) == 0)
    camera.speed = 57600;
  else if (strncmp(csp, "38400", 5) == 0)
    camera.speed = 38400;
  else if (strncmp(csp, "19200", 5) == 0)
    camera.speed = 19200;
  else
    camera.speed = 9600;

  save_rcfile();

  return 1;

}

/****************************************************************************/

static int _pick_nth(struct psa50_dir *tree, int n, char *path, char *attr) {          
                                                                           
  int i=0;                                                                 
                                                                           
  if (tree == NULL)                                                        
    return 0;                                                              
                                                                           
  path = strchr(path, 0);                                                  
  *path = '\\';
                                                                           
  while (i<n && tree->name) {                                              
    strcpy(path+1, tree->name);
    if (is_image(tree->name)) {
      *attr = tree->attrs;
      i++;
    } else if (!tree->is_file)
      i += _pick_nth(tree->user, n-i, path, attr);
    tree++;
  }                                                                        
  return i;                                                                
}                                                                          


static void pick_nth(int n,char *path, char *attr)
{
    (void) _pick_nth(cached_tree,n,path,attr);
}


#define JPEG_END	0xFFFFFFD9
#define JPEG_ESC	0xFFFFFFFF

static char *canon_get_picture(int picture_number, int thumbnail, int *size)
{
   	char *image;
    char path[300];
    char attribs;
    char file[300], dir[300];
    int size2,j;
    void *ptr;

	
    if (!check_readiness()) {
      return NULL;
    }
	
    switch (camera.model) {
		case CANON_PS_A5:
		case CANON_PS_A5_ZOOM:

			picture_number=picture_number*2-1;
			if (thumbnail) picture_number+=1;
			debug_message("Picture number %d\n",picture_number);

			image = malloc(sizeof(*image));
			if (!image) {
				perror("malloc");
				return NULL;
			}
			memset(image,0,sizeof(*image));
			if (!picture_number || picture_number > cached_images) {
				gp_interface_status("Invalid index");
				free(image);
				return NULL;
			}
			gp_interface_status(cached_paths[picture_number]);
			if (!check_readiness()) {
				free(image);
				return NULL;
			}
			image = psa50_get_file(cached_paths[picture_number], size);
			if (image) return image;
			free(image);
			return NULL;
			break;
		default:
      /* For A50 or others */
      /* clear_readiness(); */
		if (!update_dir_cache()) {
			gp_interface_status("Could not obtain directory listing");
			return 0;
		}
		image = malloc(sizeof(*image));
		if (!image) {
			perror("malloc");
			return NULL;
		}
		memset(image,0,sizeof(*image));
		if (!picture_number || picture_number > cached_images) {
			gp_interface_status("Invalid index");
			free(image);
/*	if (command_line_mode==1)
	  psa50_end(); */
			return NULL;
		}
		strcpy(path,cached_drive);
		attribs = 0;
		pick_nth(picture_number,path,&attribs);
		gp_interface_status(path);
		if (!check_readiness()) {
			free(image);
			return NULL;
		}
		if (thumbnail) {
			ptr=image;
			if ( (image = psa50_get_thumbnail(path,size)) == NULL) {
/*	  if (command_line_mode == 1)
	    psa50_end(); */
				free(ptr);
				return NULL;
			}
	/* we count the byte returned until the end of the jpeg data
	   which is FF D9 */

	/* 24/07/2000 (pm) I've commented out following code
		do we really need that now that we use exif to parse data ?
	*/

/*		for(size2=1;size2<size;size2++)
			if(image[size2]==JPEG_END) {
				if(image[size2-1]==JPEG_ESC) break;
	  		}
			size = size2+1; */
		}
		else {
			image = psa50_get_file(path,size);
			debug_message("We now have to set the \"downloaded\" flag on the  picture\n");
			j = strrchr(path, '\\') - path;
			strncpy(dir,path,j);
			dir[j]='\0';
			strcpy(file, path+j+1);
			debug_message("The old file attributes were: %#x\n",attribs);
			attribs = attribs & 0xdf; // 0xdf = !0x20
			psa50_set_file_attributes(file,dir,attribs);
		}
/*      if (command_line_mode==1) 
	psa50_end(); */
		if (image) return image;
     	 //if(receive_error==FATAL_ERROR) clear_readiness();
		free(image);
		return NULL;
		break;
    }
}


int camera_file_get(int index, CameraFile *file)
{
	char *data;
	int buflen,j,i;
	char attribs=0;
	char path[300], filename[300];

	index++;
	data = canon_get_picture(index, 0, &buflen);
	if (!data)
		return GP_ERROR;

	file->data = data;
	strcpy(file->type, "image/jpeg");
	file->size = buflen;

	pick_nth(index,path,&attribs);
	j = strrchr(path, '\\') - path;

	i=0;
	while(path[j+1] != '\0') {
		filename[i] = tolower(path[j+1]);
		i++;
		j++;
	}
	filename[i]='\0';

	snprintf(file->name, sizeof(file->name), "%s",
		filename);

	return GP_OK;
}
int camera_file_get_preview(int index, CameraFile *preview)
{
	char *data;
	int buflen,j,i;
	char attribs=0;
	char path[300], filename[300];

	index++;
	data = canon_get_picture(index, 1, &buflen);
	if (!data)
		return GP_ERROR;

	preview->data = data;
	strcpy(preview->type, "image/jpeg");
	preview->size = buflen;

	pick_nth(index,path,&attribs);
	j = strrchr(path, '\\') - path;

	i=0;
	while(path[j+1] != '\0') {
		filename[i] = tolower(path[j+1]);
		i++;
		j++;
	}
	filename[i]='\0';

	snprintf(preview->name, sizeof(preview->name), "%s",
		filename);

	return GP_OK;
}
/****************************************************************************/


int camera_file_count(void)
{
  /* clear_readiness(); */
  if (!update_dir_cache()) {
    gp_interface_status("Could not obtain directory listing");
    return 0;
  }
  switch (camera.model) {
  case CANON_PS_A5:
  case CANON_PS_A5_ZOOM:
    return cached_images/2; /* Odd is pictures even is thumbs */
  default:
/*	if (command_line_mode==1)
		psa50_end(); */
	return cached_images;
  } 
};

/**
 * This routine initializes the serial port and also load the
 * camera settings. Right now it is only the speed that is
 * saved.
 */
int camera_init(CameraInit *init)
{
  char fname[1024];
  FILE *conf;

	gphoto2_debug = init->debug;
  fprintf(stderr,"canon_initialize()\n");
 /* Default speed */
  camera.speed = 9600;

  setFileName(fname);
  if ((conf = fopen(fname, "r"))) {
    char buf[256];
    char *sp, *vp;
    while ((sp = fgets(buf, sizeof(buf)-1, conf)) != NULL)
      {
       if (*sp == '#' || *sp == '*')
         continue;
       sp = strtok(buf, " \t\r\n");
       if (!sp)
         continue;    /* skip blank lines */
       vp = strtok(NULL, " \t\r\n");
       if (!vp)
         {
            printf("No value for %s - ignored\n", sp);
            continue;
         }
       if (strcasecmp(sp, "Speed") == 0) {
         if (strncmp(vp, "115200", 6) == 0)
           camera.speed = 115200;
         else if (strncmp(vp, "57600", 5) == 0)
           camera.speed = 57600;
         else if (strncmp(vp, "38400", 5) == 0)
           camera.speed = 38400;
         else if (strncmp(vp, "19200", 5) == 0)
           camera.speed = 19200;
         else if (strncmp(vp, "9600", 5) == 0)
           camera.speed = 9600;
       }

       /* This is just an option in the file, that's why we initialize
	  the variable before */
       if (strcasecmp(sp, "Debug") == 0) {
	 if (strncmp(vp,"0",1) == 0)
	   canon_debug_driver = 0;
	 if (strncmp(vp,"1",1) == 0)
	   canon_debug_driver = 1;
	 if (strncmp(vp,"1",1) == 0)
	   canon_debug_driver = 1;
	 if (strncmp(vp,"2",1) == 0)
	   canon_debug_driver = 2;
	 if (strncmp(vp,"3",1) == 0)
	   canon_debug_driver = 3;
	 if (strncmp(vp,"4",1) == 0)
	   canon_debug_driver = 4;
	 if (strncmp(vp,"5",1) == 0)
	   canon_debug_driver = 5;
	 if (strncmp(vp,"6",1) == 0)
	   canon_debug_driver = 6;
	 if (strncmp(vp,"7",1) == 0)
	   canon_debug_driver = 7;
	 if (strncmp(vp,"8",1) == 0)
	   canon_debug_driver = 8;
	 if (strncmp(vp,"9",1) == 0)
	   canon_debug_driver = 9;
	 fprintf(stderr,"Debug level: %i\n",canon_debug_driver);
       }
      }
    fclose(conf);
  }
//FIXME: How to know the link type ?
/*  switch () { //  is a gphoto global variable
  case GP_PORT_USB:
      debug_message("GPhoto tells us tha we should use a USB link.\n");
      canon_comm_method = CANON_USB;
      break;
  case GP_PORT_SERIAL:
  default:
      debug_message("GPhoto tells us tha we should use a RS232 link.\n");
      canon_comm_method = CANON_SERIAL_RS232;
      break;
  }
*/

	canon_comm_method = CANON_SERIAL_RS232;

  if (canon_comm_method == CANON_SERIAL_RS232)
      debug_message("Camera transmission speed : %i\n", camera.speed);

  return !canon_serial_init(init->port_settings.path);
}


/****************************************************************************/


static void pretty_number(int number,char *buffer)
{
    int len,tmp,digits;
    char *pos;

    len = 0;
    tmp = number;
    do {
	len++;
	tmp /= 10;
    }
    while (tmp);
    len += (len-1)/3;
    pos = buffer+len;
    *pos = 0;
    digits = 0;
    do {
	*--pos = (number % 10)+'0';
	number /= 10;
	if (++digits == 3) {
	    *--pos = '\'';
	    digits = 0;
	}
    }
    while (number);
}

int camera_summary(CameraText *summary)
{
    char a[20],b[20];
    char *model;

    /*clear_readiness();*/
    if (!update_disk_cache()) return GP_ERROR;
    pretty_number(cached_capacity,a);
    pretty_number(cached_available,b);
    model = "Canon Powershot";
    switch (camera.model) {
    case CANON_PS_A5:      model = "Canon Powershot A5"; break;
    case CANON_PS_A5_ZOOM: model = "Canon Powershot A5 Zoom"; break;
    case CANON_PS_A50:     model = "Canon Powershot A50"; break;
    case CANON_PS_A70:     model = "Canon Powershot A70"; break;
    case CANON_PS_S10:     model = "Canon Powershot S10"; break;
    case CANON_PS_S20:     model = "Canon Powershot S20"; break; 
    }
    sprintf(summary->text,"%s\nDrive %s\n%11s bytes total\n%11s bytes available\n",
      model,cached_drive,a,b);
/*	if (!(camera.model == CANON_PS_A5 
	      || camera.model == CANON_PS_A5_ZOOM) && command_line_mode==1) psa50_end(); */
    return GP_OK;
}

/****************************************************************************/

int camera_about(CameraText *about)
{
    strcpy(about->text, "Canon PowerShot series driver by\n"
	    "Wolfgang G. Reissnegger,\n"
            "Werner Almesberger,\n"
	    "Edouard Lafargue,\n"
	    "Philippe Marzouk,\n"
            "A5 additions by Ole W. Saastad\n"
	    );

	return GP_OK;
}

/****************************************************************************/

int camera_file_delete(int picture_number)
{ 
  char path[300];
  char file[300], dir[300], atrs;
  int j;
  
  /*clear_readiness();*/
  if (!check_readiness()) {
    return 0;
  }
  if (!(camera.model == CANON_PS_A5 ||
	camera.model == CANON_PS_A5_ZOOM)) { /* this is tested only on powershot A50 */
    
    if (!update_dir_cache()) {
      gp_interface_status("Could not obtain directory listing");
      return 0;
    }
	/* gphoto2 numbering starts at 0 but we start at 1 */
	picture_number++;
    if (!picture_number || picture_number > cached_images) {
      gp_interface_status("Invalid index");
      /* psa50_end(); */
      return 0;
    }
    strcpy(path,cached_drive);
    atrs=0;
    pick_nth(picture_number,path,&atrs);
    gp_interface_status(path);
    
    j = strrchr(path, '\\') - path;
    strncpy(dir,path,j);
    dir[j]='\0';
    strcpy(file, path+j+1);

    if (psa50_delete_file(file,dir)) {
      gp_interface_status("error deleting file");
      /* psa50_end(); */
      return -1;
    }
    else {
      /*		psa50_end(); */
      cached_ready = 0; /*RAA: do smarter!*/
      cached_disk = 0;
      if (cached_dir) clear_dir_cache();
      cached_dir = 0;
      return 1;
    }
  }
  return 0; 
}

/****************************************************************************/
/*
static struct Image *canon_get_preview(void) { return NULL; }
static int canon_take_picture(void) { return 0; };

struct _Camera canon =
{
    canon_initialize,
    canon_get_picture,
    canon_get_preview,
    canon_delete_image,
    canon_take_picture,
    canon_number_of_pictures,
    canon_configure,
    canon_summary,
    canon_description
};
*/
