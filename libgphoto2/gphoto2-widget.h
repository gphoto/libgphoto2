/* gphoto2-widget.h
 *
 * Copyright (C) 2000 Scott Fritzinger
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, 
 * but WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details. 
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __GPHOTO2_WIDGET_H__
#define __GPHOTO2_WIDGET_H__

#include <gphoto2-context.h>

/* You don't really want to know what's inside, do you? */
typedef struct _CameraWidget CameraWidget;

#include <gphoto2-camera.h>

typedef enum {			/* Value (get/set):	*/
	GP_WIDGET_WINDOW,
	GP_WIDGET_SECTION,
	GP_WIDGET_TEXT,		/* char *		*/
	GP_WIDGET_RANGE,	/* float		*/
	GP_WIDGET_TOGGLE,	/* int			*/
	GP_WIDGET_RADIO,	/* char *		*/
	GP_WIDGET_MENU,		/* char *		*/
	GP_WIDGET_BUTTON,	/* CameraWidgetCallback */
	GP_WIDGET_DATE		/* int			*/
} CameraWidgetType;

typedef int (* CameraWidgetCallback) (Camera *, CameraWidget *, GPContext *);

int 	gp_widget_new 	(CameraWidgetType type, const char *label, 
		         CameraWidget **widget);
int    	gp_widget_free 	(CameraWidget *widget);
int     gp_widget_ref   (CameraWidget *widget);
int     gp_widget_unref (CameraWidget *widget);

int	gp_widget_append	(CameraWidget *widget, CameraWidget *child);
int 	gp_widget_prepend	(CameraWidget *widget, CameraWidget *child);

int 	gp_widget_count_children     (CameraWidget *widget);
int	gp_widget_get_child	     (CameraWidget *widget, int child_number, 
				      CameraWidget **child);

/* Retrieve Widgets */
int	gp_widget_get_child_by_label (CameraWidget *widget,
				      const char *label,
				      CameraWidget **child);
int	gp_widget_get_child_by_id    (CameraWidget *widget, int id, 
				      CameraWidget **child);
int	gp_widget_get_child_by_name  (CameraWidget *widget,
                                      const char *name,
				      CameraWidget **child);
int	gp_widget_get_root           (CameraWidget *widget,
                                      CameraWidget **root);
int     gp_widget_get_parent         (CameraWidget *widget,
				      CameraWidget **parent);

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
				 float *min, float *max, float *increment);

int	gp_widget_add_choice     (CameraWidget *widget, const char *choice);
int	gp_widget_count_choices  (CameraWidget *widget);
int	gp_widget_get_choice     (CameraWidget *widget, int choice_number, 
                                  const char **choice);

int	gp_widget_changed        (CameraWidget *widget);
int     gp_widget_set_changed    (CameraWidget *widget, int changed);

#endif /* __GPHOTO2_WIDGET_H__ */
