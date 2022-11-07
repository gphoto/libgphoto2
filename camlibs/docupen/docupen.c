/* docupen.c
 * Docupen camera driver (camlib).
 *
 * Copyright 2020 Ondrej Zary <ondrej@zary.sk>
 * based on Docupen tools by Florian Heinz <fh@cronon-ag.de>
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
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */
#include "config.h"

#include <gphoto2/gphoto2-library.h>
#include <gphoto2/gphoto2-result.h>
#include <libgphoto2/gphoto2-endian.h>

#include "libgphoto2/i18n.h"

#include "docupen.h"


#if 0
static const struct {
	char *model;
	int usb_vendor;
	int usb_product;
} models[] = {
	{ "Planon DocuPen RC800", 0x18dd, 0x1000 },
	{ NULL, 0, 0 }
};
#endif

#define DP_ACK 0xD1
#define DP_CMD_LEN 8

static char cmd_query[]		= { 0x1f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1a };
static char cmd_inquiry[]	= { 0x7f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1a };
static char cmd_turnoff[]	= { 0x0f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1a };
static char cmd_del_all[]	= { 0x7a, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0x1a };
static char cmd_get_profile[]	= { 0x58, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x1a }; /* length = 0x400 */
static char cmd_set_profile[]	= { 0x59, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x1a }; /* length = 0x400 */

#define RAW_HEADER_SERIAL_OFFSET 0x4e
static char raw_header[] = {
/*  0 */ 0x53, 0x53, 0x08, 0x5f, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x01, 0x40, 0xc0, /* SS._..........@. */
/* 10 */ 0x12, 0x00, 0x20, 0x48, 0x88, 0x00, 0x00, 0x68, 0x1e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* .. H...h........ */
/* 20 */ 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* ................ */
/* 30 */ 0x0d, 0x0d, 0x0c, 0x0d, 0x0d, 0x0d, 0x0c, 0x0e, 0x67, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* ........g....... */
/* 40 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x31, 0x32, /* ..............12 */
/* 50 */ 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x50        /* 34567890......P  */
};

bool dp_cmd(GPPort *port, const char *cmd)
{
	unsigned char reply[64];

	if (gp_port_write(port, cmd, DP_CMD_LEN) != DP_CMD_LEN) {
		GP_LOG_E("command write failed");
		return false;
	}
	int ret = gp_port_read(port, (void *)reply, sizeof(reply));
	if (ret < 1 || reply[0] != DP_ACK) {
		GP_LOG_E("command failed: ret=%d reply[0]=%02hhx", ret, reply[0]);
		return false;
	}
	return true;
}

static bool inquiry_read(Camera *camera)
{
	if (gp_port_read(camera->port, (void *)&camera->pl->info, 4) != 4) {	/* read header */
		GP_LOG_E("error reading info header");
		return false;
	}
	/* FIXME: len is 8bit unsigned, so can be at most 255 ... sizeof(struct dp_info) is currently over 256
	 * so this is alwas false ... It is harmless and likely ok, but weird. -Marcus 20200831 */
	if (camera->pl->info.len > sizeof(struct dp_info)) {
		GP_LOG_E("camera info too long: %d bytes", camera->pl->info.len);
		return false;
	}
	int ret = gp_port_read(camera->port, (void *)&camera->pl->info + 4, camera->pl->info.len - 4);
	if (ret != camera->pl->info.len - 4) {
		GP_LOG_E("camera info length error: expected %d bytes, got %d", camera->pl->info.len - 4, ret);
		return false;
	}

	return true;
}


int camera_exit (Camera *camera, GPContext *context);
int
camera_exit (Camera *camera, GPContext *context)
{
	if (camera->pl) {
		if (camera->pl->cache)
			fclose(camera->pl->cache);
		free(camera->pl->cache_file);
		free(camera->pl->lut);
		free(camera->pl);
		camera->pl = NULL;
	}
	dp_cmd(camera->port, cmd_turnoff);
	return GP_OK;
}


static bool dp_get_profile(Camera *camera)
{
	if (!camera->pl->profile)
		camera->pl->profile = malloc(PROFILE_LEN);
	if (!camera->pl->profile)
		return false;

	dp_cmd(camera->port, cmd_get_profile);
	if (gp_port_read(camera->port, camera->pl->profile, PROFILE_LEN) != PROFILE_LEN)
		return false;

	return true;
}

static bool dp_set_profile(Camera *camera)
{
	if (!camera->pl->profile)
		return false;

	dp_cmd(camera->port, cmd_set_profile);
	if (gp_port_write(camera->port, camera->pl->profile, PROFILE_LEN) != PROFILE_LEN)
		return false;

	return true;
}


int camera_config_get (Camera *camera, CameraWidget **window, GPContext *context);
int
camera_config_get (Camera *camera, CameraWidget **window, GPContext *context)
{
	CameraWidget *section, *widget;

	if (!dp_get_profile(camera))
		return GP_ERROR;

	gp_widget_new (GP_WIDGET_WINDOW, _("Scanner Profile Configuration"), window);

	gp_widget_new (GP_WIDGET_SECTION, _("Mono mode"), &section);
	gp_widget_append (*window, section);

        gp_widget_new (GP_WIDGET_RADIO, _("Depth"), &widget);
        gp_widget_append (section, widget);
        gp_widget_add_choice (widget, _("Mono (b/w)"));
        gp_widget_add_choice (widget, _("Grey (4bpp)"));
        gp_widget_add_choice (widget, _("Grey (8bpp)"));
        switch(camera->pl->profile[0x80]) {
        case 0x00: gp_widget_set_value (widget, _("Mono (b/w)")); break;
        case 0x01: gp_widget_set_value (widget, _("Grey (4bpp)")); break;
        case 0x02: gp_widget_set_value (widget, _("Grey (8bpp)")); break;
        }

        gp_widget_new (GP_WIDGET_RADIO, _("Lo Resolution"), &widget);
        gp_widget_append (section, widget);
        gp_widget_add_choice (widget, _("100 DPI"));
        gp_widget_add_choice (widget, _("200 DPI"));
        switch(camera->pl->profile[0x81]) {
        case RES_100DPI: gp_widget_set_value (widget, _("100 DPI")); break;
        case RES_200DPI: gp_widget_set_value (widget, _("200 DPI")); break;
        }

        gp_widget_new (GP_WIDGET_RADIO, _("Hi Resolution"), &widget);
        gp_widget_append (section, widget);
        gp_widget_add_choice (widget, _("200 DPI"));
        gp_widget_add_choice (widget, _("400 DPI"));
        switch(camera->pl->profile[0x82]) {
        case RES_200DPI: gp_widget_set_value (widget, _("200 DPI")); break;
        case RES_400DPI: gp_widget_set_value (widget, _("400 DPI")); break;
        }

	gp_widget_new (GP_WIDGET_SECTION, _("Color Document mode"), &section);
	gp_widget_append (*window, section);

        gp_widget_new (GP_WIDGET_RADIO, _("Depth"), &widget);
        gp_widget_append (section, widget);
        gp_widget_add_choice (widget, _("NQ (12bpp)"));
        switch(camera->pl->profile[0x83]) {
        case 0x04: gp_widget_set_value (widget, _("NQ (12bpp)")); break;
        }

        gp_widget_new (GP_WIDGET_RADIO, _("Lo Resolution"), &widget);
        gp_widget_append (section, widget);
        gp_widget_add_choice (widget, _("100 DPI"));
        gp_widget_add_choice (widget, _("200 DPI"));
        switch(camera->pl->profile[0x84]) {
        case RES_100DPI: gp_widget_set_value (widget, _("100 DPI")); break;
        case RES_200DPI: gp_widget_set_value (widget, _("200 DPI")); break;
        }

        gp_widget_new (GP_WIDGET_RADIO, _("Hi Resolution"), &widget);
        gp_widget_append (section, widget);
        gp_widget_add_choice (widget, _("200 DPI"));
        gp_widget_add_choice (widget, _("400 DPI"));
        switch(camera->pl->profile[0x85]) {
        case RES_200DPI: gp_widget_set_value (widget, _("200 DPI")); break;
        case RES_400DPI: gp_widget_set_value (widget, _("400 DPI")); break;
        }

	gp_widget_new (GP_WIDGET_SECTION, _("Color Photo mode"), &section);
	gp_widget_append (*window, section);

        gp_widget_new (GP_WIDGET_RADIO, _("Depth"), &widget);
        gp_widget_append (section, widget);
        gp_widget_add_choice (widget, _("HQ (24bpp)"));
        switch(camera->pl->profile[0x86]) {
        case 0x08: gp_widget_set_value (widget, _("HQ (24bpp)")); break;
        }

        gp_widget_new (GP_WIDGET_RADIO, _("Lo Resolution"), &widget);
        gp_widget_append (section, widget);
        gp_widget_add_choice (widget, _("100 DPI"));
        gp_widget_add_choice (widget, _("200 DPI"));
        switch(camera->pl->profile[0x87]) {
        case RES_100DPI: gp_widget_set_value (widget, _("100 DPI")); break;
        case RES_200DPI: gp_widget_set_value (widget, _("200 DPI")); break;
        }

        gp_widget_new (GP_WIDGET_RADIO, _("Hi Resolution"), &widget);
        gp_widget_append (section, widget);
        gp_widget_add_choice (widget, _("200 DPI"));
        gp_widget_add_choice (widget, _("400 DPI"));
        switch(camera->pl->profile[0x88]) {
        case RES_200DPI: gp_widget_set_value (widget, _("200 DPI")); break;
        case RES_400DPI: gp_widget_set_value (widget, _("400 DPI")); break;
        }

	return GP_OK;
}


int camera_config_set (Camera *camera, CameraWidget *window, GPContext *context);
int
camera_config_set (Camera *camera, CameraWidget *window, GPContext *context)
{
	CameraWidget *section, *widget;
	char *value;

	gp_widget_get_child_by_label (window, _("Mono mode"), &section);
	gp_widget_get_child_by_label (section, _("Depth"), &widget);
	if (gp_widget_changed (widget)) {
		gp_widget_get_value (widget, &value);
		gp_widget_set_changed (widget, 0);
		if (!strcmp(value, _("Mono (b/w)")))
			camera->pl->profile[0x80] = 0x00;
		else if (!strcmp(value, _("Grey (4bpp)")))
			camera->pl->profile[0x80] = 0x01;
		else if (!strcmp(value, _("Grey (8bpp)")))
			camera->pl->profile[0x80] = 0x02;
	}

	gp_widget_get_child_by_label (section, _("Lo Resolution"), &widget);
	if (gp_widget_changed (widget)) {
		gp_widget_get_value (widget, &value);
		gp_widget_set_changed (widget, 0);
		if (!strcmp(value, _("100 DPI")))
			camera->pl->profile[0x81] = RES_100DPI;
		else if (!strcmp(value, _("200 DPI")))
			camera->pl->profile[0x81] = RES_200DPI;
	}

	gp_widget_get_child_by_label (section, _("Hi Resolution"), &widget);
	if (gp_widget_changed (widget)) {
		gp_widget_get_value (widget, &value);
		gp_widget_set_changed (widget, 0);
		if (!strcmp(value, _("200 DPI")))
			camera->pl->profile[0x82] = RES_200DPI;
		else if (!strcmp(value, _("400 DPI")))
			camera->pl->profile[0x82] = RES_400DPI;
	}

	gp_widget_get_child_by_label (window, _("Color Document mode"), &section);
	gp_widget_get_child_by_label (section, _("Depth"), &widget);
	if (gp_widget_changed (widget)) {
		gp_widget_get_value (widget, &value);
		gp_widget_set_changed (widget, 0);
		if (!strcmp(value, _("NQ (12bpp)")))
			camera->pl->profile[0x83] = 0x04;
	}

	gp_widget_get_child_by_label (section, _("Lo Resolution"), &widget);
	if (gp_widget_changed (widget)) {
		gp_widget_get_value (widget, &value);
		gp_widget_set_changed (widget, 0);
		if (!strcmp(value, _("100 DPI")))
			camera->pl->profile[0x84] = RES_100DPI;
		else if (!strcmp(value, _("200 DPI")))
			camera->pl->profile[0x84] = RES_200DPI;
	}

	gp_widget_get_child_by_label (section, _("Hi Resolution"), &widget);
	if (gp_widget_changed (widget)) {
		gp_widget_get_value (widget, &value);
		gp_widget_set_changed (widget, 0);
		if (!strcmp(value, _("200 DPI")))
			camera->pl->profile[0x85] = RES_200DPI;
		else if (!strcmp(value, _("400 DPI")))
			camera->pl->profile[0x85] = RES_400DPI;
	}

	gp_widget_get_child_by_label (window, _("Color Photo mode"), &section);
	gp_widget_get_child_by_label (section, _("Depth"), &widget);
	if (gp_widget_changed (widget)) {
		gp_widget_get_value (widget, &value);
		gp_widget_set_changed (widget, 0);
		if (!strcmp(value, _("HQ (24bpp)")))
			camera->pl->profile[0x86] = 0x08;
	}

	gp_widget_get_child_by_label (section, _("Lo Resolution"), &widget);
	if (gp_widget_changed (widget)) {
		gp_widget_get_value (widget, &value);
		gp_widget_set_changed (widget, 0);
		if (!strcmp(value, _("100 DPI")))
			camera->pl->profile[0x87] = RES_100DPI;
		else if (!strcmp(value, _("200 DPI")))
			camera->pl->profile[0x87] = RES_200DPI;
	}

	gp_widget_get_child_by_label (section, _("Hi Resolution"), &widget);
	if (gp_widget_changed (widget)) {
		gp_widget_get_value (widget, &value);
		gp_widget_set_changed (widget, 0);
		if (!strcmp(value, _("200 DPI")))
			camera->pl->profile[0x88] = RES_200DPI;
		else if (!strcmp(value, _("400 DPI")))
			camera->pl->profile[0x88] = RES_400DPI;
	}

	if (!dp_set_profile(camera))
		return GP_ERROR;

	return GP_OK;
}


int
camera_summary (Camera *camera, CameraText *summary, GPContext *context);
int
camera_summary (Camera *camera, CameraText *summary, GPContext *context)
{
	sprintf(summary->text, "Scanner model: DocuPen RC800\n"
		"Images in memory: %d\n"
		"Used bytes in memory: %d\n"
		"Internal Flash ID: %04x\n"
		"Memory size: %d\n"
		"Calibration values: %d %d %d %d %d %d %d %d",
		le32toh(camera->pl->info.pagestored),
		le32toh(camera->pl->info.datacountbyte),
		le16toh(camera->pl->info.flashmemid),
		le32toh(camera->pl->info.flashmemmax),
		camera->pl->info.calibvalue[0],
		camera->pl->info.calibvalue[1],
		camera->pl->info.calibvalue[2],
		camera->pl->info.calibvalue[3],
		camera->pl->info.calibvalue[4],
		camera->pl->info.calibvalue[5],
		camera->pl->info.calibvalue[6],
		camera->pl->info.calibvalue[7]
		);
	return GP_OK;
}


int
camera_manual (Camera *camera, CameraText *manual, GPContext *context);
int
camera_manual (Camera *camera, CameraText *manual, GPContext *context)
{
	strcpy(manual->text,
	_(
	"Docupen scanner can't download/erase individual images, only everything\n"
	"at once. To work-around this, a cache file is created where a copy of the\n"
	"scanner's memory is stored. The cache fill is trigerred by downloading any\n"
	"image - so downloading the first image will take long time if the cache is\n"
	"empty or not valid. The cache is invalidated automatically when the amount\n"
	"of used memory reported by the scanner does not match cache size.\n"
	"The cache file is located at ~/.cache/docupen-SERIAL_NO.bin\n"
	"\n"
	"The scanner has a very short auto-power-off timeout - only 8 seconds - and\n"
	"it's effective even when connected by USB. You need to turn it on before\n"
	"each gphoto2 operation. In some situations, you might need to prevent it\n"
	"from turning off by repeatedly pressing any of the buttons."
	));

	return GP_OK;
}


int
camera_about (Camera *camera, CameraText *about, GPContext *context);
int
camera_about (Camera *camera, CameraText *about, GPContext *context)
{
	strcpy (about->text, _("DocuPen RC800 scanner library\n"
			       "Copyright 2020 Ondrej Zary <ondrej@zary.sk>\n"
			       "based on Docupen tools by Florian Heinz <fh@cronon-ag.de>"
			       ));

	return GP_OK;
}


int
get_file_func (CameraFilesystem *fs, const char *folder, const char *filename,
	       CameraFileType type, CameraFile *file, void *data,
	       GPContext *context);
int
get_file_func (CameraFilesystem *fs, const char *folder, const char *filename,
	       CameraFileType type, CameraFile *file, void *data,
	       GPContext *context)
{
	Camera *camera = data;
	struct dp_imagehdr header;
	size_t ret;
	char *file_data;
	gdImagePtr img;
	void *gdout;
	int gdsize;
	int nr = gp_filesystem_number(fs, folder, filename, context);
	if (nr < 0)
		return nr;
	nr++; /* libgphoto2 file numbering is 0-based, docupen 1-based */

	if (!dp_init_cache(camera)) {
		GP_LOG_E("error initializing cache");
		return GP_ERROR;
	}

	/* find the file in cache */
	fseek(camera->pl->cache, 0, SEEK_SET);
	do {
		ret = fread(&header, 1, sizeof(header), camera->pl->cache);
		if (ret < sizeof(header)) {
			GP_LOG_E("error reading image header");
			return GP_ERROR;
		}
		if (le16toh(header.magic) != DP_MAGIC) {
			GP_LOG_E("invalid magic number in image header: 0x%04x", le16toh(header.magic));
			return GP_ERROR;
		}
		if (le16toh(header.nr) == nr) /* found */
			break;
		fseek(camera->pl->cache, le32toh(header.payloadlen), SEEK_CUR);
	} while (1);

	file_data = malloc(le32toh(header.payloadlen));
	if (!file_data)
		return GP_ERROR_NO_MEMORY;

	ret = fread(file_data, 1, le32toh(header.payloadlen), camera->pl->cache);
	if (ret < le32toh(header.payloadlen)) {
		perror("fread");
		GP_LOG_E("error reading image data");
		free(file_data);
		return GP_ERROR;
	}
	switch (type) {
	case GP_FILE_TYPE_PREVIEW:
	case GP_FILE_TYPE_NORMAL:
		switch (le16toh(header.type)) {
		case TYPE_MONO:
			img = dp_get_image_mono(&header, file_data);
			break;
		case TYPE_GREY4:
		case TYPE_GREY8:
			img = dp_get_image_grey(&header, file_data, camera->pl->lut);
			break;
		case TYPE_COLOR12:
		case TYPE_COLOR24:
			img = dp_get_image_color(&header, file_data, camera->pl->lut);
			break;
		default:
			GP_LOG_E("invalid image type 0x%x", le16toh(header.type));
			free(file_data);
			return GP_ERROR;
		}
		if (!img) {
			free(file_data);
			return GP_ERROR_NO_MEMORY;
		}
		gdout = gdImagePngPtr(img, &gdsize);
		gdImageDestroy(img);
		free(file_data);
		if (!gdout) {
			GP_LOG_E("image conversion error");
			return GP_ERROR;
		}
		file_data = malloc(gdsize);
		memcpy(file_data, gdout, gdsize);
		gdFree(gdout);
		gp_file_set_mime_type (file, GP_MIME_PNG);
		gp_file_set_data_and_size (file, file_data, gdsize);
		break;
	case GP_FILE_TYPE_RAW:
		gp_file_set_mime_type(file, GP_MIME_RAW);
		memcpy(raw_header + RAW_HEADER_SERIAL_OFFSET, camera->pl->info.serialno, sizeof(camera->pl->info.serialno));
		gp_file_append(file, (void *)&raw_header, sizeof(raw_header));
		gp_file_append(file, (void *)&header, sizeof(header));
		gp_file_append(file, file_data, le32toh(header.payloadlen));
		free(file_data);
		gp_file_adjust_name_for_mime_type(file);
		break;
	default:
		free(file_data);
		return GP_ERROR_NOT_SUPPORTED;
	}

	return GP_OK;
}


int
delete_all_func (CameraFilesystem *fs, const char *folder, void *data,
		 GPContext *context);
int
delete_all_func (CameraFilesystem *fs, const char *folder, void *data,
		 GPContext *context)
{
	Camera *camera = data;
	unsigned char status;

	if (!dp_cmd(camera->port, cmd_del_all)) {
		GP_LOG_E("delete all command failed");
		return GP_ERROR_CAMERA_ERROR;
	}

	do
		gp_port_read(camera->port, (void *)&status, 1);
	while (status == DP_ACK);

	if (status != 0) {
		GP_LOG_E("erase failed");
		return GP_ERROR_CAMERA_ERROR;
	}

	if (!inquiry_read(camera)) {
		GP_LOG_E("error reading inquiry after erase");
		return GP_ERROR_CAMERA_ERROR;
	}

	dp_delete_cache(camera);

	return GP_OK;
}


int
file_list_func (CameraFilesystem *fs, const char *folder, CameraList *list,
		void *data, GPContext *context);
int
file_list_func (CameraFilesystem *fs, const char *folder, CameraList *list,
		void *data, GPContext *context)
{
	Camera *camera = data;
	return gp_list_populate(list, "docupen%03i.png", le32toh(camera->pl->info.pagestored));
}


int
storage_info_func (CameraFilesystem *fs,
		CameraStorageInformation **storageinformations,
		int *nrofstorageinformations, void *data,
		GPContext *context);
int
storage_info_func (CameraFilesystem *fs,
		CameraStorageInformation **storageinformations,
		int *nrofstorageinformations, void *data,
		GPContext *context)
{
	Camera *camera = data;
	CameraStorageInformation *sinfo;

	sinfo = malloc(sizeof(CameraStorageInformation));
	if (!sinfo)
		return GP_ERROR_NO_MEMORY;

	*storageinformations = sinfo;
	*nrofstorageinformations = 1;

	sinfo->fields  = GP_STORAGEINFO_BASE;
	strcpy(sinfo->basedir, "/");
	sinfo->fields |= GP_STORAGEINFO_ACCESS;
	sinfo->access  = GP_STORAGEINFO_AC_READONLY_WITH_DELETE;
	sinfo->fields |= GP_STORAGEINFO_STORAGETYPE;
	sinfo->type    = GP_STORAGEINFO_ST_REMOVABLE_RAM;
	sinfo->fields |= GP_STORAGEINFO_FILESYSTEMTYPE;
	sinfo->fstype  = GP_STORAGEINFO_FST_GENERICFLAT;
	sinfo->fields |= GP_STORAGEINFO_MAXCAPACITY;
	sinfo->capacitykbytes = le32toh(camera->pl->info.flashmemmax) / 1024;
	sinfo->fields |= GP_STORAGEINFO_FREESPACEKBYTES;
	sinfo->freekbytes = (le32toh(camera->pl->info.flashmemmax) - le32toh(camera->pl->info.datacountbyte)) / 1024;

	return GP_OK;
}


int
camera_id (CameraText *id)
{
	strcpy(id->text, "docupen");

	return GP_OK;
}


int
camera_abilities (CameraAbilitiesList *list)
{
	CameraAbilities a;

	memset(&a, 0, sizeof(a));
	strcpy(a.model, "Planon DocuPen RC800");
	a.status            = GP_DRIVER_STATUS_EXPERIMENTAL;
	a.port              = GP_PORT_USB;
	a.usb_vendor        = 0x18dd;
	a.usb_product       = 0x1000;
	a.speed[0]          = 0;
	a.operations        = GP_OPERATION_CONFIG;
	a.file_operations   = GP_FILE_OPERATION_RAW;
	a.folder_operations = GP_FOLDER_OPERATION_DELETE_ALL;

	gp_abilities_list_append(list, a);

	return GP_OK;
}

CameraFilesystemFuncs fsfuncs = {
	.file_list_func = file_list_func,
	.get_file_func = get_file_func,
	.delete_all_func = delete_all_func,
	.storage_info_func = storage_info_func
};

int
camera_init (Camera *camera, GPContext *context)
{
	char buf[64];

        camera->functions->exit                 = camera_exit;
        camera->functions->get_config           = camera_config_get;
        camera->functions->set_config           = camera_config_set;
        camera->functions->summary              = camera_summary;
        camera->functions->manual               = camera_manual;
        camera->functions->about                = camera_about;

	gp_filesystem_set_funcs(camera->fs, &fsfuncs, camera);

	camera->pl = calloc(1, sizeof(CameraPrivateLibrary));
	if (!camera->pl)
		return GP_ERROR_NO_MEMORY;

	if (!dp_cmd(camera->port, cmd_query)) {
		GP_LOG_E("query failed");
		camera_exit(camera, context);
		return GP_ERROR_CAMERA_ERROR;
	}
	gp_port_read(camera->port, buf, sizeof(buf));	/* read and ingore */

	if (!dp_cmd(camera->port, cmd_inquiry)) {
		GP_LOG_E("inquiry failed");
		camera_exit(camera, context);
		return GP_ERROR_CAMERA_ERROR;
	}
	if (!inquiry_read(camera)) {
		GP_LOG_E("error reading inquiry reply");
		camera_exit(camera, context);
		return GP_ERROR_CAMERA_ERROR;
	}

	return GP_OK;
}
