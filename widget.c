#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gphoto2.h>

/**
 * gp_widget_new:
 * @type: the type
 * @label: the label
 * @widget: 
 * 
 * The function creates a new widget with specified type and label.
 *
 * Return value: GPhoto error code.
 **/
int gp_widget_new (CameraWidgetType type, const char *label, 
		   CameraWidget **widget) 
{
	int x;
	static int i = 0;

	if (!label || !widget) 
		return (GP_ERROR_BAD_PARAMETERS);
	
	*widget = (CameraWidget*) malloc (sizeof (CameraWidget));
	memset (*widget, 0, sizeof (CameraWidget));

	(*widget)->type = type;
	strcpy ((*widget)->label, label);
	
	/* set the value to nothing */
	(*widget)->value_int    	= 0;
        (*widget)->value_float  	= 0.0;
        (*widget)->value_string 	= NULL;

        (*widget)->ref_count    	= 1;
	(*widget)->choice_count 	= 0;
	(*widget)->id		= i++;

        /* Alloc 64 children pointers */
	memset ((*widget)->children, 0, sizeof (CameraWidget*) * 64);
	(*widget)->children_count = 0;

	/* Clear out the choices */
	for (x = 0; x < 32; x++)
		strcpy ((*widget)->choice[x], "");

	return (GP_OK);
}

/**
 * gp_widget_free:
 * @widget: widget to free
 * 
 * The function frees a CameraWidget.
 *
 * Return value: GPhoto error code.
 **/
int gp_widget_free (CameraWidget *widget)
{
	if (!widget)
		return (GP_ERROR_BAD_PARAMETERS);

	/* Free children recursively */
	if ((widget->type == GP_WIDGET_WINDOW) ||
	    (widget->type == GP_WIDGET_SECTION)) {
	    	int x;

	    	for (x = 0; x < gp_widget_count_children (widget); x++)
			gp_widget_free (widget->children[x]);
	}
	    	
        if (widget->value_string)
            free(widget->value_string);
	free (widget);

	return (GP_OK);
}

/**
 * gp_widget_ref:
 * @widget: a CameraWidget you want to ref-count
 *
 * Increments the reference count for the CameraWidget.
 *
 * Return value: GPhoto error code.
 **/
int gp_widget_ref (CameraWidget *widget) 
{
	if (!widget)
		return (GP_ERROR_BAD_PARAMETERS);

	widget->ref_count += 1;

	return (GP_OK);
}

/**
 * gp_widget_unref:
 * @widget: a CameraWidget you want to unref
 *
 * Decrements the reference count for the CameraWidget:
 *
 * Return value: GPhoto error code.
 **/
int gp_widget_unref (CameraWidget *widget) 
{
	if (!widget)
		return (GP_ERROR_BAD_PARAMETERS);

	widget->ref_count -= 1;

	if (widget->ref_count == 0)
		gp_widget_free (widget);

	return (GP_OK);
}

/**
 * gp_widget_get_info:
 * @widget: a CameraWidget
 * @info:
 *
 * Retrieves the information about the widget.
 *
 * Return value: GPhoto error code.
 **/
int gp_widget_get_info (CameraWidget *widget, const char **info)
{
	if (!widget || !info)
		return (GP_ERROR_BAD_PARAMETERS);
	
	*info = widget->info;
	return (GP_OK);
}

/**
 * gp_widget_set_info:
 * @widget: a CameraWidget
 * @info: Information about above widget
 *
 * Sets the information about the widget.
 *
 * Return value: GPhoto error code.
 **/
int gp_widget_set_info (CameraWidget *widget, const char *info)
{
	if (!widget || !info)
		return (GP_ERROR_BAD_PARAMETERS);
	
	strcpy (widget->info, info);
	return (GP_OK);
}

/**
 * gp_widget_get_id:
 * @widget: a CameraWidget
 * @id: 
 *
 * Retreives the unique id of the CameraWidget.
 *
 * Return value: GPhoto error code.
 **/
int gp_widget_get_id (CameraWidget *widget, int *id)
{
	if (!widget || !id)
		return (GP_ERROR_BAD_PARAMETERS);

	*id = widget->id;
	return (GP_OK);
}

/**
 * gp_widget_get_type:
 * @widget: a CameraWidget
 * @type:
 *
 * Retreives the type of the CameraWidget.
 *
 * Return value: GPhoto error code.
 **/
int gp_widget_get_type (CameraWidget *widget, CameraWidgetType *type) 
{
	if (!widget || !type)
		return (GP_ERROR_BAD_PARAMETERS);
	
	*type = widget->type;
	return (GP_OK);
}

/**
 * gp_widget_get_label:
 * @widget: a CameraWidget
 * @label:
 *
 * Retreives the label of the CameraWidget.
 *
 * Return value: GPhoto error code.
 **/
int gp_widget_get_label (CameraWidget *widget, const char **label) 
{
	if (!widget || !label)
		return (GP_ERROR_BAD_PARAMETERS);
	
	*label = widget->label;
	return (GP_OK);
}

/**
 * gp_widget_set_value:
 * @widget: a CameraWidget
 * @value: 
 *
 * Sets the value of the widget.
 *
 * Return value: GPhoto error code.
 **/
int gp_widget_set_value (CameraWidget *widget, void *value) 
{
	if (!widget || !value)
		return (GP_ERROR_BAD_PARAMETERS);

        switch (widget->type) {
	case GP_WIDGET_BUTTON:
		widget->callback = (CameraWidgetCallback)value;
		return (GP_OK);
	case GP_WIDGET_MENU:
        case GP_WIDGET_TEXT:
	case GP_WIDGET_RADIO:
	        if (widget->value_string) {
                	if (strcmp (widget->value_string, (char*)value) != 0)
                    		widget->changed = 1;
                	free (widget->value_string);
        	} else
        		widget->changed = 1;
        	widget->value_string = strdup ((char*)value);
        	return (GP_OK);
        case GP_WIDGET_RANGE:
            	if (widget->value_float != *((float*)value)) {
                	widget->value_float  = *((float*)value);
                	widget->changed = 1;
            	}
            	return (GP_OK);
	case GP_WIDGET_DATE:
        case GP_WIDGET_TOGGLE:
        	if (widget->value_int != *((int*)value)) {
        		widget->value_int  = *((int*)value);
        		widget->changed = 1;
        	}
	        return (GP_OK);
	case GP_WIDGET_WINDOW:
	case GP_WIDGET_SECTION:
        default:
        	return (GP_ERROR_BAD_PARAMETERS);
        }
}

/**
 * gp_widget_get_value:
 * @widget: a CameraWidget
 * @value:
 *
 * Retreives the value of the CameraWidget.
 *
 * Return value: GPhoto error code.
 **/
int gp_widget_get_value (CameraWidget *widget, void *value) 
{
	if (!widget || !value)
		return (GP_ERROR_BAD_PARAMETERS);
	
        switch (widget->type) {
	case GP_WIDGET_BUTTON:
		*(CameraWidgetCallback*)value = widget->callback;
		return (GP_OK);
	case GP_WIDGET_RADIO:
	case GP_WIDGET_MENU:
        case GP_WIDGET_TEXT:
        	*((char**)value) = widget->value_string;
        	return (GP_OK);
        case GP_WIDGET_RANGE:
        	*((float*)value) = widget->value_float;
        	return (GP_OK);
        case GP_WIDGET_TOGGLE:
	case GP_WIDGET_DATE:
            	*((int*)value) = widget->value_int;
        	return (GP_OK);
	case GP_WIDGET_SECTION:
	case GP_WIDGET_WINDOW:
        default:
		return (GP_ERROR_BAD_PARAMETERS);
        }
}

/**
 * gp_widget_append:
 * @parent: a CameraWidget
 * @child: the CameraWidget you would like to append to above
 *
 * Appends a CameraWidget to a CameraWidget.
 *
 * Return value: GPhoto error code.
 **/
int gp_widget_append (CameraWidget *parent, CameraWidget *child) 
{
	if (!parent || !child)
		return (GP_ERROR_BAD_PARAMETERS);

	/* Return if they can't have any children */
        if ((parent->type != GP_WIDGET_WINDOW) && 
	    (parent->type != GP_WIDGET_SECTION))
		return (GP_ERROR_BAD_PARAMETERS);

	parent->children[parent->children_count] = child;
	parent->children_count += 1;
	child->parent = parent;
	child->changed = 0;

	return (GP_OK);
}

/**
 * gp_widget_prepend:
 * @parent: a CameraWidget
 * @child: the CameraWidget you would like to prepend to above
 *
 * Prepends a CameraWidget to a CameraWidget.
 *
 * Return value: GPhoto error code.
 **/
int gp_widget_prepend (CameraWidget *parent, CameraWidget *child) 
{
	int x;

	if (!parent || !child)
		return (GP_ERROR_BAD_PARAMETERS);
	
	/* Return if they can't have any children */
	if ((parent->type != GP_WIDGET_WINDOW) && 
	    (parent->type != GP_WIDGET_SECTION))
		return (GP_ERROR_BAD_PARAMETERS);

	/* Shift down 1 */
	for (x = parent->children_count; x > 0; x--)
		parent->children[x] = parent->children[x - 1];

	/* Prepend the child */
	parent->children[0] = child;
	parent->children_count += 1;
	child->parent = parent;
	child->changed = 0;

	return (GP_OK);
}

/**
 * gp_widget_count_children:
 * @widget: a CameraWidget
 *
 * Counts the children of the CameraWidget.
 *
 * Return value: GPhoto error code or number of children
 **/
int gp_widget_count_children (CameraWidget *widget) 
{
	if (!widget)
		return (GP_ERROR_BAD_PARAMETERS);

	return (widget->children_count);
}

/**
 * gp_widget_get_child:
 * @parent: a CameraWidget
 * @child_number: the number of the child
 * @child:
 *
 * Retreives the child number @child_number of the parent.
 *
 * Return value: GPhoto error code.
 **/
int gp_widget_get_child (CameraWidget *parent, int child_number, 
			 CameraWidget **child) 
{
	if (!parent || !child)
		return (GP_ERROR_BAD_PARAMETERS);

	if (child_number >= parent->children_count)
		return (GP_ERROR_BAD_PARAMETERS);

	*child = parent->children[child_number];
	return (GP_OK);
}

/**
 * gp_widget_get_child_by_label:
 * @widget: a CameraWidget
 * @label: the label of the child
 *
 * Retreives the child with label @label of the CameraWidget.
 *
 * Return value: GPhoto error code.
 **/
int gp_widget_get_child_by_label (CameraWidget *widget, const char *label, 
				  CameraWidget **child)
{
	int x;

	if (!widget || !label || !child)
		return (GP_ERROR_BAD_PARAMETERS);

	if (strcmp (widget->label, label) == 0) {
		*child = widget;
		return (GP_OK);
	}

	for (x = 0; x < widget->children_count; x++) {
		int result;
		CameraWidget *child_rec;
		
		result = gp_widget_get_child_by_label (widget->children[x], 
						       label, &child_rec);
		if (result == GP_OK) {
			*child = child_rec;
			return (GP_OK);
		}
	}

	return (GP_ERROR_BAD_PARAMETERS);
}

/**
 * gp_widget_get_child_by_id:
 * @widget: a CameraWidget
 * @id: the id of the child
 *
 * Retreives the child with id @id of the widget.
 *
 * Return value: GPhoto error code.
 **/
int gp_widget_get_child_by_id (CameraWidget *widget, int id, 
			       CameraWidget **child) 
{
	int x;

	if (!widget || !child)
		return (GP_ERROR_BAD_PARAMETERS);

	if (widget->id == id) {
		*child = widget;
		return (GP_OK);
	}
	
	for (x = 0; x < widget->children_count; x++) {
		int result;
		CameraWidget *child_rec;
		
		result = gp_widget_get_child_by_id (widget->children[x], id, 
						    &child_rec);
		if (result == GP_OK) {
			*child = child_rec;
			return (GP_OK);
		}
	}

	return (GP_ERROR_BAD_PARAMETERS);
}

/**
 * gp_widget_get_root:
 * @widget: a CameraWidget
 * @root:
 *
 * Retreives the root of the CameraWidget.
 *
 * Return value: GPhoto error code.
 **/
int gp_widget_get_root (CameraWidget *widget, CameraWidget **root) 
{
	if (!widget || !root)
		return (GP_ERROR_BAD_PARAMETERS);

	if (widget->parent) 
		return (gp_widget_get_root (widget->parent, root));
	else {
		*root = widget;
		return (GP_OK);
	}
}

/**
 * gp_widget_set_range:
 * @range: a CameraWidget of type GP_WIDGET_RANGE
 * @min:
 * @max:
 * @increment:
 *
 * Sets some range parameters of the CameraWidget.
 *
 * Return value: GPhoto error code.
 **/
int gp_widget_set_range (CameraWidget *range, float min, float max, 
			 float increment) 
{
	if (!range || (range->type != GP_WIDGET_RANGE))
		return (GP_ERROR_BAD_PARAMETERS);

	range->min = min;
	range->max = max;
	range->increment = increment;

	return (GP_OK);
}

/**
 * gp_widget_get_range
 * @range: a CameraWidget of type GP_WIDGET_RANGE
 * @min:
 * @max:
 * @increment:
 *
 * Retreives some range parameters of the CameraWidget.
 *
 * Return value: GPhoto error code.
 **/
int gp_widget_get_range (CameraWidget *range, float *min, float *max, 
			 float *increment) 
{
	if (!range || (range->type != GP_WIDGET_RANGE) || !min || !max || 
	    !increment)
		return (GP_ERROR_BAD_PARAMETERS);

	*min = range->min;
	*max = range->max;
	*increment = range->increment;

	return (GP_OK);
}

/**
 * gp_widget_add_choice:
 * @widget: a CameraWidget of type GP_WIDGET_RADIO or GP_WIDGET_MENU
 * @choice:
 *
 * Adds a choice to the CameraWidget.
 *
 * Return value: GPhoto error code.
 **/
int gp_widget_choice_add (CameraWidget *widget, const char *choice) 
{
	if (!widget || !choice)
		return (GP_ERROR_BAD_PARAMETERS);

	if ((widget->type != GP_WIDGET_RADIO) &&
	    (widget->type != GP_WIDGET_MENU))
		return (GP_ERROR_BAD_PARAMETERS);

	strncpy (widget->choice[widget->choice_count], choice, 64);
	widget->choice_count += 1;

	return (GP_OK);
}

/**
 * gp_widget_count_choices:
 * @widget: a CameraWidget of type GP_WIDGET_RADIO or GP_WIDGET_MENU
 * 
 * Counts the choices of the CameraWidget.
 *
 * Return value: GPhoto error code or number of choices.
 **/
int gp_widget_count_choices (CameraWidget *widget) 
{
	if (!widget)
		return (GP_ERROR_BAD_PARAMETERS);

	if ((widget->type != GP_WIDGET_RADIO) &&
	    (widget->type != GP_WIDGET_MENU))
		return (GP_ERROR_BAD_PARAMETERS);

	return (widget->choice_count);
}

/**
 * gp_widget_get_choice:
 * @widget: a CameraWidget of type GP_WIDGET_RADIO or GP_WIDGET_MENU
 * @choice_number:
 * @choice:
 *
 * Retreives the choice number @choice_number.
 *
 * Return value: GPhoto error code
 **/
int gp_widget_get_choice (CameraWidget *widget, int choice_number, 
			  const char **choice) 
{
	if (!widget || !choice) 
		return (GP_ERROR_BAD_PARAMETERS);

	if ((widget->type != GP_WIDGET_RADIO) &&
	    (widget->type != GP_WIDGET_MENU))
		return (GP_ERROR_BAD_PARAMETERS);

	if (choice_number >= widget->choice_count)
		return (GP_ERROR_BAD_PARAMETERS);

	*choice = widget->choice[choice_number];
	return (GP_OK);
}

/**
 * gp_widget_changed:
 * @widget: a CameraWidget
 *
 * Returns the changed flag (1 if value of widget has been changed) of the 
 * CameraWidget and sets it to 0.
 *
 * Return value: GPhoto error code or changed flag.
 **/
int gp_widget_changed (CameraWidget *widget) 
{
        int val;

	if (!widget)
		return (GP_ERROR_BAD_PARAMETERS);

        val = widget->changed;
        widget->changed = 0;

        return (val);
}

