/* GtkItemEntry - widget for gtk+
 * Copyright (C) 1999-2001 Adrian E. Feiguin <adrian@ifir.ifir.edu.ar>
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * GtkItemEntry widget by Adrian E. Feiguin
 * Based on GtkEntry widget
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <ctype.h>
#include <string.h>
#include <gdk/gdkkeysyms.h>
#include <gdk/gdki18n.h>
#include <gtk/gtkmain.h>
#include <gtk/gtkselection.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkentry.h>
#include "gtkitementry.h"

#define MIN_ENTRY_WIDTH  150
#define DRAW_TIMEOUT     20
#define INNER_BORDER     4

enum {
  ARG_0,
  ARG_MAX_LENGTH,
  ARG_VISIBILITY
};


static void gtk_entry_class_init          (GtkItemEntryClass     *klass);
static void gtk_entry_init                (GtkItemEntry          *entry);
static void gtk_entry_set_position_from_editable 
                                          (GtkEditable *editable,
			                   gint      position);
static void gtk_entry_realize             (GtkWidget         *widget);
static void gtk_entry_unrealize           (GtkWidget         *widget);
static void gtk_entry_draw_focus          (GtkWidget         *widget);
static void gtk_entry_size_allocate       (GtkWidget         *widget,
					   GtkAllocation     *allocation);
static void gtk_entry_size_request        (GtkWidget         *widget,
					   GtkRequisition    *requisition);
static void gtk_entry_make_backing_pixmap (GtkEntry *entry,
					   gint width, gint height);
static void gtk_entry_draw                (GtkWidget         *widget,
					   GdkRectangle      *area);
static gint gtk_entry_expose              (GtkWidget         *widget,
					   GdkEventExpose    *event);
static gint gtk_entry_button_press        (GtkWidget         *widget,
					   GdkEventButton    *event);
static gint gtk_entry_button_release      (GtkWidget         *widget,
					   GdkEventButton    *event);
static gint gtk_entry_motion_notify       (GtkWidget         *widget,
					   GdkEventMotion    *event);
static gint gtk_entry_key_press           (GtkWidget         *widget,
					   GdkEventKey       *event);
static void gtk_entry_draw_text           (GtkEntry          *entry);
static void gtk_entry_draw_cursor         (GtkEntry          *entry);
static void gtk_entry_draw_cursor_on_drawable
					  (GtkEntry          *entry,
					   GdkDrawable       *drawable);
#ifdef USE_XIM
static void gtk_entry_update_ic_attr      (GtkWidget         *widget);
#endif
static void gtk_entry_queue_draw          (GtkEntry          *entry);
static gint gtk_entry_timer               (gpointer           data);
static gint gtk_entry_position            (GtkEntry          *entry,
					   gint               x);
static void entry_adjust_scroll           (GtkEntry          *entry);
static void gtk_entry_grow_text           (GtkEntry          *entry);
static void gtk_entry_insert_text         (GtkEditable       *editable,
					   const gchar       *new_text,
					   gint               new_text_length,
					   gint              *position);
static void gtk_entry_delete_text         (GtkEditable       *editable,
					   gint               start_pos,
					   gint               end_pos);
static void gtk_entry_update_text         (GtkEditable       *editable,
					   gint               start_pos,
					   gint               end_pos);
static gchar *gtk_entry_get_chars         (GtkEditable       *editable,
					   gint               start_pos,
					   gint               end_pos);

/* Binding actions */
static void gtk_entry_move_cursor         (GtkEditable *editable,
					   gint         x,
					   gint         y);
static void gtk_entry_move_word           (GtkEditable *editable,
					   gint         n);
static void gtk_entry_move_to_column      (GtkEditable *editable,
					   gint         row);
static void gtk_entry_kill_char           (GtkEditable *editable,
					   gint         direction);
static void gtk_entry_kill_word           (GtkEditable *editable,
					   gint         direction);
static void gtk_entry_kill_line           (GtkEditable *editable,
					   gint         direction);

/* To be removed */
static void gtk_move_forward_character    (GtkEntry          *entry);
static void gtk_move_backward_character   (GtkEntry          *entry);
static void gtk_move_forward_word         (GtkEntry          *entry);
static void gtk_move_backward_word        (GtkEntry          *entry);
static void gtk_move_beginning_of_line    (GtkEntry          *entry);
static void gtk_move_end_of_line          (GtkEntry          *entry);
static void gtk_delete_forward_character  (GtkEntry          *entry);
static void gtk_delete_backward_character (GtkEntry          *entry);
static void gtk_delete_forward_word       (GtkEntry          *entry);
static void gtk_delete_backward_word      (GtkEntry          *entry);
static void gtk_delete_line               (GtkEntry          *entry);
static void gtk_delete_to_line_end        (GtkEntry          *entry);
static void gtk_select_word               (GtkEntry          *entry,
					   guint32            time);
static void gtk_select_line               (GtkEntry          *entry,
					   guint32            time);


static void gtk_entry_set_selection       (GtkEditable       *editable,
					   gint               start,
					   gint               end);

static gint gtk_entry_find_position       (GtkEntry          *entry, 
					   gint               position);
static void gtk_entry_set_position_from_editable (GtkEditable *editable,
						  gint         position);

static GtkWidgetClass *parent_class = NULL;
static GdkAtom ctext_atom = GDK_NONE;

static const GtkTextFunction control_keys[26] =
{
  (GtkTextFunction)gtk_move_beginning_of_line,    /* a */
  (GtkTextFunction)gtk_move_backward_character,   /* b */
  (GtkTextFunction)gtk_editable_copy_clipboard,   /* c */
  (GtkTextFunction)gtk_delete_forward_character,  /* d */
  (GtkTextFunction)gtk_move_end_of_line,          /* e */
  (GtkTextFunction)gtk_move_forward_character,    /* f */
  NULL,                                           /* g */
  (GtkTextFunction)gtk_delete_backward_character, /* h */
  NULL,                                           /* i */
  NULL,                                           /* j */
  (GtkTextFunction)gtk_delete_to_line_end,        /* k */
  NULL,                                           /* l */
  NULL,                                           /* m */
  NULL,                                           /* n */
  NULL,                                           /* o */
  NULL,                                           /* p */
  NULL,                                           /* q */
  NULL,                                           /* r */
  NULL,                                           /* s */
  NULL,                                           /* t */
  (GtkTextFunction)gtk_delete_line,               /* u */
  (GtkTextFunction)gtk_editable_paste_clipboard,  /* v */
  (GtkTextFunction)gtk_delete_backward_word,      /* w */
  (GtkTextFunction)gtk_editable_cut_clipboard,    /* x */
  NULL,                                           /* y */
  NULL,                                           /* z */
};

static const GtkTextFunction alt_keys[26] =
{
  NULL,                                           /* a */
  (GtkTextFunction)gtk_move_backward_word,        /* b */
  NULL,                                           /* c */
  (GtkTextFunction)gtk_delete_forward_word,       /* d */
  NULL,                                           /* e */
  (GtkTextFunction)gtk_move_forward_word,         /* f */
  NULL,                                           /* g */
  NULL,                                           /* h */
  NULL,                                           /* i */
  NULL,                                           /* j */
  NULL,                                           /* k */
  NULL,                                           /* l */
  NULL,                                           /* m */
  NULL,                                           /* n */
  NULL,                                           /* o */
  NULL,                                           /* p */
  NULL,                                           /* q */
  NULL,                                           /* r */
  NULL,                                           /* s */
  NULL,                                           /* t */
  NULL,                                           /* u */
  NULL,                                           /* v */
  NULL,                                           /* w */
  NULL,                                           /* x */
  NULL,                                           /* y */
  NULL,                                           /* z */
};


GtkType
gtk_item_entry_get_type (void)
{
  static GtkType entry_type = 0;

  if (!entry_type)
    {
      static const GtkTypeInfo entry_info =
      {
	"GtkItemEntry",
	sizeof (GtkItemEntry),
	sizeof (GtkItemEntryClass),
	(GtkClassInitFunc) gtk_entry_class_init,
	(GtkObjectInitFunc) gtk_entry_init,
	/* reserved_1 */ NULL,
	/* reserved_2 */ NULL,
        (GtkClassInitFunc) NULL,
      };

      entry_type = gtk_type_unique (GTK_TYPE_ENTRY, &entry_info);
    }

  return entry_type;
}

static void
gtk_entry_class_init (GtkItemEntryClass *klass)
{
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;
  GtkEditableClass *editable_class;

  object_class = (GtkObjectClass*) klass;
  widget_class = (GtkWidgetClass*) klass;
  editable_class = (GtkEditableClass*) klass;
  parent_class = gtk_type_class (GTK_TYPE_ENTRY);

  widget_class->realize = gtk_entry_realize;
  widget_class->unrealize = gtk_entry_unrealize;
  widget_class->draw_focus = gtk_entry_draw_focus;
  widget_class->size_allocate = gtk_entry_size_allocate;
  widget_class->size_request = gtk_entry_size_request;
  widget_class->draw = gtk_entry_draw;
  widget_class->expose_event = gtk_entry_expose;
  widget_class->button_press_event = gtk_entry_button_press;
  widget_class->button_release_event = gtk_entry_button_release;
  widget_class->motion_notify_event = gtk_entry_motion_notify;
  widget_class->key_press_event = gtk_entry_key_press;

  editable_class->insert_text = gtk_entry_insert_text;
  editable_class->delete_text = gtk_entry_delete_text;
  editable_class->changed = (void (*)(GtkEditable *)) entry_adjust_scroll;

  editable_class->move_cursor = gtk_entry_move_cursor;
  editable_class->move_word = gtk_entry_move_word;
  editable_class->move_to_column = gtk_entry_move_to_column;

  editable_class->kill_char = gtk_entry_kill_char;
  editable_class->kill_word = gtk_entry_kill_word;
  editable_class->kill_line = gtk_entry_kill_line;

  editable_class->update_text = gtk_entry_update_text;
  editable_class->get_chars   = gtk_entry_get_chars;
  editable_class->set_selection = gtk_entry_set_selection;
  editable_class->set_position = gtk_entry_set_position_from_editable;

}


static void
gtk_entry_init (GtkItemEntry *entry)
{
}

GtkWidget*
gtk_item_entry_new ()
{
  GtkWidget *item_entry;

  item_entry = GTK_WIDGET (gtk_type_new (gtk_item_entry_get_type ()));

  return item_entry;
}

GtkWidget*
gtk_item_entry_new_with_max_length (guint16 max)
{
  GtkItemEntry *item_entry;
  item_entry = gtk_type_new (gtk_item_entry_get_type ());
  GTK_ENTRY (item_entry)->text_max_length = max;
  return GTK_WIDGET (item_entry);
}

void
gtk_item_entry_set_text (GtkItemEntry *item_entry,
		      const gchar *text,
                      GtkJustification justification )
{
  gint tmp_pos;

  GtkEditable *editable;
  GtkEntry *entry;

  g_return_if_fail (item_entry != NULL);
  g_return_if_fail (GTK_IS_ITEM_ENTRY (item_entry));
  g_return_if_fail (text != NULL);

  editable = GTK_EDITABLE (item_entry);
  entry = GTK_ENTRY(item_entry);

  item_entry->justification = justification;
  
  gtk_entry_delete_text (GTK_EDITABLE(entry), 0, entry->text_length);

  tmp_pos = 0;
  gtk_editable_insert_text (editable, text, strlen (text), &tmp_pos);
  editable->current_pos = tmp_pos;

  editable->selection_start_pos = 0;
  editable->selection_end_pos = 0;

  if (GTK_WIDGET_DRAWABLE (entry))
    gtk_entry_draw_text (entry);
}

static void
gtk_entry_set_position_from_editable (GtkEditable *editable,
			              gint      position)
{
  GtkEntry *entry;

  g_return_if_fail (editable != NULL);
  g_return_if_fail (GTK_IS_EDITABLE (editable));

  entry = GTK_ENTRY(editable);

  if ((position == -1) || (position > entry->text_length))
    GTK_EDITABLE(entry)->current_pos = entry->text_length;
  else
    GTK_EDITABLE(entry)->current_pos = position;
  entry_adjust_scroll (entry);
}

void
gtk_item_entry_set_justification (GtkItemEntry *item_entry, GtkJustification justification)
{
  g_return_if_fail (item_entry != NULL);
  g_return_if_fail (GTK_IS_ITEM_ENTRY (item_entry));

  item_entry->justification = justification;
  entry_adjust_scroll (GTK_ENTRY(item_entry));
  gtk_widget_draw(GTK_WIDGET(item_entry), NULL);
}


static void
gtk_entry_realize (GtkWidget *widget)
{
  GtkItemEntry *item_entry;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_ITEM_ENTRY (widget));

  if (GTK_WIDGET_CLASS (parent_class)->realize)
    (* GTK_WIDGET_CLASS (parent_class)->realize) (widget);

  item_entry = GTK_ITEM_ENTRY (widget);

  item_entry->fg_gc = gdk_gc_new (widget->window);  
  item_entry->bg_gc = gdk_gc_new (widget->window);  

  gdk_gc_set_foreground (item_entry->fg_gc, &widget->style->white);
  gdk_gc_set_foreground (item_entry->bg_gc, &widget->style->black);
}

static void
gtk_entry_unrealize (GtkWidget *widget)
{
  GtkItemEntry *item_entry;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_ITEM_ENTRY (widget));

  item_entry = GTK_ITEM_ENTRY (widget);

  gdk_gc_destroy (item_entry->fg_gc);
  gdk_gc_destroy (item_entry->bg_gc);

  if (GTK_WIDGET_CLASS (parent_class)->unrealize)
    (* GTK_WIDGET_CLASS (parent_class)->unrealize) (widget);
}

static void
gtk_entry_draw_focus (GtkWidget *widget)
{
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_ITEM_ENTRY (widget));

  if (GTK_WIDGET_DRAWABLE (widget))
    {
      gtk_entry_draw_cursor (GTK_ENTRY (widget));
    }
}


static void
gtk_entry_size_allocate (GtkWidget     *widget,
			 GtkAllocation *allocation)
{
  GtkEntry *entry;
  GtkEditable *editable;
  gint width = 0, nwidth = 0, height = 0, nheight = 0;


  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_ITEM_ENTRY (widget));
  g_return_if_fail (allocation != NULL);

  widget->allocation = *allocation;
  entry = GTK_ENTRY (widget);
  editable = GTK_EDITABLE (widget);

  if (GTK_WIDGET_REALIZED (widget))
    {
      gdk_window_get_size (entry->text_area, &width, &height);
      gdk_window_move_resize (widget->window,
			      allocation->x + INNER_BORDER,
			      allocation->y + INNER_BORDER,
			      allocation->width - 2 * INNER_BORDER,
                              allocation->height - 2 * INNER_BORDER);
      gdk_window_move_resize (entry->text_area,
			      0,
			      0,
			      allocation->width - 2 * INNER_BORDER,
			      allocation->height - 2 * INNER_BORDER);

    
      gdk_window_get_size (entry->text_area, &nwidth, &nheight);
      if(nwidth != width || nheight != height){
        entry->scroll_offset = 0;
        entry_adjust_scroll (entry);
      }

#ifdef USE_XIM
      if (editable->ic &&
	  (gdk_ic_get_style (editable->ic) & GDK_IM_PREEDIT_POSITION))
	{
	  editable->ic_attr->preedit_area.width = nwidth;
	  editable->ic_attr->preedit_area.height = nheight;
	  gdk_ic_set_attr (editable->ic, editable->ic_attr,
	      		   GDK_IC_PREEDIT_AREA);
	}
#endif

    }
}

static void
gtk_entry_size_request (GtkWidget     *widget,
			GtkRequisition *requisition)
{
  GtkItemEntry *entry;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_ITEM_ENTRY (widget));
  g_return_if_fail (requisition != NULL);

  requisition->width = MIN_ENTRY_WIDTH + (widget->style->klass->xthickness + INNER_BORDER) * 2;
  requisition->height = (widget->style->font->ascent +
                         widget->style->font->descent +
                         (widget->style->klass->ythickness + INNER_BORDER) * 2);

  entry = GTK_ITEM_ENTRY(widget);

  if(entry->text_max_size > 0 && 
     requisition->width > entry->text_max_size)
                 requisition->width = entry->text_max_size;
}
 
static void
gtk_entry_draw (GtkWidget    *widget,
		GdkRectangle *area)
{
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_ENTRY (widget));
  g_return_if_fail (area != NULL);

  if (GTK_WIDGET_DRAWABLE (widget))
    {
      gtk_widget_draw_focus (widget);
      gtk_entry_draw_text (GTK_ENTRY (widget));
    }
}

static gint
gtk_entry_expose (GtkWidget      *widget,
		  GdkEventExpose *event)
{
  GtkEntry *entry;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_ENTRY (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  entry = GTK_ENTRY (widget);

  if (widget->window == event->window)
    gtk_widget_draw_focus (widget);
  else if (entry->text_area == event->window)
    gtk_entry_draw_text (GTK_ENTRY (widget));

  return FALSE;
}

static gint
gtk_entry_button_press (GtkWidget      *widget,
			GdkEventButton *event)
{
  GtkEntry *entry;
  GtkEditable *editable;
  GdkModifierType mods;
  gint tmp_pos;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_ENTRY (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  if (ctext_atom == GDK_NONE)
    ctext_atom = gdk_atom_intern ("COMPOUND_TEXT", FALSE);

  entry = GTK_ENTRY (widget);
  editable = GTK_EDITABLE (widget);
  
  if (entry->button && (event->button != entry->button))
    return FALSE;

  gdk_window_get_pointer(widget->window, NULL, NULL, &mods);
  if (mods & GDK_BUTTON3_MASK)
    return FALSE;

  entry->button = event->button;
  
  if (!GTK_WIDGET_HAS_FOCUS (widget))
    gtk_widget_grab_focus (widget);

  if (event->button == 1)
    {
      switch (event->type)
	{
	case GDK_BUTTON_PRESS:
	  gtk_grab_add (widget);

	  tmp_pos = gtk_entry_position (entry, event->x + entry->scroll_offset);
	  /* Set it now, so we display things right. We'll unset it
	   * later if things don't work out */
	  editable->has_selection = TRUE;
	  gtk_entry_set_selection (editable, tmp_pos, tmp_pos);
	  editable->current_pos = editable->selection_start_pos;
	  break;

	case GDK_2BUTTON_PRESS:
	  gtk_select_word (entry, event->time);
	  break;

	case GDK_3BUTTON_PRESS:
	  gtk_select_line (entry, event->time);
	  break;

	default:
	  break;
	}
    }
  else if (event->type == GDK_BUTTON_PRESS)
    {
      if ((event->button == 2) && editable->editable)
	{
	  if (editable->selection_start_pos == editable->selection_end_pos ||
	      editable->has_selection)
	    editable->current_pos = gtk_entry_position (entry, event->x + entry->scroll_offset);
	  gtk_selection_convert (widget, GDK_SELECTION_PRIMARY,
				 ctext_atom, event->time);
	}
      else
	{
	  gtk_grab_add (widget);

	  tmp_pos = gtk_entry_position (entry, event->x + entry->scroll_offset);
	  gtk_entry_set_selection (editable, tmp_pos, tmp_pos);
	  editable->has_selection = FALSE;
	  editable->current_pos = editable->selection_start_pos;

	  if (gdk_selection_owner_get (GDK_SELECTION_PRIMARY) == widget->window)
	    gtk_selection_owner_set (NULL, GDK_SELECTION_PRIMARY, event->time);
	}
    }

  return FALSE;
}

static gint
gtk_entry_button_release (GtkWidget      *widget,
			  GdkEventButton *event)
{
  GtkEntry *entry;
  GtkEditable *editable;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_ENTRY (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  entry = GTK_ENTRY (widget);
  editable = GTK_EDITABLE (widget);

  if (entry->button != event->button)
    return FALSE;

  entry->button = 0;
  
  if (event->button == 1)
    {
      gtk_grab_remove (widget);

      editable->has_selection = FALSE;
      if (editable->selection_start_pos != editable->selection_end_pos)
	{
	  if (gtk_selection_owner_set (widget,
				       GDK_SELECTION_PRIMARY,
				       event->time))
	    editable->has_selection = TRUE;
	  else
	    gtk_entry_queue_draw (entry);
	}
      else
	{
	  if (gdk_selection_owner_get (GDK_SELECTION_PRIMARY) == widget->window)
	    gtk_selection_owner_set (NULL, GDK_SELECTION_PRIMARY, event->time);
	}
    }
  else if (event->button == 3)
    {
      gtk_grab_remove (widget);
    }

  return FALSE;
}

static gint
gtk_entry_motion_notify (GtkWidget      *widget,
			 GdkEventMotion *event)
{
  GtkEntry *entry;
  gint x;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_ENTRY (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  entry = GTK_ENTRY (widget);

  if (entry->button == 0)
    return FALSE;

  x = event->x;
  if (event->is_hint || (entry->text_area != event->window))
    gdk_window_get_pointer (entry->text_area, &x, NULL, NULL);

  GTK_EDITABLE(entry)->selection_end_pos = gtk_entry_position (entry, x + entry->scroll_offset);
  GTK_EDITABLE(entry)->current_pos = GTK_EDITABLE(entry)->selection_end_pos;
  entry_adjust_scroll (entry);
  gtk_entry_queue_draw (entry);

  return FALSE;
}

static gint
gtk_entry_key_press (GtkWidget   *widget,
		     GdkEventKey *event)
{
  GtkEntry *entry;
  GtkEditable *editable;

  gint return_val;
  gint key;
  guint initial_pos;
  gint extend_selection;
  gint extend_start;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_ENTRY (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  entry = GTK_ENTRY (widget);
  editable = GTK_EDITABLE (widget);
  return_val = FALSE;

  if(!GTK_WIDGET_HAS_FOCUS(widget)) return FALSE;

  if(editable->editable == FALSE)
    switch(event->keyval){
	case GDK_Left: case GDK_Right: case GDK_Up: case GDK_Down:
        case GDK_Tab: case GDK_Return:
         break;
        default:
         return TRUE;     
    }

  initial_pos = editable->current_pos;

  extend_selection = event->state & GDK_SHIFT_MASK;
  extend_start = FALSE;

  if (extend_selection)
    {
      if (editable->selection_start_pos == editable->selection_end_pos)
	{
	  editable->selection_start_pos = editable->current_pos;
	  editable->selection_end_pos = editable->current_pos;
	}
      
      extend_start = (editable->current_pos == editable->selection_start_pos);
    }

  switch (event->keyval)
    {
    case GDK_BackSpace:
      return_val = TRUE;
      if (event->state & GDK_CONTROL_MASK)
	gtk_delete_backward_word (entry);
      else
	gtk_delete_backward_character (entry);
      break;
    case GDK_Clear:
      return_val = TRUE;
      gtk_delete_line (entry);
      break;
    case GDK_Insert:
      return_val = TRUE;
      if (event->state & GDK_SHIFT_MASK)
	{
	  extend_selection = FALSE;
	  gtk_editable_paste_clipboard (editable);
	}
      else if (event->state & GDK_CONTROL_MASK)
	{
	  gtk_editable_copy_clipboard (editable);
	}
      else
	{
	  /* gtk_toggle_insert(entry) -- IMPLEMENT */
	}
      break;
    case GDK_Delete:
      return_val = TRUE;
      if (event->state & GDK_CONTROL_MASK)
	gtk_delete_forward_word (entry);
      else if (event->state & GDK_SHIFT_MASK)
	{
	  extend_selection = FALSE;
	  gtk_editable_cut_clipboard (editable);
	}
      else
	gtk_delete_forward_character (entry);
      break;
    case GDK_Home:
      return_val = TRUE;
      gtk_move_beginning_of_line (entry);
      break;
    case GDK_End:
      return_val = TRUE;
      gtk_move_end_of_line (entry);
      break;
    case GDK_Left:
      return_val = TRUE;
      if (event->state & GDK_CONTROL_MASK)
	gtk_move_backward_word (entry);
      else
	gtk_move_backward_character (entry);
      break;
    case GDK_Right:
      return_val = TRUE;
      if (event->state & GDK_CONTROL_MASK)
	gtk_move_forward_word (entry);
      else
	gtk_move_forward_character (entry);
      break;
    case GDK_Shift_L:
    case GDK_Shift_R:
    case GDK_Tab:
    case GDK_Escape:
    case GDK_Up:
    case GDK_Down:
    case GDK_Return:
      return_val = TRUE;
      gtk_signal_emit_by_name (GTK_OBJECT (entry), "activate");
      break;
    default:
      if ((event->keyval >= 0x20) && (event->keyval <= 0xFF))
	{
	  key = event->keyval;

	  if (event->state & GDK_CONTROL_MASK)
	    {
	      if ((key >= 'A') && (key <= 'Z'))
		key -= 'A' - 'a';

	      if ((key >= 'a') && (key <= 'z') && control_keys[key - 'a'])
		{
		  (* control_keys[key - 'a']) (editable, event->time);
		  return_val = TRUE;
		}
	      break;
	    }
	  else if (event->state & GDK_MOD1_MASK)
	    {
	      if ((key >= 'A') && (key <= 'Z'))
		key -= 'A' - 'a';

	      if ((key >= 'a') && (key <= 'z') && alt_keys[key - 'a'])
		{
		  (* alt_keys[key - 'a']) (editable, event->time);
		  return_val = TRUE;
		}
	      break;
	    }
	}
      if (event->length > 0)
	{
	  gint tmp_pos;

	  extend_selection = FALSE;
	  gtk_editable_delete_selection (editable);

	  tmp_pos = editable->current_pos;
	  gtk_editable_insert_text (editable, event->string, event->length, &tmp_pos);
	  editable->current_pos = tmp_pos;

	  return_val = TRUE;
	}
      break;
    }

  /* since we emit signals from within the above code,
   * the widget might already be destroyed or at least
   * unrealized.
   */
  if (GTK_WIDGET_REALIZED (editable) &&
      return_val && (editable->current_pos != initial_pos))
    {
      if (extend_selection)
	{
	  if (editable->current_pos < editable->selection_start_pos)
	    editable->selection_start_pos = editable->current_pos;
	  else if (editable->current_pos > editable->selection_end_pos)
	    editable->selection_end_pos = editable->current_pos;
	  else
	    {
	      if (extend_start)
		editable->selection_start_pos = editable->current_pos;
	      else
		editable->selection_end_pos = editable->current_pos;
	    }
	}
      else
	{
	  editable->selection_start_pos = 0;
	  editable->selection_end_pos = 0;
	}

      gtk_editable_claim_selection (editable,
				    editable->selection_start_pos != editable->selection_end_pos,
				    event->time);
      
      entry_adjust_scroll (entry);
      gtk_entry_queue_draw (entry);
    }

  return return_val;
}


static void
gtk_entry_make_backing_pixmap (GtkEntry *entry, gint width, gint height)
{
  gint pixmap_width, pixmap_height;

  if (!entry->backing_pixmap)
    {
      /* allocate */
      entry->backing_pixmap = gdk_pixmap_new (entry->text_area,
					      width, height,
					      -1);
    }
  else
    {
      /* reallocate if sizes don't match */
      gdk_window_get_size (entry->backing_pixmap,
			   &pixmap_width, &pixmap_height);
      if ((pixmap_width != width) || (pixmap_height != height))
	{
	  gdk_pixmap_unref (entry->backing_pixmap);
	  entry->backing_pixmap = gdk_pixmap_new (entry->text_area,
						  width, height,
						  -1);
	}
    }
}

static void
gtk_entry_draw_text (GtkEntry *entry)
{
  GtkWidget *widget;
  GtkItemEntry *item_entry;
  GtkEditable *editable;
  GtkStateType selected_state;
  gint start_pos;
  gint end_pos;
  gint start_xoffset;
  gint selection_start_pos;
  gint selection_end_pos;
  gint selection_start_xoffset;
  gint selection_end_xoffset;
  gint width, height;
  gint y;
  GdkDrawable *drawable;
  gint use_backing_pixmap;
  GdkWChar *stars;
  GdkWChar *toprint;

  g_return_if_fail (entry != NULL);
  g_return_if_fail (GTK_IS_ENTRY (entry));
  g_return_if_fail (GTK_IS_ITEM_ENTRY (entry));

  item_entry = GTK_ITEM_ENTRY (entry);

  if (entry->timer)
    {
      gtk_timeout_remove (entry->timer);
      entry->timer = 0;
    }

  if (GTK_WIDGET_DRAWABLE (entry))
    {
      widget = GTK_WIDGET (entry);
      editable = GTK_EDITABLE (entry);

      gdk_window_get_size (entry->text_area, &width, &height);
      if (!entry->text)
	{	  
	  gdk_draw_rectangle (entry->text_area,
			      item_entry->bg_gc,
			      TRUE,
			      0, 0,
			      width,
			      height);

	  if (editable->editable)
	    gtk_entry_draw_cursor (entry);
	  return;
	}


      /*
	If the widget has focus, draw on a backing pixmap to avoid flickering
	and copy it to the text_area.
	Otherwise draw to text_area directly for better speed.
      */
      use_backing_pixmap = GTK_WIDGET_HAS_FOCUS (widget) && (entry->text != NULL);

      if (use_backing_pixmap)
	{
	  gtk_entry_make_backing_pixmap (entry, width, height);
	  drawable = entry->backing_pixmap;
	  gdk_draw_rectangle (drawable,
			      item_entry->bg_gc,
			      TRUE,
			      0, 0,
			      width,
			      height);
	}
      else
	{
	   drawable = entry->text_area;
	   gdk_draw_rectangle (entry->text_area,
			       item_entry->bg_gc,
			       TRUE,
			       0, 0,
			       width,
			       height);

        }      


/* CENTER */
/*      y = height; 
      y = (height - (widget->style->font->ascent + widget->style->font->descent)) / 2;
      y += widget->style->font->ascent;
*/
/* BOTTOM */
      y = height;
      y = (y - widget->style->font->ascent - widget->style->font->descent);
      y += widget->style->font->ascent;

      start_pos = gtk_entry_find_position (entry, entry->scroll_offset);
      start_xoffset = entry->char_offset[start_pos] - entry->scroll_offset;

      end_pos = gtk_entry_find_position (entry, entry->scroll_offset + width);
      if (end_pos < entry->text_length)
	end_pos += 1;

      selected_state = GTK_STATE_SELECTED;
      if (!editable->has_selection)
	selected_state = GTK_STATE_ACTIVE;

      selection_start_pos = MIN (editable->selection_start_pos, editable->selection_end_pos);
      selection_end_pos = MAX (editable->selection_start_pos, editable->selection_end_pos);
      
      selection_start_pos = CLAMP (selection_start_pos, start_pos, end_pos);
      selection_end_pos = CLAMP (selection_end_pos, start_pos, end_pos);

      selection_start_xoffset = 
	entry->char_offset[selection_start_pos] - entry->scroll_offset;
      selection_end_xoffset = 
	entry->char_offset[selection_end_pos] -entry->scroll_offset;

      /* if entry->visible, print a bunch of stars.  If not, print the standard text. */
      if (entry->visible)
	{
	  toprint = entry->text + start_pos;
	}
      else
	{
	  gint i;
	  
	  stars = g_new (GdkWChar, end_pos - start_pos);
	  for (i = 0; i < end_pos - start_pos; i++)
	    stars[i] = '*';
	  toprint = stars;
	}

      if (selection_start_pos > start_pos)
	gdk_draw_text_wc (drawable, widget->style->font,
			  item_entry->fg_gc,
			  start_xoffset, y,
			  toprint,
			  selection_start_pos - start_pos);

      if ((selection_end_pos >= start_pos) && 
	  (selection_start_pos < end_pos) &&
	  (selection_start_pos != selection_end_pos))
	 {
	    gint yoffset = 
	    (height - 
	     (widget->style->font->ascent + widget->style->font->descent));
	    gtk_paint_flat_box (widget->style, drawable, 
				selected_state, GTK_SHADOW_NONE,
				NULL, widget, "text",
				selection_start_xoffset,
				yoffset,
				selection_end_xoffset - selection_start_xoffset,
				widget->style->font->ascent + widget->style->font->descent);
	    gdk_draw_text_wc (drawable, widget->style->font,
			      widget->style->fg_gc[selected_state],
			      selection_start_xoffset, y,
			      toprint + selection_start_pos - start_pos,
			      selection_end_pos - selection_start_pos);
   }

       if (selection_end_pos < end_pos)
	 gdk_draw_text_wc (drawable, widget->style->font,
			   item_entry->fg_gc,
			   selection_end_xoffset, y,
			   toprint + selection_end_pos - start_pos,
			   end_pos - selection_end_pos);
       /* free the space allocated for the stars if it's neccessary. */
      if (!entry->visible)
	g_free (toprint);

      if (editable->editable)
	gtk_entry_draw_cursor_on_drawable (entry, drawable);

      if (use_backing_pixmap)
	gdk_draw_pixmap(entry->text_area,
			item_entry->fg_gc,
			entry->backing_pixmap,
			0, 0, 0, 0, width, height);	  
    }


}

static void
gtk_entry_draw_cursor (GtkEntry *entry)
{
  g_return_if_fail (entry != NULL);
  g_return_if_fail (GTK_IS_ENTRY (entry));

  gtk_entry_draw_cursor_on_drawable (entry, entry->text_area);
}

static void
gtk_entry_draw_cursor_on_drawable (GtkEntry *entry, GdkDrawable *drawable)
{
  GtkWidget *widget;
  GtkEditable *editable;
  gint xoffset;
  gint text_area_height;

  g_return_if_fail (entry != NULL);
  g_return_if_fail (GTK_IS_ENTRY (entry));
  g_return_if_fail (GTK_IS_ITEM_ENTRY (entry));

  if (GTK_WIDGET_DRAWABLE (entry))
    {
      widget = GTK_WIDGET (entry);
      editable = GTK_EDITABLE (entry);

      xoffset = entry->char_offset[editable->current_pos];
      xoffset -= entry->scroll_offset;

      gdk_window_get_size (entry->text_area, NULL, &text_area_height);

      if (GTK_WIDGET_HAS_FOCUS (widget) &&
	  (editable->selection_start_pos == editable->selection_end_pos))
	{
	  gint yoffset = 
	    (text_area_height -
	     (widget->style->font->ascent + widget->style->font->descent));

	  gdk_draw_line (drawable, widget->style->fg_gc[GTK_STATE_NORMAL], 
			 xoffset, yoffset,
			 xoffset, text_area_height);
	}
      else
	{
	  gint yoffset = 
	    (text_area_height - 
	     (widget->style->font->ascent + widget->style->font->descent));

	  gtk_paint_flat_box (widget->style, drawable,
			      GTK_WIDGET_STATE(widget), GTK_SHADOW_NONE,
			      NULL, widget, "entry_bg", 
			      xoffset, yoffset, 
			      1, text_area_height);
	  /* Draw the character under the cursor again */
/*
	  gdk_draw_text_wc (drawable, widget->style->font,
			    widget->style->fg_gc[GTK_WIDGET_STATE (widget)],
			    xoffset, yoffset,
			    entry->text + editable->current_pos, 1);
*/
    }


#ifdef USE_XIM
      if (GTK_WIDGET_HAS_FOCUS(widget) && gdk_im_ready() && editable->ic && 
	  (gdk_ic_get_style (editable->ic) & GDK_IM_PREEDIT_POSITION))
	{
	  editable->ic_attr->spot_location.x = xoffset;
	  editable->ic_attr->spot_location.y =
	    (text_area_height + (widget->style->font->ascent
	        - widget->style->font->descent) + 1) / 2;

	  gdk_ic_set_attr (editable->ic,
	      		   editable->ic_attr, GDK_IC_SPOT_LOCATION);
	}
#endif 
    }
}

static void
gtk_entry_queue_draw (GtkEntry *entry)
{
  g_return_if_fail (entry != NULL);
  g_return_if_fail (GTK_IS_ENTRY (entry));
  g_return_if_fail (GTK_IS_ITEM_ENTRY (entry));

  if (!entry->timer)
    entry->timer = gtk_timeout_add (DRAW_TIMEOUT, gtk_entry_timer, entry);
}

static gint
gtk_entry_timer (gpointer data)
{
  GtkEntry *entry;

  GDK_THREADS_ENTER ();

  entry = GTK_ENTRY (data);
  entry->timer = 0;
  gtk_entry_draw_text (entry);

  GDK_THREADS_LEAVE ();

  return FALSE;
}

static gint
gtk_entry_find_position (GtkEntry *entry,
			 gint      x)
{
  gint start = 0;
  gint end = entry->text_length;
  gint half;

  if (x <= 0)
    return 0;
  if (x >= entry->char_offset[end])
    return end;
  
  /* invariant - char_offset[start] <= x < char_offset[end] */

  while (start != end)
    {
      half = (start+end)/2;
      if (half == start)
	return half;
      else if (entry->char_offset[half] <= x)
	start = half;
      else
	end = half;
    }

  return start;
}

static gint
gtk_entry_position (GtkEntry *entry,
		    gint      x)
{
  return gtk_entry_find_position(entry, x);
}


static void
entry_adjust_scroll (GtkEntry *entry)
{
  GtkItemEntry *item_entry;
  gint xoffset;
  gint text_area_width, text_area_height;
  gint char_width, text_width;

  g_return_if_fail (entry != NULL);
  g_return_if_fail (GTK_IS_ENTRY (entry));
  g_return_if_fail (GTK_IS_ITEM_ENTRY (entry));

  item_entry = GTK_ITEM_ENTRY (entry);

  if (!entry->text_area)
    return;

  gdk_window_get_size (entry->text_area, &text_area_width, &text_area_height);

  char_width = gdk_char_width (GTK_WIDGET(item_entry)->style->font,(gchar)'X');

  entry->scroll_offset = 0 ;

  switch(item_entry->justification){

   case GTK_JUSTIFY_FILL:
   case GTK_JUSTIFY_LEFT:

/* LEFT JUSTIFICATION */

    if (GTK_EDITABLE(entry)->current_pos > 0)
      xoffset = gdk_text_width (GTK_WIDGET (entry)->style->font, gtk_entry_get_text(entry), GTK_EDITABLE(entry)->current_pos);
    else
      xoffset = 0;

    xoffset -= entry->scroll_offset;
    if (xoffset < 0)
      entry->scroll_offset += xoffset;
    else if (xoffset > text_area_width){
      if(item_entry->text_max_size != 0 &&
         text_area_width + char_width <= item_entry->text_max_size){

       text_width = gdk_text_width (GTK_WIDGET (item_entry)->style->font, 
                  gtk_entry_get_text(entry), strlen(gtk_entry_get_text(entry)));
       GTK_WIDGET(item_entry)->allocation.width = text_width + 
                                                  2 * INNER_BORDER + 1;
       gtk_entry_size_allocate(GTK_WIDGET(item_entry),
                          (GtkAllocation *)(&GTK_WIDGET(item_entry)->allocation));
       gtk_widget_queue_draw(GTK_WIDGET(item_entry));
      }else{
       entry->scroll_offset += (xoffset - text_area_width) + 1;
      }
    }

    break;

   case GTK_JUSTIFY_RIGHT:

/* RIGHT JUSTIFICATION FOR NUMBERS */
    if(entry->text){

     text_width = gdk_text_width (GTK_WIDGET (item_entry)->style->font, gtk_entry_get_text(entry), strlen(gtk_entry_get_text(entry)));
 
     entry->scroll_offset=  -(text_area_width - text_width) + 1;
     if(entry->scroll_offset > 0){
      if(item_entry->text_max_size != 0 &&
         text_area_width + char_width <= item_entry->text_max_size){
       GTK_WIDGET(item_entry)->allocation.x = 
                                       GTK_WIDGET(item_entry)->allocation.x+
                                       GTK_WIDGET(item_entry)->allocation.width-
                                       (text_area_width+char_width);
       GTK_WIDGET(item_entry)->allocation.width = text_area_width + char_width;
       gtk_entry_size_allocate(GTK_WIDGET(item_entry),
                          (GtkAllocation *)(&GTK_WIDGET(item_entry)->allocation));
       gtk_widget_queue_draw(GTK_WIDGET(item_entry));
      }
      else
      {
       entry->scroll_offset= -(text_area_width  
 - gdk_text_width (GTK_WIDGET (item_entry)->style->font, gtk_entry_get_text(entry), GTK_EDITABLE(entry)->current_pos)) + 1;
       if(entry->scroll_offset < 0) entry->scroll_offset = 0;
      }
     }
    }
    else
     entry->scroll_offset=0;
  
    break;
   case GTK_JUSTIFY_CENTER:

    if(entry->text){

     text_width = gdk_text_width (GTK_WIDGET (item_entry)->style->font, gtk_entry_get_text(entry), strlen(gtk_entry_get_text(entry)));
 
     entry->scroll_offset=  -(text_area_width - text_width)/2;
     if(entry->scroll_offset > 0){
      if(item_entry->text_max_size != 0 &&
                      text_area_width+char_width<=item_entry->text_max_size){
       GTK_WIDGET(item_entry)->allocation.x=GTK_WIDGET(item_entry)->allocation.x+
                                       GTK_WIDGET(item_entry)->allocation.width/2-
                                       (text_area_width+char_width)/2;
       GTK_WIDGET(item_entry)->allocation.width=text_area_width+char_width;
       gtk_entry_size_allocate(GTK_WIDGET(item_entry),
                          (GtkAllocation *)(&GTK_WIDGET(item_entry)->allocation));
       gtk_widget_queue_draw(GTK_WIDGET(item_entry));
      }
      else
      {
       entry->scroll_offset= -(text_area_width  
 - gdk_text_width (GTK_WIDGET (item_entry)->style->font, gtk_entry_get_text(entry), GTK_EDITABLE(entry)->current_pos)) + 1;
       if(entry->scroll_offset < 0) entry->scroll_offset = 0;
      }
     }
    }
    else
     entry->scroll_offset=0;
  
    break;

   }

}

static void
gtk_entry_grow_text (GtkEntry *entry)
{
  gint previous_size;
  gint i;

  g_return_if_fail (entry != NULL);
  g_return_if_fail (GTK_IS_ENTRY (entry));
  g_return_if_fail (GTK_IS_ITEM_ENTRY (entry));

  previous_size = entry->text_size;
  if (!entry->text_size)
    entry->text_size = 128;
  else
    entry->text_size *= 2;
  entry->text = g_realloc (entry->text, entry->text_size * sizeof(GdkWChar));
  entry->char_offset = g_realloc (entry->char_offset, 
				  entry->text_size * sizeof(guint));

  if (entry->text_length == 0)	/* initial allocation */
    {
      entry->char_offset[0] = 0;
    }

  for (i = previous_size; i < entry->text_size; i++)
    entry->text[i] = '\0';
}

static void
gtk_entry_insert_text (GtkEditable *editable,
		       const gchar *new_text,
		       gint         new_text_length,
		       gint        *position)
{
  GdkWChar *text;
  gint start_pos;
  gint end_pos;
  gint last_pos;
  gint max_length;
  gint i;

  guchar *new_text_nt;
  gint insertion_length;
  GdkWChar *insertion_text;
  
  GtkEntry *entry;
  GtkWidget *widget;
  
  g_return_if_fail (editable != NULL);
  g_return_if_fail (GTK_IS_ENTRY (editable));

  entry = GTK_ENTRY (editable);
  widget = GTK_WIDGET (editable);

  if ((entry->text_length == 0) && (entry->use_wchar == FALSE))
    {
      if (!GTK_WIDGET_REALIZED (widget))
	gtk_widget_ensure_style (widget);
      if ((widget->style) && (widget->style->font->type == GDK_FONT_FONTSET))
	entry->use_wchar = TRUE;
    }

  if (new_text_length < 0)
    {
      new_text_nt = (gchar *)new_text;
      new_text_length = strlen (new_text);
      if (new_text_length <= 0) return;
    }
  else if (new_text_length == 0)
    {
      return;
    }
  else
    {
      /* make a null-terminated copy of new_text */
      new_text_nt = g_new (gchar, new_text_length + 1);
      memcpy (new_text_nt, new_text, new_text_length);
      new_text_nt[new_text_length] = 0;
    }
    
  /* The algorithms here will work as long as, the text size (a
   * multiple of 2), fits into a guint16 but we specify a shorter
   * maximum length so that if the user pastes a very long text, there
   * is not a long hang from the slow X_LOCALE functions.  */
 
  if (entry->text_max_length == 0)
    max_length = 2047;
  else
    max_length = MIN (2047, entry->text_max_length);

  /* Convert to wide characters */
  insertion_text = g_new (GdkWChar, new_text_length);
  if (entry->use_wchar)
    insertion_length = gdk_mbstowcs (insertion_text, new_text_nt,
				     new_text_length);
  else
    for (insertion_length=0; new_text_nt[insertion_length]; insertion_length++)
      insertion_text[insertion_length] = new_text_nt[insertion_length];
  if (new_text_nt != (guchar *)new_text)
    g_free (new_text_nt);

  /* Make sure we do not exceed the maximum size of the entry. */
  if (insertion_length + entry->text_length > max_length)
    insertion_length = max_length - entry->text_length;

  /* Don't insert anything, if there was nothing to insert. */
  if (insertion_length <= 0)
    {
      g_free(insertion_text);
      return;
    }

  /* Make sure we are inserting at integral character position */
  start_pos = *position;
  if (start_pos < 0)
    start_pos = 0;
  else if (start_pos > entry->text_length)
    start_pos = entry->text_length;

  end_pos = start_pos + insertion_length;
  last_pos = insertion_length + entry->text_length;

  if (editable->selection_start_pos >= *position)
    editable->selection_start_pos += insertion_length;
  if (editable->selection_end_pos >= *position)
    editable->selection_end_pos += insertion_length;

  while (last_pos >= entry->text_size)
    gtk_entry_grow_text (entry);

  text = entry->text;
  for (i = last_pos - 1; i >= end_pos; i--)
    text[i] = text[i- (end_pos - start_pos)];
  for (i = start_pos; i < end_pos; i++)
    text[i] = insertion_text[i - start_pos];
  g_free (insertion_text);

  /* Fix up the the character offsets */
  
  if (GTK_WIDGET_REALIZED (entry))
    {
      gint offset = 0;
      
      for (i = last_pos; i >= end_pos; i--)
	entry->char_offset[i] 
	  = entry->char_offset[i - insertion_length];
      
      for (i=start_pos; i<end_pos; i++)
	{
	  entry->char_offset[i] = entry->char_offset[start_pos] + offset;
	  if (entry->visible)
	    {
	      offset += gdk_char_width_wc (GTK_WIDGET (entry)->style->font,
					   entry->text[i]);
	    }
	  else
	    {
	      offset += gdk_char_width (GTK_WIDGET (entry)->style->font, '*');
	    }
	}
      for (i = end_pos; i <= last_pos; i++)
	entry->char_offset[i] += offset;
    }

  entry->text_length += insertion_length;
  *position = end_pos;

  entry->text_mb_dirty = 1;
  gtk_entry_queue_draw (entry);
}

static void
gtk_entry_delete_text (GtkEditable *editable,
		       gint         start_pos,
		       gint         end_pos)
{
  GdkWChar *text;
  gint deletion_length;
  gint i;

  GtkEntry *entry;
  
  g_return_if_fail (editable != NULL);
  g_return_if_fail (GTK_IS_ENTRY (editable));

  entry = GTK_ENTRY (editable);

  if (end_pos < 0)
    end_pos = entry->text_length;

  if (editable->selection_start_pos > start_pos)
    editable->selection_start_pos -= MIN(end_pos, editable->selection_start_pos) - start_pos;
  if (editable->selection_end_pos > start_pos)
    editable->selection_end_pos -= MIN(end_pos, editable->selection_end_pos) - start_pos;
  
  if ((start_pos < end_pos) &&
      (start_pos >= 0) &&
      (end_pos <= entry->text_length))
    {
      text = entry->text;
      deletion_length = end_pos - start_pos;

      /* Fix up the character offsets */
      if (GTK_WIDGET_REALIZED (entry))
	{
	  gint deletion_width = 
	    entry->char_offset[end_pos] - entry->char_offset[start_pos];

	  for (i = 0 ; i <= entry->text_length - end_pos; i++)
	    entry->char_offset[start_pos+i] = entry->char_offset[end_pos+i] - deletion_width;
	}

      for (i = end_pos; i < entry->text_length; i++)
        text[i - deletion_length] = text[i];

      for (i = entry->text_length - deletion_length; i < entry->text_length; i++)
        text[i] = '\0';

      entry->text_length -= deletion_length;
      editable->current_pos = start_pos;
    }

  entry->text_mb_dirty = 1;
  gtk_entry_queue_draw (entry);
}

static void
gtk_entry_update_text (GtkEditable *editable,
		       gint         start_pos,
		       gint         end_pos)
{
  gtk_entry_queue_draw (GTK_ENTRY(editable));
}

static gchar *    
gtk_entry_get_chars      (GtkEditable   *editable,
			  gint           start_pos,
			  gint           end_pos)
{
  GtkEntry *entry;
  
  g_return_val_if_fail (editable != NULL, NULL);
  g_return_val_if_fail (GTK_IS_ENTRY (editable), NULL);

  entry = GTK_ENTRY (editable);

  if (end_pos < 0)
    end_pos = entry->text_length;

  start_pos = MIN(entry->text_length, start_pos);
  end_pos = MIN(entry->text_length, end_pos);

  if (start_pos <= end_pos)
    {
      guchar *mbstr;
      if (entry->use_wchar)
	{
	  GdkWChar ch;
	  if (end_pos >= entry->text_size)
	    gtk_entry_grow_text(entry);
	  ch = entry->text[end_pos];
	  entry->text[end_pos] = 0;
	  mbstr = gdk_wcstombs (entry->text + start_pos);
	  entry->text[end_pos] = ch;
	  return (gchar *)mbstr;
	}
      else
	{
	  gint i;
	  mbstr = g_new (gchar, end_pos - start_pos + 1);
	  for (i=0; i<end_pos-start_pos; i++)
	    mbstr[i] = entry->text[start_pos + i];
	  mbstr[i] = 0;
	  return (gchar *)mbstr;
	}
    }
  else
    return NULL;
}

static void 
gtk_entry_move_cursor (GtkEditable *editable,
		       gint         x,
		       gint         y)
{
  GtkEntry *entry;
  entry = GTK_ENTRY (editable);

  /* Horizontal motion */
  if ((gint)editable->current_pos < -x)
    editable->current_pos = 0;
  else if (editable->current_pos + x > entry->text_length)
    editable->current_pos = entry->text_length;
  else
    editable->current_pos += x;

  /* Ignore vertical motion */
}

static void
gtk_move_forward_character (GtkEntry *entry)
{
  gtk_entry_move_cursor (GTK_EDITABLE (entry), 1, 0);
}

static void
gtk_move_backward_character (GtkEntry *entry)
{
  gtk_entry_move_cursor (GTK_EDITABLE (entry), -1, 0);
}

static void 
gtk_entry_move_word (GtkEditable *editable,
		     gint         n)
{
  while (n-- > 0)
    gtk_move_forward_word (GTK_ENTRY (editable));
  while (n++ < 0)
    gtk_move_backward_word (GTK_ENTRY (editable));
}

static void
gtk_move_forward_word (GtkEntry *entry)
{
  GtkEditable *editable;
  GdkWChar *text;
  gint i;

  editable = GTK_EDITABLE (entry);

  if (entry->text && (editable->current_pos < entry->text_length))
    {
      text = entry->text;
      i = editable->current_pos;
	  
      if ((entry->use_wchar) ? (!gdk_iswalnum (text[i])) : (!isalnum (text[i])))
	for (; i < entry->text_length; i++)
	  {
	    if ((entry->use_wchar) ? gdk_iswalnum (text[i]) : isalnum (text[i]))
	      break;
	  }

      for (; i < entry->text_length; i++)
	{
	  if ((entry->use_wchar) ? (!gdk_iswalnum (text[i])) : (!isalnum (text[i])))
	    break;
	}

      editable->current_pos = i;
    }
}

static void
gtk_move_backward_word (GtkEntry *entry)
{
  GtkEditable *editable;
  GdkWChar *text;
  gint i;

  editable = GTK_EDITABLE (entry);

  if (entry->text && editable->current_pos > 0)
    {
      text = entry->text;
      i = editable->current_pos - 1;
      if ((entry->use_wchar) ? (!gdk_iswalnum (text[i])) : (!isalnum (text[i])))
	for (; i >= 0; i--)
	  {
	    if ((entry->use_wchar) ? gdk_iswalnum (text[i]) : isalnum (text[i]))
	      break;
	  }
      for (; i >= 0; i--)
	{
	  if ((entry->use_wchar) ? (!gdk_iswalnum (text[i])) : (!isalnum (text[i])))
	    {
	      i++;
	      break;
	    }
	}
	  
      if (i < 0)
	i = 0;
	  
      editable->current_pos = i;
    }
}

static void
gtk_entry_move_to_column (GtkEditable *editable, gint column)
{
  GtkEntry *entry;

  entry = GTK_ENTRY (editable);
  
  if (column < 0 || column > entry->text_length)
    editable->current_pos = entry->text_length;
  else
    editable->current_pos = column;
}

static void
gtk_move_beginning_of_line (GtkEntry *entry)
{
  gtk_entry_move_to_column (GTK_EDITABLE (entry), 0);
}

static void
gtk_move_end_of_line (GtkEntry *entry)
{
  gtk_entry_move_to_column (GTK_EDITABLE (entry), -1);
}

static void
gtk_entry_kill_char (GtkEditable *editable,
		     gint         direction)
{
  if (editable->selection_start_pos != editable->selection_end_pos)
    gtk_editable_delete_selection (editable);
  else
    {
      gint old_pos = editable->current_pos;
      if (direction >= 0)
	{
	  gtk_entry_move_cursor (editable, 1, 0);
	  gtk_editable_delete_text (editable, old_pos, editable->current_pos);
	}
      else
	{
	  gtk_entry_move_cursor (editable, -1, 0);
	  gtk_editable_delete_text (editable, editable->current_pos, old_pos);
	}
    }
}

static void
gtk_delete_forward_character (GtkEntry *entry)
{
  gtk_entry_kill_char (GTK_EDITABLE (entry), 1);
}

static void
gtk_delete_backward_character (GtkEntry *entry)
{
  gtk_entry_kill_char (GTK_EDITABLE (entry), -1);
}

static void
gtk_entry_kill_word (GtkEditable *editable,
		     gint         direction)
{
  if (editable->selection_start_pos != editable->selection_end_pos)
    gtk_editable_delete_selection (editable);
  else
    {
      gint old_pos = editable->current_pos;
      if (direction >= 0)
	{
	  gtk_entry_move_word (editable, 1);
	  gtk_editable_delete_text (editable, old_pos, editable->current_pos);
	}
      else
	{
	  gtk_entry_move_word (editable, -1);
	  gtk_editable_delete_text (editable, editable->current_pos, old_pos);
	}
    }
}

static void
gtk_delete_forward_word (GtkEntry *entry)
{
  gtk_entry_kill_word (GTK_EDITABLE (entry), 1);
}

static void
gtk_delete_backward_word (GtkEntry *entry)
{
  gtk_entry_kill_word (GTK_EDITABLE (entry), -1);
}

static void
gtk_entry_kill_line (GtkEditable *editable,
		     gint         direction)
{
  gint old_pos = editable->current_pos;
  if (direction >= 0)
    {
      gtk_entry_move_to_column (editable, -1);
      gtk_editable_delete_text (editable, old_pos, editable->current_pos);
    }
  else
    {
      gtk_entry_move_to_column (editable, 0);
      gtk_editable_delete_text (editable, editable->current_pos, old_pos);
    }
}

static void
gtk_delete_line (GtkEntry *entry)
{
  gtk_entry_move_to_column (GTK_EDITABLE (entry), 0);
  gtk_entry_kill_line (GTK_EDITABLE (entry), 1);
}

static void
gtk_delete_to_line_end (GtkEntry *entry)
{
  gtk_editable_delete_text (GTK_EDITABLE(entry), GTK_EDITABLE(entry)->current_pos, entry->text_length);
}

static void
gtk_select_word (GtkEntry *entry,
		 guint32   time)
{
  GtkEditable *editable;
  gint start_pos;
  gint end_pos;

  editable = GTK_EDITABLE (entry);

  gtk_move_backward_word (entry);
  start_pos = editable->current_pos;

  gtk_move_forward_word (entry);
  end_pos = editable->current_pos;

  editable->has_selection = TRUE;
  gtk_entry_set_selection (editable, start_pos, end_pos);
  gtk_editable_claim_selection (editable, start_pos != end_pos, time);
}

static void
gtk_select_line (GtkEntry *entry,
		 guint32   time)
{
  GtkEditable *editable;

  editable = GTK_EDITABLE (entry);

  editable->has_selection = TRUE;
  gtk_entry_set_selection (editable, 0, entry->text_length);
  gtk_editable_claim_selection (editable, entry->text_length != 0, time);

  editable->current_pos = editable->selection_end_pos;
}

static void 
gtk_entry_set_selection (GtkEditable       *editable,
			 gint               start,
			 gint               end)
{
  g_return_if_fail (editable != NULL);
  g_return_if_fail (GTK_IS_ENTRY (editable));

  if (end < 0)
    end = GTK_ENTRY (editable)->text_length;
  
  editable->selection_start_pos = start;
  editable->selection_end_pos = end;

  gtk_entry_queue_draw (GTK_ENTRY (editable));
}

#ifdef USE_XIM
static void
gtk_entry_update_ic_attr (GtkWidget *widget)
{
  GtkEditable *editable = (GtkEditable *) widget;
  GdkICAttributesType mask = 0;

  gdk_ic_get_attr (editable->ic, editable->ic_attr,
		   GDK_IC_PREEDIT_FOREGROUND |
		   GDK_IC_PREEDIT_BACKGROUND |
		   GDK_IC_PREEDIT_FONTSET);

  if (editable->ic_attr->preedit_foreground.pixel != 
      widget->style->fg[GTK_STATE_NORMAL].pixel)
    {
      mask |= GDK_IC_PREEDIT_FOREGROUND;
      editable->ic_attr->preedit_foreground
	= widget->style->fg[GTK_STATE_NORMAL];
    }
  if (editable->ic_attr->preedit_background.pixel != 
      widget->style->base[GTK_STATE_NORMAL].pixel)
    {
      mask |= GDK_IC_PREEDIT_BACKGROUND;
      editable->ic_attr->preedit_background
	= widget->style->base[GTK_STATE_NORMAL];
    }
  if ((gdk_ic_get_style (editable->ic) & GDK_IM_PREEDIT_POSITION) && 
      !gdk_font_equal (editable->ic_attr->preedit_fontset,
		       widget->style->font))
    {
      mask |= GDK_IC_PREEDIT_FONTSET;
      editable->ic_attr->preedit_fontset = widget->style->font;
    }
  
  if (mask)
    gdk_ic_set_attr (editable->ic, editable->ic_attr, mask);
}
#endif /* USE_XIM */
    			  
