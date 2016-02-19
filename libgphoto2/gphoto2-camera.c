/** \file
 *
 * Implement Camera object representing a camera attached to the system.
 *
 * \author Copyright 2000 Scott Fritzinger
 * \author Copyright 2001-2002 Lutz Mueller <lutz@users.sf.net>
 *
 * \note
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * \note
 * This library is distributed in the hope that it will be useful, 
 * but WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details. 
 *
 * \note
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

#include "config.h"
#include <gphoto2/gphoto2-camera.h>

#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>

#include <ltdl.h>

#include <gphoto2/gphoto2-result.h>
#include <gphoto2/gphoto2-library.h>
#include <gphoto2/gphoto2-port-log.h>

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
	int r1 = (result);						\
									\
	if (r1 < 0) {							\
		/* libgphoto2_port doesn't have a GPContext */		\
		gp_context_error ((ctx), _("An error occurred "		\
			"in the io-library ('%s'): %s"),		\
			gp_port_result_as_string (r1),			\
			gp_port_get_error ((c) ? (c)->port : NULL));	\
		if (c)							\
			CAMERA_UNUSED (c,ctx);				\
		return (r1);						\
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
	int r2;								\
									\
	if (strcmp ((c)->pc->a.model,"Directory Browse")) {		\
		r2 = gp_port_open ((c)->port);				\
		if (r2 < 0) {						\
			CAMERA_UNUSED (c,ctx);				\
			return (r2);					\
		}							\
	}								\
	if ((c)->functions->pre_func) {					\
		r2 = (c)->functions->pre_func (c,ctx);			\
		if (r2 < 0) {						\
			CAMERA_UNUSED (c,ctx);				\
			return (r2);					\
		}							\
	}								\
}
#else
#define CHECK_OPEN(c,ctx)						\
{                                                                       \
	if ((c)->functions->pre_func) {					\
		int r2 = (c)->functions->pre_func (c,ctx);		\
		if (r2 < 0) {                                           \
			CAMERA_UNUSED (c,ctx);				\
			return (r2);                                    \
		}                                                       \
	}                                                               \
}
#endif

#ifdef HAVE_MULTI
#define CHECK_CLOSE(c,ctx)					\
{								\
	if (strcmp ((c)->pc->a.model,"Directory Browse"))	\
		gp_port_close ((c)->port);			\
	if ((c)->functions->post_func) {			\
		int r3 = (c)->functions->post_func (c,ctx);	\
		if (r3 < 0) {					\
			CAMERA_UNUSED (c,ctx);			\
			return (r3);				\
		}						\
	}							\
}
#else
#define CHECK_CLOSE(c,ctx)					\
{								\
	if ((c)->functions->post_func) {			\
		int r3 = (c)->functions->post_func (c,ctx);	\
		if (r3 < 0) {					\
			CAMERA_UNUSED (c,ctx);			\
			return (r3);				\
		}						\
	}							\
}
#endif

#define CRS(c,res,ctx)							\
{									\
	int r4 = (res);							\
									\
	if (r4 < 0) {							\
		CAMERA_UNUSED (c,ctx);                              	\
		return (r4);						\
	}								\
}

#define CRSL(c,res,ctx,list)						\
{									\
	int r5 = (res);							\
									\
	if (r5 < 0) {							\
		CAMERA_UNUSED (c,ctx);                              	\
		gp_list_free (list);					\
		return (r5);						\
	}								\
}

#define CHECK_RESULT_OPEN_CLOSE(c,result,ctx)				\
{									\
	int r6;								\
									\
	CHECK_OPEN (c,ctx);						\
	r6 = (result);							\
	if (r6 < 0) {							\
		GP_LOG_E ("'%s' failed: %d", #result, r6);		\
		CHECK_CLOSE (c,ctx);					\
		CAMERA_UNUSED (c,ctx);                              	\
		return (r6);						\
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
 * Close connection to camera.
 *
 * @param camera a #Camera object
 * @param context a #GPContext object
 * @return a gphoto2 error code.
 *
 * Closes a connection to the camera and therefore gives other application
 * the possibility to access the camera, too.
 *
 * It is recommended that you 
 * call this function when you currently don't need the camera. The camera
 * will get reinitialized by gp_camera_init() automatically if you try to 
 * access the camera again.
 *
 */
int
gp_camera_exit (Camera *camera, GPContext *context)
{
	C_PARAMS (camera);

	GP_LOG_D ("Exiting camera ('%s')...", camera->pc->a.model);

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
#if !defined(VALGRIND)
		lt_dlclose (camera->pc->lh);
		lt_dlexit ();
#endif
		camera->pc->lh = NULL;
	}

	gp_filesystem_reset (camera->fs);

	return (GP_OK);
}


/**
 * Allocates the memory for a #Camera.
 *
 * @param camera the #Camera object to initialize.
 * @return a gphoto2 error code
 *
 */
int
gp_camera_new (Camera **camera)
{
	int result;

	C_PARAMS (camera);

        C_MEM (*camera = calloc (1, sizeof (Camera)));

        (*camera)->functions = calloc (1, sizeof (CameraFunctions));
        (*camera)->pc        = calloc (1, sizeof (CameraPrivateCore));
	if (!(*camera)->functions || !(*camera)->pc) {
		result = GP_ERROR_NO_MEMORY;
		goto error;
	}

        (*camera)->pc->ref_count = 1;

	/* Create the filesystem */
	result = gp_filesystem_new (&(*camera)->fs);
	if (result < GP_OK)
		goto error;

	/* Create the port */
	result = gp_port_new (&(*camera)->port);
	if (result < GP_OK)
		goto error;

        return(GP_OK);

error:
	gp_camera_free (*camera);
	return result;
}


/**
 * \brief Sets the camera abilities. 
 *
 * @param camera a #Camera
 * @param abilities the #CameraAbilities to be set
 * @return a gphoto2 error code
 *
 * You need to call this function before calling #gp_camera_init the
 * first time unless you want gphoto2 to autodetect cameras and choose
 * the first detected one. By setting the \c abilities, you 
 * tell gphoto2 what model the \c camera is and what camera driver should 
 * be used for accessing the \c camera. You can get \c abilities by calling
 * #gp_abilities_list_get_abilities.
 *
 */
int
gp_camera_set_abilities (Camera *camera, CameraAbilities abilities)
{
	GP_LOG_D ("Setting abilities ('%s')...", abilities.model);

	C_PARAMS (camera);

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
 * Retrieve the \c abilities of the \c camera. 
 *
 * @param camera a #Camera
 * @param abilities
 * @return a gphoto2 error code
 *
 */
int
gp_camera_get_abilities (Camera *camera, CameraAbilities *abilities)
{
	C_PARAMS (camera && abilities);

	memcpy (abilities, &camera->pc->a, sizeof (CameraAbilities));

	return (GP_OK);
}


int
gp_camera_get_port_info (Camera *camera, GPPortInfo *info)
{
	C_PARAMS (camera && info);

	CR (camera, gp_port_get_info (camera->port, info), NULL);

	return (GP_OK);
}


int
gp_camera_set_port_info (Camera *camera, GPPortInfo info)
{
	char	*name, *path;
	C_PARAMS (camera);

	/*
	 * If the camera is currently initialized, terminate that connection.
	 * We don't care if we are successful or not.
	 */
	if (camera->pc->lh)
		gp_camera_exit (camera, NULL);

	gp_port_info_get_name (info, &name);
	gp_port_info_get_path (info, &path);
	GP_LOG_D ("Setting port info for port '%s' at '%s'...", name, path);
	CR (camera, gp_port_set_info (camera->port, info), NULL);

	return (GP_OK);
}


/**
 * Set the camera speed.
 *
 * @param camera a #Camera
 * @param speed the speed
 * @return a gphoto2 error code
 *
 * This function is typically used prior first initialization 
 * using #gp_camera_init for debugging purposes. Normally, a camera driver
 * will try to figure out the current speed of the camera and set the speed
 * to the optimal one automatically. Note that this function only works with 
 * serial ports. In other words, you have to set the camera's port to a 
 * serial one (using #gp_camera_set_port_path or #gp_camera_set_port_name)
 * prior calling this function.
 *
 */
int
gp_camera_set_port_speed (Camera *camera, int speed)
{
	GPPortSettings settings;

	C_PARAMS (camera);

	C_PARAMS_MSG (camera->port,
		      "You need to set a port prior trying to set the speed");
	C_PARAMS_MSG (camera->port->type == GP_PORT_SERIAL,
		      "You can specify a speed only with serial ports");

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
 * Retrieve the current speed.
 *
 * @param camera a #Camera
 * @return The current speed or a gphoto2 error code
 *
 */
int
gp_camera_get_port_speed (Camera *camera)
{
	C_PARAMS (camera);

	return (camera->pc->speed);
}


/**
 * Increment the reference count of a \c camera.
 *
 * @param camera a #Camera
 * @return a gphoto2 error code
 *
 */
int
gp_camera_ref (Camera *camera)
{
	C_PARAMS (camera);

	camera->pc->ref_count += 1;

	return (GP_OK);
}


/**
 * Decrements the reference count of a #Camera.
 *
 * @param camera a #Camera
 * @return a gphoto2 error code
 *
 * If the reference count reaches %0, the \c camera will be freed 
 * automatically.
 *
 */
int
gp_camera_unref (Camera *camera)
{
	C_PARAMS (camera);

	if (!camera->pc->ref_count) {
		GP_LOG_E ("gp_camera_unref on a camera with ref_count == 0 "
			"should not happen at all");
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
 * Free the \c camera.
 *
 * @param camera a #Camera
 * @return a gphoto2 error code
 *
 * \deprecated 
 * This function should never be used. Please use #gp_camera_unref instead.
 *
 */
int
gp_camera_free (Camera *camera)
{
	C_PARAMS (camera);

	GP_LOG_D ("Freeing camera...");

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
 * Autodetect all detectable camera
 *
 * @param list a #CameraList that receives the autodetected cameras
 * @param context a #GPContext
 * @return a gphoto2 error code
 *
 * This camera will autodetected all cameras that can be autodetected.
 * This will for instance detect all USB cameras.
 *
 *   CameraList *list;
 *   gp_list_new (&list);
 *   gp_camera_autodetect (list, context);
 *   ... done! ...
 */
int
gp_camera_autodetect (CameraList *list, GPContext *context)
{
	CameraAbilitiesList	*al = NULL;
	GPPortInfoList		*il = NULL;
	int			ret, i;
	CameraList		*xlist = NULL;

	ret = gp_list_new (&xlist);
	if (ret < GP_OK) goto out;
	if (!il) {
		/* Load all the port drivers we have... */
		ret = gp_port_info_list_new (&il);
		if (ret < GP_OK) goto out;
		ret = gp_port_info_list_load (il);
		if (ret < 0) goto out;
		ret = gp_port_info_list_count (il);
		if (ret < 0) goto out;
	}
	/* Load all the camera drivers we have... */
	ret = gp_abilities_list_new (&al);
	if (ret < GP_OK) goto out;
	ret = gp_abilities_list_load (al, context);
	if (ret < GP_OK) goto out;

	/* ... and autodetect the currently attached cameras. */
        ret = gp_abilities_list_detect (al, il, xlist, context);
	if (ret < GP_OK) goto out;

	/* Filter out the "usb:" entry */
        ret = gp_list_count (xlist);
	if (ret < GP_OK) goto out;
	for (i=0;i<ret;i++) {
		const char *name, *value;

		gp_list_get_name (xlist, i, &name);
		gp_list_get_value (xlist, i, &value);
		if (!strcmp ("usb:",value)) continue;
		gp_list_append (list, name, value);
	}
out:
	if (il) gp_port_info_list_free (il);
	if (al) gp_abilities_list_free (al);
	gp_list_free (xlist);
	if (ret < GP_OK)
		return ret;
	return gp_list_count(list);
}

/**
 * Initiate a connection to the \c camera. 
 *
 * @param camera a #Camera
 * @param context a #GPContext
 * @return a gphoto2 error code
 *
 * Before calling this function, the
 * \c camera should be set up using gp_camera_set_port_path() or
 * gp_camera_set_port_name() and gp_camera_set_abilities(). If that has been
 * omitted, gphoto2 tries to autodetect any cameras and chooses the first one
 * if any cameras are found. It is generally a good idea to call
 * gp_camera_exit() after transactions have been completed in order to give
 * other applications the chance to access the camera, too.
 *
 */
int
gp_camera_init (Camera *camera, GPContext *context)
{
	CameraAbilities a;
	const char *model, *port;
	CameraLibraryInitFunc init_func;
	int result;

	GP_LOG_D ("Initializing camera...");

	C_PARAMS (camera);
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
		GPPortInfoList	*il;
		int		m, p;
		char		*ppath;
		GPPortType	ptype;
		GPPortInfo	info;
        	CameraList	*list;

		result = gp_list_new (&list);
		if (result < GP_OK)
			return result;

		GP_LOG_D ("Neither port nor model set. Trying auto-detection...");

		/* Call auto-detect and choose the first camera */
		CRSL (camera, gp_abilities_list_new (&al), context, list);
		CRSL (camera, gp_abilities_list_load (al, context), context, list);
		CRSL (camera, gp_port_info_list_new (&il), context, list);
		CRSL (camera, gp_port_info_list_load (il), context, list);
		CRSL (camera, gp_abilities_list_detect (al, il, list, context), context, list);
		if (!gp_list_count (list)) {
			gp_abilities_list_free (al);
			gp_port_info_list_free (il);
			gp_context_error (context, _("Could not detect "
					     "any camera"));
			gp_list_free (list);
			return (GP_ERROR_MODEL_NOT_FOUND);
		}
		p = 0;
		CRSL (camera, gp_port_get_info (camera->port, &info), context, list);
		CRSL (camera, gp_port_info_get_path (info, &ppath), context, list);
		CRSL (camera, gp_port_info_get_type (info, &ptype), context, list);
		/* if the port was set before, then use that entry, but not if it is "usb:" */
		if ((ptype == GP_PORT_USB) && strlen(ppath) && strcmp(ppath, "usb:")) {
			for (p = gp_list_count (list);p--;) {
				const char *xp;

				gp_list_get_value (list, p, &xp);
				if (!strcmp (xp, ppath))
					break;
			}
			if (p<0) {
				gp_abilities_list_free (al);
				gp_port_info_list_free (il);
				gp_context_error (context, _("Could not detect any camera at port %s"), ppath);
				gp_list_free (list);
				return (GP_ERROR_FILE_NOT_FOUND);
			}
		}

		CRSL (camera, gp_list_get_name  (list, p, &model), context, list);
		m = gp_abilities_list_lookup_model (al, model);
		CRSL (camera, m, context, list);
		CRSL (camera, gp_abilities_list_get_abilities (al, m, &a), context, list);
		gp_abilities_list_free (al);
		CRSL (camera, gp_camera_set_abilities (camera, a), context, list);
		CRSL (camera, gp_list_get_value (list, p, &port), context, list);
		p = gp_port_info_list_lookup_path (il, port);
		CRSL (camera, p, context, list);
		CRSL (camera, gp_port_info_list_get_info (il, p, &info), context, list);
		CRSL (camera, gp_camera_set_port_info (camera, info), context, list);
		gp_port_info_list_free (il);
		gp_list_free (list);
	}

	if (strcasecmp (camera->pc->a.model, "Directory Browse")) {
		switch (camera->port->type) {
		case GP_PORT_NONE:
			gp_context_error (context, _("You have to set the "
				"port prior to initialization of the camera."));
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
	GP_LOG_D ("Loading '%s'...", camera->pc->a.library);
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
 * Retrieve a configuration \c window for the \c camera.
 *
 * @param camera a #Camera
 * @param window a #CameraWidget
 * @param context a #GPContext
 * @return gphoto2 error code
 *
 * This \c window can be used for construction of a configuration dialog.
 *
 */
int
gp_camera_get_config (Camera *camera, CameraWidget **window, GPContext *context)
{
	C_PARAMS (camera);
	CHECK_INIT (camera, context);

	if (!camera->functions->get_config) {
		gp_context_error (context, _("This camera does "
			"not provide any configuration options."));
		CAMERA_UNUSED (camera, context);
                return (GP_ERROR_NOT_SUPPORTED);
	}

	CHECK_RESULT_OPEN_CLOSE (camera, camera->functions->get_config (
					camera, window, context), context);

	CAMERA_UNUSED (camera, context);
	return (GP_OK);
}


/**
 * Retrieve a single configuration \c widget for the \c camera.
 *
 * @param camera a #Camera
 * @param name the name of a configuration widget
 * @param widget a #CameraWidget
 * @param context a #GPContext
 * @return gphoto2 error code
 *
 * This \c widget will then contain the current and the possible values and the type.
 *
 */
int
gp_camera_get_single_config (Camera *camera, const char *name, CameraWidget **widget, GPContext *context)
{
	CameraWidget		*rootwidget, *child;
	CameraWidgetType	type;
	const char		*label;
	int			ret, ro;

	C_PARAMS (camera);
	CHECK_INIT (camera, context);

	if (camera->functions->get_single_config) {
		CHECK_RESULT_OPEN_CLOSE (camera, camera->functions->get_single_config (
						camera, name, widget, context), context);

		CAMERA_UNUSED (camera, context);
		return GP_OK;
	}

	if (!camera->functions->get_config) {
		gp_context_error (context, _("This camera does not provide any configuration options."));
		CAMERA_UNUSED (camera, context);
		return GP_ERROR_NOT_SUPPORTED;
	}
	/* emulate it ... */
	CHECK_OPEN (camera, context);

	ret = camera->functions->get_config ( camera, &rootwidget, context);
	if (ret != GP_OK) {
		CHECK_CLOSE (camera, context);
		CAMERA_UNUSED (camera, context);
		return ret;
	}
	ret = gp_widget_get_child_by_name (rootwidget, name, &child);
	if (ret != GP_OK) {
		gp_widget_free (rootwidget);
		CHECK_CLOSE (camera, context);
		CAMERA_UNUSED (camera, context);
		return ret;
	}

	/* We need to duplicate the widget, as we will free the widgettree */
	gp_widget_get_type (child, &type);
	gp_widget_get_label (child, &label);
	gp_widget_get_readonly (child, &ro);

	ret = gp_widget_new (type, label, widget);
	if (ret != GP_OK)
		goto out;
	gp_widget_set_name (*widget, name);
	gp_widget_set_readonly (*widget, ro);

	switch (type) {
        case GP_WIDGET_MENU:
        case GP_WIDGET_RADIO: {
		char *value;
		int i, nrofchoices;

		nrofchoices = gp_widget_count_choices (child);
		for (i = 0; i < nrofchoices; i++) {
			const char *choice;

			gp_widget_get_choice (child, i, &choice);
			gp_widget_add_choice (*widget, choice);
		}
		gp_widget_get_value (child, &value);
		gp_widget_set_value (*widget, value);
		break;
	}
        case GP_WIDGET_TEXT: {
		char *value;

		gp_widget_get_value (child, &value);
		gp_widget_set_value (*widget, value);
		break;
	}
        case GP_WIDGET_RANGE: {
		float value, rmin, rmax, rstep;

		gp_widget_get_range (child, &rmin, &rmax, &rstep);
		gp_widget_set_range (*widget, rmin, rmax, rstep);
		gp_widget_get_value (child, &value);
		gp_widget_set_value (*widget, &value);
                break;
	}
        case GP_WIDGET_TOGGLE:
        case GP_WIDGET_DATE: {
		int value;

		gp_widget_get_value (child, &value);
		gp_widget_set_value (*widget, &value);
                break;
	}
        case GP_WIDGET_BUTTON:
        case GP_WIDGET_SECTION:
        case GP_WIDGET_WINDOW:
        default:
                ret = GP_ERROR_BAD_PARAMETERS;
		break;
	}
out:
	gp_widget_free (rootwidget);
	CHECK_CLOSE (camera, context);
	CAMERA_UNUSED (camera, context);
	return ret;
}


/**
 * Retrieve a configuration \c list for the \c camera.
 *
 * @param camera a #Camera
 * @param list a #CameraList
 * @param context a #GPContext
 * @return gphoto2 error code
 *
 * The names in list can be used for the single set and get configuration calls.
 *
 */

static void
_get_widget_names (CameraWidget *widget, CameraList *list)
{
	CameraWidgetType	type;

	gp_widget_get_type (widget, &type);
	switch (type) {
        case GP_WIDGET_MENU:
        case GP_WIDGET_RADIO:
        case GP_WIDGET_TEXT:
        case GP_WIDGET_RANGE:
        case GP_WIDGET_TOGGLE:
        case GP_WIDGET_DATE: {
		const char *name;

		gp_widget_get_name (widget, &name);
		gp_list_append (list, name, NULL);
                break;
	}
        case GP_WIDGET_SECTION:
        case GP_WIDGET_WINDOW: {
		int i, nrofchildren;

		nrofchildren = gp_widget_count_children (widget);
		for (i = 0; i < nrofchildren; i++) {
			CameraWidget *child;

			gp_widget_get_child (widget, i, &child);
			_get_widget_names (child, list);
		}
		break;
	}
        case GP_WIDGET_BUTTON:
        default:
		break;
	}

}

int
gp_camera_list_config (Camera *camera, CameraList *list, GPContext *context)
{
	CameraWidget		*rootwidget;
	int			ret;
	C_PARAMS (camera);
	CHECK_INIT (camera, context);

	if (camera->functions->list_config) {
		CHECK_RESULT_OPEN_CLOSE (camera, camera->functions->list_config (
						camera, list, context), context);

		CAMERA_UNUSED (camera, context);
		return GP_OK;
	}
	if (!camera->functions->get_config) {
		gp_context_error (context, _("This camera does not provide any configuration options."));
		CAMERA_UNUSED (camera, context);
		return GP_ERROR_NOT_SUPPORTED;
	}
	/* emulate it ... */
	CHECK_OPEN (camera, context);

	ret = camera->functions->get_config ( camera, &rootwidget, context);
	if (ret != GP_OK) {
		CHECK_CLOSE (camera, context);
		CAMERA_UNUSED (camera, context);
		return ret;
	}

	_get_widget_names (rootwidget, list);


	gp_widget_free (rootwidget);
	CHECK_CLOSE (camera, context);
	CAMERA_UNUSED (camera, context);
	return ret;
}


/**
 * Sets the configuration.
 *
 * @param camera a #Camera
 * @param window a #CameraWidget
 * @param context a #GPContext
 * @return a gphoto2 error code
 *
 * Typically, a \c window is retrieved using #gp_camera_get_config and passed 
 * to this function in order to adjust the settings on the camera.
 *
 **/
int
gp_camera_set_config (Camera *camera, CameraWidget *window, GPContext *context)
{
	C_PARAMS (camera && window);
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
 * Set a single configuration \c widget for the \c camera.
 *
 * @param camera a #Camera
 * @param name the name of a configuration widget
 * @param widget a #CameraWidget
 * @param context a #GPContext
 * @return gphoto2 error code
 *
 * This \c widget contains the new value of the widget to set.
 *
 */
int
gp_camera_set_single_config (Camera *camera, const char *name, CameraWidget *widget, GPContext *context)
{
	CameraWidget		*rootwidget, *child;
	CameraWidgetType	type;
	int			ret;

	C_PARAMS (camera);
	CHECK_INIT (camera, context);

	if (camera->functions->set_single_config) {
		CHECK_RESULT_OPEN_CLOSE (camera, camera->functions->set_single_config (
						camera, name, widget, context), context);

		CAMERA_UNUSED (camera, context);
		return GP_OK;
	}

	if (!camera->functions->set_config) {
		gp_context_error (context, _("This camera does not provide any configuration options."));
		CAMERA_UNUSED (camera, context);
		return GP_ERROR_NOT_SUPPORTED;
	}
	/* emulate single config with the full tree */
	CHECK_OPEN (camera, context);

	ret = camera->functions->get_config ( camera, &rootwidget, context);
	if (ret != GP_OK) {
		CHECK_CLOSE (camera, context);
		CAMERA_UNUSED (camera, context);
		return ret;
	}
	ret = gp_widget_get_child_by_name (rootwidget, name, &child);
	if (ret != GP_OK) {
		gp_widget_free (rootwidget);
		CHECK_CLOSE (camera, context);
		CAMERA_UNUSED (camera, context);
		return ret;
	}

	gp_widget_get_type (child, &type);
	ret = GP_OK;
	switch (type) {
        case GP_WIDGET_MENU:
        case GP_WIDGET_RADIO:
        case GP_WIDGET_TEXT: {
		char *value;

		gp_widget_get_value (widget, &value);
		gp_widget_set_value (child, value);
		break;
	}
        case GP_WIDGET_RANGE: {
		float value;

		gp_widget_get_value (widget, &value);
		gp_widget_set_value (child, &value);
                break;
	}
        case GP_WIDGET_TOGGLE:
        case GP_WIDGET_DATE: {
		int value;

		gp_widget_get_value (widget, &value);
		gp_widget_set_value (child, &value);
                break;
	}
        case GP_WIDGET_BUTTON:
        case GP_WIDGET_SECTION:
        case GP_WIDGET_WINDOW:
        default:
                ret = GP_ERROR_BAD_PARAMETERS;
		break;
	}
	gp_widget_set_changed (child, 1);

	if (ret == GP_OK)
		ret = camera->functions->set_config (camera, rootwidget, context);
	gp_widget_free (rootwidget);
	CHECK_CLOSE (camera, context);
	CAMERA_UNUSED (camera, context);
	return ret;
}


/**
 * Retrieves a camera summary.
 *
 * @param camera a #Camera
 * @param summary a #CameraText
 * @param context a #GPContext
 * @return a gphoto2 error code
 *
 * This summary typically contains information like manufacturer, pictures
 * taken, or generally information that is not configurable.
 *
 **/
int
gp_camera_get_summary (Camera *camera, CameraText *summary, GPContext *context)
{
	C_PARAMS (camera && summary);
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
 * Retrieves the \c manual for given \c camera.
 *
 * @param camera a #Camera
 * @param manual a #CameraText
 * @param context a #GPContext
 * @return a gphoto2 error code
 *
 * This manual typically contains information about using the camera.
 *
 **/
int
gp_camera_get_manual (Camera *camera, CameraText *manual, GPContext *context)
{
	C_PARAMS (camera && manual);
	CHECK_INIT (camera, context);

	if (!camera->functions->manual) {
		gp_context_error (context, _("This camera "
			"does not provide a manual."));
		CAMERA_UNUSED (camera, context);
                return (GP_ERROR_NOT_SUPPORTED);
	}

	CHECK_RESULT_OPEN_CLOSE (camera, camera->functions->manual (camera,
						manual, context), context);

	CAMERA_UNUSED (camera, context);
	return (GP_OK);
}

/**
 * Retrieves information about the camera driver.
 *
 * @param camera a #Camera
 * @param about a #CameraText
 * @param context a #GPContext
 * @return a gphoto2 error code
 *
 * Typically, this information contains name and address of the author,
 * acknowledgements, etc.
 *
 **/
int
gp_camera_get_about (Camera *camera, CameraText *about, GPContext *context)
{
	C_PARAMS (camera && about);
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
 * Captures an image, movie, or sound clip depending on the given \c type.
 *
 * @param camera a #Camera
 * @param type a #CameraCaptureType
 * @param path a #CameraFilePath
 * @param context a #GPContext
 * @return a gphoto2 error code
 *
 * The resulting file will be stored on the camera. The location gets stored
 * in \c path. The file can then be downloaded using #gp_camera_file_get.
 *
 **/
int
gp_camera_capture (Camera *camera, CameraCaptureType type,
		   CameraFilePath *path, GPContext *context)
{
	C_PARAMS (camera);
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
 * Triggers capture of one or more images.
 *
 * @param camera a #Camera
 * @param context a #GPContext
 * @return a gphoto2 error code
 *
 * This functions just remotely causes the shutter release and returns
 * immediately. You will want to run #gp_camera_wait_event until a image
 * is added which can be downloaded using #gp_camera_file_get.
 **/
int
gp_camera_trigger_capture (Camera *camera, GPContext *context)
{
	C_PARAMS (camera);
	CHECK_INIT (camera, context);

	if (!camera->functions->trigger_capture) {
		gp_context_error (context, _("This camera can not trigger capture."));
		CAMERA_UNUSED (camera, context);
                return (GP_ERROR_NOT_SUPPORTED);
	}
	CHECK_RESULT_OPEN_CLOSE (camera, camera->functions->trigger_capture (camera,
						context), context);
	CAMERA_UNUSED (camera, context);
	return (GP_OK);
}

/**
 * Captures a preview that won't be stored on the camera but returned in 
 * supplied file. 
 *
 * @param camera a #Camera
 * @param file a #CameraFile
 * @param context a #GPContext
 * @return a gphoto2 error code
 *
 * For example, you could use gp_capture_preview() for taking some sample
 * pictures before calling gp_capture().
 *
 **/
int
gp_camera_capture_preview (Camera *camera, CameraFile *file, GPContext *context)
{
	char *xname;
	C_PARAMS (camera && file);
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
	gp_file_get_name_by_type (file, "capture_preview", GP_FILE_TYPE_NORMAL, &xname);
	/* FIXME: Marcus ... will go away, just keep compatible now. */
	gp_file_set_name (file, xname);
	free (xname);

	CAMERA_UNUSED (camera, context);
	return (GP_OK);
}


/**
 * Wait for an event from the camera.
 *
 * @param camera a Camera
 * @param timeout amount of time to wait in 1/1000 seconds
 * @param eventtype received CameraEventType [out]
 * @param eventdata received event specific data [out]
 * @param context a GPContext
 * @return gphoto2 error code
 *
 * This function blocks and waits for an event to come from the camera.  If
 * timeout occurs before an event is received then
 * *eventtype==GP_EVENT_TIMEOUT and eventdata is left unchanged.
 * If an event is received then eventtype is set to the type of event, and
 * eventdata is set to event specific data.  See the CameraEventType enum
 * to see which eventtype's match to which types of eventdata.
 *
 */
int
gp_camera_wait_for_event (Camera *camera, int timeout,
		          CameraEventType *eventtype, void **eventdata,
			  GPContext *context)
{
	C_PARAMS (camera);
	CHECK_INIT (camera, context);

	if (!camera->functions->wait_for_event) {
		CAMERA_UNUSED (camera, context);
                return (GP_ERROR_NOT_SUPPORTED);
	}
	CHECK_RESULT_OPEN_CLOSE (camera, camera->functions->wait_for_event (
					camera, timeout, eventtype, eventdata,
					context), context);
	CAMERA_UNUSED (camera, context);
	return (GP_OK);
}

/**
 * Lists the files in supplied \c folder.
 *
 * @param camera a #Camera
 * @param folder a folder
 * @param list a #CameraList
 * @param context a #GPContext
 * @return a gphoto2 error code
 *
 **/
int
gp_camera_folder_list_files (Camera *camera, const char *folder, 
			     CameraList *list, GPContext *context)
{
	GP_LOG_D ("Listing files in '%s'...", folder);

	C_PARAMS (camera && folder && list);
	CHECK_INIT (camera, context);
	CR (camera, gp_list_reset (list), context);

	CHECK_RESULT_OPEN_CLOSE (camera, gp_filesystem_list_files (camera->fs,
					folder, list, context), context);

	CR (camera, gp_list_sort (list), context);
	CAMERA_UNUSED (camera, context);
        return (GP_OK);
}

/**
 * Lists the folders in supplied \c folder.
 *
 * @param camera a #Camera
 * @param folder a folder
 * @param list a #CameraList
 * @param context a #GPContext
 * @return a gphoto2 error code
 *
 **/
int
gp_camera_folder_list_folders (Camera *camera, const char* folder, 
			       CameraList *list, GPContext *context)
{
	GP_LOG_D ("Listing folders in '%s'...", folder);

	C_PARAMS (camera && folder && list);
	CHECK_INIT (camera, context);
	CR (camera, gp_list_reset (list), context);

	CHECK_RESULT_OPEN_CLOSE (camera, gp_filesystem_list_folders (
				camera->fs, folder, list, context), context);

	CR (camera, gp_list_sort (list), context);
	CAMERA_UNUSED (camera, context);
        return (GP_OK);
}

/**
 * Deletes all files in a given \c folder.
 *
 * @param camera a #Camera
 * @param folder a folder
 * @param context a #GPContext
 * @return a gphoto2 error code
 *
 **/
int
gp_camera_folder_delete_all (Camera *camera, const char *folder,
			     GPContext *context)
{
	GP_LOG_D ("Deleting all files in '%s'...", folder);

	C_PARAMS (camera && folder);
	CHECK_INIT (camera, context);

	CHECK_RESULT_OPEN_CLOSE (camera, gp_filesystem_delete_all (camera->fs,
						folder, context), context);

	CAMERA_UNUSED (camera, context);
	return (GP_OK);
}

/**
 * Uploads a file into given \c folder.
 *
 * @param camera a #Camera
 * @param folder a folder
 * @param file a #CameraFile
 * @param context a #GPContext
 * @return a gphoto2 error code
 *
 **/
int
gp_camera_folder_put_file (Camera *camera,
			   const char *folder, const char *filename,
			   CameraFileType type,
			   CameraFile *file, GPContext *context)
{
	GP_LOG_D ("Uploading file into '%s'...",
		folder);

	C_PARAMS (camera && folder && file);
	CHECK_INIT (camera, context);

	CHECK_RESULT_OPEN_CLOSE (camera, gp_filesystem_put_file (camera->fs,
					folder, filename, type, file, context), context);

	CAMERA_UNUSED (camera, context);
	return (GP_OK);
}

/**
 * Retrieves information about a file.
 *
 * @param camera a #Camera
 * @param folder a folder
 * @param file the name of the file
 * @param info
 * @param context a #GPContext
 * @return a gphoto2 error code
 *
 **/
int
gp_camera_file_get_info (Camera *camera, const char *folder, 
			 const char *file, CameraFileInfo *info,
			 GPContext *context)
{
	int result = GP_OK;
	const char *mime_type;
	const char *data;
	/* long int size; */
	CameraFile *cfile;

	GP_LOG_D ("Getting file info for '%s' in '%s'...", file, folder);

	C_PARAMS (camera && folder && file && info);
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
		unsigned long size;
		info->preview.fields |= GP_FILE_INFO_SIZE | GP_FILE_INFO_TYPE;
		gp_file_get_data_and_size (cfile, &data, &size);
		info->preview.size = size;
		gp_file_get_mime_type (cfile, &mime_type);
		strncpy (info->preview.type, mime_type,
			 sizeof (info->preview.type));
	}
	gp_file_unref (cfile);

	CAMERA_UNUSED (camera, context);
	return (GP_OK);
}

/**
 * Sets some file properties like name or permissions.
 *
 * @param camera a #Camera
 * @param folder a folder
 * @param file the name of a file
 * @param info the #CameraFileInfo
 * @param context a #GPContext
 * @return a gphoto2 error code
 *
 **/
int
gp_camera_file_set_info (Camera *camera, const char *folder, 
			 const char *file, CameraFileInfo info,
			 GPContext *context)
{
	C_PARAMS (camera && folder && file);
	CHECK_INIT (camera, context);

	CHECK_RESULT_OPEN_CLOSE (camera, gp_filesystem_set_info (camera->fs,
					folder, file, info, context), context);

	CAMERA_UNUSED (camera, context);
	return (GP_OK);
}

/**
 * Retrieves a file from the #Camera.
 *
 * @param camera a #Camera
 * @param folder a folder
 * @param file the name of a file
 * @param type the #CameraFileType
 * @param camera_file a #CameraFile
 * @param context a #GPContext
 * @return a gphoto2 error code
 *
 **/
int 
gp_camera_file_get (Camera *camera, const char *folder, const char *file,
		    CameraFileType type, CameraFile *camera_file,
		    GPContext *context)
{
	GP_LOG_D ("Getting file '%s' in folder '%s'...", file, folder);

	C_PARAMS (camera && folder && file && camera_file);
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
 * Reads a file partially from the #Camera.
 *
 * @param camera a #Camera
 * @param folder a folder
 * @param file the name of a file
 * @param type the #CameraFileType
 * @param offset the offset into the camera file
 * @param data the buffer receiving the data
 * @param size the size to be read and that was read
 * @param context a #GPContext
 * @return a gphoto2 error code
 *
 **/
int
gp_camera_file_read (Camera *camera, const char *folder, const char *file,
		    CameraFileType type,
		    uint64_t offset, char *buf, uint64_t *size,
		    GPContext *context)
{
	GP_LOG_D ("Getting file '%s' in folder '%s'...", file, folder);

	C_PARAMS (camera && folder && file && buf && size);
	CHECK_INIT (camera, context);

	/* Did we get reasonable foldername/filename? */
	if (strlen (folder) == 0) {
		CAMERA_UNUSED (camera, context);
		return (GP_ERROR_DIRECTORY_NOT_FOUND);
	}
	if (strlen (file) == 0) {
		CAMERA_UNUSED (camera, context);
		return (GP_ERROR_FILE_NOT_FOUND);
	}
  
	CHECK_RESULT_OPEN_CLOSE (camera, gp_filesystem_read_file (camera->fs,
			folder, file, type, offset, buf, size, context), context);

	CAMERA_UNUSED (camera, context);
	return (GP_OK);
}

/**
 * Deletes the file from \c folder.
 *
 * \param camera a #Camera
 * \param folder a folder
 * \param file the name of a file
 * \param context a #GPContext
 * \return a gphoto2 error code
 *
 **/
int
gp_camera_file_delete (Camera *camera, const char *folder, const char *file,
		       GPContext *context)
{
	GP_LOG_D ("Deleting file '%s' in folder '%s'...", file, folder);

	C_PARAMS (camera && folder && file);
	CHECK_INIT (camera, context);

	CHECK_RESULT_OPEN_CLOSE (camera, gp_filesystem_delete_file (
				camera->fs, folder, file, context), context);

	CAMERA_UNUSED (camera, context);
	return (GP_OK);
}

/**
 * Creates a new directory called \c name in the given \c folder.
 *
 * @param camera a #Camera
 * @param folder the location where to create the new directory
 * @param name the name of the directory to be created
 * @param context a #GPContext
 * @return a gphoto2 error code
 *
 **/
int
gp_camera_folder_make_dir (Camera *camera, const char *folder,
			   const char *name, GPContext *context)
{
	C_PARAMS (camera && folder && name);
	CHECK_INIT (camera, context);

	CHECK_RESULT_OPEN_CLOSE (camera, gp_filesystem_make_dir (camera->fs,
					folder, name, context), context);

	CAMERA_UNUSED (camera, context);
	return (GP_OK);
}

/**
 * Removes an (empty) directory called \c name from the given \c folder.
 *
 * @param camera a #Camera
 * @param folder the folder from which to remove the directory
 * @param name the name of the directory to be removed
 * @param context a #GPContext
 * @return a gphoto2 error code
 *
 */
int
gp_camera_folder_remove_dir (Camera *camera, const char *folder,
			     const char *name, GPContext *context)
{
	C_PARAMS (camera && folder && name);
	CHECK_INIT (camera, context);

	CHECK_RESULT_OPEN_CLOSE (camera, gp_filesystem_remove_dir (camera->fs,
					folder, name, context), context);

	CAMERA_UNUSED (camera, context);
	return (GP_OK);
}

/**
 * \brief Gets information on the camera attached storage.
 *
 * \param camera a #Camera
 * \param sifs Pointer to receive a pointer to/array of storage info items
 * \param nrofsifs Pointer to receive number of array entries
 * \param context a #GPContext
 * \return a gphoto2 error code
 *
 * Retrieves the storage information, like maximum and free space, for
 * the specified filesystem, if supported by the device. The storage
 * information is returned in an newly allocated array of
 * #CameraStorageInformation objects, to which the pointer pointed to
 * by #sifs will be set.
 *
 * The variable pointed to by #nrofsifs will be set to the number of
 * elements in that array.
 *
 * It is the caller's responsibility to free the memory of the array.
 *
 **/
int
gp_camera_get_storageinfo (
	Camera *camera, CameraStorageInformation **sifs,
	int *nrofsifs, GPContext *context)
{
	C_PARAMS (camera && sifs && nrofsifs);
	CHECK_INIT (camera, context);

	CHECK_RESULT_OPEN_CLOSE (camera,
		gp_filesystem_get_storageinfo (
			camera->fs, sifs, nrofsifs, context
		),
		context
	);
	CAMERA_UNUSED (camera, context);
	return (GP_OK);
}

/**
 * @param camera a Camera
 * @param start_func
 * @param stop_func
 * @param data
 * @return a gphoto2 error code
 *
 * If your frontend has something like idle loops, it is recommended you
 * use #gp_camera_set_timeout_funcs in order to give the camera driver the
 * possibility to keep up the connection to the camera.
 *
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
 * @param camera a #Camera
 * @param timeout number of seconds that should pass between each call to
 * 	     \c func
 * @param func the function that should be called each \c timeout seconds
 * @return The id of the background process or a gphoto2 error code
 *
 * This function should be called by the camera driver during camera_init()
 * if the camera needs to be sent messages periodically in order to prevent
 * it from shutting down.
 *
 */
int
gp_camera_start_timeout (Camera *camera, unsigned int timeout,
			 CameraTimeoutFunc func)
{
	int id;

	C_PARAMS (camera && camera->pc);

	if (!camera->pc->timeout_start_func)
		return (GP_ERROR_NOT_SUPPORTED);

	/*
	 * We remember the id here in order to automatically remove
	 * the timeout on gp_camera_exit.
	 */
	C_MEM (camera->pc->timeout_ids =
			realloc (camera->pc->timeout_ids, sizeof (int) *
					(camera->pc->timeout_ids_len + 1)));

	id = camera->pc->timeout_start_func (camera, timeout, func,
					     camera->pc->timeout_data);
	if (id < 0)
		return (id);
	camera->pc->timeout_ids[camera->pc->timeout_ids_len] = id;
	camera->pc->timeout_ids_len++;

	return (id);
}


/**
 * Stop periodic calls to keepalive function.
 *
 * @param camera a #Camera
 * @param id the id of the background process previously returned by 
 *       #gp_camera_start_timeout
 *
 * Call this function in the camera driver if you want to stop a periodic
 * call to a function that has been started using #gp_camera_start_timeout.
 *
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
