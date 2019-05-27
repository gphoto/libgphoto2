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
int buflen = 10000;
static char Buffer[10000-1]; 
int ReadoutMode = 0; // this should be picked up from the settings.... 0-> JPG; 1->RAW; 2 -> Thumbnails
char* cameraShutterSpeed = "B"; // //placeholder to store the value of the shutterspeed set in camera; "B" is for bulb.
int captureDuration = 10; //placeholder to store the value of the bulb shot this should be taken as input. note that my primary goal is in fact to perform bulb captures. but this should be extended for sure to take Shutter Speed capture as set in camera 

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


int camera_capture (Camera *camera, CameraCaptureType type, CameraFilePath *path, GPContext *context);

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


/**
 * FIXME.
 *
 * This function is a CameraFilesystem method.
 */
int
set_info_func (CameraFilesystem *fs, const char *folder, const char *file,
	       CameraFileInfo info, void *data, GPContext *context);
int
set_info_func (CameraFilesystem *fs, const char *folder, const char *file,
	       CameraFileInfo info, void *data, GPContext *context)
{
	/*Camera *camera = data;*/

	/* Set the file info here from <info> */

	return GP_OK;
}


/**
 * List available folders in the specified folder.
 *
 * This function is a CameraFilesystem method.
 */
int
folder_list_func (CameraFilesystem *fs, const char *folder, CameraList *list,
		  void *data, GPContext *context);
int
folder_list_func (CameraFilesystem *fs, const char *folder, CameraList *list,
		  void *data, GPContext *context)
{
	/*Camera *camera = data;*/

	/* List your folders here */

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
	/*Camera *camera = data;*/

	/* List your files here */

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

/*size_t read_callback(char *buffer, size_t size, size_t nitems, void *userdata);*/

static size_t
read_callback(void *ptr, size_t size, size_t nmemb, void *stream) {
	curl_off_t nread;
	/* in real-world cases, this would probably get this data differently
	   as this fread() stuff is exactly what the library already would do
	   by default internally */ 
	size_t retcode = fread(ptr, size, nmemb, stream);
	nread = (curl_off_t)retcode;
	return retcode;
}

static char* loadCmd (Camera *camera,char* cmd) {
        CURL *curl;
	CURLcode res;
	int  stream = strcmp (cmd, STARTSTREAM);
	char URL[100];
	//xmlDoc *doc = NULL;
	//xmlNode *root_element = NULL;

	curl = curl_easy_init();

	snprintf( URL,100, "http://%s/cam.cgi%s",CAMERAIP, cmd);
	curl_easy_setopt(curl, CURLOPT_URL, URL);
	//curl_easy_setopt(curl, CURLOPT_READ, URL);
	if (!stream) { 
		curl_easy_setopt(curl, CURLOPT_READFUNCTION, read_callback);
		curl_easy_setopt(curl, CURLOPT_READDATA, camera);
	} 
	res = curl_easy_perform(curl);
	if(res != CURLE_OK) {
		fprintf(stderr, "curl_easy_perform() failed: %s\n",
		curl_easy_strerror(res));
		return NULL;
	} else {
		/* read the XML that is now in Buffer*/ 
	}
	curl_easy_cleanup(curl);
	if (!stream) {
		return Buffer;
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
static char*  NumberPix(Camera *camera) {
	xmlChar *key;
	xmlDocPtr doc = xmlParseDoc((unsigned char*)loadCmd(camera,NUMPIX));
	xmlNodePtr cur = NULL; 
	cur = xmlDocGetRootElement(doc);   
	if (cur == NULL) {
		fprintf(stderr,"empty document\n");
		xmlFreeDoc(doc);
		return NULL;
	}
	cur = cur->xmlChildrenNode;
	while (cur != NULL) {
		if ((!xmlStrcmp(cur->name, (const xmlChar *)"content_number"))){
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			return (char*) key;
		}
		cur = cur->next;
	}
	return "";	
}

/*utility function to creat a SOAP envelope for the lumix cam */  
static char*
SoapEnvelop(int start, int num){
	static char  Envelop[500];
	snprintf(Envelop,500, "<?xml version=\"1.0\" encoding=\"utf-8\"?><s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\"><s:Body><u:Browse xmlns:u=\"urn:schemas-upnp-org:service:ContentDirectory:1\"xmlns:pana=\"urn:schemas-panasonic-com:pana\"><ObjectID>0</ObjectID><BrowseFlag>BrowseDirectChildren</BrowseFlag><Filter>*</Filter><StartingIndex> %d </StartingIndex><RequestedCount>%d</RequestedCount><SortCriteria></SortCriteria><pana:X_FromCP>LumixLink2.0</pana:X_FromCP></u:Browse></s:Body></s:Envelope>",start,num);

	return Envelop;
}

/*
 * retrieves the XML doc with the last num pictures avail in the camera.
 */
static char*  GetPix(Camera *camera,int num) {
	loadCmd(camera,PLAYMODE);
	int Start  = 0; 
	long NumPix  = strtol(NumberPix(camera), NULL, 10);
	char* SoapMsg = SoapEnvelop((NumPix - num)> 0?(NumPix - num):0, num);
	CURL *curl;
	curl = curl_easy_init();
	CURLcode res;
	struct curl_slist *list = NULL;

	list = curl_slist_append(list, "soapaction");
	list = curl_slist_append(list, "Content-Type: text/xml; charset=\"utf-8\"");
	list = curl_slist_append(list, "urn:schemas-upnp-org:service:ContentDirectory:1#Browse:");	

	char URL[1000];
	snprintf( URL,1000, "http://%s/cam.cgi%s",CAMERAIP, CDS_Control);
	curl_easy_setopt(curl, CURLOPT_URL, URL);
	//curl_easy_setopt(curl, CURLOPT_READ, URL);
	curl_easy_setopt(curl, CURLOPT_READFUNCTION, read_callback);
	curl_easy_setopt(curl, CURLOPT_READDATA, Buffer);
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, list);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, SoapMsg);
	res = curl_easy_perform(curl);
	if(res != CURLE_OK) {
	fprintf(stderr, "curl_easy_perform() failed: %s\n",
	curl_easy_strerror(res));
	}	
	else {
	/* read the XML that is now in Buffer*/ 

	}
	curl_easy_cleanup(curl);
	return Buffer;

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
	sleep(Duration * 1000); // Sleep for the duration to simulate exposure, if this is in Bulb mode 
	loadCmd(camera,SHUTTERSTOP);
	return TRUE;
}

static size_t dl_write(void *buffer, size_t size, size_t nmemb, void *stream)
{    
	return fwrite(buffer, size, nmemb, (FILE*)stream); 
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
static char* ReadImageFromCamera(Camera *camera) {
	xmlDocPtr Pictures =xmlParseDoc((xmlChar*)GetPix(camera,1));                  //the XML with all the results from the camea
	xmlNodePtr    PictureList = NULL;                    //the list of picture items extratec
	xmlNode 	*Picture, *cur;                  		//a singlepicture item
	cur = xmlDocGetRootElement(Pictures);   
	char *Images[250]; 					//the array of the urls in the camera
	long nRead[250]; 
	int  j = -1;
	int SendStatus  = -1; 
	int length  = 0; 

	PictureList = xmlFirstElementChild(xmlFirstElementChild(xmlFirstElementChild(xmlFirstElementChild(Pictures->last))))->children ;//items
	//xmlFirstElementChild
	//PictureList = Pictures.LastChild.FirstChild.FirstChild.FirstChild.FirstChild.ChildNodes 'items

	loadCmd(camera,PLAYMODE);             //   'making sure the camera is in Playmode

	Picture = PictureList->xmlChildrenNode;
	while (Picture != NULL) {
		//			if ((!xmlStrcmp(cur->name, (const xmlChar *)"content_number"))){
		//				key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
		//				return (char*) key;
		//				}

		// Else 'JPG or Thumbnails
		char*  lookuptag; 
		int  lookupNum; 
		char* xmlimageName;

		if (ReadoutMode == 0) { 
			lookuptag = "DO" ;//full JPG
			lookupNum = 3;
			xmlimageName = (char*)Picture->children->next->next->name;
		} else {
			lookuptag = "DL" ; //large thumbnail
			lookupNum = 5;
			xmlimageName = (char*)Picture->children->next->next->next->next->name;
		}


		if ((strstr(xmlimageName,"JPG")&& strstr(xmlimageName, lookuptag))||strstr(xmlimageName,"RAW")) {
			//  If Picture.ChildNodes(lookupNum).InnerText.Contains(".JPG") And Picture.ChildNodes(lookupNum).InnerText.Contains(lookuptag) Then
			j = j + 1;
			Images[j] = strdup(xmlimageName);
			nRead[j] = 0;
			CURL* imageUrl;
			imageUrl = curl_easy_init();
			CURLcode res;
			double bytesread = 0;
			FILE* imageFile;  	
			char filename[100];
			char* tempPath = "C:\\"; 
			long http_response;
			int ret_val=0;

			while (ret_val!=2) {
				snprintf(filename,100,"%s%s",tempPath,Images[j][strlen(Images[j])- 13]);
				imageFile = fopen(filename,"ab");
				GP_DEBUG("reading stream %s  position %ld", Images[j],  nRead[j]);

				curl_easy_setopt(imageUrl, CURLOPT_URL, Images[j]);
				curl_easy_setopt(imageUrl,CURLOPT_TCP_KEEPALIVE,1L);
				curl_easy_setopt(imageUrl, CURLOPT_TCP_KEEPIDLE, 120L);
				curl_easy_setopt(imageUrl, CURLOPT_TCP_KEEPINTVL, 60L);
				curl_easy_setopt(imageUrl, CURLOPT_WRITEFUNCTION, dl_write);
				//curl_easy_setopt(imageUrl, CURLOPT_PROGRESSFUNCTION, dl_progress);
				//curl_easy_setopt(imageUrl, CURLOPT_NOPROGRESS, 0);

				if (nRead[j]) {
					curl_easy_setopt(imageUrl, CURLOPT_RESUME_FROM, nRead[j]);
					GetPix(camera,1);//'if the file not found happened then this trick is to get the camera in a readmode again and making sure it remembers the filename
					GP_DEBUG("continuing the read where it stopped %s  position %ld", Images[j],  nRead[j]);
				}
				curl_easy_setopt(imageUrl, CURLOPT_WRITEDATA, imageFile);

				res = curl_easy_perform(imageUrl);
				if(res != CURLE_OK) {
					double x;
					curl_easy_getinfo(imageUrl, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &x);
					nRead[j] = x;
					fprintf(stderr, "curl_easy_perform() failed: %s\n",
					curl_easy_strerror(res));
					GP_DEBUG("error in reading stream %s  position %ld", Images[j],  nRead[j]);

					curl_easy_getinfo(imageUrl, CURLINFO_RESPONSE_CODE, &http_response);

					GP_DEBUG("CURLINFO_RESPONSE_CODE:%ld\n", http_response);
				} else {
					GP_DEBUG("read the whole file");
					ret_val=2;
				}
			}
			curl_easy_cleanup(imageUrl);
			fclose(imageFile);
			return strdup(filename);
		}
		Picture = Picture->next;
	}
	return NULL;
}


/**
* Capture an image and tell libgphoto2 where to find it by filling
* out the path.
*
* This function is a method of the Camera object.
*/
int camera_capture (Camera *camera, CameraCaptureType type, CameraFilePath *path, GPContext *context);
int camera_capture (Camera *camera, CameraCaptureType type, CameraFilePath *path, GPContext *context){
	loadCmd(camera,"capture"); //we should really multithread so as to not block while waiting
	if (!strcmp(cameraShutterSpeed, "B")) {  
		waitBulb(camera,captureDuration);//trying captureDuration sec to start before we know how to make a bulb capture of x sec .... 
	}
	path = ReadImageFromCamera(camera);
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


/**
* Fill list with abilities of the cameras supported by this camlib.
*
* For each camera, fill a CameraAbilities structure with data about
* that camera and append it to the list.
*
* The job of this function is  basically to extract data from a
* camlib specific database and insert it into the libgphoto2 camera
* database. Due to redundant data and other issues, we may decide to
* revise that database mechanism and replace it by something more
* flexible and efficient.
*
* This is a camlib API function.
*/
int camera_abilities (CameraAbilitiesList *list) {
CameraAbilities a;

memset(&a, 0, sizeof(a));
strcpy(a.model, "Panasonic:LumixGSeries");
a.status = GP_DRIVER_STATUS_EXPERIMENTAL;
a.port     =  GP_PORT_TCP;
a.speed[0] = 0;
a.operations        = 	GP_CAPTURE_IMAGE| GP_OPERATION_CAPTURE_VIDEO| GP_OPERATION_CONFIG;
a.file_operations   = GP_FILE_OPERATION_PREVIEW  ; 
/* it should be possible to browse and DL images the files using the ReadImageFromCamera() function but for now lets keep it simple*/ 
a.folder_operations = 	GP_FOLDER_OPERATION_NONE;
gp_abilities_list_append(list, a);

return GP_OK;
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
//	.file_list_func = file_list_func,
//	.folder_list_func = folder_list_func,
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

	if (loadCmd(camera,PLAYMODE))
		return GP_OK;
	else
		return GP_ERROR_IO;
}
