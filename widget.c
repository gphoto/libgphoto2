#include <stdlib.h>
#include <gphoto2.h>

CameraWidget* gp_widget_new(CameraWidgetType type, char *label) {

	CameraWidget *w;

	w = (CameraWidget*)malloc(sizeof(CameraWidget));
	memset(w, 0, sizeof(CameraWidget));

	w->type = type;
	strcpy(w->label, label);
	
	/* for now, for ease of mem management, pre-alloc 64 children pointers */
	w->children = (CameraWidget**)malloc(sizeof(CameraWidget*)*64);
	memset(w->children, 0, sizeof(CameraWidget*)*64);	
	w->children_count = 0;

	return (w);
}

int gp_widget_append(CameraWidget *parent, CameraWidget *child) {

	parent->children[parent->children_count++] = child;
	
	return (GP_OK);
}

int gp_widget_prepend(CameraWidget *parent, CameraWidget *child) {

	int x;
	
	/* Shift down 1 */
	for (x=parent->children_count; x>0; x--)
		parent->children[x] = parent->children[x-1];

	/* Prepend the child */
	parent->children[0] = child;	

	return (GP_OK);
}

int gp_widget_child_count(CameraWidget *parent) {

	return (parent->children_count);
}

CameraWidget*   gp_widget_child(CameraWidget *parent, int child_number) {

	return (parent->children[child_number]);
}

int gp_widget_free(CameraWidget *widget) {
	/* Recursively delete the widget and all children */

	if (widget->children_count == 0)
		return (GP_OK);

	for (x=0; x<widget->children_count; x++) {
		gp_widget_free(widget->children[x]);
		free(widget->children[x]);
	}

	free(widget);

	return (GP_OK);
}
