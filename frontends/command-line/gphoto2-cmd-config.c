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
#include <math.h>

#include <gphoto2-widget.h>
#include <cdk.h>

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

#ifndef MAX
# define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif
#ifndef MIN
# define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

typedef struct {
	Camera *camera;
	CDKSCREEN *screen;
	CameraWidget *window;
	GPContext *context;
} CmdConfig;

#define CHECK(result) {int r=(result);if(r<0)return(r);}

static int show_widget (CmdConfig *cmd_config, CameraWidget *widget);

static int
set_config (CmdConfig *cmd_config)
{
	int result, selection;
	char *msg[10];
	char *buttons[] = {N_("</B/24>Continue"), N_("</B16>Cancel")};
	CDKDIALOG *question = NULL;

	result = gp_camera_set_config (cmd_config->camera, cmd_config->window, 
				       cmd_config->context);
	if (result < 0) {
		msg[0] = N_("<C></5>Error");
		msg[1] = "";
		msg[2] = N_("Could not set configuration:");
		msg[3] = (char*) gp_result_as_string (result);
		question = newCDKDialog (cmd_config->screen, CENTER, CENTER,
					 msg, 4, buttons, 2,
					 COLOR_PAIR (2) | A_REVERSE,
					 TRUE, TRUE, FALSE);
		if (!question)
			return (GP_ERROR);
		selection = activateCDKDialog (question, 0);
		if (question->exitType == vNORMAL) {
			switch (selection) {
			case 0: /* Continue */
				destroyCDKDialog (question);
				return (GP_OK);
			default:
				destroyCDKDialog (question);
				return (result);
			}
		} else {
			destroyCDKDialog (question);
			return (result);
		}
	}

	return (GP_OK);
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
	count++;
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

	/* Month in CDK starts with 1 */
	day = date_info->tm_mday;
	month = date_info->tm_mon + 1;
	year = date_info->tm_year + 1900;

	gp_widget_get_label (date, &label);
	snprintf (title, sizeof (title), "<C></5>%s", label);

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
		date_info = localtime ((time_t *) &time);

		/* Month in CDK starts with 1 */
		date_info->tm_mday = calendar->day;
		date_info->tm_mon = calendar->month - 1;
		date_info->tm_year = calendar->year - 1900;

		time = mktime (date_info);
		gp_widget_set_value (date, &time);
		set_config (cmd_config);
	}

	destroyCDKCalendar (calendar);
	return (GP_OK);
}

static int
time_preprocess (EObjectType cdktype, void *object, void *clientData,
		 chtype input)
{
	CDKENTRY *entry = object;

	/* Check a predefined binding... */
	if (checkCDKObjectBind (vENTRY, entry, input) != 0)
		return (1);

	/* Check ':' */
	if ((input == ':') || ((input >= '0') && (input <= '9')))
		return (1);

	/* Check other known keys */
	switch (input) {
	case KEY_UP:
	case KEY_DOWN:
	case CDK_BEGOFLINE:
	case CDK_TRANSPOSE:
	case CDK_ENDOFLINE:
	case KEY_LEFT:
	case CDK_BACKCHAR:
	case KEY_RIGHT:
	case CDK_FORCHAR:
	case DELETE:
	case ('H') & 0x1f:
	case KEY_BACKSPACE:
	case KEY_DC:
	case KEY_ESC:
	case CDK_ERASE:
	case CDK_CUT:
	case CDK_COPY:
	case CDK_PASTE:
	case KEY_RETURN:
	case KEY_TAB:
	case KEY_ENTER:
	case CDK_REFRESH:
		return (1);
	default:
		Beep ();
		return (0);
	}
}

static int
show_time (CmdConfig *cmd_config, CameraWidget *date)
{
	CDKENTRY *entry = NULL;
	const char *label, *info;
	char title[1024], time_string[9];
	int time;
	struct tm *date_info;

	gp_widget_get_label (date, &label);
	snprintf (title, sizeof (title), "<C></5>%s", label);

	entry = newCDKEntry (cmd_config->screen, CENTER, CENTER, title,
			     _("Time: "), A_NORMAL, ' ', vMIXED, 40, 0,
			     8, TRUE, FALSE);
	if (!entry)
		return (GP_ERROR);

	gp_widget_get_value (date, &time);
	date_info = localtime ((time_t *) &time);
	snprintf (time_string, sizeof (time_string), "%2i:%02i:%02i",
		  date_info->tm_hour, date_info->tm_min, date_info->tm_sec);
	setCDKEntryValue (entry, time_string);

	setCDKEntryPreProcess (entry, time_preprocess, NULL);

	info = activateCDKEntry (entry, 0);
	if (entry->exitType == vNORMAL) {
		date_info = localtime ((time_t *) &time);
		sscanf (info, "%d:%d:%d", &date_info->tm_hour,
			&date_info->tm_min, &date_info->tm_sec);
		time = mktime (date_info);
		gp_widget_set_value (date, &time);
		set_config (cmd_config);
	} 
	destroyCDKEntry (entry);
	return (GP_OK);
}

static int
show_radio (CmdConfig *cmd_config, CameraWidget *radio)
{
	CDKITEMLIST *list = NULL;
	const char *label, *value, *current_value;
	char title[1024], *items[100];
	int x, count, current = 0, selection, found;

	gp_widget_get_label (radio, &label);
	snprintf (title, sizeof (title), "<C></5>%s", label);
	gp_widget_get_value (radio, &current_value);
	count = gp_widget_count_choices (radio);

	/* Check if the current value is in the list */
	current = found = 0;
	for (x = 0; x < count; x++) {
		gp_widget_get_choice (radio, x, &value);
		if (!strcmp (value, current_value)) {
			current = x;
			found = 1;
			break;
		}
	}
	if (!found)
		items[0] = copyChar ((char *) current_value);

	/* Add all items */
	for (x = 0; x < count; x++) {
		gp_widget_get_choice (radio, x, &value);
		items[x + 1 - found] = copyChar ((char *) value);
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
show_text (CmdConfig *cmd_config, CameraWidget *text)
{
	CDKENTRY *entry = NULL;
	const char *label, *value;
	char title[1024], *info;

	CHECK (gp_widget_get_value (text, &value));
	CHECK (gp_widget_get_label (text, &label));

	snprintf (title, sizeof (title), "<C></5>%s", label);
	entry = newCDKEntry (cmd_config->screen, CENTER, CENTER, title,
			     _("Value: "), A_NORMAL, ' ', vMIXED, 40, 0,
			     256, TRUE, FALSE);
	if (!entry)
		return (GP_ERROR);

	setCDKEntryValue (entry, (char*) value);
	info = activateCDKEntry (entry, 0);
	if (entry->exitType == vNORMAL) {
		gp_widget_set_value (text, info);
		set_config (cmd_config);
	}

	destroyCDKEntry (entry);
	return (GP_OK);
}

static int
show_toggle (CmdConfig *cmd_config, CameraWidget *toggle)
{
	CDKITEMLIST *list = NULL;
	int value, selection;
	const char *label;
	char title[1024], *info[] = {N_("Yes"), N_("No")};

	CHECK (gp_widget_get_value (toggle, &value));
	CHECK (gp_widget_get_label (toggle, &label));
	snprintf (title, sizeof (title), "<C></5>%s", label);

	list = newCDKItemlist (cmd_config->screen, CENTER, CENTER, title, "",
			       info, 2, 1 - value, TRUE, FALSE);
	if (!list)
		return (GP_ERROR);

	selection = activateCDKItemlist (list, 0);
	if (list->exitType == vNORMAL) {
		selection = 1 - selection;
		gp_widget_set_value (toggle, &selection);
		set_config (cmd_config);
	}

	destroyCDKItemlist (list);

	return (GP_OK);
}

static int
show_range_int (CmdConfig *cmd_config, CameraWidget *range)
{
	CDKSLIDER *slider = NULL;
	float value, min, max, increment;
	const char *label;
	char title[1024];
	int selection;

	CHECK (gp_widget_get_value (range, &value));
	CHECK (gp_widget_get_label (range, &label));
	snprintf (title, sizeof (title), "<C></5>%s", label);
	CHECK (gp_widget_get_range (range, &min, &max, &increment));

	slider = newCDKSlider (cmd_config->screen, CENTER, CENTER, title,
			       _("Value: "), '-',
			       50, (int) value, min, max,
			       increment, 
			       MAX (increment, (max - min)/20.0),
			       TRUE,
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
get_digits (double d)
{
	/*
	 * Returns the number of non-zero digits to the right of the decimal
	 * point, up to a max of 4.
	 */
	double x;
	int i;
	for (i = 0, x = d * 1.0; i < 4;  i++) {

		/*
		 * The small number really depends on how many digits (4)
		 * we are checking for, but as long as it's small enough
		 * this works fine. We need the "<" as the floating point
		 * arithmetic does not always give an exact 0.0 (and can
		 * even give a -0.0).
		 */
		if (fabs (x - floor (x)) < 0.000001) 
			return(i);
		x = x * 10.0;
	}
	return(i);
}

static int
show_range_float (CmdConfig *cmd_config, CameraWidget *range)
{
#ifdef HAVE_CDK_20010421
	return (show_range_int (cmd_config, range));
#else
        CDKFSCALE *fscale = NULL;
        float value, min, max, increment;
        const char *label;
        char title[1024];
        float selection;

        CHECK (gp_widget_get_value (range, &value));
        CHECK (gp_widget_get_label (range, &label));
        snprintf (title, sizeof (title), "<C></5>%s", label);
        CHECK (gp_widget_get_range (range, &min, &max, &increment));

        fscale = newCDKFScale (cmd_config->screen, CENTER, CENTER, title,
                               _("Value: "), A_STANDOUT,
                               50, value, min, max,
                               increment, 
			       MAX (increment, (max - min) / 20.0),
			       get_digits (increment),
			       TRUE, FALSE);
        if (!fscale)
                return (GP_ERROR);

	selection = activateCDKFScale (fscale, 0);
        if (fscale->exitType == vNORMAL) {
		value = selection;
                gp_widget_set_value (range, &value);
                set_config (cmd_config);
        }

        destroyCDKFScale (fscale);
        return (GP_OK);
#endif
}

static int
show_range (CmdConfig *cmd_config, CameraWidget *range)
{
	float min, max, increment;

	CHECK (gp_widget_get_range (range, &min, &max, &increment));

	if (!get_digits (increment))
		return (show_range_int (cmd_config, range));
	else
		return (show_range_float (cmd_config, range));
}

static int
show_widget (CmdConfig *cmd_config, CameraWidget *widget)
{
	CameraWidget *parent;
	CameraWidgetType type;

	CHECK (gp_widget_get_type (widget, &type));
	switch (type) {
	case GP_WIDGET_WINDOW:
	case GP_WIDGET_SECTION:
		CHECK (show_section (cmd_config, widget));
		break;
	case GP_WIDGET_DATE:
		CHECK (show_date (cmd_config, widget));
		CHECK (show_time (cmd_config, widget));
		CHECK (gp_widget_get_parent (widget, &parent));
		CHECK (show_widget (cmd_config, parent));
		break;
	case GP_WIDGET_MENU:
	case GP_WIDGET_RADIO:
		CHECK (show_radio (cmd_config, widget));
		CHECK (gp_widget_get_parent (widget, &parent));
		CHECK (show_widget (cmd_config, parent));
		break;
	case GP_WIDGET_RANGE:
		CHECK (show_range (cmd_config, widget));
		CHECK (gp_widget_get_parent (widget, &parent));
		CHECK (show_widget (cmd_config, parent));
		break;
	case GP_WIDGET_TEXT:
		CHECK (show_text (cmd_config, widget));
		CHECK (gp_widget_get_parent (widget, &parent));
		CHECK (show_widget (cmd_config, parent));
		break;
	case GP_WIDGET_TOGGLE:
		CHECK (show_toggle (cmd_config, widget));
		CHECK (gp_widget_get_parent (widget, &parent));
		CHECK (show_widget (cmd_config, parent));
		break;
	default:
		return (GP_ERROR_NOT_SUPPORTED);
	}

	return (GP_OK);
}

int
gp_cmd_config (Camera *camera, GPContext *context)
{
	CmdConfig cmd_config;
	CameraWidget *config;
	int result;
	CDKSCREEN *screen = NULL;
	WINDOW *window = NULL;

	if (!camera)
		return (GP_ERROR_BAD_PARAMETERS);

	result = gp_camera_get_config (camera, &config, context);
	if (result < 0)
		return (result);

	/* Set up CDK. */
	window = initscr ();
	screen = initCDKScreen (window);

	/* Set up CDK Colors. */
	initCDKColor ();

	/* Go! */
	cmd_config.camera  = camera;
	cmd_config.screen  = screen;
	cmd_config.window  = config;
	cmd_config.context = context;
	result = show_widget (&cmd_config, config);

	/* Clean up */
	destroyCDKScreen (screen);
	delwin (window);
	endCDK ();

	return (result);
}
