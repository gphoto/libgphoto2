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
#ifndef __GTK_ITEM_ENTRY_H__
#define __GTK_ITEM_ENTRY_H__


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#define GTK_TYPE_ITEM_ENTRY            (gtk_item_entry_get_type ())
#define GTK_ITEM_ENTRY(obj)            (GTK_CHECK_CAST (obj, gtk_item_entry_get_type (), GtkItemEntry))
#define GTK_ITEM_ENTRY_CLASS(klass)    (GTK_CHECK_CLASS_CAST (klass, gtk_item_entry_get_type (), GtkItemEntryClass))
#define GTK_IS_ITEM_ENTRY(obj)         (GTK_CHECK_TYPE (obj, gtk_item_entry_get_type ()))
#define GTK_IS_ITEM_ENTRY_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), GTK_TYPE_ENTRY))


typedef struct _GtkItemEntry       GtkItemEntry;
typedef struct _GtkItemEntryClass  GtkItemEntryClass;

struct _GtkItemEntry
{
  GtkEntry parent;

  gint text_max_size;

  GdkGC *fg_gc;
  GdkGC *bg_gc;

  GtkJustification justification;
};

struct _GtkItemEntryClass
{
  GtkEntryClass parent_class;
};

GtkType    gtk_item_entry_get_type       (void);
GtkWidget* gtk_item_entry_new            (void);
GtkWidget* gtk_item_entry_new_with_max_length (guint16   max);
void       gtk_item_entry_set_text            (GtkItemEntry *item_entry, 
                                               const gchar *text,
                                               GtkJustification justification);
void       gtk_item_entry_set_justification (GtkItemEntry        *item_entry,
					     GtkJustification   justification);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_ITEM_ENTRY_H__ */
