/*
    
  file : tree_list_util.h
  author : jae gangemi (jgangemi@yahoo.com)

  header file for tree_list_util.c 
  
*/

#define MAX_PATH 1024

typedef struct {
  char buf[MAX_PATH];
  GtkWidget *branch;
} tree_sig;


int isDir(char *buf);
void create_sub_tree(char *szPath, char *szDir, GtkWidget *item);

GtkWidget *create_camera_tree(GtkWidget *window);
GtkWidget *create_local_tree(GtkWidget *window);

