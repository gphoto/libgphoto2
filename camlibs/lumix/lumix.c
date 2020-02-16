/** \file lumix.c
 *
 * \author Copyright 2019 Robert Hasson <robert_hasson@yahoo.com>
 * \author Copyright 2019 Marcus Meissner <marcus@jet.franken.de>
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
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301  USA
 *
 */
#include "config.h"


#include <string.h>
#include <errno.h>
#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xmlmemory.h>
#include <libxml/xmlreader.h>


#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h>


#include <gphoto2/gphoto2-library.h>
#include <gphoto2/gphoto2-result.h>


#define GP_MODULE "lumix"

#define CHECK(result)                                   \
{                                                       \
	int res = (result);                             \
	\
	if (res < 0) {                                  \
		gp_log (GP_LOG_DEBUG, "lumix", "Operation failed in %s (%i)!", __FUNCTION__, res);     \
		return (res); 	\
	}                      	\
}


#ifdef ENABLE_NLS
#  include <libintl.h>
#  undef _
/**
* This define is the string translation macro used in
* libgphoto2. It will resolve to a dcgettext() function call and
* does both the translation itself and also marks up the string
* for the collector (xgettext).
*/
#  define _(String) dgettext (GETTEXT_PACKAGE, String)
#  ifdef gettext_noop
/**
* This is the noop translation macro, which does not translate the
* string, but marks it up for the extraction of translatable strings.
*/
#    define N_(String) gettext_noop (String)
#  else
#    define N_(String) (String)
#  endif
#else
#  define _(String) (String)
#  define N_(String) (String)
#endif


char* CDS_Control  = ":60606/Server0/CDS_control";
int ReadoutMode = 2; // this should be picked up from the settings.... 0-> JPG; 1->RAW; 2 -> Thumbnails
char* cameraShutterSpeed = "B"; // //placeholder to store the value of the shutterspeed set in camera; "B" is for bulb.
int captureDuration = 10; //placeholder to store the value of the bulb shot this should be taken as input. note that my primary goal is in fact to perform bulb captures. but this should be extended for sure to take Shutter Speed capture as set in camera 

static int NumberPix(Camera *camera);
static char* loadCmd (Camera *camera,char* cmd);
static char* switchToRecMode(Camera *camera);
static char* switchToPlayMode(Camera *camera);

typedef struct {
	char   *data;
	size_t 	size;
} LumixMemoryBuffer;

typedef struct {
	char	*id;
	char	*url_raw;
	char	*url_movie;
	char	*url_large;
	char	*url_medium;
	char	*url_thumb;
} LumixPicture;

struct _CameraPrivateLibrary {
	/* all private data */

	int		numpics;
	LumixPicture	*pics;

	int		liveview;
	int		udpsocket;
};


static int
camera_exit (Camera *camera, GPContext *context) 
{
	if (camera->pl->udpsocket > 0) {
		close (camera->pl->udpsocket);
		camera->pl->udpsocket = 0;
	}
	return GP_OK;
}

static int
camera_capture_preview (Camera *camera, CameraFile *file, GPContext *context)
{
	int			valread;
	struct sockaddr_in	serv_addr;
	unsigned char		buffer[65536];
	GPPortInfo      	info;
	int			i, start, end, tries;

	switchToRecMode (camera);

	if (!camera->pl->liveview) {
		loadCmd(camera,"cam.cgi?mode=startstream&value=49199");
		camera->pl->liveview = 1;
		if (camera->pl->udpsocket <= 0) {
			if ((camera->pl->udpsocket = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
				GP_LOG_E("\n Socket creation error \n");
				return GP_ERROR;
			}

			gp_port_get_info (camera->port, &info);

			memset(&serv_addr, 0, sizeof(serv_addr));

			serv_addr.sin_family = AF_INET;
			serv_addr.sin_port = htons(49199);
			serv_addr.sin_addr.s_addr = 0;

			if (bind(camera->pl->udpsocket, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
				GP_LOG_E("bind Failed: %d", errno);
				return GP_ERROR;
			}
		}
	} else {
		/* this reminds the camera we are still doing it */
		loadCmd(camera,"cam.cgi?mode=getstate");
	}
	tries = 3;
	while (tries--) {
		valread = recv ( camera->pl->udpsocket, buffer, sizeof(buffer), 0);
		if (valread == -1) {
			GP_LOG_E("recv failed: %d", errno);
			return GP_ERROR;
		}

		GP_LOG_DATA((char*)buffer,valread,"read from udp port");

		if (valread == 0)
			continue;

		start = end = -1;
		for (i = 0; i<valread-1; i++) {
			if ((buffer[i] == 0xff) && (buffer[i+1] == 0xd8)) {
				start = i;
			}
			if ((buffer[i] == 0xff) && (buffer[i+1] == 0xd9)) {
				end = i+2;
			}
		}
		//GP_LOG_DATA(buffer+start,end-start,"read from udp port");
		gp_file_set_mime_type (file, GP_MIME_JPEG);
		return gp_file_append (file, (char*)buffer+start, end-start);
	}
	return GP_ERROR;
}

static int camera_about (Camera *camera, CameraText *about, GPContext *context);


/**
 * Put a file onto the camera.
 *
 * This function is a CameraFilesystem method.
 */
int
put_file_func (CameraFilesystem *fs, const char *folder, const char *name,
	       CameraFileType type, CameraFile *file, void *data, GPContext *context);
int
put_file_func (CameraFilesystem *fs, const char *folder, const char *name,
	       CameraFileType type, CameraFile *file, void *data, GPContext *context)
{
	/*Camera *camera = data;*/

	/*
	 * Upload the file to the camera. Use gp_file_get_data_and_size, etc
	 */

	return GP_OK;
}


/**
 * Delete a file from the camera.
 *
 * This function is a CameraFilesystem method.
 */
int
delete_file_func (CameraFilesystem *fs, const char *folder,
		  const char *filename, void *data, GPContext *context);
int
delete_file_func (CameraFilesystem *fs, const char *folder,
		  const char *filename, void *data, GPContext *context)
{
	/*Camera *camera = data;*/

	/* Delete the file from the camera. */

	return GP_OK;
}


/**
 * Delete all files from the camera.
 *
 * This function is a CameraFilesystem method.
 */
int
delete_all_func (CameraFilesystem *fs, const char *folder, void *data,
		 GPContext *context);
int
delete_all_func (CameraFilesystem *fs, const char *folder, void *data,
		 GPContext *context)
{
	/*Camera *camera = data;*/

	/*
	 * Delete all files in the given folder. If your camera doesn't have
	 * such a functionality, just don't implement this function.
	 */

	return GP_OK;
}


/**
 * Get the file info here and write it to space provided by caller.
 * 
 * \param info Space provided by caller in which file info is written.
 *
 * This function is a CameraFilesystem method.
 */
int
get_info_func (CameraFilesystem *fs, const char *folder, const char *filename,
	       CameraFileInfo *info, void *data, GPContext *context);
int
get_info_func (CameraFilesystem *fs, const char *folder, const char *filename,
	       CameraFileInfo *info, void *data, GPContext *context)
{
	/*Camera *camera = data;*/

	return GP_OK;
}


static int
set_info_func (CameraFilesystem *fs, const char *folder, const char *file,
	       CameraFileInfo info, void *data, GPContext *context)
{
	/*Camera *camera = data;*/

	/* Set the file info here from <info> */

	return GP_OK;
}


static int
folder_list_func (CameraFilesystem *fs, const char *folder, CameraList *list,
		  void *data, GPContext *context)
{
	/*Camera *camera = data;*/
	/* currently no folders exposed */
	return GP_OK;
}


/**
 * List available files in the specified folder.
 *
 * This function is a CameraFilesystem method.
 */
static int
file_list_func (CameraFilesystem *fs, const char *folder, CameraList *list,
		void *data, GPContext *context)
{
	Camera *camera = data;
	int	i;

	for (i=0;i<camera->pl->numpics;i++) {
		if (camera->pl->pics[i].url_raw) {
			char	*s = strrchr(camera->pl->pics[i].url_raw, '/')+1;
			gp_list_append (list, s, NULL);
			continue;
		}
		if (camera->pl->pics[i].url_large) {
			char	*s = strrchr(camera->pl->pics[i].url_large, '/')+1;
			gp_list_append (list, s, NULL);
			continue;
		}
		if (camera->pl->pics[i].url_movie) {
			char	*s = strrchr(camera->pl->pics[i].url_movie, '/')+1;
			gp_list_append (list, s, NULL);
			continue;
		}
	}
	return GP_OK;
}

/**
 * Get information on all available storages in the camera.
 *
 * This function is a CameraFilesystem method.
 */
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
	/*Camera *camera = data;*/

	/* List your storages here */

	return GP_ERROR_NOT_SUPPORTED;
}

/*@}*/


/**********************************************************************/
/**
 * @name camlib API functions
 *
 * @{
 */
/**********************************************************************/


static size_t
write_callback(char *contents, size_t size, size_t nmemb, void *userp)
{
	size_t 			realsize = size * nmemb;
	size_t 			oldsize;
	LumixMemoryBuffer	*lmb = userp;

	oldsize = lmb->size;
	/* 1 additionaly byte for 0x00 */
	lmb->data = realloc(lmb->data, lmb->size+realsize+1);
	lmb->size += realsize;
	lmb->data[lmb->size] = 0x00;

	GP_LOG_DATA(contents, realsize, "lumix read from url");

	memcpy(lmb->data+oldsize,contents,realsize);
	return realsize;
}


static char*
loadCmd (Camera *camera,char* cmd) {
	CURL		*curl;
	CURLcode	res;
	char		URL[100];
	GPPortInfo      info;
	char		*xpath;
	LumixMemoryBuffer	lmb;

	curl = curl_easy_init();
	gp_port_get_info (camera->port, &info);
	gp_port_info_get_path (info, &xpath); /* xpath now contains ip:192.168.1.1 */
	snprintf( URL,100, "http://%s/%s", xpath+strlen("ip:"), cmd);
	GP_LOG_D("cam url is %s", URL);

	curl_easy_setopt(curl, CURLOPT_URL, URL);

	lmb.size = 0;
	lmb.data = malloc(0);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &lmb);

	res = curl_easy_perform(curl);
	if(res != CURLE_OK) {
		fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
		return NULL;
	} else {
		/* read the XML that is now in Buffer*/ 
		GP_LOG_D("result %s\n", lmb.data);

		/* <?xml version="1.0" encoding="UTF-8"?> <camrply><result>ok</result></camrply> */
	}
	curl_easy_cleanup(curl);
	return lmb.data;
}

static char*
switchToRecMode(Camera *camera) {
	return loadCmd(camera,"cam.cgi?mode=camcmd&value=recmode");
}

static char*
switchToPlayMode(Camera *camera) {
	return loadCmd(camera,"cam.cgi?mode=camcmd&value=playmode");
}


static void Set_ISO(Camera *camera,const char * ISOValue) {
	char buf[200];
	sprintf(buf, "?mode=setsetting&type=iso&value=%s",ISOValue);
	loadCmd(camera,buf);
}

static char*
generic_setting_getter(Camera *camera, char *type) {
	char		*result, *s;
        xmlDocPtr       docin;
        xmlNodePtr      docroot, output;
	xmlAttr		*attr;
	char		url[50];

	sprintf(url,"cam.cgi?mode=getsetting&type=%s", type);
	result = loadCmd(camera, url);
	docin = xmlReadMemory (result, strlen(result), "http://gphoto.org/", "utf-8", 0);

	/*<?xml version="1.0" encoding="UTF-8"?>
	 * <camrply><result>ok</result><settingvalue TYPE="CONTENT"></settingvalue></camrply>
	 */

	if (!docin) return NULL;
	docroot = xmlDocGetRootElement (docin);
	if (!docroot) {
		xmlFreeDoc (docin);
		return NULL;
	}
	if (strcmp((char*)docroot->name,"camrply")) {
		GP_LOG_E ("docroot name unexpected %s", docroot->name);
		return NULL;
	}
	output = docroot->children;
	if (strcmp((char*)output->name, "result")) {
		GP_LOG_E ("node name expected 'result', got %s", output->name);
		return NULL;
	}
	if (strcmp((char*)xmlNodeGetContent (output),"ok")) {
		GP_LOG_E ("result was not 'ok', got %s", (char*)xmlNodeGetContent (output));
		return NULL;
	}
	output = xmlNextElementSibling (output);
	if (strcmp((char*)output->name, "settingvalue")) {
		GP_LOG_E ("node name expected 'settingvalue', got %s", output->name);
		return NULL;
	}
	attr = output->properties;
	if (strcmp((char*)attr->name, type)) {
		GP_LOG_E ("attr name expected '%s', got %s", type, output->name);
		return NULL;
	}
	s = (char*)xmlNodeGetContent (attr->children);
	GP_LOG_D("%s content %s", type, s);
	xmlFreeDoc (docin);
	return strdup(s);
}

static char*
Get_Clock(Camera *camera) {
	return generic_setting_getter(camera,"clock");
}

static char*
Get_Video_Quality(Camera *camera) {
	return generic_setting_getter(camera,"videoquality");
}

static char*
Get_ISO(Camera *camera) {
	/* <?xml version="1.0" encoding="UTF-8"?>
	 * <camrply><result>ok</result><settingvalue iso="auto"></settingvalue></camrply>
	 */
	return generic_setting_getter(camera,"iso");
}

static char*
Get_ShutterSpeed(Camera *camera) {
	/*<?xml version="1.0" encoding="UTF-8"?>
	 * <camrply><result>ok</result><settingvalue shtrspeed="2560/256"></settingvalue></camrply>
	 */
	return generic_setting_getter(camera, "shtrspeed");
}

static char*
Get_Aperture(Camera *camera) {
	/* <?xml version="1.0" encoding="UTF-8"?>
	 * <camrply><result>ok</result><settingvalue focal="1366/256"></settingvalue></camrply>
	 */
	return generic_setting_getter(camera, "focal");
}

static char*
Get_AFMode(Camera *camera) {
	return generic_setting_getter(camera,"afmode");
}

static char*
Get_LiveViewSize(Camera *camera) {
	return generic_setting_getter(camera,"liveviewsize");
}

static char*
Get_DeviceName(Camera *camera) {
	return generic_setting_getter(camera,"device_name");
}

static char*
Get_FocusMode(Camera *camera) {
	return generic_setting_getter(camera,"focusmode");
}

static char*
Get_MFAssist(Camera *camera) {
	return generic_setting_getter(camera,"mf_asst");
}

static char*
Get_MFAssist_Mag(Camera *camera) {
	return generic_setting_getter(camera,"mf_asst_mag");
}

static char*
Get_ExTeleConv(Camera *camera) {
	return generic_setting_getter(camera,"ex_tele_conv");
}

static char*
Get_Capability(Camera *camera) {
	return loadCmd(camera,"cam.cgi?mode=getinfo&type=capability");
}

static char*
Get_Lens(Camera *camera) {
	return loadCmd(camera,"cam.cgi?mode=getinfo&type=lens");
}

static char*
Get_AllMenu(Camera *camera) {
	return loadCmd(camera,"cam.cgi?mode=getinfo&type=allmenu");
}

static char*
Get_CurMenu(Camera *camera) {
	return loadCmd(camera,"cam.cgi?mode=getinfo&type=curmenu");
}


static void Set_ShutterSpeed(Camera *camera,const char* SpeedValue) {
	char buf[200];
	sprintf(buf, "cam.cgi?mode=setsetting&type=shtrspeed&value=%s",SpeedValue);
	loadCmd(camera,buf);
}

static char*
Get_Quality(Camera *camera) {
	return generic_setting_getter(camera,"quality");
}

static void
Set_quality(Camera *camera,const char* Quality) {
	char buf[200];
	sprintf(buf,"cam.cgi?mode=setsetting&type=quality&value=%s", Quality);
	loadCmd(camera,buf);
}


static int
startCapture(Camera *camera) {
	char *result;

	result = loadCmd(camera,"cam.cgi?mode=camcmd&value=capture");

	if (strstr(result,"<result>ok</result>")) return GP_OK;
	if (strstr(result,"<result>err_busy</result>")) return GP_ERROR_CAMERA_BUSY;
	return GP_ERROR;
}

static int
stopCapture(Camera *camera) {
	char	*result;

	result = loadCmd(camera,"cam.cgi?mode=camcmd&value=capture_cancel");

	if (strstr(result,"<result>ok</result>")) return GP_OK;
	if (strstr(result,"<result>err_busy</result>")) return GP_ERROR_CAMERA_BUSY;
	return GP_ERROR;
}

static void startMovie(Camera *camera) {
	loadCmd(camera,"cam.cgi?mode=camcmd&value=video_recstart");
}

static void stopMovie(Camera *camera) {
	loadCmd(camera,"cam.cgi?mode=camcmd&value=video_recstop");
}

static void zoomIn(Camera *camera) {
	loadCmd(camera,"cam.cgi?mode=camcmd&value=tele-fast");
}

static void zoomOut(Camera *camera) {
	loadCmd(camera,"cam.cgi?mode=camcmd&value=wide-fast");
}

static void zoomStop(Camera *camera) {
	loadCmd(camera,"cam.cgi?mode=camcmd&value=zoomstop");
}

static void focusIn(Camera *camera) {
	loadCmd(camera,"cam.cgi?mode=camctrl&type=focus&value=tele-fast");
}

static void focusOut(Camera *camera) {
	loadCmd(camera,"cam.cgi?mode=camctrl&type=focus&value=wide-fast");
}

/*

this is the XML sample to be parsed by the function below   NumberPix() 
<?xml version="1.0" encoding="UTF-8"?>
<camrply><result>ok</result><current_position>341</current_position><total_content_number>372</total_content_number><content_number>342</content_number></camrply>


*/
static int
strend(const char *s, const char *t)
{
    size_t ls = strlen(s); // find length of s
    size_t lt = strlen(t); // find length of t
    if (ls >= lt)  // check if t can fit in s
    {
        // point s to where t should start and compare the strings from there
        return (0 == memcmp(t, s + (ls - lt), lt));
    }
    return 0; // t was longer than s
}

static int
NumberPix(Camera *camera) {
	xmlChar		*keyz = NULL;
	int		numpics = 0;
	char		*temp = loadCmd(camera,"cam.cgi?mode=get_content_info");
	xmlDocPtr	doc = xmlParseDoc((unsigned char*) temp);
	xmlNodePtr	cur = NULL; 

	cur = xmlDocGetRootElement(doc);   

	if (cur == NULL) {
		GP_LOG_E("empty xml result document");
		xmlFreeDoc(doc);
		return GP_ERROR;
	}
	/*
	 * busy mode:
	 <?xml version="1.0" encoding="UTF-8"?> <camrply><result>err_busy</result></camrply>

	 * regular return:
	 <?xml version="1.0" encoding="UTF-8"?>
	 <camrply><result>ok</result><current_position>2</current_position><total_content_number>651</total_content_number><content_number>651</content_number></camrply>
	 */
	if (strstr(temp, "<result>err_busy</result>")) {
		xmlFreeDoc(doc);
		return GP_ERROR_CAMERA_BUSY;
	}

	cur = cur->xmlChildrenNode;
	while (cur != NULL) {
		/*GP_LOG_D("NumberPix Decode current node is %s \n", (char*)cur);*/
		if ((!xmlStrcmp(cur->name, (const xmlChar *)"content_number"))) {
			keyz = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			break;
		}
		cur = cur->next;
	}
	if (!keyz) {
		xmlFreeDoc(doc);
		return GP_ERROR;
	}
	GP_LOG_D("NumberPix Found is %s", (char *) keyz);
	numpics = strtol((char*)keyz, NULL, 10);

	xmlFreeDoc(doc);
	return numpics;
}

/*utility function to creat a SOAP envelope for the lumix cam */  
static char*
SoapEnvelop(int start, int num){
	static char  Envelop[1000];
	snprintf(Envelop,1000, "<?xml version=\"1.0\" encoding=\"utf-8\"?><s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\"><s:Body><u:Browse xmlns:u=\"urn:schemas-upnp-org:service:ContentDirectory:1\"xmlns:pana=\"urn:schemas-panasonic-com:pana\"><ObjectID>0</ObjectID><BrowseFlag>BrowseDirectChildren</BrowseFlag><Filter>*</Filter><StartingIndex>%d</StartingIndex><RequestedCount>%d</RequestedCount><SortCriteria></SortCriteria><pana:X_FromCP>LumixLink2.0</pana:X_FromCP></u:Browse></s:Body></s:Envelope>",start,num);

	return Envelop;
}

/*
 * retrieves the XML doc with the last num pictures avail in the camera.
 */
static char*
GetPixRange(Camera *camera, int start, int num) {
	long		NumPix;
	int		numreadint;
        xmlDocPtr       docin, docin2;
        xmlNodePtr      docroot, docroot2, output, output2, next;
	xmlChar 	*xchar, *numread;
	char*		SoapMsg;
	CURL 		*curl;
	CURLcode	res;
	struct curl_slist *list = NULL;
	GPPortInfo      info;
	char		*xpath;
	LumixMemoryBuffer	lmb;
	char		URL[1000];

	switchToPlayMode (camera);
	NumPix  = NumberPix(camera);
	//GP_LOG_D("NumPix is %d \n", NumPix);
	if (NumPix < GP_OK) return NULL;

	if (camera->pl->numpics < NumPix) {
		camera->pl->pics = realloc(camera->pl->pics,NumPix * sizeof(camera->pl->pics[0]));
		memset(camera->pl->pics+camera->pl->numpics, 0, (NumPix - camera->pl->numpics) * sizeof(camera->pl->pics[0]));
		camera->pl->numpics = NumPix;
	}

	while (num > 0) {
		SoapMsg = SoapEnvelop(start, num);
		//GP_LOG_D("SoapMsg is %s \n", SoapMsg);

		curl = curl_easy_init();

		list = curl_slist_append(list, "SOAPaction: urn:schemas-upnp-org:service:ContentDirectory:1#Browse");
		list = curl_slist_append(list, "Content-Type: text/xml; charset=\"utf-8\"");
		list = curl_slist_append(list, "Accept: text/xml");

		gp_port_get_info (camera->port, &info);
		gp_port_info_get_path (info, &xpath); /* xpath now contains ip:192.168.1.1 */
		snprintf( URL, 1000, "http://%s%s",xpath+strlen("ip:"), CDS_Control);
		curl_easy_setopt(curl, CURLOPT_URL, URL);

		lmb.size = 0;
		lmb.data = malloc(0);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &lmb);

		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, list);
		curl_easy_setopt(curl, CURLOPT_POST,1);
		curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long) strlen(SoapMsg));

		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, SoapMsg);

		GP_LOG_D("camera url is %s", URL);
		GP_LOG_D("posting %s", SoapMsg);

		res = curl_easy_perform(curl);
		if(res != CURLE_OK) {
			fprintf(stderr, "curl_easy_perform() failed: %s\n",
			curl_easy_strerror(res));
			curl_easy_cleanup(curl);
			return NULL;
		}
		curl_easy_cleanup(curl);

		docin = xmlReadMemory (lmb.data, lmb.size, "http://gphoto.org/", "utf-8", 0);
		if (!docin) return NULL;
		docroot = xmlDocGetRootElement (docin);
		if (!docroot) {
			xmlFreeDoc (docin);
			return NULL;
		}
		if (strcmp((char*)docroot->name,"Envelope")) {
			GP_LOG_E("root node is '%s', expected 'Envelope'", docroot->name);
			xmlFreeDoc (docin);
			return NULL;
		}
		output = xmlFirstElementChild (docroot);
		if (strcmp((char*)output->name,"Body")) {
			GP_LOG_E("second level node is %s, expected Body", docroot->name);
			xmlFreeDoc (docin);
			return NULL;
		}
		output = xmlFirstElementChild (output);
		if (strcmp((char*)output->name,"BrowseResponse")) {
			GP_LOG_E("third level node is %s, expected BrowseResponse", docroot->name);
			xmlFreeDoc (docin);
			return NULL;
		}
		output = xmlFirstElementChild (output);
		if (strcmp((char*)output->name,"Result")) {
			GP_LOG_E("fourth level node is %s, expected Result", docroot->name);
			xmlFreeDoc (docin);
			return NULL;
		}
		xchar = xmlNodeGetContent (output);
		GP_LOG_D("content of %s is %s", output->name, xchar);
	/* 

	<DIDL-Lite xmlns="urn:schemas-upnp-org:metadata-1-0/DIDL-Lite/" xmlns:dc="http://purl.org/dc/elements/1.1/" xmlns:upnp="urn:schemas-upnp-org:metadata-1-0/upnp/">
		<item id="1030822" parentID="0" restricted="0">
			<dc:title>103-0822</dc:title>
			<upnp:writeStatus>WRITABLE</upnp:writeStatus>
			<upnp:class name="imageItem">object.item.imageItem</upnp:class>
			<res protocolInfo="http-get:*:image/jpeg:DLNA.ORG_PN=JPEG_LRG;DLNA.ORG_OP=01;DLNA.ORG_CI=0;DLNA.ORG_FLAGS=00900000000000000000000000000000;PANASONIC.COM_PN=CAM_ORG">
				http://192.168.54.1:50001/DO1030822.JPG
			</res>
			<res protocolInfo="http-get:*:image/jpeg:DLNA.ORG_PN=JPEG_TN;DLNA.ORG_OP=01;DLNA.ORG_CI=1;DLNA.ORG_FLAGS=00900000000000000000000000000000;PANASONIC.COM_PN=CAM_TN">
				http://192.168.54.1:50001/DT1030822.JPG
			</res>
			<res protocolInfo="http-get:*:image/jpeg:DLNA.ORG_PN=JPEG_MED;DLNA.ORG_OP=01;DLNA.ORG_CI=1;DLNA.ORG_FLAGS=00900000000000000000000000000000;PANASONIC.COM_PN=CAM_LRGTN">
				http://192.168.54.1:50001/DL1030822.JPG
			</res>
		</item>
	</DIDL-Lite>

	*/
		docin2 = xmlReadMemory ((char*)xchar, strlen((char*)xchar), "http://gphoto.org/", "utf-8", 0);
		if (!docin2) return NULL;
		docroot2 = xmlDocGetRootElement (docin2);
		if (!docroot) {
			xmlFreeDoc (docin2);
			return NULL;
		}
		if (strcmp((char*)docroot2->name,"DIDL-Lite")) {
			GP_LOG_E("expected DIDL-Lite, got %s", docroot2->name);
			xmlFreeDoc (docin2);
			return NULL;
		}
		output2 = xmlFirstElementChild (docroot2);
		if (strcmp((char*)output2->name,"item")) {
			GP_LOG_E("expected item, got %s", output2->name);
			xmlFreeDoc (docin2);
			return NULL;
		}
	/*
		<item id="1030822" parentID="0" restricted="0">
			<dc:title>103-0822</dc:title>
			<upnp:writeStatus>WRITABLE</upnp:writeStatus>
			<upnp:class name="imageItem">object.item.imageItem</upnp:class>
			<res protocolInfo="http-get:*:image/jpeg:DLNA.ORG_PN=JPEG_LRG;DLNA.ORG_OP=01;DLNA.ORG_CI=0;DLNA.ORG_FLAGS=00900000000000000000000000000000;PANASONIC.COM_PN=CAM_ORG">
				http://192.168.54.1:50001/DO1030822.JPG
			</res>
			<res protocolInfo="http-get:*:image/jpeg:DLNA.ORG_PN=JPEG_TN;DLNA.ORG_OP=01;DLNA.ORG_CI=1;DLNA.ORG_FLAGS=00900000000000000000000000000000;PANASONIC.COM_PN=CAM_TN">
				http://192.168.54.1:50001/DT1030822.JPG
			</res>
			<res protocolInfo="http-get:*:image/jpeg:DLNA.ORG_PN=JPEG_MED;DLNA.ORG_OP=01;DLNA.ORG_CI=1;DLNA.ORG_FLAGS=00900000000000000000000000000000;PANASONIC.COM_PN=CAM_LRGTN">
				http://192.168.54.1:50001/DL1030822.JPG
			</res>
		</item>

we have:
Picture:
	http-get:*:image/jpeg:DLNA.ORG_PN=JPEG_LRG;DLNA.ORG_OP=01;DLNA.ORG_CI=0;DLNA.ORG_FLAGS=00900000000000000000000000000000;PANASONIC.COM_PN=CAM_ORG
	http-get:*:image/jpeg:DLNA.ORG_PN=JPEG_TN;DLNA.ORG_OP=01;DLNA.ORG_CI=1;DLNA.ORG_FLAGS=00900000000000000000000000000000;PANASONIC.COM_PN=CAM_TN
	http-get:*:image/jpeg:DLNA.ORG_PN=JPEG_MED;DLNA.ORG_OP=01;DLNA.ORG_CI=1;DLNA.ORG_FLAGS=00900000000000000000000000000000;PANASONIC.COM_PN=CAM_LRGTN

Movie:
	http-get:*:application/octet-stream:DLNA.ORG_OP=01;PANASONIC.COM_PN=CAM_AVC_MP4_HP_2160_30P_AAC
	http-get:*:image/jpeg:DLNA.ORG_PN=JPEG_TN;DLNA.ORG_OP=01;DLNA.ORG_CI=1;DLNA.ORG_FLAGS=00900000000000000000000000000000;PANASONIC.COM_PN=CAM_TN
	content is http-get:*:image/jpeg:DLNA.ORG_PN=JPEG_SM;DLNA.ORG_OP=01;DLNA.ORG_CI=1;DLNA.ORG_FLAGS=00900000000000000000000000000000;PANASONIC.COM_PN=CAM_LRGTN

Raw:
	http-get:*:application/octet-stream;PANASONIC.COM_PN=CAM_RAW_JPG
	http-get:*:image/jpeg:DLNA.ORG_PN=JPEG_TN;DLNA.ORG_OP=01;DLNA.ORG_CI=1;DLNA.ORG_FLAGS=00900000000000000000000000000000;PANASONIC.COM_PN=CAM_TN
	http-get:*:image/jpeg:DLNA.ORG_PN=JPEG_MED;DLNA.ORG_OP=01;DLNA.ORG_CI=1;DLNA.ORG_FLAGS=00900000000000000000000000000000;PANASONIC.COM_PN=CAM_LRGTN
	http-get:*:application/octet-stream;PANASONIC.COM_PN=CAM_RAW

	*/
		next = output2;
		do {
			char	*id = NULL;
			int	i;
			xmlNode	*child;
			xmlAttr *attr = next->properties;

			GP_LOG_D("got %s", next->name);

			while (attr) {
				GP_LOG_D("\t attribute %s", attr->name);
				GP_LOG_D("\t attribute content %s", xmlNodeGetContent(attr->children));

				if (!strcmp((char*)attr->name, "id")) {
					id = (char*)xmlNodeGetContent(attr->children);
					break;
				}
				attr = attr->next;
			}
			/* look if we have the id */
			for (i=0;i<camera->pl->numpics;i++)
				if (camera->pl->pics[i].id && !strcmp(camera->pl->pics[i].id, id))
					break;
			if (i<camera->pl->numpics) {
				GP_LOG_D("already read id %s, is element %d\n", id, i);
				continue;
			}
			for (i=0;i<camera->pl->numpics;i++)
				if (!camera->pl->pics[i].id)
					break;
			if (i==camera->pl->numpics) {
				GP_LOG_D("no free slot found for id %s\n", id);
				continue;
			}
			camera->pl->pics[i].id = strdup(id);
			child = next->children;

		/*
			<res protocolInfo="http-get:*:image/jpeg:DLNA.ORG_PN=JPEG_MED;DLNA.ORG_OP=01;DLNA.ORG_CI=1;DLNA.ORG_FLAGS=00900000000000000000000000000000;PANASONIC.COM_PN=CAM_LRGTN">
				http://192.168.54.1:50001/DL1030822.JPG
			</res>
		*/
			while (child) {
				xmlAttr *attr;
				xmlChar	*attrcontent;

				if (strcmp((char*)child->name,"res")) {
					GP_LOG_D("skipping %s", child->name);
					child = child->next;
					continue;
				}
				attr = child->properties;

				if (strcmp((char*)attr->name,"protocolInfo")) {
					GP_LOG_D("expected attribute protocolInfo, got %s", attr->name);
				}
				attrcontent = xmlNodeGetContent(attr->children);
				GP_LOG_D("\t\tattribute content is %s", attrcontent);

				GP_LOG_D("\t child content %s", xmlNodeGetContent(child));

				/* FIXME: perhaps shorten match? */
				if (strstr((char*)attrcontent,"PANASONIC.COM_PN=CAM_AVC_MP4_HP_2160_30P_AAC")) {
					camera->pl->pics[i].url_movie = strdup((char*)xmlNodeGetContent(child));
				}
				// http-get:*:application/octet-stream;PANASONIC.COM_PN=CAM_RAW
				// but not
				// http-get:*:application/octet-stream;PANASONIC.COM_PN=CAM_RAW_JPG
				if (strstr((char*)attrcontent,"PANASONIC.COM_PN=CAM_RAW") &&
				    !strstr((char*)attrcontent,"PANASONIC.COM_PN=CAM_RAW_JPG")
				) {
					camera->pl->pics[i].url_raw = strdup((char*)xmlNodeGetContent(child));
				}
				if (strstr((char*)attrcontent,"PANASONIC.COM_PN=CAM_ORG")) {
					camera->pl->pics[i].url_large = strdup((char*)xmlNodeGetContent(child));
				}
				if (strstr((char*)attrcontent,"PANASONIC.COM_PN=CAM_LRGTN")) {
					camera->pl->pics[i].url_medium = strdup((char*)xmlNodeGetContent(child));
				}
				if (strstr((char*)attrcontent,"PANASONIC.COM_PN=CAM_TN")) {
					camera->pl->pics[i].url_thumb = strdup((char*)xmlNodeGetContent(child));
				}
				child = child->next;
			}
		} while ((next = xmlNextElementSibling (next)));


		output = xmlNextElementSibling (output);
		if (strcmp((char*)output->name,"NumberReturned")) {
			GP_LOG_E("fourth level node is %s, expected NumberReturned", output->name);
			xmlFreeDoc (docin);
			return NULL;
		}
		numread = xmlNodeGetContent (output);
		GP_LOG_D("content of %s is %s", output->name, numread);

		numreadint = 1;
		sscanf((char*)numread,"%d", &numreadint);

		start += numreadint;
		num -= numreadint;
	}
	return strdup((char*)xchar);
}

static struct shuttermap {
	char	*cameraspeed;
	char	*speed;
} shutterspeeds[] = {
	{"3328/256","8000"},
	{"3243/256","6400"},
	{"3158/256","5000"},
	{"3072/256","4000"},
	{"2987/256","3200"},
	{"2902/256","2500"},
	{"2816/256","2000"},
	{"2731/256","1600"},
	{"2646/256","1300"},
	{"2560/256","1000"},
	{"2475/256","800"},
	{"2390/256","640"},
	{"2304/256","500"},
	{"2219/256","400"},
	{"2134/256","320"},
	{"2048/256","250"},
	{"1963/256","200"},
	{"1878/256","160"},
	{"1792/256","125"},
	{"1707/256","100"},
	{"1622/256","80"},
	{"1536/256","60"},
	{"1451/256","50"},
	{"1366/256","40"},
	{"1280/256","30"},
	{"1195/256","25"},
	{"1110/256","20"},
	{"1024/256","15"},
	{"939/256","13"},
	{"854/256","10"},
	{"768/256","8"},
	{"683/256","6"},
	{"598/256","5"},
	{"512/256","4"},
	{"427/256","3.2"},
	{"342/256","2.5"},
	{"256/256","2"},
	{"171/256","1.6"},
	{"86/256","1.3"},
	{"0/256","1"},
	{"-85/256","1.3s"},
	{"-170/256","1.6s"},
	{"-256/256","2s"},
	{"-341/256","2.5s"},
	{"-426/256","3.2s"},
	{"-512/256","4s"},
	{"-682/256","5s"},
	{"-768/256","6s"},
	{"-853/256","8s"},
	{"-938/256","10s"},
	{"-1024/256","13s"},
	{"-1109/256","15s"},
	{"-1194/256","20s"},
	{"-1280/256","25s"},
	{"-1365/256","30s"},
	{"-1450/256","40s"},
	{"-1536/256","50s"},
	{"16384/256","60s"},
	{"256/256","B"},
};

static struct aperturemap {
	char	*cameraaperture;
	char	*aperture;
} apertures[] = {
	{"392/256","1.7"},
	{"427/256","1.8"},
	{"512/256","2"},
	{"598/256","2.2"},
	{"683/256","2.5"},
	{"768/256","2.8"},
	{"854/256","3.2"},
	{"938/256","3.5"},
	{"1024/256","4"},
	{"1110/256","4.5"},
	{"1195/256","5"},
	{"1280/256","5.6"},
	{"1366/256","6.3"},
	{"1451/256","7.1"},
	{"1536/256","8"},
	{"1622/256","9"},
	{"1707/256","10"},
	{"1792/256","11"},
	{"1878/256","13"},
	{"1963/256","14"},
	{"2048/256","16"},
};

static int
camera_config_get (Camera *camera, CameraWidget **window, GPContext *context) 
{
        CameraWidget	*widget,*section;
        int		ret, i, valset;
	char		*val, *curval;

	switchToRecMode (camera);

        gp_widget_new (GP_WIDGET_WINDOW, _("Lumix Configuration"), window);
        gp_widget_set_name (*window, "config");

	gp_widget_new (GP_WIDGET_SECTION, _("Camera Settings"), &section);
	gp_widget_set_name (section, "settings");
	gp_widget_append (*window, section);

	gp_widget_new (GP_WIDGET_TEXT, _("Clock"), &widget);
	gp_widget_set_name (widget, "clock");
	gp_widget_set_value (widget, Get_Clock(camera));
	gp_widget_append (section, widget);


	val = Get_ShutterSpeed(camera);
	if (!val) val = "unknown";
	gp_widget_new (GP_WIDGET_RADIO, _("Shutterspeed"), &widget);
	gp_widget_set_name (widget, "shutterspeed");
	valset = 0;
	for (i=0;i<sizeof(shutterspeeds)/sizeof(shutterspeeds[0]);i++) {
		gp_widget_add_choice (widget, shutterspeeds[i].speed);
		if (!strcmp(val, shutterspeeds[i].cameraspeed)) {
			valset = 1;
			gp_widget_set_value (widget, shutterspeeds[i].speed);
		}
	}
	if (!valset) gp_widget_set_value (widget, val);
	gp_widget_append (section, widget);

	gp_widget_new (GP_WIDGET_TEXT, _("Quality"), &widget);
	gp_widget_set_name (widget, "quality");
	gp_widget_set_value (widget, Get_Quality(camera));
	gp_widget_append (section, widget);

	gp_widget_new (GP_WIDGET_TEXT, _("Video Quality"), &widget);
	gp_widget_set_name (widget, "videoquality");
	gp_widget_set_value (widget, Get_Video_Quality(camera));
	gp_widget_append (section, widget);


	val = Get_Aperture(camera);
	if (!val) val = "unknown";
	gp_widget_new (GP_WIDGET_RADIO, _("Aperture"), &widget);
	gp_widget_set_name (widget, "aperture");
	valset = 0;
	for (i=0;i<sizeof(apertures)/sizeof(apertures[0]);i++) {
		gp_widget_add_choice (widget, apertures[i].aperture);
		if (!strcmp(val, apertures[i].cameraaperture)) {
			valset = 1;
			gp_widget_set_value (widget, apertures[i].aperture);
		}
	}
	if (!valset) gp_widget_set_value (widget, val);
	gp_widget_append (section, widget);


	gp_widget_new (GP_WIDGET_RADIO, _("ISO"), &widget);
	gp_widget_set_name (widget, "iso");
	gp_widget_set_value (widget, Get_ISO(camera));
	gp_widget_add_choice (widget, "auto");
	gp_widget_add_choice (widget, "80");
	gp_widget_add_choice (widget, "100");
	gp_widget_add_choice (widget, "200");
	gp_widget_add_choice (widget, "400");
	gp_widget_add_choice (widget, "800");
	gp_widget_add_choice (widget, "1600");
	gp_widget_add_choice (widget, "3200");
	gp_widget_add_choice (widget, "6400");
	gp_widget_add_choice (widget, "12800");
	gp_widget_append (section, widget);

	valset = 2;
	gp_widget_new (GP_WIDGET_TOGGLE, _("Bulb"), &widget);
	gp_widget_set_name (widget, "bulb");
	gp_widget_set_value (widget, &valset);
	gp_widget_append (section, widget);

	valset = 2;
	gp_widget_new (GP_WIDGET_TOGGLE, _("Movie"), &widget);
	gp_widget_set_name (widget, "movie");
	gp_widget_set_value (widget, &valset);
	gp_widget_append (section, widget);


	gp_widget_new (GP_WIDGET_TEXT, _("Autofocus Mode"), &widget);
	gp_widget_set_name (widget, "afmode");
	gp_widget_set_value (widget, Get_AFMode(camera));
	gp_widget_append (section, widget);

	gp_widget_new (GP_WIDGET_TEXT, _("Liveview Size"), &widget);
	gp_widget_set_name (widget, "liveviewsize");
	gp_widget_set_value (widget, Get_LiveViewSize(camera));
	gp_widget_append (section, widget);

	gp_widget_new (GP_WIDGET_TEXT, _("Device Name"), &widget);
	gp_widget_set_name (widget, "devicename");
	gp_widget_set_value (widget, Get_DeviceName(camera));
	gp_widget_append (section, widget);

	gp_widget_new (GP_WIDGET_TEXT, _("Focus Mode"), &widget);
	gp_widget_set_name (widget, "focusmode");
	gp_widget_set_value (widget, Get_FocusMode(camera));
	gp_widget_append (section, widget);

	gp_widget_new (GP_WIDGET_TEXT, _("MF Assist"), &widget);
	gp_widget_set_name (widget, "mf_assist");
	gp_widget_set_value (widget, Get_MFAssist(camera));
	gp_widget_append (section, widget);

	gp_widget_new (GP_WIDGET_TEXT, _("MF Assist Mag"), &widget);
	gp_widget_set_name (widget, "mf_assist_mag");
	gp_widget_set_value (widget, Get_MFAssist_Mag(camera));
	gp_widget_append (section, widget);

	gp_widget_new (GP_WIDGET_TEXT, _("Ex Teleconv"), &widget);
	gp_widget_set_name (widget, "ex_tele_conv");
	gp_widget_set_value (widget, Get_ExTeleConv(camera));
	gp_widget_append (section, widget);

	gp_widget_new (GP_WIDGET_TEXT, _("Lens"), &widget);
	gp_widget_set_name (widget, "lens");
	gp_widget_set_value (widget, Get_Lens(camera));
	gp_widget_append (section, widget);

	gp_widget_new (GP_WIDGET_RADIO, _("Zoom"), &widget);
	gp_widget_set_name (widget, "zoom");
	gp_widget_set_value (widget, "none");
	gp_widget_add_choice (widget, "wide-fast");
	gp_widget_add_choice (widget, "wide-normal");
	gp_widget_add_choice (widget, "tele-normal");
	gp_widget_add_choice (widget, "tele-fast");
	gp_widget_add_choice (widget, "stop");
	gp_widget_append (section, widget);

#if 0
	gp_widget_new (GP_WIDGET_TEXT, _("Capability"), &widget);
	gp_widget_set_name (widget, "capability");
	gp_widget_set_value (widget, Get_Capability(camera));
	gp_widget_append (section, widget);

	/* lots of stuff ... lets see if we can use it */
	gp_widget_new (GP_WIDGET_TEXT, _("All Menu"), &widget);
	gp_widget_set_name (widget, "allmenu");
	gp_widget_set_value (widget, Get_AllMenu(camera));
	gp_widget_append (section, widget);

	gp_widget_new (GP_WIDGET_TEXT, _("Cur Menu"), &widget);
	gp_widget_set_name (widget, "curmenu");
	gp_widget_set_value (widget, Get_CurMenu(camera));
	gp_widget_append (section, widget);
#endif

	return GP_OK;
}

static int
camera_config_set (Camera *camera, CameraWidget *window, GPContext *context) 
{
	CameraWidget	*widget;
	char		*val;
	char		buf[50];
	int		i, ret;

	if ((GP_OK == gp_widget_get_child_by_name(window, "zoom", &widget)) && gp_widget_changed (widget)) {
		if (GP_OK != (ret = gp_widget_get_value (widget, &val)))
			return ret;

		/* all valid values */
		if (	strcmp(val, "wide-fast")	&&
			strcmp(val, "wide-normal")	&&
			strcmp(val, "tele-normal")	&&
			strcmp(val, "tele-fast")	&&
			strcmp(val, "stop")
		)
			return GP_ERROR_BAD_PARAMETERS;

		if (!strcmp(val, "stop"))
			val = "zoomstop";

		sprintf(buf,"cam.cgi?mode=camcmd&value=%s", val);
		loadCmd(camera,buf);
	}

	if ((GP_OK == gp_widget_get_child_by_name(window, "shutterspeed", &widget)) && gp_widget_changed (widget)) {
		char *cameraspeed = NULL;

		if (GP_OK != (ret = gp_widget_get_value (widget, &val)))
			return ret;
		for (i=0;i<sizeof(shutterspeeds)/sizeof(shutterspeeds[0]);i++) {
			if (!strcmp(val, shutterspeeds[i].speed)) {
				cameraspeed = shutterspeeds[i].cameraspeed;
				break;
			}
		}
		if (!cameraspeed) cameraspeed = val;
		Set_ShutterSpeed (camera, cameraspeed);
	}

	if ((GP_OK == gp_widget_get_child_by_name(window, "aperture", &widget)) && gp_widget_changed (widget)) {
		char *cameraaperture = NULL;

		if (GP_OK != (ret = gp_widget_get_value (widget, &val)))
			return ret;
		for (i=0;i<sizeof(apertures)/sizeof(apertures[0]);i++) {
			if (!strcmp(val, apertures[i].aperture)) {
				cameraaperture = apertures[i].cameraaperture;
				break;
			}
		}
		if (!cameraaperture) cameraaperture = val;
		sprintf(buf,"cam.cgi?mode=setsetting&type=focal&value=%s", cameraaperture);
		loadCmd(camera,buf);
	}
	if ((GP_OK == gp_widget_get_child_by_name(window, "iso", &widget)) && gp_widget_changed (widget)) {
		char buf[50];

		if (GP_OK != (ret = gp_widget_get_value (widget, &val)))
			return ret;
		sprintf(buf,"cam.cgi?mode=setsetting&type=iso&value=%s", val);
		loadCmd(camera,buf);
	}
	if ((GP_OK == gp_widget_get_child_by_name(window, "liveviewsize", &widget)) && gp_widget_changed (widget)) {
		char buf[50];

		if (GP_OK != (ret = gp_widget_get_value (widget, &val)))
			return ret;
		sprintf(buf,"cam.cgi?mode=setsetting&type=liveviewsize&value=%s", val);
		loadCmd(camera,buf);
	}
	if ((GP_OK == gp_widget_get_child_by_name(window, "devicename", &widget)) && gp_widget_changed (widget)) {
		char buf[50];

		if (GP_OK != (ret = gp_widget_get_value (widget, &val)))
			return ret;
		sprintf(buf,"cam.cgi?mode=setsetting&type=device_name&value=%s", val);
		loadCmd(camera,buf);
	}
	if ((GP_OK == gp_widget_get_child_by_name(window, "bulb", &widget)) && gp_widget_changed (widget)) {
		int	val;

		if (GP_OK != (ret = gp_widget_get_value (widget, &val)))
			return ret;

		if (val) {
			ret = startCapture (camera);
			if (ret != GP_OK)
				return ret;
		} else {
			stopCapture(camera);
		}
	}
	if ((GP_OK == gp_widget_get_child_by_name(window, "movie", &widget)) && gp_widget_changed (widget)) {
		int	val;

		if (GP_OK != (ret = gp_widget_get_value (widget, &val)))
			return ret;

		if (val) {
			startMovie (camera);
		} else {
			stopMovie(camera);
		}
	}
	return GP_OK;
}



static char*
processNode(xmlTextReaderPtr reader) {
	char* ret =""; 
	char* lookupImgtag="";

	switch (ReadoutMode) {
	case 0 : //'jpg
		lookupImgtag = "CAM_RAW_JPG";
	break; 
	case 1 :// 'raw
		lookupImgtag = "CAM_RAW";
	break;
	case 2  ://'thumb
		lookupImgtag = "CAM_LRGTN";
	break;
	}

    const xmlChar *name, *value;

    name = xmlTextReaderConstName(reader);
    if (name == NULL)
	name = BAD_CAST "--";

    if (xmlTextReaderNodeType(reader) == 1) {  // Element
	while (xmlTextReaderMoveToNextAttribute(reader)) {
		if (strend((char*)xmlTextReaderConstValue(reader),lookupImgtag)) {
			xmlTextReaderRead(reader);
			ret = (char*)xmlTextReaderConstValue(reader);
			printf("the image file is %s\n" ,ret);
		}
	}
    }
    return ret;
}

/*

below are 2 samples of XML files to be parsed one in case of a JPG/Thumb retrieve the other for RAW

case JPG/Thumb

<?xml version="1.0"?>
-<s:Envelope s:encodingStyle="http://schemas.xmlsoap.org/soap/encoding/" xmlns:s="http://schemas.xmlsoap.org/soap/envelope/">
-<s:Body>
-<u:BrowseResponse xmlns:u="urn:schemas-upnp-org:service:ContentDirectory:1">
-<Result>
-<DIDL-Lite xmlns:upnp="urn:schemas-upnp-org:metadata-1-0/upnp/" xmlns:dc="http://purl.org/dc/elements/1.1/" xmlns="urn:schemas-upnp-org:metadata-1-0/DIDL-Lite/">
-<item restricted="0" parentID="0" id="1030516">
<dc:title>103-0516</dc:title>
<upnp:writeStatus>WRITABLE</upnp:writeStatus>
<upnp:class name="imageItem">object.item.imageItem</upnp:class>
<res protocolInfo="http-get:*:application/octet-stream;PANASONIC.COM_PN=CAM_RAW_JPG">http://192.168.1.26:50001/DO1030516.JPG</res>
<res protocolInfo="http-get:*:image/jpeg:DLNA.ORG_PN=JPEG_TN;DLNA.ORG_OP=01;DLNA.ORG_CI=1;DLNA.ORG_FLAGS=00900000000000000000000000000000;PANASONIC.COM_PN=CAM_TN">http://192.168.1.26:50001/DT1030516.JPG</res>
<res protocolInfo="http-get:*:image/jpeg:DLNA.ORG_PN=JPEG_MED;DLNA.ORG_OP=01;DLNA.ORG_CI=1;DLNA.ORG_FLAGS=00900000000000000000000000000000;PANASONIC.COM_PN=CAM_LRGTN">http://192.168.1.26:50001/DL1030516.JPG</res>
<res protocolInfo="http-get:*:application/octet-stream;PANASONIC.COM_PN=CAM_RAW">http://192.168.1.26:50001/DO1030516.RW2</res>
</item>
</DIDL-Lite>
</Result>
<NumberReturned>1</NumberReturned>
<TotalMatches>343</TotalMatches>
<UpdateID>1</UpdateID>
</u:BrowseResponse>
</s:Body>
</s:Envelope>

case RAW:
<?xml version="1.0"?>
-<s:Envelope s:encodingStyle="http://schemas.xmlsoap.org/soap/encoding/" xmlns:s="http://schemas.xmlsoap.org/soap/envelope/">
-<s:Body>
-<u:BrowseResponse xmlns:u="urn:schemas-upnp-org:service:ContentDirectory:1">
-<Result>
-<DIDL-Lite xmlns:upnp="urn:schemas-upnp-org:metadata-1-0/upnp/" xmlns:dc="http://purl.org/dc/elements/1.1/" xmlns="urn:schemas-upnp-org:metadata-1-0/DIDL-Lite/">
-<item restricted="0" parentID="0" id="1030517">
<dc:title>103-0517</dc:title>
<upnp:writeStatus>WRITABLE</upnp:writeStatus>
<upnp:class name="imageItem">object.item.imageItem</upnp:class>
<res protocolInfo="http-get:*:application/octet-stream;PANASONIC.COM_PN=CAM_RAW">http://192.168.1.26:50001/DO1030517.RAW</res>
<res protocolInfo="http-get:*:image/jpeg:DLNA.ORG_PN=JPEG_TN;DLNA.ORG_OP=01;DLNA.ORG_CI=1;DLNA.ORG_FLAGS=00900000000000000000000000000000;PANASONIC.COM_PN=CAM_TN">http://192.168.1.26:50001/DT1030517.JPG</res>
<res protocolInfo="http-get:*:image/jpeg:DLNA.ORG_PN=JPEG_MED;DLNA.ORG_OP=01;DLNA.ORG_CI=0;DLNA.ORG_FLAGS=00900000000000000000000000000000;PANASONIC.COM_PN=CAM_LRGTN">http://192.168.1.26:50001/DL1030517.JPG</res>
</item>
</DIDL-Lite>
</Result>
<NumberReturned>1</NumberReturned>
<TotalMatches>344</TotalMatches>
<UpdateID>1</UpdateID>
</u:BrowseResponse>
</s:Body>
</s:Envelope>

*/
static int
add_objectid_and_upload (Camera *camera, CameraFilePath *path, GPContext *context, char *ximage, size_t size) {
        int                     ret;
        CameraFile              *file = NULL;
        CameraFileInfo          info;

        ret = gp_file_new(&file);
        if (ret!=GP_OK) return ret;
        gp_file_set_mtime (file, time(NULL));
        strcpy(info.file.type, GP_MIME_JPEG);

        GP_LOG_D ("setting size");
        ret = gp_file_set_data_and_size(file, ximage, size);
        if (ret != GP_OK) {
                gp_file_free (file);
                return ret;
        }
        GP_LOG_D ("append to fs");
        ret = gp_filesystem_append(camera->fs, path->folder, path->name, context);
        if (ret != GP_OK) {
                gp_file_free (file);
                return ret;
        }
        GP_LOG_D ("adding filedata to fs");
        ret = gp_filesystem_set_file_noop(camera->fs, path->folder, path->name, GP_FILE_TYPE_NORMAL, file, context);
        if (ret != GP_OK) {
                gp_file_free (file);
                return ret;
        }

        /* We have now handed over the file, disclaim responsibility by unref. */
        gp_file_unref (file);

        /* we also get the fs info for free, so just set it */
        info.file.fields = GP_FILE_INFO_TYPE |
                        /*GP_FILE_INFO_WIDTH | GP_FILE_INFO_HEIGHT |*/
                        GP_FILE_INFO_SIZE /*| GP_FILE_INFO_MTIME */ ;
        strcpy(info.file.type, GP_MIME_JPEG);

        info.preview.fields = 0; /* so far none */
/*
        info.file.width         = oi->ImagePixWidth;
        info.file.height        = oi->ImagePixHeight;
        info.file.size          = oi->ObjectCompressedSize;
        info.file.mtime         = time(NULL);

        info.preview.fields = GP_FILE_INFO_TYPE |
                        GP_FILE_INFO_WIDTH | GP_FILE_INFO_HEIGHT |
                        GP_FILE_INFO_SIZE;
        strcpy_mime (info.preview.type, params->deviceinfo.VendorExtensionID, oi->ThumbFormat);
        info.preview.width      = oi->ThumbPixWidth;
        info.preview.height     = oi->ThumbPixHeight;
        info.preview.size       = oi->ThumbCompressedSize;
*/

        GP_LOG_D ("setting fileinfo in fs");
        return gp_filesystem_set_info_noop(camera->fs, path->folder, path->name, info, context);
}

static int
ReadImageFromCamera(Camera *camera, CameraFilePath *path, GPContext *context) {
	char			*image;
	long			nRead;
	LumixMemoryBuffer	lmb;
	char			*xmlimageName;
	xmlDocPtr		doc; /* the resulting document tree */
	int			ret;
	char			* imageURL="";
	xmlTextReaderPtr	reader;

	switchToPlayMode (camera);

	reader = xmlReaderForDoc((xmlChar*)GetPixRange(camera,NumberPix(camera)-1,1), NULL,"noname.xml", XML_PARSE_DTDATTR |  /* default DTD attributes */ XML_PARSE_NOENT); 
	ret = xmlTextReaderRead(reader);
	while (ret == 1) {
	    imageURL = processNode(reader); 
            if (strlen(imageURL))
		break; 
            ret = xmlTextReaderRead(reader);
	}

	GP_DEBUG("the image URL is  %s\n",imageURL); 
	image = strdup(imageURL);
	nRead = 0;
	CURL* imageUrl;
	imageUrl = curl_easy_init();
	CURLcode res;
	double bytesread = 0;
	char filename[100];
	long http_response;
	int ret_val=0;


	while (ret_val!=2) {
		GP_DEBUG("reading stream %s position %ld", image, nRead);

		curl_easy_setopt(imageUrl, CURLOPT_URL, image);
		//curl_easy_setopt(imageUrl,CURLOPT_TCP_KEEPALIVE,1L);
		//curl_easy_setopt(imageUrl, CURLOPT_TCP_KEEPIDLE, 120L);
		//curl_easy_setopt(imageUrl, CURLOPT_TCP_KEEPINTVL, 60L);

#if 0
		/* FIXME trick wont work anymore with new buffering code -Marcus */
		if (nRead) {
			curl_easy_setopt(imageUrl, CURLOPT_RESUME_FROM, nRead);
			GetPix(camera,1);//'if the file not found happened then this trick is to get the camera in a readmode again and making sure it remembers the filename
			GP_DEBUG("continuing the read where it stopped %s  position %ld", image,  nRead);
		}
#endif
		lmb.size = 0;
		lmb.data = malloc(0);
		curl_easy_setopt(imageUrl, CURLOPT_WRITEFUNCTION, write_callback);
		curl_easy_setopt(imageUrl, CURLOPT_WRITEDATA, &lmb);

		res = curl_easy_perform(imageUrl);

		if (res != CURLE_OK) {
			GP_LOG_E("curl_easy_perform() failed: %s", curl_easy_strerror(res));
			GP_DEBUG("error in reading stream %s  position %ld", image,  nRead);
			curl_easy_getinfo(imageUrl, CURLINFO_RESPONSE_CODE, &http_response);
			GP_DEBUG("CURLINFO_RESPONSE_CODE:%ld\n", http_response);
		} else {
			GP_DEBUG("read the whole file");
			ret_val=2;
		}
	}
	curl_easy_cleanup(imageUrl);
	strcpy(path->name, strrchr(image,'/')+1);
	strcpy(path->folder, "/");
	return add_objectid_and_upload (camera, path, context, lmb.data, lmb.size);
}


/**
* Capture an image and tell libgphoto2 where to find it by filling
* out the path.
*
* This function is a method of the Camera object.
*/
static int
camera_capture (Camera *camera, CameraCaptureType type, CameraFilePath *path, GPContext *context) {
	int	ret, tries, before, after;
	char	*s, *url;


	tries = 10;
	do {
		ret = NumberPix(camera);
		if (ret == GP_ERROR_CAMERA_BUSY)
			sleep(1);
	} while ((ret == GP_ERROR_CAMERA_BUSY) && (tries--));

	if (ret < GP_OK)
		return ret;

	before = ret;
	GP_LOG_D("numberpix before=%d", ret);

	switchToRecMode (camera);

	sleep(2);

	ret = startCapture (camera);
	if (ret != GP_OK)
		return ret;


	if (strcmp(cameraShutterSpeed, "B")!=0) {  
		sleep(captureDuration); // Sleep for the duration to simulate exposure, if this is in Bulb mode 
	} else {
		sleep(3);
	}
	stopCapture(camera);

	tries = 10;
	do {
		ret = NumberPix(camera);
		if (ret == GP_ERROR_CAMERA_BUSY)
			sleep(1);
	} while ((ret == GP_ERROR_CAMERA_BUSY) && (tries--));

	if (ret < GP_OK)
		return ret;

	after = ret;
	GP_LOG_D("numberpix after=%d", ret);

	if (after > before)
		GetPixRange(camera,before,after-before);
	/* handle case where we have more than one picture, otherwise we just take one */
	url = "unknown";
	if (camera->pl->pics[after-1].url_large) url = camera->pl->pics[after-1].url_large;
	if (camera->pl->pics[after-1].url_raw) url = camera->pl->pics[after-1].url_raw;
	s = strrchr(url,'/')+1;
	strcpy(path->name, s);
	strcpy(path->folder, "/");
	return GP_OK;
}

/**
* Fill out the summary with textual information about the current 
* state of the camera (like pictures taken, etc.).
*
* This function is a method of the Camera object.
*/
static int
camera_summary (Camera *camera, CameraText *summary, GPContext *context)
{
	return GP_OK;
}


/**
* Return the camera drivers manual.
* If you would like to tell the user some information about how 
* to use the camera or the driver, this is the place to do.
*
* This function is a method of the Camera object.
*/
static int camera_manual (Camera *camera, CameraText *manual, GPContext *context) {
	return GP_OK;
}


/**
* Return "About" content as textual description.
* Will be translated.
*
* This function is a method of the Camera object.
*/
int  camera_about (Camera *camera, CameraText *about, GPContext *context);
int camera_about (Camera *camera, CameraText *about, GPContext *context) {
	strcpy (about->text, _("Lumix WiFi Library\n"
	"Robert Hasson <robert_hasson@yahoo.com>\n"
	"Connects to Lumix Cameras over Wifi.\n"
	"using the http GET commands."));
	return GP_OK;
}

/*@}*/


/**********************************************************************/
/**
* @name CameraFilesystem member functions
*
* @{
*/
/**********************************************************************/


/**
* Get the file from the camera.
*
* This function is a CameraFilesystem method.
*/
static int
get_file_func (CameraFilesystem *fs, const char *folder, const char *filename, CameraFileType type, CameraFile *file, void *data, GPContext *context)
{
	Camera		*camera = data;
	int		i;
	CURLcode	res;
	CURL		*imageUrl;
	double		bytesread = 0;
	long		http_response;
	int		ret_val = 0;
	long			nRead;
	LumixMemoryBuffer	lmb;
	const char	*url;

	for (i=0;i<camera->pl->numpics;i++) {
		char *s;

		if (camera->pl->pics[i].url_movie) {
			s = strrchr(camera->pl->pics[i].url_movie,'/')+1;
			if (!strcmp(s,filename)) {
				url = camera->pl->pics[i].url_movie;
				break;
			}
		}
		if (camera->pl->pics[i].url_raw) {
			s = strrchr(camera->pl->pics[i].url_raw,'/')+1;
			if (!strcmp(s,filename)) {
				url = camera->pl->pics[i].url_raw;
				break;
			}
		}
		if (camera->pl->pics[i].url_large) {
			s = strrchr(camera->pl->pics[i].url_large,'/')+1;
			if (!strcmp(s,filename)) {
				url = camera->pl->pics[i].url_large;
				break;
			}
		}
	}
	if (i == camera->pl->numpics) /* not found */
		return GP_ERROR;

	switch (type) {
	case GP_FILE_TYPE_PREVIEW:
		if (camera->pl->pics[i].url_thumb)
			url = camera->pl->pics[i].url_thumb;
		break;
	case GP_FILE_TYPE_NORMAL:
	default:
		/* just use regular url */
		break;
	}

	switchToPlayMode (camera);

	imageUrl = curl_easy_init();

	while (ret_val != 2) {
		GP_DEBUG("reading stream %s position %ld", url, nRead);

		curl_easy_setopt(imageUrl, CURLOPT_URL, url);
		//curl_easy_setopt(imageUrl,CURLOPT_TCP_KEEPALIVE,1L);
		//curl_easy_setopt(imageUrl, CURLOPT_TCP_KEEPIDLE, 120L);
		//curl_easy_setopt(imageUrl, CURLOPT_TCP_KEEPINTVL, 60L);

#if 1
		/* FIXME trick wont work anymore with new buffering code -Marcus */
		if (nRead) {
			curl_easy_setopt(imageUrl, CURLOPT_RESUME_FROM, nRead);
			GetPixRange(camera,NumberPix(camera)-1,1);//'if the file not found happened then this trick is to get the camera in a readmode again and making sure it remembers the filename
			GP_DEBUG("continuing the read where it stopped %s  position %ld", url,  nRead);
		}
#endif
		lmb.size = 0;
		lmb.data = malloc(0);
		curl_easy_setopt(imageUrl, CURLOPT_WRITEFUNCTION, write_callback);
		curl_easy_setopt(imageUrl, CURLOPT_WRITEDATA, &lmb);

		res = curl_easy_perform(imageUrl);

		if (res != CURLE_OK) {
			GP_LOG_E("curl_easy_perform() failed: %s", curl_easy_strerror(res));
			GP_DEBUG("error in reading stream %s  position %ld", url,  nRead);
			curl_easy_getinfo(imageUrl, CURLINFO_RESPONSE_CODE, &http_response);
			GP_DEBUG("CURLINFO_RESPONSE_CODE:%ld\n", http_response);
			return GP_ERROR_IO;
		} else {
			GP_DEBUG("read the whole file");
			ret_val=2;
		}
	}
	curl_easy_cleanup(imageUrl);

	return gp_file_set_data_and_size (file, lmb.data, lmb.size);
}


int camera_abilities (CameraAbilitiesList *list) {
	CameraAbilities a;

	memset(&a, 0, sizeof(a));
	strcpy(a.model, "Panasonic:LumixGSeries");
	a.status	= GP_DRIVER_STATUS_EXPERIMENTAL;
	a.port		= GP_PORT_IP;
	a.operations	= GP_CAPTURE_IMAGE| GP_OPERATION_CAPTURE_VIDEO | GP_OPERATION_CONFIG;
	a.file_operations = GP_FILE_OPERATION_PREVIEW  ; 
	/* it should be possible to browse and DL images the files using the ReadImageFromCamera() function but for now lets keep it simple*/ 
	a.folder_operations = GP_FOLDER_OPERATION_NONE;
	return gp_abilities_list_append(list, a);
}

/**
* All filesystem accessor functions.
*
* This should contain all filesystem accessor functions
* available in the camera library. Non-present fields
* are NULL.
*
*/
CameraFilesystemFuncs fsfuncs = {
	.file_list_func = file_list_func,
	.folder_list_func = folder_list_func,
//	.get_info_func = get_info_func,
//	.set_info_func = set_info_func,
	.get_file_func = get_file_func,
//	.del_file_func = delete_file_func,
//	.put_file_func = put_file_func,
//	.delete_all_func = delete_all_func,
//	.storage_info_func = storage_info_func
};

/**
* Initialize a Camera object.
*
* Sets up all the proper object function pointers, initialize camlib
* internal data structures, and probably establish a connection to
* the camera.
*
* This is a camlib API function.
*/
int
camera_init (Camera *camera, GPContext *context) 
{
	GPPortInfo      info;
	int		ret;

	camera->pl = calloc(sizeof(CameraPrivateLibrary),1);

	/* First, set up all the function pointers */
	camera->functions->exit                 = camera_exit;
	camera->functions->get_config           = camera_config_get;
	camera->functions->set_config           = camera_config_set;
	camera->functions->capture              = camera_capture;
	camera->functions->capture_preview      = camera_capture_preview;
	camera->functions->summary              = camera_summary;
	camera->functions->manual               = camera_manual;
	camera->functions->about                = camera_about;

	LIBXML_TEST_VERSION

	curl_global_init(CURL_GLOBAL_ALL);

	ret = gp_port_get_info (camera->port, &info);
	if (ret != GP_OK) {
		GP_LOG_E ("Failed to get port info?");
		return ret;
	}
	gp_filesystem_set_funcs (camera->fs, &fsfuncs, camera);

/*
	startup code might need:

	loadCmd(camera,"cam.cgi?mode=accctrl&type=req_acc&value=4D454930-0100-1000-8001-020D0090325B&value2=GT-I9300");
	loadCmd(camera,"cam.cgi?mode=setsetting&type=device_name&value=GT-I9300");
*/

	if (switchToRecMode (camera) != NULL) {
		int numpix;

		Set_quality(camera,"raw_fine");

		switchToPlayMode (camera);

		numpix = NumberPix(camera);
		GetPixRange(camera,0,numpix);
		return GP_OK;
	} else
		return GP_ERROR_IO;
}


int
camera_id (CameraText *id) 
{
	strcpy(id->text, "Lumix Wifi");

	return GP_OK;
}
