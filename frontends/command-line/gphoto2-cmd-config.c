/* gphoto2-cmd-config.c:
 *
 * Copyright (C) 2001 Lutz Müller <urc8@rz.uni-karlsruhe.de>
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

#include <config.h>
#include "gphoto2-cmd-config.h"

#include <time.h>

#include <gphoto2-widget.h>
#include <cdk/cdk.h>

#ifdef ENABLE_NLS
#  include <libintl.h>
#  undef _
#  define _(String) dgettext (PACKAGE, String)
#  ifdef gettext_noop
#    define N_(String) gettext_noop (String)
#  else
#    define N_(String) (String)
#  endif
#else
#  define textdomain(String) (String)
#  define gettext(String) (String)
#  define dgettext(Domain,Message) (Message)
#  define dcgettext(Domain,Message,Type) (Message)
#  define bindtextdomain(Domain,Directory) (Domain)
#  define _(String) (String)
#  define N_(String) (String)
#endif

typedef struct {
	Camera *camera;
	CDKSCREEN *screen;
	const char *folder;
	const char *file;
	CameraWidget *window;
} CmdConfig;

static int show_widget (CmdConfig *cmd_config, CameraWidget *widget);

static int
set_config (CmdConfig *cmd_config)
{
	if (cmd_config->file)
		return (gp_camera_file_set_config (cmd_config->camera,
						   cmd_config->folder,
						   cmd_config->file,
						   cmd_config->window));
	else if (cmd_config->folder)
		return (gp_camera_folder_set_config (cmd_config->camera,
						     cmd_config->folder,
						     cmd_config->window));
	else
		return (gp_camera_set_config (cmd_config->camera,
					      cmd_config->window));
}

static int
show_section (CmdConfig *cmd_config, CameraWidget *section)
{
	CameraWidget *child, *parent;
	CameraWidgetType type;
	CDKSCROLL *scroll = NULL;
	const char *label;
	char *items[100];
	int count, x, selection;
	char title[1024];
	int show_parent = 0, show_child = 0;

	/* Create the scroll list */
	gp_widget_get_type (section, &type);
	gp_widget_get_label (section, &label);
	snprintf (title, sizeof (title), "<C></5>%s", label);
	if (type == GP_WIDGET_WINDOW)
		items[0] = copyChar (_("Exit"));
	else
		items[0] = copyChar (_("Back"));
	count = gp_widget_count_children (section);
	for (x = 0; x < count; x++) {
		gp_widget_get_child (section, x, &child);
		gp_widget_get_label (child, &label);
		items[x + 1] = copyChar ((char *) label);
	}
	scroll = newCDKScroll (cmd_config->screen, CENTER, CENTER, RIGHT,
			       10, 50, title,
			       items, count, NUMBERS, A_REVERSE, TRUE, FALSE);
	if (!scroll)
		return (GP_ERROR);

	selection = activateCDKScroll (scroll, 0);
	if (scroll->exitType == vNORMAL) {
		if (selection)
			show_child = selection;
		else if (type != GP_WIDGET_WINDOW)
			show_parent = 1;
	}

	/* Clean up */
	destroyCDKScroll (scroll);

	if (show_parent) {
		gp_widget_get_parent (section, &parent);
		show_widget (cmd_config, parent);
	} else if (show_child) {
		gp_widget_get_child (section, show_child - 1, &child);
		show_widget (cmd_config, child);
	}

	return (GP_OK);
}

static int
show_date (CmdConfig *cmd_config, CameraWidget *date)
{
	CDKCALENDAR *calendar = NULL;
	int time, day, month, year, selection;
	struct tm *date_info;
	const char *label;
	char title[1024];

	gp_widget_get_value (date, &time);
	date_info = localtime ((time_t *) &time);
	day = date_info->tm_mday;
	month = date_info->tm_mon;
	year = date_info->tm_year + 1900;

	gp_widget_get_label (date, &label);
	snprintf (title, sizeof (title), "<C></U>%s", label);

	/* Create the calendar */
	calendar = newCDKCalendar (cmd_config->screen, CENTER, CENTER, title,
				   day, month, year,
				   COLOR_PAIR(16)|A_BOLD,
				   COLOR_PAIR(24)|A_BOLD,
				   COLOR_PAIR(32)|A_BOLD,
				   COLOR_PAIR(40)|A_REVERSE, TRUE, FALSE);
	if (!calendar)
		return (GP_ERROR);

	drawCDKCalendar (calendar, TRUE);
	selection = activateCDKCalendar (calendar, 0);

	if (calendar->exitType == vNORMAL) {
		date_info->tm_mday = calendar->day;
		date_info->tm_mon = calendar->month;
		date_info->tm_year = calendar->year - 1900;
		time = mktime (date_info);
		gp_widget_set_value (date, &time);
		set_config (cmd_config);
	}

	destroyCDKCalendar (calendar);
	return (GP_OK);
}

static int
show_radio (CmdConfig *cmd_config, CameraWidget *radio)
{
	CDKITEMLIST *list = NULL;
	const char *label, *value, *current_value;
	char title[1024], *items[100];
	int x, count, current = 0, selection;

	gp_widget_get_label (radio, &label);
	snprintf (title, sizeof (title), "<C></U>%s", label);
	gp_widget_get_value (radio, &current_value);
	count = gp_widget_count_choices (radio);
	for (x = 0; x < count; x++) {
		gp_widget_get_choice (radio, x, &value);
		items[x] = copyChar ((char *) value);
		if (!strcmp (current_value, value))
			current = x;
	}

	list = newCDKItemlist (cmd_config->screen, CENTER, CENTER,
			       title, _("Value: "), items, count,
			       current, TRUE, FALSE);
	if (!list)
		return (GP_ERROR);

	selection = activateCDKItemlist (list, 0);
	if (list->exitType == vNORMAL) {
		gp_widget_get_choice (radio, selection, &value);
		gp_widget_set_value (radio, (void *) value);
		set_config (cmd_config);
	}

	destroyCDKItemlist (list);
	return (GP_OK);
}

static int
show_range (CmdConfig *cmd_config, CameraWidget *range)
{
	CDKSLIDER *slider = NULL;
	float value, min, max, increment;
	const char *label;
	char title[1024];
	int selection;

	gp_widget_get_value (range, &value);
	gp_widget_get_label (range, &label);
	snprintf (title, sizeof (title), "<C></U>%s", label);
	gp_widget_get_range (range, &min, &max, &increment);

	slider = newCDKSlider (cmd_config->screen, CENTER, CENTER, title,
			       _("Value: "), A_REVERSE | COLOR_PAIR (29) | ' ',
			       50, (int) value, min, max,
			       increment, increment * 10, TRUE,
			       FALSE);
	if (!slider)
		return (GP_ERROR);

	selection = activateCDKSlider (slider, 0);
	if (slider->exitType == vNORMAL) {
		value = selection;
		gp_widget_set_value (range, &value);
		set_config (cmd_config);
	}
	
	destroyCDKSlider (slider);
	return (GP_OK);
}

static int
show_widget (CmdConfig *cmd_config, CameraWidget *widget)
{
	CameraWidget *parent;
	CameraWidgetType type;

	gp_widget_get_type (widget, &type);
	switch (type) {
	case GP_WIDGET_WINDOW:
	case GP_WIDGET_SECTION:
		show_section (cmd_config, widget);
		break;
	case GP_WIDGET_DATE:
		show_date (cmd_config, widget);
		gp_widget_get_parent (widget, &parent);
		show_widget (cmd_config, parent);
		break;
	case GP_WIDGET_MENU:
	case GP_WIDGET_RADIO:
		show_radio (cmd_config, widget);
		gp_widget_get_parent (widget, &parent);
		show_widget (cmd_config, parent);
		break;
	case GP_WIDGET_RANGE:
		show_range (cmd_config, widget);
		gp_widget_get_parent (widget, &parent);
		show_widget (cmd_config, parent);
		break;
	default:
		return (GP_ERROR_NOT_SUPPORTED);
	}

	return (GP_OK);
}

static void
status_func (Camera *camera, const char *status, void *data)
{
	CmdConfig *cmd_config = data;

	/* We need to do something in here */
	cmd_config = NULL;
}

int
gp_cmd_config (Camera *camera, const char *folder, const char *file)
{
	CmdConfig cmd_config;
	CameraWidget *config;
	int result;
	CDKSCREEN *screen = NULL;
	WINDOW *window = NULL;

	if (!camera)
		return (GP_ERROR_BAD_PARAMETERS);

	if (file) {
		if (!folder)
			return (GP_ERROR_BAD_PARAMETERS);
		result = gp_camera_file_get_config (camera, folder, file,
						    &config);
	} else if (folder)
		result = gp_camera_folder_get_config (camera, folder, &config);
	else
		result = gp_camera_get_config (camera, &config);
	if (result < 0)
		return (result);

	/* Set up CDK. */
	window = initscr ();
	screen = initCDKScreen (window);

	/* Set up CDK Colors. */
	initCDKColor ();

	/* Go! */
	cmd_config.camera = camera;
	cmd_config.folder = folder;
	cmd_config.file = file;
	cmd_config.screen = screen;
	cmd_config.window = config;
	gp_camera_set_status_func (camera, status_func, &cmd_config);
	result = show_widget (&cmd_config, config);

	/* Clean up */
	destroyCDKScreen (screen);
	delwin (window);
	endCDK ();

	return (result);
}
