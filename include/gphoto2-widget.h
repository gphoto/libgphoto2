/* 	Header file for gPhoto2

	Author: Scott Fritzinger <scottf@unr.edu>

	This library is covered by the LGPL.
*/

CameraWidget* 	gp_widget_new  (CameraWidgetType type, char *label);
int           	gp_widget_free (CameraWidget *widget);

int 		gp_widget_append  (CameraWidget *parent, CameraWidget *child);
int 		gp_widget_prepend (CameraWidget *parent, CameraWidget *child);

int 		gp_widget_child_count(CameraWidget *parent);

CameraWidget* 	gp_widget_child(CameraWidget *parent, int child_number);
CameraWidget* 	gp_widget_child_by_label(CameraWidget *parent, char *child_label);

int		gp_widget_type (CameraWidget *widget);
char*		gp_widget_label(CameraWidget *widget);

CameraWidgetCallback gp_widget_callback (CameraWidget *widget);
int 		gp_widget_callback_set (CameraWidget *widget, CameraWidgetCallback callback);

int		gp_widget_value_set (CameraWidget *widget, char *value);
char*		gp_widget_value_get (CameraWidget *widget);

int		gp_widget_range_set (CameraWidget *range, float low,  float high, 
				     float increment);
int		gp_widget_range_get (CameraWidget *range, float *low, float *high, 
				     float *increment);

int		gp_widget_choice_add (CameraWidget *widget, char *choice);
int		gp_widget_choice_count (CameraWidget *widget);
char*		gp_widget_choice (CameraWidget *widget, int choice_number);

int		gp_widget_dump (CameraWidget *widget);

int		gp_widget_changed (CameraWidget *widget);
