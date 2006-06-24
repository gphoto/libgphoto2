/* ptp-private.h
 *
 * Copyright (C) 2006 Marcus Meissner <marcus@jet.franken.de>
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

/* ptp2 camlib private functions */

/* config.c */
int camera_get_config (Camera *camera, CameraWidget **window, GPContext *context);
int camera_set_config (Camera *camera, CameraWidget *window, GPContext *context);

/* library.c */
void report_result (GPContext *context, short result, short vendor);
int translate_ptp_result (short result);

struct _CameraPrivateLibrary {
	PTPParams params;
	unsigned long bugs;
};

