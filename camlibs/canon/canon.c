/***************************************************************************
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
#if 0
#include <gtk/gtk.h>
#endif
#include <gphoto2.h>
#include <gpio.h>

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
struct psa50_dir *cached_tree;
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
        "Canon Digital IXUS",
        "Canon PowerShot S100",
        NULL
};
char *models_serialandUSB[] = {
        "Canon PowerShot S10",
        "Canon PowerShot S20",
        NULL
};

int camera_id(CameraText *id)
{
        strcpy(id->text, "canon");

        return GP_OK;
}

int camera_manual(Camera *camera, CameraText *manual)
{
        strcpy(manual->text, "Manual Not Available");

        return GP_OK;
}

int camera_abilities(CameraAbilitiesList *list)
{
    int i;
    CameraAbilities *a;

    

    i=0;
    while(models_serialandUSB[i]) {
        a = gp_abilities_new();
        strcpy(a->model, models_serialandUSB[i]);
        a->port = GP_PORT_SERIAL | GP_PORT_USB;
        a->speed[0]   = 9600;
        a->speed[1]   = 19200;
        a->speed[2]   = 38400;
        a->speed[3]   = 57600;
        a->speed[4]   = 115200;
        a->speed[5]   = 0;
        a->capture    = 0;
        a->config     = 0;
        a->file_delete = 1;
        a->file_preview = 1;
        a->file_put = 0;
        gp_abilities_list_append(list, a);
        i++;
    }

    i=0;
    while(models_USB[i]) {
        a = gp_abilities_new();
        strcpy(a->model, models_USB[i]);
        a->port = GP_PORT_USB;
        a->capture    = 0;
        a->config     = 0;
        a->file_delete = 1;
        a->file_preview = 1;
        a->file_put = 0;
        gp_abilities_list_append(list, a);
        i++;
    }

    i=0;
    while(models_serial[i]) {
        a = gp_abilities_new();
        strcpy(a->model, models_serial[i]);
        a->port = GP_PORT_SERIAL;
        a->speed[0]   = 9600;
        a->speed[1]   = 19200;
        a->speed[2]   = 38400;
        a->speed[3]   = 57600;
        a->speed[4]   = 115200;
        a->speed[5]   = 0;
        a->capture    = 0;
        a->config     = 0;
        a->file_delete = 1;
        a->file_preview = 1;
        a->file_put = 1;
        gp_abilities_list_append(list, a);
        i++;
    }

    return GP_OK;
}

int camera_folder_list(Camera *camera, CameraList *list, char *folder)
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
      debug_message("Camera type:  %d\n",camera_data.model);
      cached_ready = 1;
      return 1;
    }
    gp_camera_status(NULL, "Camera unavailable");
    return 0;
}

void switch_camera_off(void)
{
	gp_camera_status(NULL, "Switching Camera Off");
	psa50_off();
	clear_readiness();
}

int camera_exit(Camera *camera)
{
	CanonDataStruct *cs = (CanonDataStruct*)camera->camlib_data;
	switch_camera_off();
	
	free(cs);
	canon_serial_close();
	return GP_OK;
}

int canon_get_batt_status(int *pwr_status, int *pwr_source)
{
	if (!check_readiness())
	  return -1;
	
	return psa50_get_battery(pwr_status, pwr_source);
}
#if 0
static void canon_set_owner(GtkWidget *win,GtkWidget *owner)
{
        char *entry;

        gp_camera_status(NULL, "Setting owner name.");
        entry = gtk_entry_get_text(GTK_ENTRY(owner));
        debug_message("New owner name: %s\n",entry);
        psa50_set_owner_name(entry);
}
#endif

char *camera_model_string()
{
        if (!check_readiness())
                return "Camera unavailable";

        switch (camera_data.model) {
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
                case CANON_PS_S100:
                        return "Powershot S100 / Digital IXUS";
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
                gp_camera_status(NULL, "No response");
                return 0;
        }
        strcpy(cached_drive,disk);
        sprintf(root,"%s\\",disk);
        if (!psa50_disk_info(root,&cached_capacity,&cached_available)) {
                gp_camera_status(NULL, "No response");
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
    switch (camera_data.model) {
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

static int _canon_file_list(struct psa50_dir *tree, CameraList *list, char *folder)
{
/*
    if (!tree) {
        debug_message("no directory listing available!\n");
        return GP_ERROR;
	}
*/
    while (tree->name) {
        if(is_image(tree->name))
			gp_list_append(list,tree->name,GP_LIST_FILE);
        else if (!tree->is_file) { 
            _canon_file_list(tree->user, list, folder);      
        }
        tree++;
    }

    return GP_OK;
}

static int canon_file_list(CameraList *list, char *folder)
{

    if(!update_dir_cache()) {
        gp_camera_status(NULL, "Could not obtain directory listing");
        return GP_ERROR;
    }

    _canon_file_list(cached_tree, list, folder);

    return GP_OK;
}


int camera_file_list(Camera *camera, CameraList *list, char *folder)
{
    
    return canon_file_list(list, folder);

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

#if 0
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
#endif
#if 0
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
    gp_camera_status(NULL, path);
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
    gp_camera_status(NULL, "File saved");
}
#endif

#if 0
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
#endif

#if 0
static void cb_get_all(GtkItem *item) {

  get_all(cached_tree);

}
#endif

#if 0
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
#endif

#if 0
static void cb_clear(GtkWidget *widget,GtkWidget *window)
{
  //    gtk_widget_destroy(window);

    cached_ready = 0;
    cached_disk = 0;
    if (cached_dir) clear_dir_cache();
    cached_dir = 0;
}
#endif

/**
 * Save current driver setup into a settings file (~/.gphoto/powershotrc)
 * Note from scott: use gp_settings_set/get to save and retrieve settings
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
    switch (camera_data.speed) {
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
    gp_camera_status(NULL, "Saved configuration");
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

#if 0
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
  gtk_entry_set_text(GTK_ENTRY(owner_entry),camera_data.owner);
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
          camera_data.firmwrev[3],camera_data.firmwrev[2],
          camera_data.firmwrev[1],camera_data.firmwrev[0] );
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
  switch (camera_data.speed) {
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
    camera_data.speed = 115200;
  else if (strncmp(csp, "57600", 5) == 0)
    camera_data.speed = 57600;
  else if (strncmp(csp, "38400", 5) == 0)
    camera_data.speed = 38400;
  else if (strncmp(csp, "19200", 5) == 0)
    camera_data.speed = 19200;
  else
    camera_data.speed = 9600;

  save_rcfile();

  return 1;

}
#endif

/****************************************************************************/

#define JPEG_END        0xFFFFFFD9
#define JPEG_ESC        0xFFFFFFFF

static char *canon_get_picture(char *filename, char *path, int thumbnail, int *size)
{
    char *image;
    char attribs;
    char file[300];
    void *ptr;
	int j;
	
    if (!check_readiness()) {
		return NULL;
    }
	
    switch (camera_data.model) {
	 case CANON_PS_A5:
	 case CANON_PS_A5_ZOOM:
#if 0
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
			gp_camera_status(NULL, "Invalid index");
			free(image);
			return NULL;
		}
		gp_camera_status(NULL, cached_paths[picture_number]);
		if (!check_readiness()) {
			free(image);
			return NULL;
		}
		image = psa50_get_file(cached_paths[picture_number], size);
                        if (image) return image;
		free(image);
#endif
		return NULL;
		break;
	 default:
		/* For A50 or others */
		/* clear_readiness(); */
		if (!update_dir_cache()) {
			gp_camera_status(NULL, "Could not obtain directory listing");
			return 0;
		}
		image = malloc(sizeof(*image));
		if (!image) {
			perror("malloc");
			return NULL;
                }
		memset(image,0,sizeof(*image));
		
		sprintf(file,"%s%s",path,filename);
		
		attribs = 0;
		if (!check_readiness()) {
			free(image);
			return NULL;
		}
		if (thumbnail) {
			ptr=image;
			if ( (image = psa50_get_thumbnail(file,size)) == NULL) {
				free(ptr);
				return NULL;
			}
		}
		else {
			image = psa50_get_file(file,size);
			j = strrchr(path, '\\') - path;
			path[j] = '\0';
			debug_message("We now have to set the \"downloaded\" flag on the  picture\n");
			debug_message("The old file attributes were: %#x\n",attribs);
			attribs = attribs & 0xdf; // 0xdf = !0x20
			psa50_set_file_attributes(filename,path,attribs);
		}
		if (image) return image;
		//if(receive_error==FATAL_ERROR) clear_readiness();
		free(image);
		return NULL;
		break;
    }
}

static int _get_file_path(struct psa50_dir *tree, char *filename, char *path)
{
    if (tree==NULL) return GP_ERROR;    
 
    path = strchr(path,0);
    *path = '\\';

    while(tree->name) {
        if (!is_image(tree->name)) 
            strcpy(path+1,tree->name);
        if (is_image(tree->name) && strcmp(tree->name,filename)==0) {
            return GP_OK;
        }
        else if (!tree->is_file) 
            if (_get_file_path(tree->user,filename, path) == GP_OK)
                   return GP_OK;
        tree++;
    }
    return GP_ERROR;

}

static int get_file_path(char *filename, char *path)
{

    return _get_file_path(cached_tree, filename, path);

}

int camera_file_get(Camera *camera, CameraFile *file, char *folder, char *filename)
{
    char *data;
    int buflen,j;
    char path[300];


    strcpy(path, cached_drive);

    if (get_file_path(filename,path) == GP_ERROR) {
        debug_message("Filename not found!\n");
        return GP_ERROR;
    }
    j = strrchr(path,'\\') - path;
    path[j+1] = '\0';     

    data = canon_get_picture(filename, path, 0, &buflen);
    if (!data)
        return GP_ERROR;

    file->data = data;
    strcpy(file->type, "image/jpeg");
    file->size = buflen;

    snprintf(file->name, sizeof(file->name), "%s",
        filename);

    return GP_OK;
}

int camera_file_get_preview(Camera *camera, CameraFile *preview, char *folder, char *filename)
{
    char *data;
    int buflen,i,j,size;
    char path[300], tempfilename[300];

    strcpy(path, cached_drive);

    if (get_file_path(filename,path) == GP_ERROR) {
        debug_message("Filename not found!\n");
        return GP_ERROR;
    }
    j = strrchr(path,'\\') - path;
    path[j+1] = '\0';        

    data = canon_get_picture(filename, path, 1, &buflen);
    if (!data)
        return GP_ERROR;

    preview->data = data;
    strcpy(preview->type, "image/jpeg");

    /* we count the byte returned until the end of the jpeg data
       which is FF D9 */
    /* It would be prettier to get that info from the exif tags     */

    for(size=1;size<buflen;size++)
    if(data[size]==JPEG_END) {
        if(data[size-1]==JPEG_ESC) break;
    }
    buflen = size+1;

    preview->size = buflen;

    i=0;
    if (A5==0) {
        tempfilename[i++]='t';
        tempfilename[i++]='h';
        tempfilename[i++]='u';
        tempfilename[i++]='m';
        tempfilename[i++]='b';
        tempfilename[i++]='_';
    }
    while(filename[i+1] != '\0') {
        tempfilename[i] = tolower(filename[i]);
        i++;
    }

    tempfilename[i]='\0';

    snprintf(preview->name, sizeof(preview->name), "%s",
            tempfilename);

    return GP_OK;
}
/****************************************************************************/

#if 0
int camera_file_count(Camera *camera)
{
  /* clear_readiness(); */
  if (!update_dir_cache()) {
    gp_camera_status(NULL, "Could not obtain directory listing");
    return 0;
  }
	
	
  switch (camera_data.model) {
  case CANON_PS_A5:
  case CANON_PS_A5_ZOOM:
    return cached_images/2; /* Odd is pictures even is thumbs */
  default:
        return cached_images;
  }
};
#endif

/**
 * This routine initializes the serial port and also load the
 * camera settings. Right now it is only the speed that is
 * saved.
 */
int camera_init(Camera *camera, CameraInit *init)
{
        char fname[1024];
        FILE *conf;
        CanonDataStruct *cs;

        /* First, set up all the function pointers */
        camera->functions->id                = camera_id;
        camera->functions->abilities         = camera_abilities;
        camera->functions->init      = camera_init;
        camera->functions->exit      = camera_exit;
        camera->functions->folder_list = camera_folder_list;
        camera->functions->file_list = camera_file_list;
        camera->functions->file_get  = camera_file_get;
        camera->functions->file_get_preview =  camera_file_get_preview;
        camera->functions->file_put  = camera_file_put;
        camera->functions->file_delete = camera_file_delete;
        camera->functions->config_get = camera_config_get;
        camera->functions->config_set = camera_config_set;
        camera->functions->capture   = camera_capture;
        camera->functions->summary   = camera_summary;
        camera->functions->manual    = camera_manual;
        camera->functions->about     = camera_about;

        cs = (CanonDataStruct*)malloc(sizeof(CanonDataStruct));
        camera->camlib_data = cs;

        gphoto2_debug = init->debug;
        fprintf(stderr,"canon_initialize()\n");

        camera_data.speed = init->port_settings.speed;
        /* Default speed */
        if (camera_data.speed == 0)
            camera_data.speed = 9600;

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

            if (camera->debug == 1) {
                if (strncmp(vp, "0", 1) == 0)
                    canon_debug_driver = 0;
                if (strncmp(vp, "1", 1) == 0)
                    canon_debug_driver = 1;
                if (strncmp(vp, "1", 1) == 0)
                    canon_debug_driver = 1;
                if (strncmp(vp, "2", 1) == 0)
                    canon_debug_driver = 2;
                if (strncmp(vp, "3", 1) == 0)
                    canon_debug_driver = 3;
                if (strncmp(vp, "4", 1) == 0)
                    canon_debug_driver = 4;
                if (strncmp(vp, "5", 1) == 0)
                    canon_debug_driver = 5;
                if (strncmp(vp, "6", 1) == 0)
                    canon_debug_driver = 6;
                if (strncmp(vp, "7", 1) == 0)
                    canon_debug_driver = 7;
                if (strncmp(vp, "8", 1) == 0)
                    canon_debug_driver = 8;
                if (strncmp(vp, "9", 1) == 0)
                    canon_debug_driver = 9;
                fprintf(stderr,"Debug level: %i\n",canon_debug_driver);
            }
				canon_debug_driver = 1;
        }
        fclose(conf);
  }
  switch (init->port_settings.type) { 
  case GP_PORT_USB:
      debug_message("GPhoto tells us that we should use a USB link.\n");
      canon_comm_method = CANON_USB;
      break;
  case GP_PORT_SERIAL:
  default:
      debug_message("GPhoto tells us that we should use a RS232 link.\n");
      canon_comm_method = CANON_SERIAL_RS232;
      break;
  }

  if (canon_comm_method == CANON_SERIAL_RS232)
      debug_message("Camera transmission speed : %i\n", camera_data.speed);

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

int camera_summary(Camera *camera, CameraText *summary)
{
    char a[20],b[20];
    char *model;

    /*clear_readiness();*/
    if (!update_disk_cache()) return GP_ERROR;
    pretty_number(cached_capacity,a);
    pretty_number(cached_available,b);
    model = "Canon Powershot";
    switch (camera_data.model) {
    case CANON_PS_A5:      model = "Canon Powershot A5"; break;
    case CANON_PS_A5_ZOOM: model = "Canon Powershot A5 Zoom"; break;
    case CANON_PS_A50:     model = "Canon Powershot A50"; break;
    case CANON_PS_A70:     model = "Canon Powershot A70"; break;
    case CANON_PS_S10:     model = "Canon Powershot S10"; break;
    case CANON_PS_S20:     model = "Canon Powershot S20"; break;
        case CANON_PS_S100:    model = "Canon Powershot S100 / Digital IXUS"; break;
    }
    sprintf(summary->text,"%s\nDrive %s\n%11s bytes total\n%11s bytes available\n",
      model,cached_drive,a,b);
    return GP_OK;
}

/****************************************************************************/

int camera_about(Camera *camera, CameraText *about)
{
        strcpy(about->text,
                "Canon PowerShot series driver by\n"
                "Wolfgang G. Reissnegger,\n"
                "Werner Almesberger,\n"
                "Edouard Lafargue,\n"
                "Philippe Marzouk,\n"
                "A5 additions by Ole W. Saastad\n"
        );

        return GP_OK;
}

/****************************************************************************/
#if 0
/* FIXME: (pm) Do we need to update the directory cache and camera_file_list
          on deletion ? */

/*
 * mark an entry as deleted in the directory cache
 *
*/
int _entry_delete(struct psa50_dir *tree, char *filename)
{
    char path[300],file[300], tempfile[300];
    char attribs;
        int i,j;

    if(!update_dir_cache()) {
        gp_camera_status(NULL, "Could not obtain directory listing");
        return GP_ERROR;
    }

    fprintf(stderr,"delete: %s\n",file);

    i=0;
    while(tree->name) {
        strcpy(tempfile, tree->name);
        fprintf(stderr,"tempfile: %s\n",tempfile);
        if(strcmp(tempfile, file)==0) {
            sprintf(tree->name,"deleted");
            i++;
        }
        else if (!tree->is_file) {
            i += _entry_delete(tree->user, picture_number - i);
        }
        tree++;
    } 
    return i;
}

int entry_delete(char *filename)
{
    return _entry_delete(cached_tree, filename);
}
#endif
int camera_file_delete(Camera *camera, char *folder, char *filename)
{
    char path[300];
    int j;

        /*clear_readiness();*/
    if (!check_readiness()) {
        return 0;
    }

    if (!(camera_data.model == CANON_PS_A5 ||
        camera_data.model == CANON_PS_A5_ZOOM)) { /* this is tested only on powershot A50 */

        if (!update_dir_cache()) {
            gp_camera_status(NULL, "Could not obtain directory listing");
            return 0;
        }
        strcpy(path, cached_drive);

        if (get_file_path(filename,path) == GP_ERROR) {
            debug_message("Filename not found!\n");
            return GP_ERROR;
        }
        j = strrchr(path,'\\') - path;
        path[j] = '\0';

        if (psa50_delete_file(filename,path)) {
            gp_camera_status(NULL, "error deleting file");
            return GP_ERROR;
        }
        else {
     //       entry_delete(filename);
            return GP_OK;
        }
    }
    return GP_ERROR;
}

int camera_file_put(Camera *camera, CameraFile *file, char *folder)
{
    char destpath[300], destname[300];
	int j;

	if (!check_readiness()) {
		return 0;
	}
	
	strcpy(destname,file->name);

    if(!update_dir_cache()) {
        gp_camera_status(NULL, "Could not obtain directory listing");
        return GP_ERROR;
    }
	
	/* FIXME: Just a hack for testing purposes then we will have to
	   find the suitable directory to create or to store in*/
    sprintf(destpath,"D:\\DCIM\\CANONMSC\\");
	sprintf(destname,"PLAY_00.MRK");
	/* end of FIXME */
	
	j = strrchr(destpath,'\\') - destpath;
	destpath[j] = '\0';
	
	if(!psa50_directory_operations(destpath, DIR_CREATE)) 
	  return GP_ERROR;
	
	destpath[j] = '\\';
	
    return psa50_put_file(file, destname, destpath);     
}

int camera_config_get(Camera *camera, CameraWidget *window)
{
	CameraWidget *t, *section;
	char power_stats[48];
	int pwr_status, pwr_source;
	struct tm *camtm;
	time_t camtime;
	
	if (cached_ready) {
		camtime = psa50_get_time();
		camtm = gmtime(&camtime);
	}

	/* set the window label to something more specific */
	strcpy(window->label, "Canon PowerShot series");
	
	section = gp_widget_new(GP_WIDGET_SECTION, "Owner name");
	gp_widget_append(window,section);
	
	t = gp_widget_new(GP_WIDGET_TEXT,"Camera Model");
	strcpy(t->value,camera_data.ident);
	gp_widget_append(section,t);

	t = gp_widget_new(GP_WIDGET_TEXT,"Owner name");
	strcpy(t->value,camera_data.owner);
	gp_widget_append(section,t);

	t = gp_widget_new(GP_WIDGET_TEXT, "date");
	if (cached_ready)
	  strcpy(t->value,asctime(camtm));
	else
	  sprintf(t->value,"Unavailable");
	gp_widget_append(section,t);
	
	t = gp_widget_new(GP_WIDGET_BUTTON, "Set camera date to PC date");
	strcpy(t->value,"psa50_set_time()");
	gp_widget_append(section,t);
	
	t = gp_widget_new(GP_WIDGET_TEXT,"Firmware revision");
	sprintf(t->value,"%i.%i.%i.%i",camera_data.firmwrev[3], 
			camera_data.firmwrev[2],camera_data.firmwrev[1],
			camera_data.firmwrev[0]);
	gp_widget_append(section,t);
	
	if (cached_ready) {
		canon_get_batt_status(&pwr_status,&pwr_source);
		switch (pwr_source) {
		 case CAMERA_ON_AC:
			strcpy(power_stats, "AC adapter ");
			break;
		 case CAMERA_ON_BATTERY:
			strcpy(power_stats, "on battery ");
			break;
		 default:
			sprintf(power_stats,"unknown (%i",pwr_source);
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
			strcat(power_stats,cde);
			sprintf(cde," - %i)",pwr_status);
			break;
		}
	}
	else
	  strcpy(power_stats,"Power: camera unavailable");
	
	t = gp_widget_new(GP_WIDGET_TEXT,"Power");
	strcpy(t->value, power_stats);
	gp_widget_append(section,t);
	
	t = gp_widget_new(GP_WIDGET_MENU, "Debug level");
	gp_widget_choice_add (t, "none");
	gp_widget_choice_add (t, "functions");
	gp_widget_choice_add (t, "complete");
	switch (canon_debug_driver) {
	 case 0:
	 default:
		gp_widget_value_set(t, "none");
		break;
	 case 1:
		gp_widget_choice_add (t, "functions");
		break;
	 case 9:
		gp_widget_choice_add (t, "complete");
		break;
	}
	
    return GP_OK;
}

int camera_config_set(Camera *camera, CameraSetting *setting, int count)
{
    return GP_ERROR;
}

int camera_capture(Camera *camera, CameraFile *file, CameraCaptureInfo *info)
{
    return GP_ERROR;
}
