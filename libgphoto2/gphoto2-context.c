/* gphoto2-context.c
 *
 * Copyright (C) 2001 Lutz Müller <lutz@users.sourceforge.net>
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
#include "gphoto2-context.h"

#include <stdlib.h>

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

	unsigned int ref_count;
};

/**
 * gp_context_new:
 *
 * Creates a new context.
 *
 * Return value: a #GPContext.
 **/
GPContext *
gp_context_new (void)
{
	GPContext *context;

	context = malloc (sizeof (GPContext));
	if (!context)
		return (NULL);
	memset (context, 0, sizeof (GPContext));

	context->ref_count = 1;

	return (context);
};

/**
 * gp_context_ref:
 * @context: a #GPContext
 *
 * Increments the reference count of the @context.
 **/
void
gp_context_ref (GPContext *context)
{
	if (!context)
		return;

	context->ref_count++;
};

static void
gp_context_free (GPContext *context)
{
	if (!context)
		return;

	free (context);
};

/**
 * gp_context_unref (GPContext *context)
 * @context: a #GPContext
 *
 * Decrements the reference count of the @context.
 **/
void
gp_context_unref (GPContext *context)
{
	if (!context)
		return;

	context->ref_count--;
	if (!context->ref_count)
		gp_context_free (context);
};

/**
 * gp_context_idle:
 * @context: a #GPContext
 *
 * Tells the frontend that it can do other processing at this moment. Backends
 * should call this function every time when an interruption of the transfer
 * is possible.
 **/
void
gp_context_idle (GPContext *context)
{
	if (!context)
		return;

	if (context->idle_func)
		context->idle_func (context, context->idle_func_data);
}

unsigned int
gp_context_progress_start (GPContext *context, float target,
			   const char *format, ...)
{
	va_list args;
	unsigned int id;

	if (!context)
		return (0);

	if (context->progress_start_func) {
		va_start (args, format);
		id = context->progress_start_func (context, target, format,
					args, context->progress_func_data);
		va_end (args);
	}

	return (0);
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

	if (!context)
		return;

	if (context->error_func) {
		va_start (args, format);
		context->error_func (context, format, args,
				     context->error_func_data);
		va_end (args);
	}
}

void
gp_context_status (GPContext *context, const char *format, ...)
{
	va_list args;

	if (!context)
		return;

	if (context->status_func) {
		va_start (args, format);
		context->status_func (context, format, args,
				      context->status_func_data);
		va_end (args);
	}
}

/**
 * gp_context_question:
 * @context: a #GPContext
 * @format:
 * @...:
 *
 * Asks the user a question that he must answer either with "Ok" or "Cancel".
 *
 * Return value: The user's answer in form of a #GPContextFeedback.
 **/
GPContextFeedback
gp_context_question (GPContext *context, const char *format, ...)
{
	GPContextFeedback feedback;
	va_list args;

	if (!context)
		return (GP_CONTEXT_FEEDBACK_OK);

	if (context->question_func) {
		va_start (args, format);
		feedback = context->question_func (context, format, args,
						   context->question_func_data);
		va_end (args);
		return (feedback);
	} else
		return (GP_CONTEXT_FEEDBACK_OK);
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
