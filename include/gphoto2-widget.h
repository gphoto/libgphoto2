/* 	Header file for gPhoto2

	Author: Scott Fritzinger <scottf@unr.edu>

	This library is covered by the LGPL.
*/

int 	gp_widget_new 	(CameraWidgetType type, const char *label, 
		         CameraWidget **widget);
int    	gp_widget_free 	(CameraWidget *widget);
int     gp_widget_ref   (CameraWidget *widget);
int     gp_widget_unref (CameraWidget *widget);

int	gp_widget_append	(CameraWidget *parent, CameraWidget *child);
int 	gp_widget_prepend	(CameraWidget *parent, CameraWidget *child);

int 	gp_widget_count_children     (CameraWidget *parent);
int	gp_widget_get_child	     (CameraWidget *parent, int child_number, 
				      CameraWidget **child);

/* Retrieve Widgets from a parent widget */
int	gp_widget_get_child_by_label (CameraWidget *parent,
				      const char *child_label,
				      CameraWidget **child);
int	gp_widget_get_child_by_id    (CameraWidget *parent, int id, 
				      CameraWidget **child);
int	gp_widget_get_child_by_name  (CameraWidget *parent,
                                      const char *child_name,
				      CameraWidget **child);
int	gp_widget_get_root           (CameraWidget *widget,
                                      CameraWidget **root);

/* Get/set value/name/info */
int	gp_widget_set_value     (CameraWidget *widget, void *value);
int	gp_widget_get_value     (CameraWidget *widget, void *value);

int     gp_widget_set_name      (CameraWidget *widget, const char  *name);
int     gp_widget_get_name      (CameraWidget *widget, const char **name);

int	gp_widget_set_info      (CameraWidget *widget, const char  *info);
int	gp_widget_get_info      (CameraWidget *widget, const char **info);




int	gp_widget_get_id	(CameraWidget *widget, int *id);
int	gp_widget_get_type	(CameraWidget *widget, CameraWidgetType *type);
int	gp_widget_get_label	(CameraWidget *widget, const char **label);

int	gp_widget_set_range	(CameraWidget *range, 
				 float  low, float  high, float  increment);
int	gp_widget_get_range	(CameraWidget *range, 
				 float *low, float *high, float *increment);

int	gp_widget_add_choice     (CameraWidget *widget, const char *choice);
int	gp_widget_count_choices  (CameraWidget *widget);
int	gp_widget_get_choice     (CameraWidget *widget, int choice_number, 
                                  const char **choice);

int	gp_widget_changed        (CameraWidget *widget);
