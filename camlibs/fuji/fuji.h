/* fuji.h
 *
 * Copyright (C) 2002 Lutz Müller <lutz@users.sourceforge.net>
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

#ifndef __FUJI_H__
#define __FUJI_H__

#include <gphoto2-context.h>
#include <gphoto2-camera.h>

typedef enum _FujiCmd FujiCmd;
enum _FujiCmd {
        FUJI_CMD_VERSION        = 0x09, /* Version Info */
        FUJI_CMD_SIZE           = 0x17, /* Get picture size */
        FUJI_CMD_DELETE         = 0x19, /* Delete picture */
        FUJI_CMD_TAKE           = 0x27, /* Take picture */
        FUJI_CMD_CHARGE_FLASH   = 0x34, /* Charge the flash */
        FUJI_CMD_CMDS_VALID     = 0x4C, /* Get list of supported commands */
        FUJI_CMD_PREVIEW        = 0x64 /* Get a preview */
};

int fuji_get_cmds  (Camera *camera, unsigned char *cmds, GPContext *context);

int fuji_ping      (Camera *camera, GPContext *context);
int fuji_pic_count (Camera *camera, GPContext *context);
int fuji_pic_name  (Camera *camera, unsigned int i, const char **name,
		    GPContext *context);

#endif /* __FUJI_H__ */
