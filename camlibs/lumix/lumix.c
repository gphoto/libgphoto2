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
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <stdlib.h>
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

char* NUMPIX  = "cam.cgi?mode=get_content_info";
char* RECMODE  = "cam.cgi?mode=camcmd&value=recmode";
char* PLAYMODE  = "cam.cgi?mode=camcmd&value=playmode";
char* AFMODE  = "cam.cgi?mode=camcmd&value=afmode";
char* SHUTTERSTART  = "cam.cgi?mode=camcmd&value=capture";
char* SHUTTERSTOP = "cam.cgi?mode=camcmd&value=capture_cancel";
char* SETAPERTURE  = "cam.cgi?mode=setsetting&type=focal&value=";
char* CDS_Control  = ":60606/Server0/CDS_control";
char* STARTSTREAM  = "cam.cgi?mode=startstream&value=49199";
char* CAMERAIP = "192.168.1.24"; //placeholder until there is a better way to discover the IP from the network and via PTPIP
int ReadoutMode = 2; // this should be picked up from the settings.... 0-> JPG; 1->RAW; 2 -> Thumbnails
char* cameraShutterSpeed = "B"; // //placeholder to store the value of the shutterspeed set in camera; "B" is for bulb.
int captureDuration = 10; //placeholder to store the value of the bulb shot this should be taken as input. note that my primary goal is in fact to perform bulb captures. but this should be extended for sure to take Shutter Speed capture as set in camera 

static int NumberPix(Camera *camera);
static char* loadCmd (Camera *camera,char* cmd);

typedef struct {
	char   *data;
	size_t 	size;
} LumixMemoryBuffer;

typedef struct {
	char	*id;
	char	*url_large;
	char	*url_medium;
	char	*url_thumb;
} LumixPicture;

struct _CameraPrivateLibrary {
	/* all private data */

	int		numpics;
	LumixPicture	*pics;

	int		liveview;
};


static int
camera_exit (Camera *camera, GPContext *context) 
{
	return GP_OK;
}

int camera_config_get (Camera *camera, CameraWidget **window, GPContext *context);

static int
camera_config_set (Camera *camera, CameraWidget *window, GPContext *context) 
{

	return GP_OK;
}


static int
camera_capture_preview (Camera *camera, CameraFile *file, GPContext *context)
{
	int			sock = 0, valread;
	struct sockaddr_in	serv_addr;
	unsigned char		buffer[65536];
	GPPortInfo      	info;
	char			*xpath;
	int			i, start, end, tries;

	loadCmd (camera, RECMODE);

	if (!camera->pl->liveview) {
		loadCmd(camera,"cam.cgi?mode=startstream&value=49199");
		camera->pl->liveview = 1;
		if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
			GP_LOG_E("\n Socket creation error \n");
			return GP_ERROR;
		}

		gp_port_get_info (camera->port, &info);
		gp_port_info_get_path (info, &xpath); /* xpath now contains tcp:192.168.1.1 */

		memset(&serv_addr, 0, sizeof(serv_addr));

		serv_addr.sin_family = AF_INET;
		serv_addr.sin_port = htons(49199);
		serv_addr.sin_addr.s_addr = 0;

		if (bind(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
			GP_LOG_E("bind Failed: %d", errno);
			return GP_ERROR;
		}
	} else {
		/* this reminds the camera we are still doing it */
		loadCmd(camera,"cam.cgi?mode=getstate");
	}
	tries = 3;
	while (tries--) {
		valread = recv ( sock , buffer, sizeof(buffer), 0);
		if (valread == -1) {
			GP_LOG_E("recv failed: %d", errno);
			return GP_ERROR;
		}

		GP_LOG_DATA(buffer,valread,"read from udp port");

		if (valread == 0)
			continue;

		start = end = -1;
		for (i = 0; i<valread-1; i++) {
			if ((buffer[i] == 0xff) && (buffer[i+1] == 0xd8)) {
				start = i;
			}
			if ((buffer[i] == 0xff) && (buffer[i+1] == 0xd9)) {
				end = i;
			}
		}
		//GP_LOG_DATA(buffer+start,end-start,"read from udp port");
		gp_file_set_mime_type (file, GP_MIME_JPEG);
		return gp_file_append (file, buffer+start, end-start);
	}
	return GP_ERROR;
}


//int camera_capture (Camera *camera, CameraCaptureType type, CameraFilePath *path, GPContext *context);

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
		if (camera->pl->pics[i].url_large) {
			char	*s = strrchr(camera->pl->pics[i].url_large, '/')+1;
			gp_list_append (list, s, NULL);
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


int
camera_id (CameraText *id) 
{
	strcpy(id->text, "Lumix Wifi");

	return GP_OK;
}

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
	int		stream = strcmp (cmd, STARTSTREAM);
	char		URL[100];
	GPPortInfo      info;
	char		*xpath;
	LumixMemoryBuffer	lmb;

	curl = curl_easy_init();
	gp_port_get_info (camera->port, &info);
	gp_port_info_get_path (info, &xpath); /* xpath now contains tcp:192.168.1.1 */
	snprintf( URL,100, "http://%s/%s", xpath+4, cmd);
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

	if (strcmp(cmd,PLAYMODE)==0) {
	}
	return lmb.data;
}

static void switchToRecMode(Camera *camera) {
	loadCmd(camera,"?mode=camcmd&value=recmode");
}

static void Set_ISO(Camera *camera,const char * ISOValue) {
	char buf[200];
	sprintf(buf, "?mode=setsetting&type=iso&value=%s",ISOValue);
	loadCmd(camera,buf);
}

static char*
Get_Clock(Camera *camera) {
	return loadCmd(camera,"cam.cgi?mode=getsetting&type=clock");
}

static char*
Get_ISO(Camera *camera) {
	return loadCmd(camera,"cam.cgi?mode=getsetting&type=iso");
}

static char*
Get_ShutterSpeed(Camera *camera) {
	return loadCmd(camera,"cam.cgi?mode=getsetting&type=shtrspeed");
}

static char*
Get_Aperture(Camera *camera) {
	return loadCmd(camera,"cam.cgi?mode=getsetting&type=focal");
}

static char*
Get_AFMode(Camera *camera) {
	return loadCmd(camera,"cam.cgi?mode=getsetting&type=afmode");
}

static char*
Get_FocusMode(Camera *camera) {
	return loadCmd(camera,"cam.cgi?mode=getsetting&type=focusmode");
}

static char*
Get_MFAssist(Camera *camera) {
	return loadCmd(camera,"cam.cgi?mode=getsetting&type=mf_asst");
}

static char*
Get_MFAssist_Mag(Camera *camera) {
	return loadCmd(camera,"cam.cgi?mode=getsetting&type=mf_asst_mag");
}

static char*
Get_ExTeleConv(Camera *camera) {
	return loadCmd(camera,"cam.cgi?mode=getsetting&type=ex_tele_conv");
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


static void Set_Speed(Camera *camera,const char* SpeedValue) {
	char buf[200];
	sprintf(buf, "?mode=setsetting&type=shtrspeed&value=%s",SpeedValue);
	loadCmd(camera,buf);
}

static char*
Get_Quality(Camera *camera) {
	return loadCmd(camera,"cam.cgi?mode=getsetting&type=quality");
}

static void
Set_quality(Camera *camera,const char* Quality) {
	char buf[200];
	sprintf(buf,"cam.cgi?mode=setsetting&type=quality&value=%s", Quality);
	loadCmd(camera,buf);
}


static void
shotPicture(Camera *camera) {
	loadCmd(camera,"?mode=camcmd&value=capture");
}

static void stopCapture(Camera *camera) {
	loadCmd(camera,"?mode=camcmd&value=capture_cancel");
}

static void startMovie(Camera *camera) {
	loadCmd(camera,"?mode=camcmd&value=video_recstart");
}

static void stopMovie(Camera *camera) {
	loadCmd(camera,"?mode=camcmd&value=video_recstop");
}

static void zoomIn(Camera *camera) {
	loadCmd(camera,"?mode=camcmd&value=tele-fast");
}

static void zoomOut(Camera *camera) {
	loadCmd(camera,"?mode=camcmd&value=wide-fast");
}

static void zoomStop(Camera *camera) {
	loadCmd(camera,"?mode=camcmd&value=zoomstop");
}

static void focusIn(Camera *camera) {
	loadCmd(camera,"?mode=camctrl&type=focus&value=tele-fast");
}

static void focusOut(Camera *camera) {
	loadCmd(camera,"?mode=camctrl&type=focus&value=wide-fast");
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
	xmlChar *keyz = NULL;
	int	numpics = 0;

	char *temp = loadCmd(camera,NUMPIX);
	xmlDocPtr doc = xmlParseDoc((unsigned char*) temp);
	xmlNodePtr cur = NULL; 
	cur = xmlDocGetRootElement(doc);   
	/*GP_LOG_D("NumberPix Decode current root node is %s \n", doc); */

	if (cur == NULL) {
		fprintf(stderr,"empty document\n");
		xmlFreeDoc(doc);
		return GP_ERROR;
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
	GP_LOG_D("NumberPix Found is %s \n", (char *) keyz);
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

static int
traverse_tree (int depth, xmlNodePtr node)
{
        xmlNodePtr      next;
        xmlChar         *xchar;
        int             n;
        char            *xx;


        if (!node) return 0;
        xx = malloc (depth * 4 + 1);
        memset (xx, ' ', depth*4);
        xx[depth*4] = 0;

        n = xmlChildElementCount (node);

        next = node;
        do {
                fprintf(stderr,"%snode %s\n", xx,next->name);
                fprintf(stderr,"%selements %d\n", xx,n);
                xchar = xmlNodeGetContent (next);
                fprintf(stderr,"%scontent %s\n", xx,xchar);
                traverse_tree (depth+1,xmlFirstElementChild (next));
        } while ((next = xmlNextElementSibling (next)));
        free (xx);
        return 1;
}


/*
 * retrieves the XML doc with the last num pictures avail in the camera.
 */
static char*
GetPix(Camera *camera,int num) {
	long		NumPix;
        xmlDocPtr       docin, docin2;
        xmlNodePtr      docroot, docroot2, output, next;
	xmlChar 	*xchar;
	char*		SoapMsg;
	CURL 		*curl;
	CURLcode	res;
	struct curl_slist *list = NULL;
	GPPortInfo      info;
	char		*xpath;
	LumixMemoryBuffer	lmb;
	char		URL[1000];

	loadCmd(camera,PLAYMODE);
	NumPix  = NumberPix(camera);
	//GP_LOG_D("NumPix is %d \n", NumPix);
	if (NumPix < GP_OK) return NULL;

	if (camera->pl->numpics < NumPix) {
		camera->pl->pics = realloc(camera->pl->pics,NumPix * sizeof(camera->pl->pics[0]));
		memset(camera->pl->pics+camera->pl->numpics, 0, NumPix * sizeof(camera->pl->pics[0]));
		camera->pl->numpics = NumPix;
	}

	SoapMsg = SoapEnvelop((NumPix - num)> 0?(NumPix - num):0, num);
	//GP_LOG_D("SoapMsg is %s \n", SoapMsg);

	curl = curl_easy_init();

	list = curl_slist_append(list, "SOAPaction: urn:schemas-upnp-org:service:ContentDirectory:1#Browse");
	list = curl_slist_append(list, "Content-Type: text/xml; charset=\"utf-8\"");
	list = curl_slist_append(list, "Accept: text/xml");

	gp_port_get_info (camera->port, &info);
	gp_port_info_get_path (info, &xpath); /* xpath now contains tcp:192.168.1.1 */
	snprintf( URL, 1000, "http://%s%s",xpath+4, CDS_Control);
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
		GP_LOG_E("expected DIDL-Lite, got %s", docroot->name);
		xmlFreeDoc (docin2);
		return NULL;
	}
	output = xmlFirstElementChild (docroot2);
	if (strcmp((char*)output->name,"item")) {
		GP_LOG_E("expected item, got %s", output->name);
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
*/
	next = output;
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

			if (strstr((char*)attrcontent,"DLNA.ORG_PN=JPEG_LRG")) {
				camera->pl->pics[i].url_large = strdup((char*)xmlNodeGetContent(child));
			}
			if (strstr((char*)attrcontent,"DLNA.ORG_PN=JPEG_TN")) {
				camera->pl->pics[i].url_medium = strdup((char*)xmlNodeGetContent(child));
			}
			if (strstr((char*)attrcontent,"DLNA.ORG_PN=JPEG_MED")) {
				camera->pl->pics[i].url_thumb = strdup((char*)xmlNodeGetContent(child));
			}
			child = child->next;
		}
        } while ((next = xmlNextElementSibling (next)));
	return strdup((char*)xchar);
}

/**
* Get the full configuration tree of the camera.
*
* This function is a method of the Camera object.
*  TO DO  - - i started to include some items to set the quality (raw and raw+jpg) as well as dictionary for ISO and shutter speed 
*  mostly every thing is still to do
*/
int camera_config_get (Camera *camera, CameraWidget **window, GPContext *context);
int
camera_config_get (Camera *camera, CameraWidget **window, GPContext *context) 
{
        CameraWidget *widget,*section;
        int ret;

	loadCmd (camera, RECMODE);

        gp_widget_new (GP_WIDGET_WINDOW, _("Lumix Configuration"), window);
        gp_widget_set_name (*window, "config");

	gp_widget_new (GP_WIDGET_SECTION, _("Camera Settings"), &section);
	gp_widget_set_name (section, "settings");
	gp_widget_append (*window, section);

	gp_widget_new (GP_WIDGET_TEXT, _("Clock"), &widget);
	gp_widget_set_name (widget, "clock");
	gp_widget_set_value (widget, Get_Clock(camera));
	gp_widget_append (section, widget);

	gp_widget_new (GP_WIDGET_TEXT, _("Shutterspeed"), &widget);
	gp_widget_set_name (widget, "shutterspeed");
	gp_widget_set_value (widget, Get_ShutterSpeed(camera));
	gp_widget_append (section, widget);

	gp_widget_new (GP_WIDGET_TEXT, _("Quality"), &widget);
	gp_widget_set_name (widget, "quality");
	gp_widget_set_value (widget, Get_Quality(camera));
	gp_widget_append (section, widget);

	gp_widget_new (GP_WIDGET_TEXT, _("Aperture"), &widget);
	gp_widget_set_name (widget, "aperture");
	gp_widget_set_value (widget, Get_Aperture(camera));
	gp_widget_append (section, widget);

	gp_widget_new (GP_WIDGET_TEXT, _("ISO"), &widget);
	gp_widget_set_name (widget, "iso");
	gp_widget_set_value (widget, Get_ISO(camera));
	gp_widget_append (section, widget);


	gp_widget_new (GP_WIDGET_TEXT, _("Autofocus Mode"), &widget);
	gp_widget_set_name (widget, "afmode");
	gp_widget_set_value (widget, Get_AFMode(camera));
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

	gp_widget_new (GP_WIDGET_TEXT, _("Capability"), &widget);
	gp_widget_set_name (widget, "capability");
	gp_widget_set_value (widget, Get_Capability(camera));
	gp_widget_append (section, widget);

#if 0
	/* lots of stuff */
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

	loadCmd(camera,PLAYMODE);	//   'making sure the camera is in Playmode
	char* xmlimageName;
	xmlDocPtr doc; /* the resulting document tree */
	int ret;
	LIBXML_TEST_VERSION

	char* imageURL="";
	xmlTextReaderPtr reader;
	reader = xmlReaderForDoc((xmlChar*)GetPix(camera,1), NULL,"noname.xml", XML_PARSE_DTDATTR |  /* default DTD attributes */ XML_PARSE_NOENT); 
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

		if (nRead) {
			curl_easy_setopt(imageUrl, CURLOPT_RESUME_FROM, nRead);
			GetPix(camera,1);//'if the file not found happened then this trick is to get the camera in a readmode again and making sure it remembers the filename
			GP_DEBUG("continuing the read where it stopped %s  position %ld", image,  nRead);
		}
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
int camera_capture (Camera *camera, CameraCaptureType type, CameraFilePath *path, GPContext *context);
int camera_capture (Camera *camera, CameraCaptureType type, CameraFilePath *path, GPContext *context){
	loadCmd(camera,SHUTTERSTART); //we should really multithread so as to not block while waiting

	if (strcmp(cameraShutterSpeed, "B")!=0) {  
		sleep(captureDuration); // Sleep for the duration to simulate exposure, if this is in Bulb mode 
	}

	loadCmd(camera,SHUTTERSTOP);
	return ReadImageFromCamera(camera, path, context);
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
		if (!camera->pl->pics[i].url_large) continue;

		s = strrchr(camera->pl->pics[i].url_large,'/')+1;

		if (!strcmp(s,filename)) break;
	}
	if (i == camera->pl->numpics) /* not found */
		return GP_ERROR;

	url = camera->pl->pics[i].url_large;
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

	loadCmd(camera,PLAYMODE);

	imageUrl = curl_easy_init();

	while (ret_val != 2) {
		GP_DEBUG("reading stream %s position %ld", url, nRead);

		curl_easy_setopt(imageUrl, CURLOPT_URL, url);
		//curl_easy_setopt(imageUrl,CURLOPT_TCP_KEEPALIVE,1L);
		//curl_easy_setopt(imageUrl, CURLOPT_TCP_KEEPIDLE, 120L);
		//curl_easy_setopt(imageUrl, CURLOPT_TCP_KEEPINTVL, 60L);

		if (nRead) {
			curl_easy_setopt(imageUrl, CURLOPT_RESUME_FROM, nRead);
			GetPix(camera,1);//'if the file not found happened then this trick is to get the camera in a readmode again and making sure it remembers the filename
			GP_DEBUG("continuing the read where it stopped %s  position %ld", url,  nRead);
		}
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
	a.port		= GP_PORT_TCP;
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
.get_file_func = get_file_func, // maybe this works for liveview preview
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
	char            *xpath;
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

	curl_global_init(CURL_GLOBAL_ALL);

	ret = gp_port_get_info (camera->port, &info);
	if (ret != GP_OK) {
		GP_LOG_E ("Failed to get port info?");
		return ret;
	}
	gp_port_info_get_path (info, &xpath);
	/* xpath now contains tcp:192.168.1.1 */

	gp_filesystem_set_funcs (camera->fs, &fsfuncs, camera);

	if (loadCmd(camera,RECMODE) != NULL) {
		int numpix;

		Set_quality(camera,"raw_fine");

		loadCmd(camera,PLAYMODE);             //   'making sure the camera is in Playmode

		numpix = NumberPix(camera);
		GetPix(camera,numpix);

		return GP_OK;
	} else
		return GP_ERROR_IO;
}
