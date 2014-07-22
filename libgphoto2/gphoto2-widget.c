/** \file
 *
 * \author Copyright 2000 Scott Fritzinger
 *
 * \par License
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * \par
 * This library is distributed in the hope that it will be useful, 
 * but WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details. 
 *
 * \par
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */
#include "config.h"
#include <gphoto2/gphoto2-widget.h>

#include <stdlib.h>
#include <string.h>

#include <gphoto2/gphoto2-result.h>
#include <gphoto2/gphoto2-port-log.h>

/**
 * CameraWidget:
 *
 * The internals of the #CameraWidget are only visible to gphoto2. You can
 * only access them using the functions provided by gphoto2.
 **/
struct _CameraWidget {
	CameraWidgetType type;
	char    label [256];
	char    info [1024];
	char    name [256];

	CameraWidget *parent;

	/* Current value of the widget */
	char   *value_string;
	int     value_int;
	float   value_float;

	/* For Radio and Menu */
	char    **choice;
	int     choice_count;

	/* For Range */
	float   min; 
	float   max; 
	float   increment;

	/* Child info */
	CameraWidget **children;
	int           children_count;

	/* Widget was changed */
	int     changed;

	/* Widget is read only */
	int	readonly;

	/* Reference count */
	int     ref_count;

	/* Unique identifier */
	int     id;

	/* Callback */
	CameraWidgetCallback callback;
};

/**
 * \brief Create a new widget.
 *
 * The function creates a new #CameraWidget of specified type and with
 * given label.
 *
 * @param type the type
 * @param label the label
 * @param widget 
 * @return a gphoto2 error code.
 * 
 **/
int
gp_widget_new (CameraWidgetType type, const char *label, 
		   CameraWidget **widget) 
{
	static int i = 0;

	C_PARAMS (label && widget);

	C_MEM (*widget = calloc (1, sizeof (CameraWidget)));

	(*widget)->type = type;
	strcpy ((*widget)->label, label);
	
	/* set the value to nothing */
	(*widget)->value_int    	= 0;
        (*widget)->value_float  	= 0.0;
        (*widget)->value_string 	= NULL;

        (*widget)->ref_count    	= 1;
	(*widget)->choice_count 	= 0;
	(*widget)->choice 		= NULL;
	(*widget)->readonly 		= 0;
	(*widget)->id			= i++;

        /* Clear all children pointers */
	free ((*widget)->children);
	(*widget)->children = NULL;
	(*widget)->children_count = 0;

	return (GP_OK);
}

/**
 * \brief Frees a CameraWidget
 *
 * @param widget the #CameraWidget to be freed
 * @return a gphoto2 error code.
 * 
 **/
int
gp_widget_free (CameraWidget *widget)
{
	int x;
	C_PARAMS (widget);

	/* Free children recursively */
	if ((widget->type == GP_WIDGET_WINDOW) ||
	    (widget->type == GP_WIDGET_SECTION)) {
	    	for (x = 0; x < gp_widget_count_children (widget); x++)
			gp_widget_free (widget->children[x]);
		free (widget->children);
	}
	for (x = 0; x < widget->choice_count; x++)
		free (widget->choice[x]);
	free (widget->choice);
	free (widget->value_string);
	free (widget);
	return (GP_OK);
}

/**
 * \brief Increments the reference count for the CameraWidget
 *
 * @param widget a #CameraWidget you want to ref-count
 * @return a gphoto2 error code.
 *
 **/
int
gp_widget_ref (CameraWidget *widget) 
{
	C_PARAMS (widget);

	widget->ref_count += 1;

	return (GP_OK);
}

/**
 * \brief Decrements the reference count for the CameraWidget
 *
 * @param widget a CameraWidget you want to unref
 * @return a gphoto2 error code.
 *
 **/
int
gp_widget_unref (CameraWidget *widget) 
{
	C_PARAMS (widget);

	widget->ref_count -= 1;

	if (widget->ref_count == 0)
		gp_widget_free (widget);

	return (GP_OK);
}

/**
 * \brief Retrieves the information about the widget
 *
 * @param widget a #CameraWidget
 * @param info
 * @return a gphoto2 error code.
 *
 **/
int
gp_widget_get_info (CameraWidget *widget, const char **info)
{
	C_PARAMS (widget && info);

	*info = widget->info;
	return (GP_OK);
}


/**
 * \brief Sets the information about the widget
 *
 * @param widget a #CameraWidget
 * @param info Information about above widget
 * @return a gphoto2 error code.
 *
 **/
int
gp_widget_set_info (CameraWidget *widget, const char *info)
{
	C_PARAMS (widget && info);

	strcpy (widget->info, info);
	return (GP_OK);
}

/**
 * \brief Gets the name of the widget
 *
 * @param widget a #CameraWidget
 * @param name Name of above widget
 * @return a gphoto2 error code.
 *
 **/
int
gp_widget_get_name (CameraWidget *widget, const char **name)
{
	C_PARAMS (widget && name);

	*name = widget->name;
	return (GP_OK);
}

/**
 * \brief Sets the name of the widget
 *
 * @param widget a #CameraWidget
 * @param name Name of above widget
 * @return a gphoto2 error code.
 *
 **/
int
gp_widget_set_name (CameraWidget *widget, const char *name)
{
	C_PARAMS (widget && name);

	strcpy (widget->name, name);
	return (GP_OK);
}



/**
 * \brief Retrieves the unique id of the #CameraWidget
 *
 * @param widget a #CameraWidget
 * @param id 
 * @return a gphoto2 error code.
 *
 **/
int
gp_widget_get_id (CameraWidget *widget, int *id)
{
	C_PARAMS (widget && id);

	*id = widget->id;
	return (GP_OK);
}

/**
 * \brief Tells that the widget has been changed
 *
 * @param widget a #CameraWidget
 * @param changed a boolean whether we changed or not
 * @return a gphoto2 error code
 *
 * Sets the changed of the CameraWidget depending on 
 * the changed parameter.
 *
 **/
int
gp_widget_set_changed (CameraWidget *widget, int changed)
{
	C_PARAMS (widget);

	widget->changed = changed;
	return (GP_OK);
}

/**
 * \brief Tells that the widget is readonly
 *
 * @param widget a #CameraWidget
 * @param changed a boolean whether we are readonly or not
 * @return a gphoto2 error code
 *
 * Sets the readonly of the CameraWidget depending on 
 * the changed parameter.
 *
 * Only useful when called from the camera driver.
 **/
int
gp_widget_set_readonly (CameraWidget *widget, int readonly)
{
	C_PARAMS (widget);

	widget->readonly = readonly;
	return (GP_OK);
}

/**
 * \brief Retrieves the readonly state of the #CameraWidget
 *
 * @param widget a #CameraWidget
 * @param readonly
 * @return a gphoto2 error code.
 *
 **/
int
gp_widget_get_readonly (CameraWidget *widget, int *readonly) 
{
	C_PARAMS (widget && readonly);

	*readonly = widget->readonly;
	return (GP_OK);
}

/**
 * \brief Retrieves the type of the #CameraWidget
 *
 * @param widget a #CameraWidget
 * @param type
 * @return a gphoto2 error code.
 *
 **/
int
gp_widget_get_type (CameraWidget *widget, CameraWidgetType *type) 
{
	C_PARAMS (widget && type);

	*type = widget->type;
	return (GP_OK);
}

/**
 * \brief Retrieves the label of the #CameraWidget
 *
 * @param widget a #CameraWidget
 * @param label
 * @return a gphoto2 error code.
 *
 **/
int
gp_widget_get_label (CameraWidget *widget, const char **label) 
{
	C_PARAMS (widget && label);

	*label = widget->label;
	return (GP_OK);
}

/**
 * \brief Sets the value of the widget
 *
 * @param widget a #CameraWidget
 * @param value 
 * @return a gphoto2 error code.
 *
 * Please pass
 * (char*) for GP_WIDGET_MENU, GP_WIDGET_TEXT, GP_WIDGET_RADIO,
 * (float) for GP_WIDGET_RANGE,
 * (int)   for GP_WIDGET_DATE, GP_WIDGET_TOGGLE, and
 * (CameraWidgetCallback) for GP_WIDGET_BUTTON.
 *
 **/
int
gp_widget_set_value (CameraWidget *widget, const void *value) 
{
	C_PARAMS (widget && value);

        switch (widget->type) {
	case GP_WIDGET_BUTTON:
		widget->callback = (CameraWidgetCallback) value;
		return (GP_OK);
	case GP_WIDGET_MENU:
	case GP_WIDGET_RADIO:
        case GP_WIDGET_TEXT:
		GP_LOG_D ("Setting value of widget '%s' to '%s'...",
			widget->label, (char*) value);
		if (widget->value_string) {
                	if (strcmp (widget->value_string, (char*) value))
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
 * \brief Retrieves the value of the #CameraWidget
 *
 * @param widget a #CameraWidget
 * @param value
 * @return a gphoto2 error code.
 *
 **/
int
gp_widget_get_value (CameraWidget *widget, void *value) 
{
	C_PARAMS (widget && value);

        switch (widget->type) {
	case GP_WIDGET_BUTTON:
		*(CameraWidgetCallback*)value = widget->callback;
		return (GP_OK);
	case GP_WIDGET_MENU:
	case GP_WIDGET_RADIO:
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
 * \brief Appends a #CameraWidget to a #CameraWidget
 *
 * @param widget a #CameraWidget
 * @param child the #CameraWidget you would like to append to above
 * @return a gphoto2 error code.
 *
 **/
int
gp_widget_append (CameraWidget *widget, CameraWidget *child) 
{
	C_PARAMS (widget && child);

	/* Return if they can't have any children */
        C_PARAMS ((widget->type == GP_WIDGET_WINDOW) ||
                  (widget->type == GP_WIDGET_SECTION));

	C_MEM (widget->children = realloc(widget->children, sizeof(CameraWidget*)*(widget->children_count+1)));
	widget->children[widget->children_count] = child;
	widget->children_count += 1;
	child->parent = widget;
	child->changed = 0;

	return (GP_OK);
}

/**
 * \brief Prepends a #CameraWidget to a #CameraWidget
 *
 * @param widget a #CameraWidget
 * @param child the #CameraWidget you would like to prepend to above
 * @return a gphoto2 error code.
 *
 **/
int
gp_widget_prepend (CameraWidget *widget, CameraWidget *child) 
{
	int x;

	C_PARAMS (widget && child);

	/* Return if they can't have any children */
	C_PARAMS ((widget->type == GP_WIDGET_WINDOW) ||
		  (widget->type == GP_WIDGET_SECTION));

	C_MEM (widget->children = realloc(widget->children, sizeof(CameraWidget*)*(widget->children_count+1)));

	/* Shift down 1 */
	for (x = widget->children_count; x > 0; x--)
		widget->children[x] = widget->children[x - 1];

	/* Prepend the child */
	widget->children[0] = child;
	widget->children_count += 1;
	child->parent = widget;
	child->changed = 0;

	return (GP_OK);
}

/**
 * \brief Counts the children of the #CameraWidget
 *
 * @param widget a #CameraWidget
 * @return a gphoto2 error code or number of children
 *
 **/
int
gp_widget_count_children (CameraWidget *widget) 
{
	C_PARAMS (widget);

	return (widget->children_count);
}

/**
 * \brief Retrieves the child number \c child_number of the parent
 *
 * @param widget a #CameraWidget
 * @param child_number the number of the child
 * @param child
 * @return a gphoto2 error code.
 *
 **/
int
gp_widget_get_child (CameraWidget *widget, int child_number, 
		     CameraWidget **child) 
{
	C_PARAMS (widget && child);
	C_PARAMS (child_number < widget->children_count);

	*child = widget->children[child_number];
	return (GP_OK);
}

/**
 * \brief Retrieves the child with label \c label of the #CameraWidget
 *
 * @param widget a #CameraWidget
 * @param label the label of the child
 * @param child
 * @return a gphoto2 error code.
 *
 **/
int
gp_widget_get_child_by_label (CameraWidget *widget, const char *label, 
			      CameraWidget **child)
{
	int x;

	C_PARAMS (widget && label && child);

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
 * \brief Retrieves the child with id \c id of the widget
 *
 * @param widget a #CameraWidget
 * @param id the id of the child
 * @param child
 * @return a gphoto2 error code.
 *
 **/
int
gp_widget_get_child_by_id (CameraWidget *widget, int id, CameraWidget **child) 
{
	int x;

	C_PARAMS (widget && child);

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
 * \brief Retrieves the child with name \c name of the widget
 *
 * @param widget a #CameraWidget
 * @param name the name of the child
 * @return a gphoto2 error code.
 *
 **/
int
gp_widget_get_child_by_name (CameraWidget *widget, const char *name,
			     CameraWidget **child)
{
	int x;

	C_PARAMS (widget && child);

	if (!strcmp (widget->name, name)) {
		*child = widget;
		return (GP_OK);
	}
	
	for (x = 0; x < widget->children_count; x++) {
		int result;
		CameraWidget *child_rec;
		
		result = gp_widget_get_child_by_name (widget->children[x], name,
						      &child_rec);
		if (result == GP_OK) {
			*child = child_rec;
			return (GP_OK);
		}
	}

	return (GP_ERROR_BAD_PARAMETERS);
}

/**
 * \brief Retrieves the parent of a CameraWidget
 *
 * @param widget a #CameraWidget
 * @param parent the pointer to the parent to return
 * @return a gphoto2 error code.
 *
 **/
int
gp_widget_get_parent (CameraWidget *widget, CameraWidget **parent)
{
	C_PARAMS (widget && parent);

	*parent = widget->parent;

	return (GP_OK);
}

/**
 * \brief Retrieves the root of the #CameraWidget
 *
 * @param widget a #CameraWidget
 * @param root
 * @return a gphoto2 error code.
 *
 **/
int
gp_widget_get_root (CameraWidget *widget, CameraWidget **root) 
{
	C_PARAMS (widget && root);

	if (widget->parent) 
		return (gp_widget_get_root (widget->parent, root));
	else {
		*root = widget;
		return (GP_OK);
	}
}

/**
 * \brief Sets some range parameters of the #CameraWidget
 *
 * @param range a #CameraWidget of type GP_WIDGET_RANGE
 * @param min
 * @param max
 * @param increment
 * @return a gphoto2 error code.
 *
 **/
int
gp_widget_set_range (CameraWidget *range, float min, float max, float increment)
{
	C_PARAMS (range);
	C_PARAMS (range->type == GP_WIDGET_RANGE);

	range->min = min;
	range->max = max;
	range->increment = increment;

	return (GP_OK);
}

/**
 * \brief Retrieves some range parameters of the #CameraWidget
 *
 * @param range a #CameraWidget of type GP_WIDGET_RANGE
 * @param min
 * @param max
 * @param increment
 * @return a gphoto2 error code.
 *
 **/
int
gp_widget_get_range (CameraWidget *range, float *min, float *max, 
		     float *increment) 
{
	C_PARAMS (range && min && max && increment);
	C_PARAMS (range->type == GP_WIDGET_RANGE);

	*min = range->min;
	*max = range->max;
	*increment = range->increment;

	return (GP_OK);
}

/**
 * \brief Adds a choice to the #CameraWidget
 *
 * @param widget a #CameraWidget of type GP_WIDGET_RADIO or GP_WIDGET_MENU
 * @param choice
 * @return a gphoto2 error code.
 *
 **/
int
gp_widget_add_choice (CameraWidget *widget, const char *choice) 
{
	C_PARAMS (widget && choice);
	C_PARAMS ((widget->type == GP_WIDGET_RADIO) ||
		  (widget->type == GP_WIDGET_MENU));

	C_MEM (widget->choice = realloc (widget->choice, sizeof(char*)*(widget->choice_count+1)));
	widget->choice[widget->choice_count] = strdup(choice);
	widget->choice_count += 1;
	return (GP_OK);
}

/**
 * \brief Counts the choices of the #CameraWidget
 *
 * @param widget a #CameraWidget of type GP_WIDGET_RADIO or GP_WIDGET_MENU
 * @return a gphoto2 error code or number of choices.
 * 
 **/
int
gp_widget_count_choices (CameraWidget *widget) 
{
	C_PARAMS (widget);
	C_PARAMS ((widget->type == GP_WIDGET_RADIO) ||
		  (widget->type == GP_WIDGET_MENU));

	return (widget->choice_count);
}

/**
 * \brief Retrieves the choice number \c choice_number
 *
 * @param widget a #CameraWidget of type GP_WIDGET_RADIO or GP_WIDGET_MENU
 * @param choice_number
 * @param choice
 * @return a gphoto2 error code
 *
 **/
int
gp_widget_get_choice (CameraWidget *widget, int choice_number, 
		      const char **choice) 
{
	C_PARAMS (widget && choice);
	C_PARAMS ((widget->type == GP_WIDGET_RADIO) ||
		  (widget->type == GP_WIDGET_MENU));
	C_PARAMS (choice_number < widget->choice_count);

	*choice = widget->choice[choice_number];
	return (GP_OK);
}

/**
 * \brief Tells if the widget has been changed
 *
 * @param widget a #CameraWidget
 * @return a gphoto2 error code or changed flag.
 *
 * Returns 1 if the state of the #CameraWidget has been changed or 0 if not.
 * In addition, it resets the changed flag to 0.
 *
 **/
int
gp_widget_changed (CameraWidget *widget) 
{
        int val;

	C_PARAMS (widget);

        val = widget->changed;
        widget->changed = 0;

        return (val);
}
