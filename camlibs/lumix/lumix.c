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
char* SHUTTERSTART  = "cam.cgi?mode=camcmd&value=capture";
char* SHUTTERSTOP = "cam.cgi?mode=camcmd&value=capture_cancel";
char* SETAPERTURE  = "cam.cgi?mode=setsetting&type=focal&value=";
char* QUALITY = "cam.cgi?mode=setsetting&type=quality&value=";
char* CDS_Control  = ":60606/Server0/CDS_control";
char* STARTSTREAM  = "cam.cgi?mode=startstream&value=49199";
char* CAMERAIP = "192.168.1.24"; //placeholder until there is a better way to discover the IP from the network and via PTPIP
int ReadoutMode = 2; // this should be picked up from the settings.... 0-> JPG; 1->RAW; 2 -> Thumbnails
char* cameraShutterSpeed = "B"; // //placeholder to store the value of the shutterspeed set in camera; "B" is for bulb.
int captureDuration = 10; //placeholder to store the value of the bulb shot this should be taken as input. note that my primary goal is in fact to perform bulb captures. but this should be extended for sure to take Shutter Speed capture as set in camera 

static int NumberPix(Camera *camera);

typedef struct {
	char   *data;
	size_t 	size;
} LumixMemoryBuffer;

struct _CameraPrivateLibrary {
	/* all private data */
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
	return GP_OK;
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
int
file_list_func (CameraFilesystem *fs, const char *folder, CameraList *list,
		void *data, GPContext *context);
int
file_list_func (CameraFilesystem *fs, const char *folder, CameraList *list,
		void *data, GPContext *context)
{
	Camera *camera = data;

	int	numpix =  NumberPix(camera);

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

static char *replaceWord(const char *s, const char *oldW, 
                                 const char *newW) 
{ 
    char *result; 
    int i, cnt = 0; 
    int newWlen = strlen(newW); 
    int oldWlen = strlen(oldW); 
  
    // Counting the number of times old word 
    // occur in the string 
    for (i = 0; s[i] != '\0'; i++) 
    { 
        if (strstr(&s[i], oldW) == &s[i]) 
        { 
            cnt++; 
  
            // Jumping to index after the old word. 
            i += oldWlen - 1; 
        } 
    } 
  
    // Making new string of enough length 
    result = (char *)malloc(i + cnt * (newWlen - oldWlen) + 1); 
  
    i = 0; 
    while (*s) 
    { 
        // compare the substring with the result 
        if (strstr(s, oldW) == s) 
        { 
            strcpy(&result[i], newW); 
            i += newWlen; 
            s += oldWlen; 
        } 
        else
            result[i++] = *s++; 
    } 
  
    result[i] = '\0'; 
    return result; 
} 

static size_t
write_callback(char *contents, size_t size, size_t nmemb, void *userp)
{
	size_t 			realsize = size * nmemb;
	size_t 			oldsize;
	LumixMemoryBuffer	*lmb = userp;

	oldsize = lmb->size;
	lmb->data = realloc(lmb->data, lmb->size+realsize);
	lmb->size += realsize;

	gp_log (GP_LOG_DATA, "read from camera", contents, realsize);

	memcpy(lmb->data+oldsize,contents,realsize);
	return realsize;
}


static char* loadCmd (Camera *camera,char* cmd) {
    CURL *curl;
	CURLcode res;
	int  stream = strcmp (cmd, STARTSTREAM);
	char URL[100];
	GPPortInfo      info;
	char *xpath;
	LumixMemoryBuffer	lmb;
	
	curl = curl_easy_init();
	gp_port_get_info (camera->port, &info);
	gp_port_info_get_path (info, &xpath); /* xpath now contains tcp:192.168.1.1 */
	snprintf( URL,100, "http://%s/%s", xpath+4, cmd);
	GP_LOG_D("cam url is %s", URL);

	curl_easy_setopt(curl, CURLOPT_URL, URL);

	if (stream!=0) {
		lmb.size = 0;
		lmb.data = malloc(0);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &lmb);
	}

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

	if (stream != 0) {
		if (strcmp(cmd,PLAYMODE)==0) {
		}
		return lmb.data;
	} else {
		return NULL;
	} // TO DO the idea would be to return the socket to the actual video stream here....
}

static void switchToRecMode(Camera *camera) {
	loadCmd(camera,"?mode=camcmd&value=recmode");
}

static void Set_ISO(Camera *camera,const char * ISOValue) {
	char buf[200];
	sprintf(buf, "?mode==setsetting&type=iso&value=%s",ISOValue);
	loadCmd(camera,buf);
}

static void Set_Speed(Camera *camera,const char* SpeedValue) {
	char buf[200];
	sprintf(buf, "?mode=setsetting&type=shtrspeed&value=%s",SpeedValue);
	loadCmd(camera,buf);
}

static void Set_quality(Camera *camera,const char* Quality) {
	char buf[200];
	sprintf(buf,"%s%s", QUALITY,Quality);
	loadCmd(camera,buf);
}


static void startStream(Camera *camera) {
	loadCmd(camera,"?mode=startstream&value=49199");
}

static void shotPicture(Camera *camera) {
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
	if (!keyz)
		return GP_ERROR;
	GP_LOG_D("NumberPix Found is %s \n", (char *) keyz);
	return strtol((char*)keyz, NULL, 10);
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
static char*  GetPix(Camera *camera,int num) {
	loadCmd(camera,PLAYMODE);
	int Start  = 0; 
	long NumPix  = NumberPix(camera);
	//GP_LOG_D("NumPix is %d \n", NumPix);
        xmlDocPtr       docin;
        xmlNodePtr      docroot, output, next;
	xmlChar  *xchar;

	if (NumPix < GP_OK) return NULL;

	char* SoapMsg = SoapEnvelop((NumPix - num)> 0?(NumPix - num):0, num);
	//GP_LOG_D("SoapMsg is %s \n", SoapMsg);
	
	CURL *curl;
	curl = curl_easy_init();
	CURLcode res;
	struct curl_slist *list = NULL;
	GPPortInfo      info;
	char *xpath;
	LumixMemoryBuffer	lmb;

	list = curl_slist_append(list, "SOAPaction: urn:schemas-upnp-org:service:ContentDirectory:1#Browse");
	list = curl_slist_append(list, "Content-Type: text/xml; charset=\"utf-8\"");
	list = curl_slist_append(list, "Accept: text/xml");
	
	char URL[1000];
	gp_port_get_info (camera->port, &info);
	gp_port_info_get_path (info, &xpath); /* xpath now contains tcp:192.168.1.1 */
	snprintf( URL,1000, "http://%s%s",xpath+4, CDS_Control);
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
	
	res = curl_easy_perform(curl);
	if(res != CURLE_OK) {
		fprintf(stderr, "curl_easy_perform() failed: %s\n",
		curl_easy_strerror(res));
		curl_easy_cleanup(curl);
		return NULL;
	}
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
	curl_easy_cleanup(curl);
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
	return GP_OK;
}

static int
waitBulb(Camera *camera, long Duration ) {
	sleep(Duration); // Sleep for the duration to simulate exposure, if this is in Bulb mode 
	loadCmd(camera,SHUTTERSTOP);
	return TRUE;
}

static size_t dl_write(void *buffer, size_t size, size_t nmemb, void *stream)
{    
	return fwrite(buffer, size, nmemb, (FILE*)stream); 
}

static char* processNode(xmlTextReaderPtr reader) {
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
		if (strend(xmlTextReaderConstValue(reader),lookupImgtag)) {
			xmlTextReaderRead(reader);
			ret = xmlTextReaderConstValue(reader);
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
static CameraFilePath * ReadImageFromCamera(Camera *camera) {
	char *Images;				//the array of the urls in the camera
	long nRead;
	int SendStatus  = -1; 
	int length  = 0; 

	loadCmd(camera,PLAYMODE);		//   'making sure the camera is in Playmode
	char* xmlimageName;
	xmlDocPtr doc; /* the resulting document tree */
	int ret;
	LIBXML_TEST_VERSION
		
	char* imageURL="";
	xmlTextReaderPtr reader;
	reader = xmlReaderForDoc(GetPix(camera,1), NULL,"noname.xml",
        XML_PARSE_DTDATTR |  /* default DTD attributes */
		XML_PARSE_NOENT); 
	ret = xmlTextReaderRead(reader);
	while (ret == 1) {
	    imageURL = processNode(reader); 
            if (strlen(imageURL))
		break; 
            ret = xmlTextReaderRead(reader);
	}

	GP_DEBUG("the image URL is  %s\n",imageURL); 
	Images = strdup(imageURL);
	nRead = 0;
	CURL* imageUrl;
	imageUrl = curl_easy_init();
	CURLcode res;
	double bytesread = 0;
	FILE* imageFile;  	
	char filename[100];
	char* tempPath = "./"; 
	long http_response;
	int ret_val=0;
	while (ret_val!=2) {
		snprintf(filename,100,"%s%s",tempPath,&Images[strlen(Images)- 13]);
		imageFile = fopen(filename,"ab");
		GP_DEBUG("reading stream %s  position %ld", Images,  nRead);

		curl_easy_setopt(imageUrl, CURLOPT_URL, Images);
		//curl_easy_setopt(imageUrl,CURLOPT_TCP_KEEPALIVE,1L);
		//curl_easy_setopt(imageUrl, CURLOPT_TCP_KEEPIDLE, 120L);
		//curl_easy_setopt(imageUrl, CURLOPT_TCP_KEEPINTVL, 60L);
		//curl_easy_setopt(imageUrl, CURLOPT_WRITEFUNCTION, dl_write);

		if (nRead) {
			curl_easy_setopt(imageUrl, CURLOPT_RESUME_FROM, nRead);
			GetPix(camera,1);//'if the file not found happened then this trick is to get the camera in a readmode again and making sure it remembers the filename
			GP_DEBUG("continuing the read where it stopped %s  position %ld", Images,  nRead);
		}
		curl_easy_setopt(imageUrl, CURLOPT_WRITEDATA, imageFile);

		res = curl_easy_perform(imageUrl);

		if(res != CURLE_OK) {

			//double x;
			//curl_easy_getinfo(imageUrl, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &x);
			//nRead = x;
			fseek(imageFile, 0, SEEK_END); // seek to end of file
			nRead= ftell(imageFile); // get current file pointer	

			fprintf(stderr, "curl_easy_perform() failed: %s\n",
			curl_easy_strerror(res));
			GP_DEBUG("error in reading stream %s  position %ld", Images,  nRead);
			curl_easy_getinfo(imageUrl, CURLINFO_RESPONSE_CODE, &http_response);
			GP_DEBUG("CURLINFO_RESPONSE_CODE:%ld\n", http_response);
		} else {
			GP_DEBUG("read the whole file");
			ret_val=2;
		}
	}
	curl_easy_cleanup(imageUrl);
	fclose(imageFile);
	return (CameraFilePath *) strdup(filename);
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
	sleep(4);
	if (strcmp(cameraShutterSpeed, "B")!=0) {  
		waitBulb(camera,captureDuration);//trying captureDuration sec to start before we know how to make a bulb capture of x sec .... 
	}
	path =  ReadImageFromCamera(camera);
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
	strcpy (about->text, _("Library Name\n"
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
int
get_file_func (CameraFilesystem *fs, const char *folder, const char *filename,CameraFileType type, CameraFile *file, void *data, GPContext *context);
int get_file_func (CameraFilesystem *fs, const char *folder, const char *filename, CameraFileType type, CameraFile *file, void *data, GPContext *context)
{
	Camera *camera = data;

	/*
	* Get the file from the camera. Use gp_file_set_mime_type,
	* gp_file_set_data_and_size, etc.
	*/

	if (type == GP_FILE_TYPE_PREVIEW) {

		loadCmd (camera,STARTSTREAM);
		//need to do stuff here to get the UDP stream back from the camera as per other 
		//("http://" & CameraIP.Text & "/cam.cgi?mode=setsetting&type=liveviewsize&value=vga"
		/*
		' UDP-Protokoll einstellen
			Winsock1.Close 'Winsock1 ->Microsoft Winsock control in VB
			Winsock1.Protocol = sckUDPProtocol
			' Port, der überwacht werden soll
			nPort = 49199 
			' Eigene IP ermitteln
			sOwnIP = Winsock1.LocalIP

		' Port an die IP "binden"
Winsock1.Bind nPort, sOwnIP

'geht nur im 'recmode'
txtStatus.Text = Inet1.OpenURL("http://" & CameraIP.Text & "/cam.cgi?mode=camcmd&value=recmode") 'Inet1 ->Microsoft Inet control

txtStatus.Text = Inet1.OpenURL("http://" & CameraIP.Text & "/cam.cgi?mode=startstream&value=49199")

Timer1.Enabled = True 'damit der Datenfluß nicht abbricht

txtStatus.Text = Inet1.OpenURL("http://" & CameraIP.Text & "/cam.cgi?mode=setsetting_ &type=liveviewsize&value=vga")

...

Private Sub Winsock1_DataArrival(ByVal bytesTotal As Long)
Dim sData As String
Dim bArray As Variant

On Error Resume Next

'im Stream werden ganze Bilder übertragen 'Header ist 167 Byte, Rest - JPG
'Das JPEG-Bild beginnt also bei Byte 168 (Lumix GX7)

Winsock1.GetData sData, vbString

sData = Mid(sData, 168)

bArray = sData

ImageBox1.ReadBinary2 bArray 'ImageBox1 ist the csxImage OCX

End Sub

Private Sub Timer1_Timer()
Dim dummy As Variant

If CameraIP.Text > "" Then

If Not Inet2.StillExecuting Then
dummy = Inet2.OpenURL("http://" & CameraIP.Text & "/cam.cgi?mode=startstream_ &value=49199")
'Inet2 ->Microsoft Inet control
End If

End If

End Sub
*/ 
struct sockaddr_in address; 
int sock = 0, valread; 
struct sockaddr_in serv_addr; 
char *hello = "Hello from client"; 
char buffer[1024] = {0}; 
if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) 
{ 
printf("\n Socket creation error \n"); 
return -1; 
} 

memset(&serv_addr, '0', sizeof(serv_addr)); 

serv_addr.sin_family = AF_INET; 
serv_addr.sin_port = htons(49199); 

// Convert IPv4 and IPv6 addresses from text to binary form 
if(inet_pton(AF_INET, CAMERAIP, &serv_addr.sin_addr)<=0) 
{ 
printf("\nInvalid address/ Address not supported \n"); 
return -1; 
} 

if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) 
{ 
printf("\nConnection Failed \n"); 
return -1; 
} 
valread = read( sock , buffer, 1024); 
printf("%s\n",buffer ); 
return 0; 


}
return GP_OK;
}


int camera_abilities (CameraAbilitiesList *list) {
	CameraAbilities a;

	memset(&a, 0, sizeof(a));
	strcpy(a.model, "Panasonic:LumixGSeries");
	a.status	= GP_DRIVER_STATUS_EXPERIMENTAL;
	a.port     	= GP_PORT_TCP;
	a.speed[0] 	= 0;
	a.operations	= GP_CAPTURE_IMAGE| GP_OPERATION_CAPTURE_VIDEO| GP_OPERATION_CONFIG;
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

	if (loadCmd(camera,RECMODE)!= NULL) {
		Set_quality(camera,"raw_fine");

		loadCmd(camera,PLAYMODE);             //   'making sure the camera is in Playmode

		NumberPix(camera);
		GetPix(camera,651);

		return GP_OK;
	} else
		return GP_ERROR_IO;
}
