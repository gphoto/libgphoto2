/** \file gphoto2-camera.c
 * \brief Implement Camera object representing a camera attached to the system.
 *
 * \author Copyright © 2000 Scott Fritzinger
 * \author 2001-2002: Lutz Müller <lutz@users.sf.net>
 *
 * \par
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
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"
#include "gphoto2-camera.h"

#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>

#include <ltdl.h>

#include "gphoto2-context.h"
#include "gphoto2-result.h"
#include "gphoto2-library.h"
#include "gphoto2-port-log.h"

#ifdef ENABLE_NLS
#  include <libintl.h>
#  undef _
#  define _(String) dgettext (GETTEXT_PACKAGE, String)
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

#define CHECK_NULL(r)              {if (!(r)) return (GP_ERROR_BAD_PARAMETERS);}

#define CAMERA_UNUSED(c,ctx)						\
{									\
	(c)->pc->used--;						\
	if (!(c)->pc->used) {						\
		if ((c)->pc->exit_requested)				\
			gp_camera_exit ((c), (ctx));			\
		if (!(c)->pc->ref_count)				\
			gp_camera_free (c);				\
	}								\
}

#define CR(c,result,ctx)						\
{									\
	int r = (result);						\
									\
	if (r < 0) {							\
									\
		/* libgphoto2_port doesn't have a GPContext */		\
		if (r > -100)						\
			gp_context_error ((ctx), _("An error occurred "	\
				"in the io-library ('%s'): %s"),	\
				gp_port_result_as_string (r),		\
				(c) ? gp_port_get_error ((c)->port) :	\
				      _("No additional information "	\
				      "available."));			\
		if (c)							\
			CAMERA_UNUSED((c),(ctx));			\
		return (r);						\
	}								\
}

/*
 * HAVE_MULTI
 * ----------
 *
 * The problem: Several different programs (gtkam, gphoto2, gimp) accessing
 * 		one camera.
 * The solutions:
 *  (1) gp_port_open before each operation, gp_port_close after. This has
 *      shown to not work with some drivers (digita/dc240) for serial ports,
 *      because the camera will notice that [1],
 *      reset itself and will therefore need to be reinitialized. If you want
 *      this behaviour, #define HAVE_MULTI.
 *  (2) Leave it up to the frontend to release the camera by calling
 *      gp_camera_exit after camera operations. This is what is implemented
 *      right now. The drawback is that re-initialization takes more time than
 *      just reopening the port. However, it works for all camera drivers.
 *
 * [1] Marr <marr@shianet.org> writes:
 *
 *     With the Digita-OS cameras at least, one of the RS-232 lines is tied
 *     to a 'Reset' signal on the camera.  I quote from the Digita 'Host
 *     Interface Specification' document:
 *
 *     "The Reset signal is a pulse on the Reset/Att line (which cooresponds 
 *     [sic] to pin 2 at the camera side) sent from the host computer to the 
 *     camera.  This pulse must be at least 50us."
 */

#ifdef HAVE_MULTI
#define CHECK_OPEN(c,ctx)						\
{									\
	int r;								\
									\
	if (strcmp ((c)->pc->a.model,"Directory Browse")) {		\
		r = gp_port_open ((c)->port);				\
		if (r < 0) {						\
			CAMERA_UNUSED (c,ctx);				\
			return (r);					\
		}							\
	}								\
	if ((c)->functions->pre_func) {					\
		r = (c)->functions->pre_func (c,ctx);			\
		if (r < 0) {						\
			CAMERA_UNUSED (c,ctx);				\
			return (r);					\
		}							\
	}								\
}
#else
#define CHECK_OPEN(c,ctx)						\
{                                                                       \
	int r;								\
									\
	if ((c)->functions->pre_func) {                                 \
		r = (c)->functions->pre_func (c,ctx);                   \
		if (r < 0) {                                            \
			CAMERA_UNUSED (c,ctx);				\
			return (r);                                     \
		}                                                       \
	}                                                               \
}
#endif

#ifdef HAVE_MULTI
#define CHECK_CLOSE(c,ctx)						\
{									\
	int r;								\
									\
	if (strcmp ((c)->pc->a.model,"Directory Browse"))		\
		gp_port_close ((c)->port);				\
	if ((c)->functions->post_func) {				\
		r = (c)->functions->post_func (c,ctx);			\
		if (r < 0) {						\
			CAMERA_UNUSED (c,ctx);				\
			return (r);					\
		}							\
	}								\
}
#else
#define CHECK_CLOSE(c,ctx)                                              \
{                                                                       \
	int r;								\
									\
	if ((c)->functions->post_func) {                                \
		r = (c)->functions->post_func (c,ctx);                  \
		if (r < 0) {                                            \
			CAMERA_UNUSED (c,ctx);				\
			return (r);                                     \
		}							\
	}                                                               \
}
#endif

#define CRS(c,res,ctx)							\
{									\
	int r = (res);							\
									\
	if (r < 0) {							\
		CAMERA_UNUSED (c,ctx);                              	\
		return (r);						\
	}								\
}

#define CRSL(c,res,ctx,list)						\
{									\
	int r = (res);							\
									\
	if (r < 0) {							\
		CAMERA_UNUSED (c,ctx);                              	\
		gp_list_free (list);					\
		return (r);						\
	}								\
}

#define CHECK_RESULT_OPEN_CLOSE(c,result,ctx)				\
{									\
	int r;								\
									\
	CHECK_OPEN (c,ctx);						\
	r = (result);							\
	if (r < 0) {							\
		CHECK_CLOSE (c,ctx);					\
		gp_log (GP_LOG_DEBUG, "gphoto2-camera", "Operation failed!");\
		CAMERA_UNUSED (c,ctx);                              	\
		return (r);						\
	}								\
	CHECK_CLOSE (c,ctx);						\
}

#define CHECK_INIT(c,ctx)						\
{									\
	if ((c)->pc->used)						\
		return (GP_ERROR_CAMERA_BUSY);				\
	(c)->pc->used++;						\
	if (!(c)->pc->lh)						\
		CR((c), gp_camera_init (c, ctx), ctx);			\
}

struct _CameraPrivateCore {

	/* Some information about the port */
	unsigned int speed;

	/* The abilities of this camera */
	CameraAbilities a;

	/* Library handle */
	lt_dlhandle lh;

	char error[2048];

	unsigned int ref_count;
	unsigned char used;
	unsigned char exit_requested;

	int initialized;

	/* Timeout functions */
	CameraTimeoutStartFunc timeout_start_func;
	CameraTimeoutStopFunc  timeout_stop_func;
	void                  *timeout_data;
	unsigned int          *timeout_ids;
	unsigned int           timeout_ids_len;
};


/**
 * \brief Close connection to camera.
 * \param camera a Camera object
 * \param context a GPContext object
 * \return a gphoto2 error code.
 *
 * Closes a connection to the camera and therefore gives other application
 * the possibility to access the camera, too.
 *
 * It is recommended that you 
 * call this function when you currently don't need the camera. The camera
 * will get reinitialized by gp_camera_init() automatically if you try to 
 * access the camera again.
 */
int
gp_camera_exit (Camera *camera, GPContext *context)
{
	CHECK_NULL (camera);

	gp_log (GP_LOG_DEBUG, "gphoto2-camera", "Exiting camera ('%s')...",
		camera->pc->a.model);

	/*
	 * We have to postpone this operation if the camera is currently 
	 * in use. gp_camera_exit will be called again if the
	 * camera->pc->used will drop to zero.
	 */
	if (camera->pc->used) {
		camera->pc->exit_requested = 1;
		return (GP_OK);
	}

	/* Remove every timeout that is still pending */
	while (camera->pc->timeout_ids_len)
		gp_camera_stop_timeout (camera, camera->pc->timeout_ids[0]);
	free (camera->pc->timeout_ids);
	camera->pc->timeout_ids = NULL;

	if (camera->functions->exit) {
#ifdef HAVE_MULTI
		gp_port_open (camera->port);
#endif
		camera->functions->exit (camera, context);
	}
	gp_port_close (camera->port);
	memset (camera->functions, 0, sizeof (CameraFunctions));

	if (camera->pc->lh) {
		lt_dlclose (camera->pc->lh);
		lt_dlexit ();
		camera->pc->lh = NULL;
	}

	gp_filesystem_reset (camera->fs);

	return (GP_OK);
}


/**
 * \brief Allocates the memory for a Camera.
 * \param camera The Camera:: object to initialize.
 * \return a gphoto2 error code
 */
int
gp_camera_new (Camera **camera)
{
	int result;

	CHECK_NULL (camera);

        *camera = malloc (sizeof (Camera));
	if (!*camera) 
		return (GP_ERROR_NO_MEMORY);
	memset (*camera, 0, sizeof (Camera));

        (*camera)->functions = malloc(sizeof(CameraFunctions));
	if (!(*camera)->functions) {
		gp_camera_free (*camera);
		return (GP_ERROR_NO_MEMORY);
	}
	memset ((*camera)->functions, 0, sizeof (CameraFunctions));

	(*camera)->pc = malloc (sizeof (CameraPrivateCore));
	if (!(*camera)->pc) {
		gp_camera_free (*camera);
		return (GP_ERROR_NO_MEMORY);
	}
	memset ((*camera)->pc, 0, sizeof (CameraPrivateCore));

        (*camera)->pc->ref_count = 1;

	/* Create the filesystem */
	result = gp_filesystem_new (&(*camera)->fs);
	if (result != GP_OK) {
		gp_camera_free (*camera);
		return (result);
	}

	/* Create the port */
	result = gp_port_new (&(*camera)->port);
	if (result < 0) {
		gp_camera_free (*camera);
		return (result);
	}

        return(GP_OK);
}


/**
 * \brief Sets the camera abilities. 
 * \param camera: a #Camera
 * \param abilities the #CameraAbilities to be set
 * \return a gphoto2 error code
 *
 * You need to call this function before calling gp_camera_init() the
 * first time unless you want gphoto2 to autodetect cameras and choose
 * the first detected one. By setting the \c abilities, you 
 * tell gphoto2 what model the \c camera is and what camera driver should 
 * be used for accessing the \c camera. You can get \c abilities by calling
 * gp_abilities_list_get_abilities().
 */
int
gp_camera_set_abilities (Camera *camera, CameraAbilities abilities)
{
	gp_log (GP_LOG_DEBUG, "gphoto2-camera", "Setting abilities ('%s')...",
		abilities.model);

	CHECK_NULL (camera);

	/*
	 * If the camera is currently initialized, terminate that connection.
	 * We don't care if we are successful or not.
	 */
	if (camera->pc->lh)
		gp_camera_exit (camera, NULL);

	memcpy (&camera->pc->a, &abilities, sizeof (CameraAbilities));

	return (GP_OK);
}


/**
 * \brief Retrieve the \c abilities of the \c camera. 
 * \param camera a #Camera
 * \param abilities
 * \return a gphoto2 error code
 */
int
gp_camera_get_abilities (Camera *camera, CameraAbilities *abilities)
{
	CHECK_NULL (camera && abilities);

	memcpy (abilities, &camera->pc->a, sizeof (CameraAbilities));

	return (GP_OK);
}


int
gp_camera_get_port_info (Camera *camera, GPPortInfo *info)
{
	CHECK_NULL (camera && info);

	CR (camera, gp_port_get_info (camera->port, info), NULL);

	return (GP_OK);
}


int
gp_camera_set_port_info (Camera *camera, GPPortInfo info)
{
	CHECK_NULL (camera);

	/*
	 * If the camera is currently initialized, terminate that connection.
	 * We don't care if we are successful or not.
	 */
	if (camera->pc->lh)
		gp_camera_exit (camera, NULL);

	gp_log (GP_LOG_DEBUG, "gphoto2-camera", "Setting port info for "
		"port '%s' at '%s'...", info.name, info.path);
	CR (camera, gp_port_set_info (camera->port, info), NULL);

	return (GP_OK);
}


/**
 * \brief Set the camera speed.
 * \param camera a Camera
 * \param speed the speed
 * \return a gphoto2 error code
 *
 * This function is typically used prior first initialization 
 * using #gp_camera_init for debugging purposes. Normally, a camera driver
 * will try to figure out the current speed of the camera and set the speed
 * to the optimal one automatically. Note that this function only works with 
 * serial ports. In other words, you have to set the camera's port to a 
 * serial one (using #gp_camera_set_port_path or #gp_camera_set_port_name)
 * prior calling this function.
 */
int
gp_camera_set_port_speed (Camera *camera, int speed)
{
	GPPortSettings settings;

	CHECK_NULL (camera);

	if (!camera->port) {
		gp_log (GP_LOG_ERROR, "camera", "You need to set "
			"a port prior trying to set the speed");
		return (GP_ERROR_BAD_PARAMETERS);
	}

	if (camera->port->type != GP_PORT_SERIAL) {
		gp_log (GP_LOG_ERROR, "camera", "You can specify "
			"a speed only with serial ports");
		return (GP_ERROR_BAD_PARAMETERS);
	}

	/*
	 * If the camera is currently initialized, terminate that connection.
	 * We don't care if we are successful or not.
	 */
	if (camera->pc->lh)
		gp_camera_exit (camera, NULL);

	CR (camera, gp_port_get_settings (camera->port, &settings), NULL);
	settings.serial.speed = speed;
	CR (camera, gp_port_set_settings (camera->port, settings), NULL);
	camera->pc->speed = speed;

	return (GP_OK);
}


/** 
 * \brief Retrieve the current speed.
 * \param camera a Camera
 * \return The current speed or a gphoto2 error code
 */
int
gp_camera_get_port_speed (Camera *camera)
{
	CHECK_NULL (camera);

	return (camera->pc->speed);
}


/**
 * \brief Increment the reference count of a @camera.
 * \param camera a Camera
 * \return a gphoto2 error code
 */
int
gp_camera_ref (Camera *camera)
{
	CHECK_NULL (camera);

	camera->pc->ref_count += 1;

	return (GP_OK);
}


/**
 * \brief Decrements the reference count of a Camera.
 * \param camera a #Camera
 * \return a gphoto2 error code
 *
 * If the reference count reaches %0, the \c camera will be freed 
 * automatically.
 */
int
gp_camera_unref (Camera *camera)
{
	CHECK_NULL (camera);

	if (!camera->pc->ref_count) {
		gp_log (GP_LOG_ERROR, "gphoto2-camera", "gp_camera_unref on "
			"a camera with ref_count == 0 should not happen "
			"at all");
		return (GP_ERROR);
	}

	camera->pc->ref_count -= 1;

	if (!camera->pc->ref_count) {

		/* We cannot free a camera that is currently in use */
		if (!camera->pc->used)
			gp_camera_free (camera);
	}

	return (GP_OK);
}


/**
 * \brief Free the \c camera.
 * \param camera a Camera
 * \return a gphoto2 error code
 *
 * \deprecated 
 * This function should never be used. Please use gp_camera_unref() instead.
 */
int
gp_camera_free (Camera *camera)
{
	CHECK_NULL (camera);

	gp_log (GP_LOG_DEBUG, "gp-camera", "Freeing camera...");

	/*
	 * If the camera is currently initialized, close the connection.
	 * We don't care if we are successful or not.
	 */
	if (camera->port && camera->pc && camera->pc->lh)
		gp_camera_exit (camera, NULL);

	/* We don't care if anything goes wrong */
	if (camera->port) {
		gp_port_free (camera->port);
		camera->port = NULL;
	}

	if (camera->pc) {
		if (camera->pc->timeout_ids)
			free (camera->pc->timeout_ids);
		free (camera->pc);
		camera->pc = NULL;
	}

	if (camera->fs) {
		gp_filesystem_free (camera->fs);
		camera->fs = NULL;
	}

        if (camera->functions) {
                free (camera->functions);
		camera->functions = NULL;
	}
 
	free (camera);

	return (GP_OK);
}


/**
 * \brief Initiate a connection to the \c camera. 
 * \param camera a camera
 * \param context a GPContext
 * \return a gphoto2 error code
 *
 * Before calling this function, the
 * @camera should be set up using #gp_camera_set_port_path or
 * #gp_camera_set_port_name and #gp_camera_set_abilities. If that has been
 * omitted, gphoto2 tries to autodetect any cameras and chooses the first one
 * if any cameras are found. It is generally a good idea to call
 * #gp_camera_exit after transactions have been completed in order to give
 * other applications the chance to access the camera, too.
 */
int
gp_camera_init (Camera *camera, GPContext *context)
{
	CameraAbilities a;
	const char *model, *port;
	CameraLibraryInitFunc init_func;
	int result;

	gp_log (GP_LOG_DEBUG, "gphoto2-camera", "Initializing camera...");

	CHECK_NULL (camera);
	/*
	 * Reset the exit_requested flag. If this flag is set, 
	 * gp_camera_exit will be called as soon as the camera is no
	 * longer in use (used flag).
	 */
	camera->pc->exit_requested = 0;

	/*
	 * If the model hasn't been indicated, try to
	 * figure it out (USB only). Beware of "Directory Browse".
	 */
	if (strcasecmp (camera->pc->a.model, "Directory Browse") &&
	    !strcmp ("", camera->pc->a.model)) {
		CameraAbilitiesList *al;
		GPPortInfoList *il;
		int m, p;
		GPPortInfo info;
        	CameraList *list;

		result = gp_list_new (&list);
		if (result < GP_OK)
			return result;

		gp_log (GP_LOG_DEBUG, "gphoto2-camera", "Neither "
			"port nor model set. Trying auto-detection...");

		/* Call auto-detect and choose the first camera */
		gp_abilities_list_new (&al);
		gp_abilities_list_load (al, context);
		gp_port_info_list_new (&il);
		gp_port_info_list_load (il);
		gp_abilities_list_detect (al, il, list, context);
		if (!gp_list_count (list)) {
			gp_abilities_list_free (al);
			gp_port_info_list_free (il);
			gp_context_error (context, _("Could not detect "
					     "any camera"));
			gp_list_free (list);
			return (GP_ERROR_MODEL_NOT_FOUND);
		}

		gp_list_get_name  (list, 0, &model);
		m = gp_abilities_list_lookup_model (al, model);
		gp_abilities_list_get_abilities (al, m, &a);
		gp_abilities_list_free (al);
		CRSL (camera, gp_camera_set_abilities (camera, a), context, list);
		CRSL (camera, gp_list_get_value (list, 0, &port), context, list);
		p = gp_port_info_list_lookup_path (il, port);
		gp_port_info_list_get_info (il, p, &info);
		gp_port_info_list_free (il);
		CRSL (camera, gp_camera_set_port_info (camera, info), context, list);
		gp_list_free (list);
	}

	if (strcasecmp (camera->pc->a.model, "Directory Browse")) {
		switch (camera->port->type) {
		case GP_PORT_NONE:
			gp_context_error (context, _("You have to set the "
				"port prior initialization of the camera."));
			return (GP_ERROR_UNKNOWN_PORT);
		case GP_PORT_USB:
			if (gp_port_usb_find_device (camera->port,
					camera->pc->a.usb_vendor,
					camera->pc->a.usb_product) != GP_OK) {
				CRS (camera, gp_port_usb_find_device_by_class
					(camera->port,
					camera->pc->a.usb_class,
					camera->pc->a.usb_subclass,
					camera->pc->a.usb_protocol), context);
					}
			break;
		default:
			break;
		}
	}

	/* Load the library. */
	gp_log (GP_LOG_DEBUG, "gphoto2-camera", "Loading '%s'...",
		camera->pc->a.library);
	lt_dlinit ();
	camera->pc->lh = lt_dlopenext (camera->pc->a.library);
	if (!camera->pc->lh) {
		gp_context_error (context, _("Could not load required "
			"camera driver '%s' (%s)."), camera->pc->a.library,
			lt_dlerror ());
		lt_dlexit ();
		return (GP_ERROR_LIBRARY);
	}

	/* Initialize the camera */
	init_func = lt_dlsym (camera->pc->lh, "camera_init");
	if (!init_func) {
		lt_dlclose (camera->pc->lh);
		lt_dlexit ();
		camera->pc->lh = NULL;
		gp_context_error (context, _("Camera driver '%s' is "
			"missing the 'camera_init' function."), 
			camera->pc->a.library);
		return (GP_ERROR_LIBRARY);
	}

	if (strcasecmp (camera->pc->a.model, "Directory Browse")) {
		result = gp_port_open (camera->port);
		if (result < 0) {
			lt_dlclose (camera->pc->lh);
			lt_dlexit ();
			camera->pc->lh = NULL;
			return (result);
		}
	}

	result = init_func (camera, context);
	if (result < 0) {
		gp_port_close (camera->port);
		lt_dlclose (camera->pc->lh);
		lt_dlexit ();
		camera->pc->lh = NULL;
		memset (camera->functions, 0, sizeof (CameraFunctions));
		return (result);
	}

	/* We don't care if that goes wrong */
#ifdef HAVE_MULTI
	gp_port_close (camera->port);
#endif

	return (GP_OK);
}


/**
 * \brief Retrieve a configuration \c window for the \c camera.
 * \param camera a Camera
 * \param window a CameraWidget
 * \param context a GPContext
 * \return gphoto2 error code
 *
 * This \c window can be used for construction of a configuration dialog.
 */
int
gp_camera_get_config (Camera *camera, CameraWidget **window, GPContext *context)
{
	CHECK_NULL (camera);
	CHECK_INIT (camera, context);

	if (!camera->functions->get_config) {
		gp_context_error (context, _("This camera does "
			"not offer any configuration options."));
		CAMERA_UNUSED (camera, context);
                return (GP_ERROR_NOT_SUPPORTED);
	}

	CHECK_RESULT_OPEN_CLOSE (camera, camera->functions->get_config (
					camera, window, context), context);

	CAMERA_UNUSED (camera, context);
	return (GP_OK);
}


/**
 * gp_camera_set_config:
 * @camera: a #Camera
 * @window: a #CameraWidget
 * @context: a #GPContext
 *
 * Sets the configuration. Typically, a @window is retrieved using
 * #gp_camera_get_config and passed to this function in order to adjust
 * the settings on the camera.
 *
 * \return a gphoto2 error code
 **/
int
gp_camera_set_config (Camera *camera, CameraWidget *window, GPContext *context)
{
	CHECK_NULL (camera && window);
	CHECK_INIT (camera, context);

	if (!camera->functions->set_config) {
		gp_context_error (context, _("This camera does "
			"not support setting configuration options."));
		CAMERA_UNUSED (camera, context);
                return (GP_ERROR_NOT_SUPPORTED);
	}

	CHECK_RESULT_OPEN_CLOSE (camera, camera->functions->set_config (camera,
						window, context), context);

	CAMERA_UNUSED (camera, context);
	return (GP_OK);
}

/**
 * gp_camera_get_summary:
 * @camera: a #Camera
 * @summary: a #CameraText
 * @context: a #GPContext
 *
 * Retrieves a camera @summary. This summary typically contains information
 * like manufacturer, pictures taken, or generally information that is
 * not configurable.
 *
 * \return a gphoto2 error code
 **/
int
gp_camera_get_summary (Camera *camera, CameraText *summary, GPContext *context)
{
	CHECK_NULL (camera && summary);
	CHECK_INIT (camera, context);

	if (!camera->functions->summary) {
		gp_context_error (context, _("This camera does "
				  "not support summaries."));
		CAMERA_UNUSED (camera, context);
                return (GP_ERROR_NOT_SUPPORTED);
	}

	CHECK_RESULT_OPEN_CLOSE (camera, camera->functions->summary (camera,
						summary, context), context);

	CAMERA_UNUSED (camera, context);
	return (GP_OK);
}

/**
 * gp_camera_get_manual:
 * @camera: a #Camera
 * @manual: a #CameraText
 * @context: a #GPContext
 *
 * Retrieves the @manual for given @camera. This manual typically contains
 * information about using the camera.
 *
 * \return a gphoto2 error code
 **/
int
gp_camera_get_manual (Camera *camera, CameraText *manual, GPContext *context)
{
	CHECK_NULL (camera && manual);
	CHECK_INIT (camera, context);

	if (!camera->functions->manual) {
		gp_context_error (context, _("This camera "
			"does not offer a manual."));
		CAMERA_UNUSED (camera, context);
                return (GP_ERROR_NOT_SUPPORTED);
	}

	CHECK_RESULT_OPEN_CLOSE (camera, camera->functions->manual (camera,
						manual, context), context);

	CAMERA_UNUSED (camera, context);
	return (GP_OK);
}

/**
 * gp_camera_get_about:
 * @camera: a #Camera
 * @about: a #CameraText
 * @context: a #GPContext
 *
 * Retrieves information about the camera driver. Typically, this information
 * contains name and address of the author, acknowledgements, etc.
 *
 * \return a gphoto2 error code
 **/
int
gp_camera_get_about (Camera *camera, CameraText *about, GPContext *context)
{
	CHECK_NULL (camera && about);
	CHECK_INIT (camera, context);

	if (!camera->functions->about) {
		gp_context_error (context, _("This camera does "
			"not provide information about the driver."));
		CAMERA_UNUSED (camera, context);
                return (GP_ERROR_NOT_SUPPORTED);
	}

	CHECK_RESULT_OPEN_CLOSE (camera, camera->functions->about (camera,
						about, context), context);

	CAMERA_UNUSED (camera, context);
	return (GP_OK);
}

/**
 * gp_camera_capture:
 * @camera: a #Camera
 * @type: a #CameraCaptureType
 * @path: a #CameraFilePath
 * @context: a #GPContext
 *
 * Captures an image, movie, or sound clip depending on the given @type.
 * The resulting file will be stored on the camera. The location gets stored
 * in @path. The file can then be downloaded using #gp_camera_file_get.
 *
 * \return a gphoto2 error code
 **/
int
gp_camera_capture (Camera *camera, CameraCaptureType type,
		   CameraFilePath *path, GPContext *context)
{
	CHECK_NULL (camera);
	CHECK_INIT (camera, context);

	if (!camera->functions->capture) {
		gp_context_error (context, _("This camera can not capture."));
		CAMERA_UNUSED (camera, context);
                return (GP_ERROR_NOT_SUPPORTED);
	}

	CHECK_RESULT_OPEN_CLOSE (camera, camera->functions->capture (camera,
						type, path, context), context);

	CAMERA_UNUSED (camera, context);
	return (GP_OK);
}

/**
 * gp_camera_capture_preview:
 * \param camera: a #Camera
 * \param file: a #CameraFile
 * \param context: a #GPContext
 *
 * Captures a preview that won't be stored on the camera but returned in 
 * supplied @file. For example, you could use #gp_capture_preview for 
 * taking some sample pictures before calling #gp_capture.
 *
 * \return a gphoto2 error code
 **/
int
gp_camera_capture_preview (Camera *camera, CameraFile *file, GPContext *context)
{
	CHECK_NULL (camera && file);
	CHECK_INIT (camera, context);

	CR (camera, gp_file_clean (file), context);

	if (!camera->functions->capture_preview) {
		gp_context_error (context, _("This camera can "
			"not capture previews."));
		CAMERA_UNUSED (camera, context);
                return (GP_ERROR_NOT_SUPPORTED);
	}

	CHECK_RESULT_OPEN_CLOSE (camera, camera->functions->capture_preview (
					camera, file, context), context);

	CAMERA_UNUSED (camera, context);
	return (GP_OK);
}

/**
 * gp_camera_folder_list_files:
 * \param camera: a #Camera
 * \param folder: a folder
 * \param list: a #CameraList
 * \param context: a #GPContext
 *
 * Lists the files in supplied @folder.
 *
 * \return a gphoto2 error code
 **/
int
gp_camera_folder_list_files (Camera *camera, const char *folder, 
			     CameraList *list, GPContext *context)
{
	gp_log (GP_LOG_DEBUG, "gphoto2-camera", "Listing files in '%s'...",
		folder);

	CHECK_NULL (camera && folder && list);
	CHECK_INIT (camera, context);
	CR (camera, gp_list_reset (list), context);

	CHECK_RESULT_OPEN_CLOSE (camera, gp_filesystem_list_files (camera->fs,
					folder, list, context), context);

	CR (camera, gp_list_sort (list), context);
	CAMERA_UNUSED (camera, context);
        return (GP_OK);
}

/**
 * gp_camera_folder_list_folders:
 * \param camera: a #Camera
 * \param folder: a folder
 * \param list: a #CameraList
 * \param context: a #GPContext
 *
 * Lists the folders in supplied @folder.
 *
 * \return a gphoto2 error code
 **/
int
gp_camera_folder_list_folders (Camera *camera, const char* folder, 
			       CameraList *list, GPContext *context)
{
	gp_log (GP_LOG_DEBUG, "gphoto2-camera", "Listing folders in '%s'...",
		folder);

	CHECK_NULL (camera && folder && list);
	CHECK_INIT (camera, context);
	CR (camera, gp_list_reset (list), context);

	CHECK_RESULT_OPEN_CLOSE (camera, gp_filesystem_list_folders (
				camera->fs, folder, list, context), context);

	CR (camera, gp_list_sort (list), context);
	CAMERA_UNUSED (camera, context);
        return (GP_OK);
}

/**
 * gp_camera_folder_delete_all:
 * \param camera: a #Camera
 * \param folder: a folder
 * \param context: a #GPContext
 *
 * Deletes all files in a given @folder.
 *
 * \return a gphoto2 error code
 **/
int
gp_camera_folder_delete_all (Camera *camera, const char *folder,
			     GPContext *context)
{
	gp_log (GP_LOG_DEBUG, "gphoto2-camera", "Deleting all files in "
		"'%s'...", folder);

	CHECK_NULL (camera && folder);
	CHECK_INIT (camera, context);

	CHECK_RESULT_OPEN_CLOSE (camera, gp_filesystem_delete_all (camera->fs,
						folder, context), context);

	CAMERA_UNUSED (camera, context);
	return (GP_OK);
}

/**
 * gp_camera_folder_put_file:
 * \param camera: a #Camera
 * \param folder: a folder
 * \param file: a #CameraFile
 * \param context: a #GPContext
 *
 * Uploads a file into given @folder.
 *
 * \return a gphoto2 error code
 **/
int
gp_camera_folder_put_file (Camera *camera, const char *folder,
			   CameraFile *file, GPContext *context)
{
	gp_log (GP_LOG_DEBUG, "gphoto2-camera", "Uploading file into '%s'...",
		folder);

	CHECK_NULL (camera && folder && file);
	CHECK_INIT (camera, context);

	CHECK_RESULT_OPEN_CLOSE (camera, gp_filesystem_put_file (camera->fs,
					folder, file, context), context);

	CAMERA_UNUSED (camera, context);
	return (GP_OK);
}

/**
 * gp_camera_file_get_info:
 * \param camera: a #Camera
 * \param folder: a folder
 * \param file: the name of the file
 * \param info:
 * \param context: a #GPContext
 *
 * Retrieves information about a @file.
 *
 * \return a gphoto2 error code
 **/
int
gp_camera_file_get_info (Camera *camera, const char *folder, 
			 const char *file, CameraFileInfo *info,
			 GPContext *context)
{
	int result = GP_OK;
	const char *mime_type;
	const char *data;
	long int size;
	CameraFile *cfile;

	gp_log (GP_LOG_DEBUG, "gphoto2-camera", "Getting file info for '%s' "
		"in '%s'...", file, folder);

	CHECK_NULL (camera && folder && file && info);
	CHECK_INIT (camera, context);

	memset (info, 0, sizeof (CameraFileInfo));

	/* Check first if the camera driver supports the filesystem */
	CHECK_OPEN (camera, context);
	result = gp_filesystem_get_info (camera->fs, folder, file, info,
					 context);
	CHECK_CLOSE (camera, context);
	if (result != GP_ERROR_NOT_SUPPORTED) {
		CAMERA_UNUSED (camera, context);
		return (result);
	}

	/*
	 * The CameraFilesystem doesn't support file info. We simply get
	 * the preview and the file and look for ourselves...
	 */

	/* It takes too long to get the file */
	info->file.fields = GP_FILE_INFO_NONE;

	/* Get the preview */
	info->preview.fields = GP_FILE_INFO_NONE;
	CRS (camera, gp_file_new (&cfile), context);
	if (gp_camera_file_get (camera, folder, file, GP_FILE_TYPE_PREVIEW,
						cfile, context) == GP_OK) {
		info->preview.fields |= GP_FILE_INFO_SIZE | GP_FILE_INFO_TYPE;
		gp_file_get_data_and_size (cfile, &data, &size);
		info->preview.size = size;
		gp_file_get_mime_type (cfile, &mime_type);
		strncpy (info->preview.type, mime_type,
			 sizeof (info->preview.type));
	}
	gp_file_unref (cfile);

	/* We don't trust the camera libraries */
	info->file.fields |= GP_FILE_INFO_NAME;
	strncpy (info->file.name, file, sizeof (info->file.name));
	info->preview.fields &= ~GP_FILE_INFO_NAME;

	CAMERA_UNUSED (camera, context);
	return (GP_OK);
}

/**
 * gp_camera_file_set_info:
 * \param camera: a #Camera
 * \param folder: a folder
 * \param file: the name of a file
 * \param info: the #CameraFileInfo
 * \param context: a #GPContext
 *
 * Sets some file properties like name or permissions.
 *
 * \return a gphoto2 error code
 **/
int
gp_camera_file_set_info (Camera *camera, const char *folder, 
			 const char *file, CameraFileInfo info,
			 GPContext *context)
{
	CHECK_NULL (camera && folder && file);
	CHECK_INIT (camera, context);

	CHECK_RESULT_OPEN_CLOSE (camera, gp_filesystem_set_info (camera->fs,
					folder, file, info, context), context);

	CAMERA_UNUSED (camera, context);
	return (GP_OK);
}

/**
 * gp_camera_file_get:
 * \param camera: a #Camera
 * \param folder: a folder
 * \param file: the name of a file
 * \param type: the #CameraFileType
 * \param camera_file: a #CameraFile
 * \param context: a #GPContext
 *
 * Retrieves a @file from the @camera.
 *
 * \return a gphoto2 error code
 **/
int 
gp_camera_file_get (Camera *camera, const char *folder, const char *file,
		    CameraFileType type, CameraFile *camera_file,
		    GPContext *context)
{
	gp_log (GP_LOG_DEBUG, "gphoto2-camera", "Getting file '%s' in "
		"folder '%s'...", file, folder);

	CHECK_NULL (camera && folder && file && camera_file);
	CHECK_INIT (camera, context);

	CR (camera, gp_file_clean (camera_file), context);

	/* Did we get reasonable foldername/filename? */
	if (strlen (folder) == 0) {
		CAMERA_UNUSED (camera, context);
		return (GP_ERROR_DIRECTORY_NOT_FOUND);
	}
	if (strlen (file) == 0) {
		CAMERA_UNUSED (camera, context);
		return (GP_ERROR_FILE_NOT_FOUND);
	}
  
	CHECK_RESULT_OPEN_CLOSE (camera, gp_filesystem_get_file (camera->fs,
			folder, file, type, camera_file, context), context);

	CAMERA_UNUSED (camera, context);
	return (GP_OK);
}

/**
 * gp_camera_file_delete:
 * \param camera: a #Camera
 * \param folder: a folder
 * \param file: the name of a file
 * \param context: a #GPContext
 *
 * Deletes the @file from a @folder.
 *
 * \return a gphoto2 error code
 **/
int
gp_camera_file_delete (Camera *camera, const char *folder, const char *file,
		       GPContext *context)
{
	gp_log (GP_LOG_DEBUG, "gphoto2-camera", "Deleting file '%s' in "
		"folder '%s'...", file, folder);

	CHECK_NULL (camera && folder && file);
	CHECK_INIT (camera, context);

	CHECK_RESULT_OPEN_CLOSE (camera, gp_filesystem_delete_file (
				camera->fs, folder, file, context), context);

	CAMERA_UNUSED (camera, context);
	return (GP_OK);
}

/**
 * gp_camera_folder_make_dir:
 * \param camera: a #Camera
 * \param folder: the location where to create the new directory
 * \param name: the name of the directory to be created
 * \param context: a #GPContext
 *
 * Creates a new directory called @name in given @folder.
 *
 * \return a gphoto2 error code
 **/
int
gp_camera_folder_make_dir (Camera *camera, const char *folder,
			   const char *name, GPContext *context)
{
	CHECK_NULL (camera && folder && name);
	CHECK_INIT (camera, context);

	CHECK_RESULT_OPEN_CLOSE (camera, gp_filesystem_make_dir (camera->fs,
					folder, name, context), context);

	CAMERA_UNUSED (camera, context);
	return (GP_OK);
}

/**
 * gp_camera_folder_remove_dir:
 * \param camera: a #Camera
 * \param folder: the folder from which to remove the directory
 * \param name: the name of the directory to be removed
 * \param context: a #GPContext
 *
 * Removes an (empty) directory called @name from the given @folder.
 *
 * \return a gphoto2 error code
 */
int
gp_camera_folder_remove_dir (Camera *camera, const char *folder,
			     const char *name, GPContext *context)
{
	CHECK_NULL (camera && folder && name);
	CHECK_INIT (camera, context);

	CHECK_RESULT_OPEN_CLOSE (camera, gp_filesystem_remove_dir (camera->fs,
					folder, name, context), context);

	CAMERA_UNUSED (camera, context);
	return (GP_OK);
}

/**
 * gp_camera_set_timeout_funcs:
 * \param camera a Camera
 * \param start_func
 * \param stop_func
 * \param data
 *
 * If your frontend has something like idle loops, it is recommended you
 * use #gp_camera_set_timeout_funcs in order to give the camera driver the
 * possibility to keep up the connection to the camera.
 *
 * \return a gphoto2 error code
 */
void
gp_camera_set_timeout_funcs (Camera *camera, CameraTimeoutStartFunc start_func,
			     CameraTimeoutStopFunc stop_func,
			     void *data)
{
	if (!camera || !camera->pc)
		return;

	camera->pc->timeout_start_func = start_func;
	camera->pc->timeout_stop_func = stop_func;
	camera->pc->timeout_data = data;
}


/**
 * gp_camera_start_timeout:
 * \param camera a #Camera
 * \param timeout number of seconds that should pass between each call to
 * 	     \c func
 * \param func the function that should be called each \c timeout seconds
 * \return The id of the background process or a gphoto2 error code
 *
 * This function should be called by the camera driver during camera_init()
 * if the camera needs to be sent messages periodically in order to prevent
 * it from shutting down.
 */
int
gp_camera_start_timeout (Camera *camera, unsigned int timeout,
			 CameraTimeoutFunc func)
{
	int id;
	unsigned int *ids;

	if (!camera || !camera->pc)
		return (GP_ERROR_BAD_PARAMETERS);

	if (!camera->pc->timeout_start_func)
		return (GP_ERROR_NOT_SUPPORTED);

	/*
	 * We remember the id here in order to automatically remove
	 * the timeout on gp_camera_exit.
	 */
	ids = realloc (camera->pc->timeout_ids, sizeof (int) *
					(camera->pc->timeout_ids_len + 1));
	if (!ids)
		return (GP_ERROR_NO_MEMORY);
	camera->pc->timeout_ids = ids;

	id = camera->pc->timeout_start_func (camera, timeout, func,
					     camera->pc->timeout_data);
	if (id < 0)
		return (id);
	camera->pc->timeout_ids[camera->pc->timeout_ids_len] = id;
	camera->pc->timeout_ids_len++;

	return (id);
}


/**
 * \brief Stop periodic calls to keepalive function.
 * \param camera a #Camera
 * \param id the id of the background process previously returned by 
 * 	gp_camera_start_timeout()
 *
 * Call this function in the camera driver if you want to stop a periodic
 * call to a function that has been started using gp_camera_start_timeout().
 */
void
gp_camera_stop_timeout (Camera *camera, unsigned int id)
{
	unsigned int i;

	if (!camera || !camera->pc)
		return;

	if (!camera->pc->timeout_stop_func)
		return;

	/* Check if we know this id. If yes, remove it. */
	for (i = 0; i < camera->pc->timeout_ids_len; i++)
		if (camera->pc->timeout_ids[i] == id)
			break;
	if (i == camera->pc->timeout_ids_len)
		return;
	memmove (camera->pc->timeout_ids + i, camera->pc->timeout_ids + i + 1,
		 sizeof (int) * (camera->pc->timeout_ids_len - i - 1));
	camera->pc->timeout_ids_len--;
	camera->pc->timeout_ids = realloc (camera->pc->timeout_ids,
				sizeof (int) * camera->pc->timeout_ids_len);

	camera->pc->timeout_stop_func (camera, id, camera->pc->timeout_data);
}
