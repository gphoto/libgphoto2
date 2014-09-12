/** \file gphoto2-context.c
 *
 * \author Copyright 2001 Lutz Mueller <lutz@users.sourceforge.net>
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

#define _BSD_SOURCE

#include "config.h"
#include <gphoto2/gphoto2-context.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gphoto2/gphoto2-port-log.h>

/**
 * \internal
 **/
struct _GPContext
{
	GPContextIdleFunc     idle_func;
	void                 *idle_func_data;

	GPContextProgressStartFunc  progress_start_func;
	GPContextProgressUpdateFunc progress_update_func;
	GPContextProgressStopFunc   progress_stop_func;
	void                       *progress_func_data;

	GPContextErrorFunc    error_func;
	void                 *error_func_data;

	GPContextQuestionFunc question_func;
	void                 *question_func_data;

	GPContextCancelFunc   cancel_func;
	void                 *cancel_func_data;

	GPContextStatusFunc   status_func;
	void                 *status_func_data;

	GPContextMessageFunc  message_func;
	void                 *message_func_data;

	unsigned int ref_count;
};

/**
 * \brief Creates a new context.
 *
 * To be used by the frontend.
 *
 * \return a GPContext.
 **/
GPContext *
gp_context_new (void)
{
	GPContext *context;

	context = calloc (1, sizeof (GPContext));
	if (!context)
		return (NULL);

	context->ref_count = 1;

	return (context);
}

/**
 * Increments the reference count of the context.
 *
 * \param context The context to bump the reference
 **/
void
gp_context_ref (GPContext *context)
{
	if (!context)
		return;

	context->ref_count++;
}

static void
gp_context_free (GPContext *context)
{
	free (context);
}

/**
 * \brief Decrements reference count of a context.
 * 
 * Decrement the reference count of a context and free if it goes to 0.
 *
 * \param context The context to drop the reference count.
 **/
void
gp_context_unref (GPContext *context)
{
	if (!context)
		return;

	context->ref_count--;
	if (!context->ref_count)
		gp_context_free (context);
}

/**
 * \brief Notify frontend of a brief idle time.
 * 
 * Tells the frontend that it can do other processing at this moment, like refresh
 * the UI. Backends should call this function every time when an
 * interruption of the transfer is possible.
 *
 * \param context a GPContext
 **/
void
gp_context_idle (GPContext *context)
{
	if (!context)
		return;

	if (context->idle_func)
		context->idle_func (context, context->idle_func_data);
}

/**
 * \brief Start progress tracking.
 * 
 * This function starts up a new progress tracking for a specified context.
 * Several nested progress reports can happen at once, depending on the backend.
 *
 * \param context The context in which to start the progress.
 * \param target The 100% value.
 * \param format A sprintf style string to print out, including the following variable arguments.
 */
unsigned int
gp_context_progress_start (GPContext *context, float target,
			   const char *format, ...)
{
	va_list args;
	char *str;
	unsigned int id;

	if (!context)
		return (0);
	if (!context->progress_start_func)
		return (0);

	va_start (args, format);
	str = gpi_vsnprintf(format, args);
	va_end (args);

	if (!str)
		return 0;

	id = context->progress_start_func (context, target, str,
				context->progress_func_data);
	free (str);
	return (id);
}

void
gp_context_progress_update (GPContext *context, unsigned int id, float current)
{
	if (!context)
		return;

	if (context->progress_update_func)
		context->progress_update_func (context, id, current,
					       context->progress_func_data);
}

void
gp_context_progress_stop (GPContext *context, unsigned int id)
{
	if (!context)
		return;

	if (context->progress_stop_func)
		context->progress_stop_func (context, id,
					     context->progress_func_data);
}

void
gp_context_error (GPContext *context, const char *format, ...)
{
	va_list args;
	char *str;

	va_start (args, format);
	str = gpi_vsnprintf(format, args);
	va_end (args);

	if (!str)
		return;

	/* Log the error message */
	gp_log( GP_LOG_ERROR, __func__, "%s", str);

	if (context && context->error_func)
		context->error_func (context, str, context->error_func_data);
	free (str);
}

void
gp_context_status (GPContext *context, const char *format, ...)
{
	va_list args;
	char *str;

	va_start (args, format);
	str = gpi_vsnprintf(format, args);
	va_end (args);

	if (!str)
		return;

	/* Log the status message */
	GP_LOG_D ("%s", str);

	if (context && context->status_func)
		context->status_func (context, str, context->status_func_data);
	free (str);
}

/**
 * \brief Print a message to the context
 *
 * This sends a message to the passed context, to be printed by
 * it in some kind of way, but do no other action.
 * 
 * To be used by camera drivers.
 *
 * \param context A GPContext
 * \param format A sprintf style format string
 * \param ... variable argument list depending on format string
 */
void
gp_context_message (GPContext *context, const char *format, ...)
{
	va_list args;
	char *str;

	va_start (args, format);
	str = gpi_vsnprintf(format, args);
	va_end (args);

	if (!str)
		return;

	/* Log the message */
	GP_LOG_D ("%s", str);

	if (context && context->message_func)
		context->message_func (context, str, context->message_func_data);
	free (str);
}

/**
 * \brief Ask frontend user a question
 *
 * Asks the user a question that he must answer either with "Ok" or "Cancel".
 *
 * To be used by a camera driver. (So far no camera driver is using it,
 * but this might change later.)
 *
 * \param context a GPContext
 * \param format a sprintf format string
 * \param ... variable arguments for format string
 * \return The user's answer in form of a GPContextFeedback.
 **/
GPContextFeedback
gp_context_question (GPContext *context, const char *format, ...)
{
	GPContextFeedback feedback;
	va_list args;
	char *str;

	va_start (args, format);
	str = gpi_vsnprintf(format, args);
	va_end (args);

	if (!str)
		return GP_CONTEXT_FEEDBACK_OK;

	feedback = GP_CONTEXT_FEEDBACK_OK;
	if (context && context->question_func)
		feedback = context->question_func (context, str, context->question_func_data);

	free (str);

	return feedback;
}

/**
 * gp_context_cancel:
 * @context: a #GPContext
 *
 * Gives the frontend the possibility to cancel the current operation that is
 * executed in this @context.
 *
 * Return value: a #GPContextFeedback.
 **/
GPContextFeedback
gp_context_cancel (GPContext *context)
{
	if (!context)
		return (GP_CONTEXT_FEEDBACK_OK);

	if (context->cancel_func)
		return (context->cancel_func (context,
					      context->cancel_func_data));
	else
		return (GP_CONTEXT_FEEDBACK_OK);
}

void
gp_context_set_idle_func (GPContext *context, GPContextIdleFunc func,
			  void *data)
{
	if (!context)
		return;

	context->idle_func      = func;
	context->idle_func_data = data;
}

void
gp_context_set_progress_funcs (GPContext *context,
			       GPContextProgressStartFunc  start_func,
			       GPContextProgressUpdateFunc update_func,
			       GPContextProgressStopFunc   stop_func,
			       void *data)
{
	if (!context)
		return;

	context->progress_start_func  = start_func;
	context->progress_update_func = update_func;
	context->progress_stop_func   = stop_func;
	context->progress_func_data   = data;
}

void
gp_context_set_error_func (GPContext *context, GPContextErrorFunc func,
			   void *data)
{
	if (!context)
		return;

	context->error_func      = func;
	context->error_func_data = data;
}

void
gp_context_set_question_func (GPContext *context, GPContextQuestionFunc func,
			      void *data)
{
	if (!context)
		return;

	context->question_func      = func;
	context->question_func_data = data;
}

void
gp_context_set_status_func (GPContext *context, GPContextStatusFunc func,
			    void *data)
{
	if (!context)
		return;

	context->status_func      = func;
	context->status_func_data = data;
}

void
gp_context_set_cancel_func (GPContext *context, GPContextCancelFunc func,
			    void *data)
{
	if (!context)
		return;

	context->cancel_func      = func;
	context->cancel_func_data = data;
}

void
gp_context_set_message_func (GPContext *context, GPContextMessageFunc func,
			     void *data)
{
	if (!context)
		return;

	context->message_func      = func;
	context->message_func_data = data;
}
