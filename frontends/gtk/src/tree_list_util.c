/* 
   
  file : tree_list_util.c
  author : jae gangemi (jgangemi@yahoo.com)
   
  contains functions to populate and manipulate the tree lists contained
  in the main gphoto2 interface window
   
*/


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>


#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include "callbacks.h"
#include "interface.h"
#include "support.h"
#include "tree_list_util.h"

GtkWidget *create_camera_tree(void) {

  /* this is where we're going to initially populate the camera tree in the 
     main window. this function definately needs to be expanded tho - 
     since we're going to have a config file, we should look for the 
     existance, and then create the tree based upon that. 

     either selecting d/l thumbnail index or some form of a click on the 
     camera should further populate the tree w/ directories etc.

     but hey - this is all open to debate, and is pretty incomplete since 
     i don't feel like lugging GTK books around on the train with me :) 
  */


  GtkWidget *win;
  GtkWidget *tree_hbox;
  GtkWidget *pixmap;
  GtkWidget *leaf_label;

  tree_hbox = gtk_hbox_new(FALSE, 2);
  gtk_widget_ref(tree_hbox);
  /*  gtk_object_set_data_full(GTK_OBJECT(NULL), "tree_hbox",
                           tree_hbox,
                           (GtkDestroyNotify) gtk_widget_unref);
  */
  pixmap = create_pixmap(win, "camera.xpm");
  gtk_widget_ref(pixmap);
  gtk_widget_show(pixmap);
  gtk_box_pack_start(GTK_BOX(tree_hbox), pixmap, FALSE, FALSE, 0);

  leaf_label = gtk_label_new("No Camera Selected");
  gtk_widget_ref(leaf_label);
  gtk_widget_show(leaf_label);
  gtk_box_pack_start(GTK_BOX(tree_hbox), leaf_label, FALSE, FALSE,0);
  
  return (tree_hbox);

} /* end create_camera_tree */

GtkWidget *create_local_tree(void) {

  GtkWidget *win;
  GtkWidget *tree_hbox;
  GtkWidget *pixmap;
  GtkWidget *leaf_label;

  DIR *DP;
  struct dirent *list;
  
  tree_hbox = gtk_hbox_new(FALSE, 2);
  gtk_widget_ref(tree_hbox);
  /*  gtk_object_set_data_full(GTK_OBJECT(NULL), "tree_hbox",
                           tree_hbox,
                           (GtkDestroyNotify) gtk_widget_unref);
  */
  pixmap = create_pixmap(win, "folder.xpm");
  gtk_widget_ref(pixmap);
  gtk_widget_show(pixmap);
  gtk_box_pack_start(GTK_BOX(tree_hbox), pixmap, FALSE, FALSE, 0);

  leaf_label = gtk_label_new("Local Directories");
  gtk_widget_ref(leaf_label);
  gtk_widget_show(leaf_label);
  gtk_box_pack_start(GTK_BOX(tree_hbox), leaf_label, FALSE, FALSE,0);
  
  if ((DP = opendir("/")) == NULL) {
    g_error("opendir");
  }

  while ((list = readdir(DP)) != NULL) {

    if (strcmp(".", list->d_name) == 0) {
      continue;
    }

    if (strcmp("..", list->d_name) == 0) {
      continue;
    }

    g_print("dir : %s\n",list->d_name);

  }

  closedir(DP);


  return (tree_hbox);

} /* end create_local_tree */

