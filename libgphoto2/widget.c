#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gphoto2.h>

/* Create/free a widget								*/
/* --------------------------------------------------------------------------	*/

CameraWidget* gp_widget_new(CameraWidgetType type, char *label) {

	CameraWidget *w;
	int x;

	w = (CameraWidget*)malloc(sizeof(CameraWidget));
	memset(w, 0, sizeof(CameraWidget));

	w->type = type;
	strcpy(w->label, label);
	
	/* set the value to nothing */
        w->value_int    = 0;
        w->value_float  = 0.0;
        w->value_string = NULL;

        w->ref_count    = 1;
	w->choice_count = 0;

        /* Alloc 64 children pointers */
	memset(w->children, 0, sizeof(CameraWidget*)*64);
	w->children_count = 0;

	/* Clear out the choices */
	for (x=0; x<32; x++)
		strcpy(w->choice[x], "");

	return (w);
}

int gp_widget_free_rec (CameraWidget *widget) {
	/* Recursively delete the widget and all children */

	int x;

	/* Return if they can't have any children */
	if ((widget->type != GP_WIDGET_WINDOW) && (widget->type != GP_WIDGET_SECTION))
		return (GP_OK);

	for (x=0; x<widget->children_count; x++) {
            gp_widget_free_rec(widget->children[x]);
            if (widget->children[x]->value_string)
                free(widget->children[x]->value_string);
            free(widget->children[x]);
	}

	return (GP_OK);
}

int gp_widget_free (CameraWidget *widget) {

	gp_widget_free_rec (widget);

        if (widget->value_string)
            free(widget->value_string);
	free(widget);

	return (GP_OK);
}

int gp_widget_ref (CameraWidget *widget) {

    widget->ref_count += 1;

    return (GP_OK);
}

int gp_widget_unref (CameraWidget *widget) {

    widget->ref_count -= 1;

    if (widget->ref_count == 0)
        gp_widget_free(widget);

    return (GP_OK);
}

/* Retrieve some common widget properties					*/
/* --------------------------------------------------------------------------	*/

int gp_widget_type (CameraWidget *widget) {

	return (widget->type);
}


char *gp_widget_label(CameraWidget *widget) {

	return (widget->label);
}

CameraWidgetCallback gp_widget_callback (CameraWidget *widget) {

	return (widget->callback);
}

int gp_widget_callback_set (CameraWidget *widget, CameraWidgetCallback callback) {

	widget->callback = callback;

	return (GP_OK);
}

/* Set and unset the value of a widget 						*/
/* --------------------------------------------------------------------------	*/

int gp_widget_value_set (CameraWidget *widget, void *value) {

        switch (widget->type) {
        case GP_WIDGET_WINDOW:
            return (GP_ERROR);
            break;
        case GP_WIDGET_SECTION:
            return (GP_ERROR);
            break;
        case GP_WIDGET_TEXT:
            if (widget->value_string) {
                if (strcmp(widget->value_string, (char*)value) != 0) {
                    widget->changed = 1;
                }
                free(widget->value_string);
            } else {
                widget->changed = 1;
            }
            widget->value_string = strdup((char*)value);
            if (!widget->value_string)
                return (GP_ERROR);
            break;
        case GP_WIDGET_RANGE:
            if (widget->value_float != *((float*)value)) {
                widget->value_float  = *((float*)value);
                widget->changed = 1;
            }
            break;
        case GP_WIDGET_TOGGLE:
            if (widget->value_int != *((int*)value)) {
                widget->value_int  = *((int*)value);
                widget->changed = 1;
            }
            break;
        case GP_WIDGET_RADIO:
            if (widget->value_string) {
                if (strcmp(widget->value_string, (char*)value) != 0) {
                    widget->changed = 1;
                }
                free(widget->value_string);
            } else {
                widget->changed = 1;
            }
            widget->value_string = strdup((char*)value);
            if (!widget->value_string)
                return (GP_ERROR);
            break;
        case GP_WIDGET_MENU:
            if (widget->value_string) {
                if (strcmp(widget->value_string, (char*)value) != 0) {
                    widget->changed = 1;
                }
                free(widget->value_string);
            } else {
                widget->changed = 1;
            }
            widget->value_string = strdup((char*)value);
            if (!widget->value_string)
                return (GP_ERROR);
            break;

        case GP_WIDGET_BUTTON:
            return (GP_ERROR);
            break;
        case GP_WIDGET_DATE:
            if (widget->value_int != *((int*)value)) {
                widget->value_int  = *((int*)value);
                widget->changed = 1;
            }
            break;
        default:
            return (GP_ERROR);
        }

	return (GP_OK);
}

int gp_widget_value_get (CameraWidget *widget, void *value) {

        switch (widget->type) {
        case GP_WIDGET_WINDOW:
            return (GP_ERROR);
            break;
        case GP_WIDGET_SECTION:
            return (GP_ERROR);
            break;
        case GP_WIDGET_TEXT:
            *((char**)value) = widget->value_string;
            break;
        case GP_WIDGET_RANGE:
            *((float*)value) = widget->value_float;
            break;
        case GP_WIDGET_TOGGLE:
            *((int*)value) = widget->value_int;
            break;
        case GP_WIDGET_RADIO:
            *((char**)value) = widget->value_string;
            break;
        case GP_WIDGET_MENU:
            *((char**)value) = widget->value_string;
            break;
        case GP_WIDGET_BUTTON:
            return (GP_ERROR);
            break;
        case GP_WIDGET_DATE:
            *((int*)value) = widget->value_int;
            break;
        default:
            return (GP_ERROR);
        }

	return (GP_OK);
}

/* Attach/Retrieve widgets to/from a parent widget				*/
/* --------------------------------------------------------------------------	*/

int gp_widget_append(CameraWidget *parent, CameraWidget *child) {

	/* Return if they can't have any children */
        if ((parent->type != GP_WIDGET_WINDOW) && (parent->type != GP_WIDGET_SECTION))
		return (GP_ERROR);

	parent->children[parent->children_count] = child;
	parent->children_count += 1;
	
	return (GP_OK);
}

int gp_widget_prepend(CameraWidget *parent, CameraWidget *child) {
	/* Do we really need this one? */

	int x;
	
	/* Return if they can't have any children */
	if ((parent->type != GP_WIDGET_WINDOW) && (parent->type != GP_WIDGET_SECTION))
		return (GP_ERROR);

	/* Shift down 1 */
	for (x=parent->children_count; x>0; x--)
		parent->children[x] = parent->children[x-1];

	/* Prepend the child */
	parent->children[0] = child;
	parent->children_count += 1;

	return (GP_OK);
}

int gp_widget_child_count(CameraWidget *parent) {

	/* Return if they can't have any children */
	if ((parent->type != GP_WIDGET_WINDOW) && (parent->type != GP_WIDGET_SECTION))
		return (0);

	return (parent->children_count);
}

CameraWidget* gp_widget_child(CameraWidget *parent, int child_number) {

	/* Return if they can't have any children */
	if ((parent->type != GP_WIDGET_WINDOW) && (parent->type != GP_WIDGET_SECTION))
		return (NULL);

	return (parent->children[child_number]);
}

CameraWidget* gp_widget_child_by_label (CameraWidget *widget, char *label) {

	int x;
	CameraWidget *child;

	if (strcmp(widget->label, label)==0)
		return (widget);

	for (x=0; x<widget->children_count; x++) {
		child = gp_widget_child_by_label(widget->children[x],label);
		if (child)
			return (child);
	}

	return (NULL);
}

/* Set/get the value of the range widget 					*/
/* --------------------------------------------------------------------------	*/

int gp_widget_range_set (CameraWidget *range, float min, float max, float increment) {

	if (range->type != GP_WIDGET_RANGE)
		return (GP_ERROR);

	range->min = min;
	range->max = max;
	range->increment = increment;

	return (GP_OK);
}

int gp_widget_range_get (CameraWidget *range, float *min, float *max, float *increment) {

	if (range->type != GP_WIDGET_RANGE)
		return (GP_ERROR);

	*min = range->min;
	*max = range->max;
	*increment = range->increment;

	return (GP_OK);
}

/* Retrieve and set choices for menus/radio buttons 				*/
/* --------------------------------------------------------------------------	*/

int gp_widget_choice_add (CameraWidget *widget, char *choice) {

	if ((widget->type != GP_WIDGET_RADIO) &&
	    (widget->type != GP_WIDGET_MENU))
		return (GP_ERROR);

	strncpy(widget->choice[widget->choice_count], choice, 64);
	widget->choice_count += 1;


	return (GP_OK);
}

int gp_widget_choice_count (CameraWidget *widget) {

	if ((widget->type != GP_WIDGET_RADIO) &&
	    (widget->type != GP_WIDGET_MENU))
		return (GP_ERROR);

	return (widget->choice_count);
}

char *gp_widget_choice (CameraWidget *widget, int choice_number) {

	if ((widget->type != GP_WIDGET_RADIO) &&
	    (widget->type != GP_WIDGET_MENU))
		return (NULL);

	if (choice_number > widget->choice_count)
		return (NULL);

	return (widget->choice[choice_number]);
}

int gp_widget_changed (CameraWidget *widget) {

        int val;

        val = widget->changed;
        widget->changed = 0;

        return (val);
}

/* Debugging output								*/
/* --------------------------------------------------------------------------	*/

void gp_widget_dump_rec (CameraWidget *widget, int depth) {

	int x;

	printf("core: ");
	for (x=0; x<depth*2; x++)
		printf(" ");
	printf("/ label=\"%s\"\n", widget->label);
	printf("core: ");
	for (x=0; x<depth*2; x++)
		printf(" ");
        switch (widget->type) {
        case GP_WIDGET_WINDOW:
        case GP_WIDGET_SECTION:
        case GP_WIDGET_BUTTON:
            break;
        case GP_WIDGET_TEXT:
        case GP_WIDGET_RADIO:
        case GP_WIDGET_MENU:
            printf("\\ value=\"%s\"", widget->value_string);
            break;
        case GP_WIDGET_RANGE:
            printf("\\ value=\"%f\"", widget->value_float);
            break;
        case GP_WIDGET_TOGGLE:
        case GP_WIDGET_DATE:
            printf("\\ value=\"%i\"", widget->value_int);
            break;
        default:
        }
        printf("\n");

	for (x=0; x<widget->children_count; x++)
		gp_widget_dump_rec(widget->children[x], depth+1);
}

int gp_widget_dump(CameraWidget *widget) {

	printf("core: Dumping widget \"%s\" and children:\n", widget->label);
	gp_widget_dump_rec(widget, 0);

	return (GP_OK);
}
