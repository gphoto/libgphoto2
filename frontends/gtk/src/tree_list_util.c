/* 
   
  file : tree_list_util.c
  author : jae gangemi (jgangemi@yahoo.com)
   
  contains functions to populate and manipulate the tree lists contained
  in the main gphoto2 interface window
   
*/


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <error.h>

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

int isDir(char *buf) {
  
  struct stat st;
  
  if (stat(buf,&st) < 0) {
    return(FALSE);
  }
  
  return(S_ISDIR(st.st_mode));

} 

void create_sub_tree(char *szPath, char *sub, GtkWidget *item) {
  
  DIR *dir;
  struct dirent *dp;
  
  GtkWidget *item_subtree = NULL;
  GtkWidget *item_new;

  char *buf,*tmp;

  if ((dir = opendir(szPath)) == NULL) {

    /* fix this - it breaks on permission denied dirs */
    perror("opendir");

  }

  while ((dp = readdir(dir))) {
    
    if (strcmp(dp->d_name, ".") == 0 || strcmp(dp->d_name, "..") == 0) {
      continue;
    }
    /* correct malloc size? we've got size of current dir +
       the size of the path before it + the / and trailing \0 */
    if ((buf = (char *)malloc(sizeof(dp->d_name)+sizeof(szPath)+5)) == NULL) {
      perror("malloc");
      exit(1);
    }
    sprintf(buf,"%s/%s",szPath, dp->d_name);

    if (isDir(buf)) {

      if (item_subtree == NULL) {
 	item_subtree = gtk_tree_new();

	/* this doesn't work - if the subtree has branches w/ their own
	   subtrees, it just segfaults. joy of all joys. so - until
	   i can find a clean way to do this - it prints an error.
	*/
	/*
	if (GTK_TREE_ITEM_SUBTREE(item)) {
	  gtk_tree_item_remove_subtree(GTK_TREE_ITEM(item));
        }
	*/

	gtk_tree_item_set_subtree(GTK_TREE_ITEM(item),item_subtree);	

      }

      item_new = gtk_tree_item_new_with_label(dp->d_name);
      gtk_widget_ref(item_new);
      gtk_object_set_data(GTK_OBJECT(item_new),"path",strdup((gpointer)buf));

      gtk_signal_connect(GTK_OBJECT(item_new), "select",
			 GTK_SIGNAL_FUNC(populate_dir), NULL);

      gtk_tree_append(GTK_TREE(item_subtree),item_new);
      gtk_widget_show(item_new);

    } 

    free(buf);
  }
  
  closedir(dir);
  gtk_widget_show(item);
  
}

GtkWidget *create_camera_tree(GtkWidget *window) {

  /* this is where we're going to initially populate the camera tree in the 
     main window. this function definately needs to be expanded tho - 
     since we're going to have a config file, we should look for the 
     existance, and then create the tree based upon that. 

     either selecting d/l thumbnail index or some form of a click on the 
     camera should further populate the tree w/ directories etc.

     but hey - this is all open to debate, and is pretty incomplete since 
     i don't feel like lugging GTK books around on the train with me :) 
  */

  GtkWidget *tree_hbox;
  GtkWidget *pixmap;
  GtkWidget *leaf_label;

  tree_hbox = gtk_hbox_new(FALSE, 2);
  gtk_widget_ref(tree_hbox);
  /*  gtk_object_set_data_full(GTK_OBJECT(NULL), "tree_hbox",
                           tree_hbox,
                           (GtkDestroyNotify) gtk_widget_unref);
  */
  pixmap = create_pixmap(window, "camera.xpm");
  gtk_widget_ref(pixmap);
  gtk_widget_show(pixmap);
  gtk_box_pack_start(GTK_BOX(tree_hbox), pixmap, FALSE, FALSE, 0);

  leaf_label = gtk_label_new("No Camera Selected");
  gtk_widget_ref(leaf_label);
  gtk_widget_show(leaf_label);
  gtk_box_pack_start(GTK_BOX(tree_hbox), leaf_label, FALSE, FALSE,0);
  
  return (tree_hbox);

} /* end create_camera_tree */

GtkWidget *create_local_tree(GtkWidget *window) {

  GtkWidget *tree_hbox;
  GtkWidget *pixmap;
  GtkWidget *leaf_label;

  GtkWidget *main_tree_local;
  GtkWidget *main_leaf_local;

  main_tree_local = gtk_tree_new();
  gtk_widget_set_name(main_tree_local, "tree");
  gtk_widget_ref (main_tree_local);
  gtk_object_set_data_full (GTK_OBJECT (window), "main_tree_local", 
			    main_tree_local,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (main_tree_local);
  
  main_leaf_local = gtk_tree_item_new();
  gtk_widget_ref(main_leaf_local);
  gtk_tree_append(GTK_TREE(main_tree_local), main_leaf_local);
  gtk_widget_show(main_leaf_local);

  tree_hbox = gtk_hbox_new(FALSE, 2);
  gtk_widget_ref(tree_hbox);
  /*  gtk_object_set_data_full(GTK_OBJECT(NULL), "tree_hbox",
                           tree_hbox,
                           (GtkDestroyNotify) gtk_widget_unref);
  */
  pixmap = create_pixmap(window, "folder.xpm");
  gtk_widget_ref(pixmap);
  gtk_widget_show(pixmap);
  gtk_box_pack_start(GTK_BOX(tree_hbox), pixmap, FALSE, FALSE, 0);

  leaf_label = gtk_label_new("Local Directories");
  gtk_widget_ref(leaf_label);
  gtk_widget_show(leaf_label);
  gtk_box_pack_start(GTK_BOX(tree_hbox), leaf_label, FALSE, FALSE,0);

  gtk_container_add(GTK_CONTAINER(main_leaf_local), tree_hbox);
  gtk_widget_show(tree_hbox);

  create_sub_tree("/","NULL",main_leaf_local);

  gtk_tree_item_expand(GTK_TREE_ITEM(main_leaf_local));
  return(main_tree_local);

} /* end create_local_tree */

