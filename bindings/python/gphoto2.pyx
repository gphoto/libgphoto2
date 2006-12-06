# Copyright (C) 2003-2004 David PHAM-VAN -- david@ab2r.com
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Lesser General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this library; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

#------------------------------------------------------------------------------
cdef extern from "Python.h":
    object PyString_FromStringAndSize(char *, int)

#------------------------------------------------------------------------------
cdef extern from "stdlib.h":
  void free(void *)

#------------------------------------------------------------------------------

cdef extern from "time.h":
  ctypedef struct time_t:
    int a

#------------------------------------------------------------------------------

cdef extern from "gphoto2/gphoto2-port-version.h":

  ctypedef enum GPVersionVerbosity:
    GP_VERSION_SHORT
    GP_VERSION_VERBOSE

  char **gp_port_library_version(GPVersionVerbosity verbose)

#------------------------------------------------------------------------------

cdef extern from "gphoto2/gphoto2-version.h":
  char **gp_library_version(GPVersionVerbosity verbose)

#------------------------------------------------------------------------------

cdef extern from "gphoto2/gphoto2-context.h":
  ctypedef struct GPContext

  GPContext *gp_context_new ()

  void gp_context_ref   (GPContext *context)
  void gp_context_unref (GPContext *context)

  ctypedef enum GPContextFeedback:
    GP_CONTEXT_FEEDBACK_OK
    GP_CONTEXT_FEEDBACK_CANCEL

  ctypedef void (* GPContextIdleFunc)     (GPContext *context, void *data)
  ctypedef void (* GPContextErrorFunc)    (GPContext *context,  char *format, args, void *data)
  ctypedef void (* GPContextStatusFunc)   (GPContext *context,  char *format, args, void *data)
  ctypedef void (* GPContextMessageFunc)  (GPContext *context,  char *format, args, void *data)
  ctypedef GPContextFeedback (* GPContextQuestionFunc) (GPContext *context, char *format, args, void *data)
  ctypedef GPContextFeedback (* GPContextCancelFunc)   (GPContext *context, void *data)
  ctypedef unsigned int (* GPContextProgressStartFunc)  (GPContext *context, float target, char *format, args, void *data)
  ctypedef void         (* GPContextProgressUpdateFunc) (GPContext *context, unsigned int id, float current, void *data)
  ctypedef void         (* GPContextProgressStopFunc)   (GPContext *context, unsigned int id, void *data)

  void gp_context_set_idle_func      (GPContext *context, GPContextIdleFunc func, void *data)
  void gp_context_set_progress_funcs (GPContext *context, GPContextProgressStartFunc  start_func, GPContextProgressUpdateFunc update_func,
                                      GPContextProgressStopFunc   stop_func, void *data)
  void gp_context_set_error_func     (GPContext *context, GPContextErrorFunc func,    void *data)
  void gp_context_set_status_func    (GPContext *context, GPContextStatusFunc func,   void *data)
  void gp_context_set_question_func  (GPContext *context, GPContextQuestionFunc func, void *data)
  void gp_context_set_cancel_func    (GPContext *context, GPContextCancelFunc func,   void *data)
  void gp_context_set_message_func   (GPContext *context, GPContextMessageFunc func,  void *data)

  void gp_context_idle     (GPContext *context)
  void gp_context_error    (GPContext *context,  char *format, ...)

  void gp_context_status   (GPContext *context,  char *format, ...)
  void gp_context_message  (GPContext *context,  char *format, ...)

  GPContextFeedback gp_context_question (GPContext *context,  char *format, ...)
  GPContextFeedback gp_context_cancel   (GPContext *context)
  unsigned int gp_context_progress_start  (GPContext *context, float target, char *format, ...)

  void         gp_context_progress_update (GPContext *context, unsigned int id, float current)
  void         gp_context_progress_stop   (GPContext *context, unsigned int id)

#------------------------------------------------------------------------------

cdef extern from "gphoto2/gphoto2-port-info-list.h":
  ctypedef enum GPPortType:
    GP_PORT_NONE
    GP_PORT_SERIAL
    GP_PORT_USB

  ctypedef struct GPPortInfo:
     GPPortType type
     char name[64]
     char path[64]
     char library_filename[1024]

  ctypedef struct GPPortInfoList

  int gp_port_info_list_new  (GPPortInfoList **list)
  int gp_port_info_list_free (GPPortInfoList *list)

  int gp_port_info_list_append (GPPortInfoList *list, GPPortInfo info)

  int gp_port_info_list_load (GPPortInfoList *list)

  int gp_port_info_list_count (GPPortInfoList *list)

  int gp_port_info_list_lookup_path (GPPortInfoList *list, char *path)
  int gp_port_info_list_lookup_name (GPPortInfoList *list, char *name)

  int gp_port_info_list_get_info (GPPortInfoList *list, int n, GPPortInfo *info)

#------------------------------------------------------------------------------

cdef extern from "gphoto2/gphoto2-port.h":

  ctypedef enum GPPortSerialParity:
    GP_PORT_SERIAL_PARITY_OFF
    GP_PORT_SERIAL_PARITY_EVEN
    GP_PORT_SERIAL_PARITY_ODD

  ctypedef enum defines_port:
    GP_PORT_MAX_BUF_LEN

  ctypedef struct GPPortSettingsSerial:
    char port[128]
    int speed
    int bits
    GPPortSerialParity parity
    int stopbits

  ctypedef struct GPPortSettingsUSB:
    int inep, outep, intep
    int config
    int interface
    int altsetting
    char port[64]

  ctypedef union GPPortSettings:
    GPPortSettingsSerial serial
    GPPortSettingsUSB usb

  enum:
    GP_PORT_USB_ENDPOINT_IN
    GP_PORT_USB_ENDPOINT_OUT
    GP_PORT_USB_ENDPOINT_INT

  ctypedef struct GPPortPrivateLibrary
  ctypedef struct GPPortPrivateCore

  ctypedef struct GPPort:
    GPPortType type
    GPPortSettings settings
    GPPortSettings settings_pending
    int timeout
    GPPortPrivateLibrary *pl
    GPPortPrivateCore    *pc

  int gp_port_new         (GPPort **port)
  int gp_port_free        (GPPort *port)

  int gp_port_set_info    (GPPort *port, GPPortInfo  info)
  int gp_port_get_info    (GPPort *port, GPPortInfo *info)

  int gp_port_open        (GPPort *port)
  int gp_port_close       (GPPort *port)

  int gp_port_write       (GPPort *port, char *data, int size)
  int gp_port_read        (GPPort *port, char *data, int size)
  int gp_port_check_int   (GPPort *port, char *data, int size)
  int gp_port_check_int_fast (GPPort *port, char *data, int size)

  int gp_port_get_timeout  (GPPort *port, int *timeout)
  int gp_port_set_timeout  (GPPort *port, int  timeout)

  int gp_port_set_settings (GPPort *port, GPPortSettings  settings)
  int gp_port_get_settings (GPPort *port, GPPortSettings *settings)

  ctypedef enum GPPin:
    GP_PIN_RTS
    GP_PIN_DTR
    GP_PIN_CTS
    GP_PIN_DSR
    GP_PIN_CD
    GP_PIN_RING

  ctypedef enum GPLevel:
    GP_LEVEL_LOW
    GP_LEVEL_HIGH

  int gp_port_get_pin   (GPPort *port, GPPin pin, GPLevel *level)
  int gp_port_set_pin   (GPPort *port, GPPin pin, GPLevel level)

  int gp_port_send_break (GPPort *port, int duration)
  int gp_port_flush      (GPPort *port, int direction)

  int gp_port_usb_find_device (GPPort *port, int idvendor, int idproduct)
  int gp_port_usb_find_device_by_class (GPPort *port, int mainclass, int subclass, int protocol)
  int gp_port_usb_clear_halt  (GPPort *port, int ep)
  int gp_port_usb_msg_write   (GPPort *port, int request, int value, int index, char *bytes, int size)
  int gp_port_usb_msg_read    (GPPort *port, int request, int value, int index, char *bytes, int size)
  int gp_port_set_error (GPPort *port,  char *format, ...)
  char *gp_port_get_error (GPPort *port)

#------------------------------------------------------------------------------

cdef extern from "gphoto2/gphoto2-port-log.h":
  ctypedef enum GPLogLevel:
    GP_LOG_ERROR
    GP_LOG_VERBOSE
    GP_LOG_DEBUG
    GP_LOG_DATA

  ctypedef void *GPLogFunc (GPLogLevel level, char *domain, char *format, args, void *data)

  int  gp_log_add_func    (GPLogLevel level, GPLogFunc func, void *data)
  int  gp_log_remove_func (int id)

  void gp_log      (GPLogLevel level, char *domain, char *format, ...)
  void gp_logv     (GPLogLevel level, char *domain, char *format, args)
  void gp_log_data (char *domain, char *data, unsigned int size)

#------------------------------------------------------------------------------

cdef extern from "gphoto2/gphoto2-port-result.h":
  ctypedef enum port_result:
    GP_OK
    GP_ERROR
    GP_ERROR_BAD_PARAMETERS
    GP_ERROR_NO_MEMORY
    GP_ERROR_LIBRARY
    GP_ERROR_UNKNOWN_PORT
    GP_ERROR_NOT_SUPPORTED
    GP_ERROR_IO
    GP_ERROR_TIMEOUT
    GP_ERROR_IO_SUPPORTED_SERIAL
    GP_ERROR_IO_SUPPORTED_USB
    GP_ERROR_IO_INIT
    GP_ERROR_IO_READ
    GP_ERROR_IO_WRITE
    GP_ERROR_IO_UPDATE
    GP_ERROR_IO_SERIAL_SPEED
    GP_ERROR_IO_USB_CLEAR_HALT
    GP_ERROR_IO_USB_FIND
    GP_ERROR_IO_USB_CLAIM
    GP_ERROR_IO_LOCK

  char *gp_port_result_as_string (int result)

#------------------------------------------------------------------------------

cdef extern from "gphoto2/gphoto2-port-result.h":
  ctypedef enum gphoto2_result:
    GP_ERROR_CORRUPTED_DATA
    GP_ERROR_FILE_EXISTS
    GP_ERROR_MODEL_NOT_FOUND
    GP_ERROR_DIRECTORY_NOT_FOUND
    GP_ERROR_FILE_NOT_FOUND
    GP_ERROR_DIRECTORY_EXISTS
    GP_ERROR_CAMERA_BUSY
    GP_ERROR_PATH_NOT_ABSOLUTE
    GP_ERROR_CANCEL

  char *gp_result_as_string(int result)

#------------------------------------------------------------------------------

cdef extern from "gphoto2/gphoto2-widget.h":
  ctypedef struct CameraWidget

  ctypedef enum CameraWidgetType:
    GP_WIDGET_WINDOW
    GP_WIDGET_SECTION
    GP_WIDGET_TEXT
    GP_WIDGET_RANGE
    GP_WIDGET_TOGGLE
    GP_WIDGET_RADIO
    GP_WIDGET_MENU
    GP_WIDGET_BUTTON
    GP_WIDGET_DATE

  ctypedef int (* CameraWidgetCallback) (Camera, CameraWidget, GPContext)

  int     gp_widget_new   (CameraWidgetType _type, char *label, CameraWidget **widget)
  int     gp_widget_free  (CameraWidget *widget)
  int     gp_widget_ref   (CameraWidget *widget)
  int     gp_widget_unref (CameraWidget *widget)

  int     gp_widget_append        (CameraWidget *widget, CameraWidget *child)
  int     gp_widget_prepend       (CameraWidget *widget, CameraWidget *child)

  int     gp_widget_count_children     (CameraWidget *widget)
  int     gp_widget_get_child          (CameraWidget *widget, int child_number,
                                        CameraWidget **child)

  int     gp_widget_get_child_by_label (CameraWidget *widget, char *label,
                                        CameraWidget **child)
  int     gp_widget_get_child_by_id    (CameraWidget *widget, int id, CameraWidget **child)
  int     gp_widget_get_child_by_name  (CameraWidget *widget, char *name, CameraWidget **child)
  int     gp_widget_get_root           (CameraWidget *widget, CameraWidget **root)
  int     gp_widget_get_parent         (CameraWidget *widget, CameraWidget **parent)

  int     gp_widget_set_value     (CameraWidget *widget,  void *value)
  int     gp_widget_get_value     (CameraWidget *widget, void *value)

  int     gp_widget_set_name      (CameraWidget *widget,  char  *name)
  int     gp_widget_get_name      (CameraWidget *widget,  char **name)

  int     gp_widget_set_info      (CameraWidget *widget,  char  *info)
  int     gp_widget_get_info      (CameraWidget *widget,  char **info)

  int     gp_widget_get_id        (CameraWidget *widget, int *id)
  int     gp_widget_get_type      (CameraWidget *widget, CameraWidgetType *type)
  int     gp_widget_get_label     (CameraWidget *widget,  char **label)

  int     gp_widget_set_range     (CameraWidget *range, float  low, float  high, float  increment)
  int     gp_widget_get_range     (CameraWidget *range, float *min, float *max, float *increment)

  int     gp_widget_add_choice     (CameraWidget *widget,  char *choice)
  int     gp_widget_count_choices  (CameraWidget *widget)
  int     gp_widget_get_choice     (CameraWidget *widget, int choice_number, char **choice)

  int     gp_widget_changed        (CameraWidget *widget)
  int     gp_widget_set_changed    (CameraWidget *widget, int changed)

#------------------------------------------------------------------------------

cdef extern from "gphoto2/gphoto2-file.h":
  ctypedef enum defines_file:
    GP_MIME_WAV
    GP_MIME_RAW
    GP_MIME_PNG
    GP_MIME_PGM
    GP_MIME_PPM
    GP_MIME_PNM
    GP_MIME_JPEG
    GP_MIME_TIFF
    GP_MIME_BMP
    GP_MIME_QUICKTIME
    GP_MIME_AVI
    GP_MIME_CRW
    GP_MIME_UNKNOWN
    GP_MIME_EXIF

  ctypedef enum CameraFileType:
    GP_FILE_TYPE_PREVIEW
    GP_FILE_TYPE_NORMAL
    GP_FILE_TYPE_RAW
    GP_FILE_TYPE_AUDIO
    GP_FILE_TYPE_EXIF

  ctypedef struct CameraFile

  int gp_file_new            (CameraFile **file)
  int gp_file_ref            (CameraFile *file)
  int gp_file_unref          (CameraFile *file)
  int gp_file_free           (CameraFile *file)

  int gp_file_open           (CameraFile *file,  char *filename)
  int gp_file_save           (CameraFile *file,  char *filename)
  int gp_file_clean          (CameraFile *file)
  int gp_file_copy           (CameraFile *destination, CameraFile *source)

  int gp_file_set_name       (CameraFile *file,  char  *name)
  int gp_file_get_name       (CameraFile *file,  char **name)

  int gp_file_set_mime_type  (CameraFile *file,  char  *mime_type)
  int gp_file_get_mime_type  (CameraFile *file,  char **mime_type)

  int gp_file_set_type       (CameraFile *file, CameraFileType  type)
  int gp_file_get_type       (CameraFile *file, CameraFileType *type)

  int gp_file_set_mtime   (CameraFile *file, mtime)
  int gp_file_get_mtime   (CameraFile *file, mtime)

  int gp_file_detect_mime_type          (CameraFile *file)
  int gp_file_adjust_name_for_mime_type (CameraFile *file)

  int gp_file_append            (CameraFile*,  char *data, unsigned long int size)
  int gp_file_set_data_and_size (CameraFile*,       char *data, unsigned long int size)
  int gp_file_get_data_and_size (CameraFile*,  char **data, unsigned long int *size)

#------------------------------------------------------------------------------

cdef extern from "gphoto2/gphoto2-list.h":
  ctypedef enum defines_list:
    MAX_ENTRIES

  cdef struct s_entry:
    char name[128]
    char value[128]

  ctypedef struct CameraList:
    int count
    s_entry entry[MAX_ENTRIES]
    int ref_count

  int gp_list_new   (CameraList **list)
  int gp_list_ref   (CameraList *list)
  int gp_list_unref (CameraList *list)
  int gp_list_free  (CameraList *list)

  int gp_list_count      (CameraList *list)
  int gp_list_append     (CameraList *list, char *name, char *value)
  int gp_list_reset      (CameraList *list)
  int gp_list_sort       (CameraList *list)

  int gp_list_get_name  (CameraList *list, int index, char **name)
  int gp_list_get_value (CameraList *list, int index, char **value)

  int gp_list_set_name  (CameraList *list, int index, char *name)
  int gp_list_set_value (CameraList *list, int index, char *value)

  int gp_list_populate  (CameraList *list, char *format, int count)

#------------------------------------------------------------------------------

cdef extern from "gphoto2/gphoto2-filesys.h":

  ctypedef enum CameraFileInfoFields:
    GP_FILE_INFO_NONE
    GP_FILE_INFO_TYPE
    GP_FILE_INFO_NAME
    GP_FILE_INFO_SIZE
    GP_FILE_INFO_WIDTH
    GP_FILE_INFO_HEIGHT
    GP_FILE_INFO_PERMISSIONS
    GP_FILE_INFO_STATUS
    GP_FILE_INFO_MTIME
    GP_FILE_INFO_ALL

  ctypedef enum CameraFilePermissions:
    GP_FILE_PERM_NONE
    GP_FILE_PERM_READ
    GP_FILE_PERM_DELETE
    GP_FILE_PERM_ALL

  ctypedef enum CameraFileStatus:
    GP_FILE_STATUS_NOT_DOWNLOADED
    GP_FILE_STATUS_DOWNLOADED

  ctypedef struct CameraFileInfoFile:
    CameraFileInfoFields fields
    CameraFileStatus status
    unsigned long size
    char type[64]

    unsigned int width, height
    char name[64]
    CameraFilePermissions permissions
    time_t mtime

  ctypedef struct CameraFileInfoPreview:
    CameraFileInfoFields fields
    CameraFileStatus status
    unsigned long size
    char type[64]
    unsigned int width, height

  ctypedef struct CameraFileInfoAudio:
    CameraFileInfoFields fields
    CameraFileStatus status
    unsigned long size
    char type[64]

  ctypedef struct CameraFileInfo:
    CameraFileInfoPreview preview
    CameraFileInfoFile    file
    CameraFileInfoAudio   audio

  ctypedef struct CameraFilesystem

  int gp_filesystem_new    (CameraFilesystem **fs)
  int gp_filesystem_free   (CameraFilesystem *fs)

  int gp_filesystem_append           (CameraFilesystem *fs,  char *folder, char *filename, GPContext *context)
  int gp_filesystem_set_info_noop    (CameraFilesystem *fs,  char *folder, CameraFileInfo info, GPContext *context)
  int gp_filesystem_set_file_noop    (CameraFilesystem *fs,  char *folder, CameraFile *file, GPContext *context)
  int gp_filesystem_delete_file_noop (CameraFilesystem *fs,  char *folder, char *filename, GPContext *context)
  int gp_filesystem_reset            (CameraFilesystem *fs)

  int gp_filesystem_count        (CameraFilesystem *fs,  char *folder, GPContext *context)
  int gp_filesystem_name         (CameraFilesystem *fs,  char *folder, int filenumber,  char **filename, GPContext *context)
  int gp_filesystem_get_folder   (CameraFilesystem *fs,  char *filename, char **folder, GPContext *context)
  int gp_filesystem_number       (CameraFilesystem *fs,  char *folder, char *filename, GPContext *context)

  ctypedef int (*CameraFilesystemListFunc) (CameraFilesystem *fs, char *folder, CameraList *list, void *data, GPContext *context)
  int gp_filesystem_set_list_funcs (CameraFilesystem *fs, CameraFilesystemListFunc file_list_func, CameraFilesystemListFunc folder_list_func, void *data)
  int gp_filesystem_list_files     (CameraFilesystem *fs,  char *folder, CameraList *list, GPContext *context)
  int gp_filesystem_list_folders   (CameraFilesystem *fs,  char *folder, CameraList *list, GPContext *context)

  ctypedef int (*CameraFilesystemSetInfoFunc) (CameraFilesystem *fs, char *folder, char *filename, CameraFileInfo info, void *data, GPContext *context)
  ctypedef int (*CameraFilesystemGetInfoFunc) (CameraFilesystem *fs, char *folder, char *filename, CameraFileInfo *info, void *data, GPContext *context)
  int gp_filesystem_set_info_funcs (CameraFilesystem *fs, CameraFilesystemGetInfoFunc get_info_func,
                                    CameraFilesystemSetInfoFunc set_info_func,
                                    void *data)
  int gp_filesystem_get_info       (CameraFilesystem *fs,  char *folder,
                                     char *filename, CameraFileInfo *info,
                                    GPContext *context)
  int gp_filesystem_set_info       (CameraFilesystem *fs,  char *folder,
                                     char *filename, CameraFileInfo info,
                                    GPContext *context)

  ctypedef int (*CameraFilesystemGetFileFunc)    (CameraFilesystem *fs,
                                                 char *folder,
                                                 char *filename,
                                                CameraFileType type,
                                                CameraFile *file, void *data,
                                                GPContext *context)
  ctypedef int (*CameraFilesystemDeleteFileFunc) (CameraFilesystem *fs,
                                                 char *folder,
                                                 char *filename,
                                                void *data, GPContext *context)
  int gp_filesystem_set_file_funcs (CameraFilesystem *fs,
                                    CameraFilesystemGetFileFunc get_file_func,
                                    CameraFilesystemDeleteFileFunc del_file_func,
                                    void *data)
  int gp_filesystem_get_file       (CameraFilesystem *fs,  char *folder,
                                     char *filename, CameraFileType type,
                                    CameraFile *file, GPContext *context)
  int gp_filesystem_delete_file    (CameraFilesystem *fs,  char *folder,
                                     char *filename, GPContext *context)

  ctypedef int (*CameraFilesystemPutFileFunc)   (CameraFilesystem *fs,
                                                 char *folder,
                                                CameraFile *file, void *data,
                                                GPContext *context)
  ctypedef int (*CameraFilesystemDeleteAllFunc) (CameraFilesystem *fs,
                                                 char *folder, void *data,
                                                GPContext *context)
  ctypedef int (*CameraFilesystemDirFunc)       (CameraFilesystem *fs,
                                                 char *folder,
                                                 char *name, void *data,
                                                GPContext *context)
  int gp_filesystem_set_folder_funcs (CameraFilesystem *fs,
                                    CameraFilesystemPutFileFunc put_file_func,
                                    CameraFilesystemDeleteAllFunc delete_all_func,
                                    CameraFilesystemDirFunc make_dir_func,
                                    CameraFilesystemDirFunc remove_dir_func,
                                    void *data)
  int gp_filesystem_put_file   (CameraFilesystem *fs,  char *folder,
                                CameraFile *file, GPContext *context)
  int gp_filesystem_delete_all (CameraFilesystem *fs,  char *folder,
                                GPContext *context)
  int gp_filesystem_make_dir   (CameraFilesystem *fs,  char *folder,
                                 char *name, GPContext *context)
  int gp_filesystem_remove_dir (CameraFilesystem *fs,  char *folder,
                                 char *name, GPContext *context)

  int gp_filesystem_dump         (CameraFilesystem *fs)

#------------------------------------------------------------------------------

cdef extern from "gphoto2/gphoto2-abilities-list.h":
  ctypedef enum CameraDriverStatus:
    GP_DRIVER_STATUS_PRODUCTION
    GP_DRIVER_STATUS_TESTING
    GP_DRIVER_STATUS_EXPERIMENTAL
    GP_DRIVER_STATUS_DEPRECATED

  ctypedef enum CameraOperation:
    GP_OPERATION_NONE
    GP_OPERATION_CAPTURE_IMAGE
    GP_OPERATION_CAPTURE_VIDEO
    GP_OPERATION_CAPTURE_AUDIO
    GP_OPERATION_CAPTURE_PREVIEW
    GP_OPERATION_CONFIG

  ctypedef enum CameraFileOperation:
    GP_FILE_OPERATION_NONE
    GP_FILE_OPERATION_DELETE
    GP_FILE_OPERATION_PREVIEW
    GP_FILE_OPERATION_RAW
    GP_FILE_OPERATION_AUDIO
    GP_FILE_OPERATION_EXIF

  ctypedef enum CameraFolderOperation:
    GP_FOLDER_OPERATION_NONE
    GP_FOLDER_OPERATION_DELETE_ALL
    GP_FOLDER_OPERATION_PUT_FILE
    GP_FOLDER_OPERATION_MAKE_DIR
    GP_FOLDER_OPERATION_REMOVE_DIR

  ctypedef struct CameraAbilities:
    char model [128]
    CameraDriverStatus status
    GPPortType port
    int speed [64]
    CameraOperation       operations
    CameraFileOperation   file_operations
    CameraFolderOperation folder_operations
    int usb_vendor, usb_product
    int usb_class, usb_subclass, usb_protocol
    char library [1024]
    char id [1024]
    int reserved1
    int reserved2
    int reserved3
    int reserved4
    int reserved5
    int reserved6
    int reserved7
    int reserved8

  ctypedef struct CameraAbilitiesList

  int gp_abilities_list_new    (CameraAbilitiesList **list)
  int gp_abilities_list_free   (CameraAbilitiesList *list)
  int gp_abilities_list_load   (CameraAbilitiesList *list, GPContext *context)
  int gp_abilities_list_reset  (CameraAbilitiesList *list)
  int gp_abilities_list_detect (CameraAbilitiesList *list, GPPortInfoList *info_list, CameraList *l, GPContext *context)
  int gp_abilities_list_append (CameraAbilitiesList *list, CameraAbilities abilities)
  int gp_abilities_list_count  (CameraAbilitiesList *list)
  int gp_abilities_list_lookup_model (CameraAbilitiesList *list, char *model)
  int gp_abilities_list_get_abilities (CameraAbilitiesList *list, int index, CameraAbilities *abilities)

#------------------------------------------------------------------------------

cdef extern from "gphoto2/gphoto2-camera.h":
  #include <gphoto2/gphoto2-context.h>

  ctypedef struct Camera

  #include <gphoto2/gphoto2-port.h>
  #include <gphoto2/gphoto2-port-info-list.h>

  #include <gphoto2/gphoto2-widget.h>
  #include <gphoto2/gphoto2-list.h>
  #include <gphoto2/gphoto2-file.h>
  #include <gphoto2/gphoto2-filesys.h>
  #include <gphoto2/gphoto2-abilities-list.h>
  #include <gphoto2/gphoto2-result.h>
  #include <gphoto2/gphoto2-context.h>

  ctypedef struct CameraText:
    char text [32 * 1024]

  ctypedef struct CameraFilePath:
    char name [128]
    char folder [1024]

  ctypedef enum CameraCaptureType:
    GP_CAPTURE_IMAGE
    GP_CAPTURE_MOVIE
    GP_CAPTURE_SOUND

  ctypedef enum CameraEventType:
    GP_EVENT_UNKNOWN        # unknown and unhandled event
    GP_EVENT_TIMEOUT        # timeout, no arguments
    GP_EVENT_FILE_ADDED     # CameraFilePath = file in camfs
    GP_EVENT_FOLDER_ADDED   # CameraFilePath = folder in camfs

  ctypedef int (*CameraExitFunc)      (Camera *camera, GPContext *context)
  ctypedef int (*CameraGetConfigFunc) (Camera *camera, CameraWidget **widget, GPContext *context)
  ctypedef int (*CameraSetConfigFunc) (Camera *camera, CameraWidget  *widget, GPContext *context)
  ctypedef int (*CameraCaptureFunc)   (Camera *camera, CameraCaptureType type, CameraFilePath *path, GPContext *context)
  ctypedef int (*CameraCapturePreviewFunc) (Camera *camera, CameraFile *file, GPContext *context)
  ctypedef int (*CameraSummaryFunc)   (Camera *camera, CameraText *text,
                                      GPContext *context)
  ctypedef int (*CameraManualFunc)    (Camera *camera, CameraText *text,
                                      GPContext *context)
  ctypedef int (*CameraAboutFunc)     (Camera *camera, CameraText *text,
                                      GPContext *context)

  ctypedef int (*CameraPrePostFunc) (Camera *camera, GPContext *context)

  ctypedef struct CameraFunctions:
    CameraPrePostFunc pre_func
    CameraPrePostFunc post_func
    CameraExitFunc exit
    CameraGetConfigFunc       get_config
    CameraSetConfigFunc       set_config
    CameraCaptureFunc        capture
    CameraCapturePreviewFunc capture_preview
    CameraSummaryFunc summary
    CameraManualFunc  manual
    CameraAboutFunc   about
    void *reserved1
    void *reserved2
    void *reserved3
    void *reserved4
    void *reserved5
    void *reserved6
    void *reserved7
    void *reserved8

  ctypedef GPPort     CameraPort
  ctypedef GPPortInfo CameraPortInfo

  ctypedef struct CameraPrivateLibrary
  ctypedef struct CameraPrivateCore

  cdef struct _Camera:
    GPPort           *port
    CameraFilesystem *fs
    CameraFunctions  *functions
    CameraPrivateLibrary  *pl
    CameraPrivateCore     *pc

  int gp_camera_new               (Camera **camera)

  int gp_camera_set_abilities     (Camera *camera, CameraAbilities  abilities)
  int gp_camera_get_abilities     (Camera *camera, CameraAbilities *abilities)
  int gp_camera_set_port_info     (Camera *camera, GPPortInfo  info)
  int gp_camera_get_port_info     (Camera *camera, GPPortInfo *info)

  int gp_camera_set_port_speed    (Camera *camera, int speed)
  int gp_camera_get_port_speed    (Camera *camera)

  int gp_camera_init               (Camera *camera, GPContext *context)
  int gp_camera_exit               (Camera *camera, GPContext *context)

  int gp_camera_ref                (Camera *camera)
  int gp_camera_unref              (Camera *camera)
  int gp_camera_free               (Camera *camera)

  int gp_camera_get_config         (Camera *camera, CameraWidget **window, GPContext *context)
  int gp_camera_set_config         (Camera *camera, CameraWidget  *window, GPContext *context)
  int gp_camera_get_summary        (Camera *camera, CameraText *summary, GPContext *context)
  int gp_camera_get_manual         (Camera *camera, CameraText *manual, GPContext *context)
  int gp_camera_get_about          (Camera *camera, CameraText *about, GPContext *context)
  int gp_camera_capture            (Camera *camera, CameraCaptureType type, CameraFilePath *path, GPContext *context)
  int gp_camera_capture_preview    (Camera *camera, CameraFile *file, GPContext *context)
  int gp_camera_wait_for_event     (Camera *camera, int timeout, CameraEventType *eventtype, void **eventdata, GPContext *context)

  int gp_camera_folder_list_files   (Camera *camera,  char *folder, CameraList *list, GPContext *context)
  int gp_camera_folder_list_folders (Camera *camera,  char *folder, CameraList *list, GPContext *context)
  int gp_camera_folder_delete_all   (Camera *camera,  char *folder, GPContext *context)
  int gp_camera_folder_put_file     (Camera *camera,  char *folder, CameraFile *file, GPContext *context)
  int gp_camera_folder_make_dir     (Camera *camera,  char *folder, char *name, GPContext *context)
  int gp_camera_folder_remove_dir   (Camera *camera,  char *folder, char *name, GPContext *context)

  int gp_camera_file_get_info     (Camera *camera,  char *folder, char *file, CameraFileInfo *info, GPContext *context)
  int gp_camera_file_set_info     (Camera *camera,  char *folder, char *file, CameraFileInfo info, GPContext *context)
  int gp_camera_file_get          (Camera *camera,  char *folder, char *file, CameraFileType type, CameraFile *camera_file, GPContext *context)
  int gp_camera_file_delete       (Camera *camera,  char *folder, char *file, GPContext *context)

  ctypedef int          (* CameraTimeoutFunc)      (Camera *camera, GPContext *context)
  ctypedef unsigned int (* CameraTimeoutStartFunc) (Camera *camera, unsigned int timeout, CameraTimeoutFunc func, void *data)
  ctypedef void         (* CameraTimeoutStopFunc)  (Camera *camera, unsigned int id, void *data)
  void         gp_camera_set_timeout_funcs (Camera *camera, CameraTimeoutStartFunc start_func, CameraTimeoutStopFunc  stop_func, void *data)
  int          gp_camera_start_timeout     (Camera *camera, unsigned int timeout, CameraTimeoutFunc func)
  void         gp_camera_stop_timeout      (Camera *camera, unsigned int id)

#------------------------------------------------------------------------------

cdef extern from "gphoto2/gphoto2-library.h":
  ctypedef int (* CameraLibraryIdFunc) (CameraText *id)
  ctypedef int (* CameraLibraryAbilitiesFunc) (CameraAbilitiesList *list)
  ctypedef int (* CameraLibraryInitFunc)      (Camera *camera, GPContext *context)
  int camera_id           (CameraText *id)
  int camera_abilities    (CameraAbilitiesList *list)
  int camera_init         (Camera *camera, GPContext *context)

#------------------------------------------------------------------------------

cdef extern from "gphoto2/gphoto2-setting.h":
  int gp_setting_set (char *id, char *key, char *value)
  int gp_setting_get (char *id, char *key, char *value)

#------------------------------------------------------------------------------

def library_version(verbose):
  cdef char **arrText
  cdef int i
  retval = []
  if verbose==False:
    arrText=gp_library_version(GP_VERSION_SHORT)
  else:
    arrText=gp_library_version(GP_VERSION_VERBOSE)
  i = 0
  while arrText[i] != NULL:
    retval.append(arrText[i])
    i=i+1
  return retval

cdef check(result):
  if result!=0:
    txt=gp_result_as_string(result)
    raise Exception('Error ('+str(result)+') : '+txt)

cdef check_unref(int result,CameraFile *file):
  if result!=0:
    gp_file_unref(file)
    txt=gp_result_as_string(result)
    raise Exception('Error ('+str(result)+') : '+txt)

cdef class portInfo:
  cdef GPPortInfo info

  def __repr__(self):
    if self.info.path:
      return "%s: %s" % (self.info.name, self.info.path)
    else:
      return self.info.name

cdef class ports:
  cdef GPPortInfoList *portInfoList

  def __new__(self):
    check(gp_port_info_list_new(&self.portInfoList))

  def __dealloc__(self):
    check(gp_port_info_list_free(self.portInfoList))

  def load(self):
    check(gp_port_info_list_load(self.portInfoList))

  def count(self):
    return gp_port_info_list_count(self.portInfoList)

  def __getitem__(self, num):
    cdef portInfo info
    info=portInfo()
    check(gp_port_info_list_get_info(self.portInfoList, num, &info.info))
    return info

cdef class cameraList:
  cdef CameraList *liste

  def __new__(self):
    check(gp_list_new(&self.liste))

  def __dealloc__(self):
    check(gp_list_free(self.liste))

  def count(self):
    return gp_list_count(self.liste)

  def __getitem__(self, index):
    cdef char *name
    cdef char *value
    check(gp_list_get_name(self.liste, index, &name))
    check(gp_list_get_value(self.liste, index, &value))
    return (name, value)

cdef class cameraAbilities:
  cdef CameraAbilities abilitie

  def __new__(self):
    pass

  def __repr__(self):
    return "Model : %s\nStatus : %d\nPort : %d\nOperations : %d\nFile Operations : %d\nFolder Operations : %d\nUSB (vendor/product) : 0x%x/0x%x\nUSB class : 0x%x/0x%x/0x%x\nLibrary : %s\nId : %s\n" % (self.abilitie.model, self.abilitie.status, self.abilitie.port, self.abilitie.operations, self.abilitie.file_operations, self.abilitie.folder_operations, self.abilitie.usb_vendor, self.abilitie.usb_product, self.abilitie.usb_class, self.abilitie.usb_subclass, self.abilitie.usb_protocol, self.abilitie.library, self.abilitie.id)

  property model:
    def __get__(self):
      return self.abilitie.model

  property status:
    def __get__(self):
      return self.abilitie.status

  property port:
    def __get__(self):
      return self.abilitie.port

  property operations:
    def __get__(self):
      return self.abilitie.operations

  property file_operations:
    def __get__(self):
      return self.abilitie.file_operations

  property folder_operations:
    def __get__(self):
      return self.abilitie.folder_operations

  property usb_vendor:
    def __get__(self):
      return self.abilitie.usb_vendor

  property usb_product:
    def __get__(self):
      return self.abilitie.usb_product

  property usb_class:
    def __get__(self):
      return self.abilitie.usb_class

  property usb_subclass:
    def __get__(self):
      return self.abilitie.usb_subclass

  property usb_protocol:
    def __get__(self):
      return self.abilitie.usb_protocol

  property library:
    def __get__(self):
      return self.abilitie.library

  property id:
    def __get__(self):
      return self.abilitie.id

cdef class abilities_list:
  cdef CameraAbilitiesList *abilitiesList

  def __new__(self):
    check(gp_abilities_list_new(&self.abilitiesList))

  def __dealloc__(self):
    check(gp_abilities_list_free(self.abilitiesList))

  def count(self):
    return gp_abilities_list_count(self.abilitiesList)

  def load(self):
    check(gp_abilities_list_load(self.abilitiesList, NULL))

  def __getitem__(self, index):
    cdef cameraAbilities ab
    ab=cameraAbilities()
    check(gp_abilities_list_get_abilities(self.abilitiesList, index, &ab.abilitie))
    return ab

  def lookup_model(self, model):
    ret=gp_abilities_list_lookup_model(self.abilitiesList, model)
    return ret

  def detect(self, ports p):
    cdef cameraList l
    l=cameraList()
    check(gp_abilities_list_detect(self.abilitiesList, p.portInfoList, l.liste, NULL))
    return l

class cameraeventtype:
    GP_EVENT_UNKNOWN,        \
    GP_EVENT_TIMEOUT,        \
    GP_EVENT_FILE_ADDED,     \
    GP_EVENT_FOLDER_ADDED    \
    = range(0,4)

cdef class camera:
  cdef Camera *camera

  def __new__(self):
    check(gp_camera_new(&self.camera))

  def init(self):
    check(gp_camera_init(self.camera, NULL))

  def reinit(self):
    gp_camera_free(self.camera)
    check(gp_camera_new(&self.camera))
    self.init()

  def __dealloc__(self):
    check(gp_camera_free(self.camera))

  property abilities:
    def __set__(self, cameraAbilities ab):
      check(gp_camera_set_abilities(self.camera, ab.abilitie))
    def __get__(self):
      cdef cameraAbilities ab
      ab=cameraAbilities()
      check(gp_camera_get_abilities(self.camera, &ab.abilitie))
      return ab

  property summary:
    def __get__(self):
      cdef CameraText txt
      check(gp_camera_get_summary(self.camera, &txt, NULL))
      return txt.text

  def capture_image(self):
    cdef CameraFilePath path
    check(gp_camera_capture(self.camera, GP_CAPTURE_IMAGE, &path, NULL))
    return (path.folder, path.name)

  def wait_for_event(self,int timeout):
    cdef CameraEventType event
    cdef void *data
    check(gp_camera_wait_for_event(self.camera,timeout,&event,&data,NULL))
    eventdata=None
    if event==GP_EVENT_FILE_ADDED:
      eventdata=((<CameraFilePath*>data).folder,(<CameraFilePath*>data).name)
      free(data)
    return (event,eventdata)

  def list_folders_in_folder(self,char *path):
    cdef cameraList l
    l=cameraList()
    check(gp_camera_folder_list_folders( self.camera, path, l.liste, NULL ));
    return l

  def list_files_in_folder(self,char *path):
    cdef cameraList l
    l=cameraList()
    check(gp_camera_folder_list_files( self.camera, path, l.liste, NULL ));
    return l

  def upload_file_to(self,char *srcpath,char *destfolder,char *destfilename):
    cdef CameraFile *cfile
    check( gp_file_new( &cfile ) )
    check_unref( gp_file_open( cfile, srcpath ), cfile )
    check_unref( gp_file_set_name( cfile, destfilename ), cfile )
    check_unref( gp_camera_folder_put_file( self.camera, destfolder, cfile, NULL ), cfile )
    gp_file_unref( cfile )

  def download_file_to(self,char *srcfolder,char *srcfilename,char *destpath):
    cdef CameraFile *cfile
    cdef char *data
    cdef unsigned long size
    check( gp_file_new( &cfile ) )
    check_unref( gp_camera_file_get ( self.camera, srcfolder, srcfilename, GP_FILE_TYPE_NORMAL, cfile, NULL ), cfile )
    check_unref( gp_file_get_data_and_size( cfile, &data, &size ), cfile )
    gp_file_unref( cfile )
    dfile = open( destpath, 'w' )
    dfile.write( PyString_FromStringAndSize( data, size ) )
    dfile.close()

