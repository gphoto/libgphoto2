/* Create a new widget */
CameraWidget* gp_widget_new(CameraWidgetType type, char *label);

/* Add a child widget to the parent widget */
int gp_widget_append(CameraWidget *parent, CameraWidget *child);
int gp_widget_prepend(CameraWidget *parent, CameraWidget *child);

/* Retrieve the number of children a parent has */
int gp_widget_child_count(CameraWidget *parent);

/* Retrieve a pointer to a child #child_number of the parent */
CameraWidget*   gp_widget_child(CameraWidget *parent, int child_number);

/* Frees a widget, as well as all the children */
int gp_widget_free(CameraWidget *widget);
