/* 
   file : interface.h
   author : jae gangemi (jgangemi@yahoo.com) 
   
   header file for all the interface functions.
*/

/* the following defines are for window creation via a callback */
#define XPORT_WIN 1
#define IMG_WIN 2


/* functions start here */
GtkWidget *create_main_win(void);
GtkWidget *create_img_win(void);
GtkWidget *create_xport_win(void);
GtkWidget *create_message_win_long(void);
GtkWidget *create_message_win(void);
