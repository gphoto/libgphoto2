/*
 * SWIG interface file for libgphoto2
 *
 * ChangeLog:
 *  * 23 Jan 2005 - Jason Watson <jason.watson@agrios.net>
 *    - initial version submitted to libgphoto2 project
 *
 * Original author for this file is Jason Watson <jason.watson@agrios.net>
 */
%module gphoto2
%javaconst(1);

%typemap(javagetcptr) SWIGTYPE %{
  public static long getCPtr($javaclassname obj) {
    return (obj == null) ? 0 : obj.swigCPtr;
  }
%}

%{
#include <gphoto2/gphoto2-abilities-list.h>
#include <gphoto2/gphoto2-camera.h>
#include <gphoto2/gphoto2-context.h>
#include <gphoto2/gphoto2-endian.h>
#include <gphoto2/gphoto2-file.h>
#include <gphoto2/gphoto2-filesys.h>
#include <gphoto2/gphoto2.h>
#include <gphoto2/gphoto2-library.h>
#include <gphoto2/gphoto2-list.h>
#include <gphoto2/gphoto2-port.h>
#include <gphoto2/gphoto2-port-info-list.h>
#include <gphoto2/gphoto2-port-library.h>
#include <gphoto2/gphoto2-port-log.h>
#include <gphoto2/gphoto2-port-portability.h>
#include <gphoto2/gphoto2-port-portability-os2.h>
#include <gphoto2/gphoto2-port-result.h>
#include <gphoto2/gphoto2-port-version.h>
#include <gphoto2/gphoto2-result.h>
#include <gphoto2/gphoto2-setting.h>
#include <gphoto2/gphoto2-version.h>
#include <gphoto2/gphoto2-widget.h>
%}


%include "gphoto2/gphoto2-abilities-list.h"
%include "gphoto2/gphoto2-camera.h"
%include "gphoto2/gphoto2-context.h"
%include "gphoto2/gphoto2-endian.h"
%include "gphoto2/gphoto2-file.h"
%include "gphoto2/gphoto2-filesys.h"
%include "gphoto2/gphoto2.h"
%include "gphoto2/gphoto2-library.h"
%include "gphoto2/gphoto2-list.h"
%include "gphoto2/gphoto2-port.h"
%include "gphoto2/gphoto2-port-info-list.h"
%include "gphoto2/gphoto2-port-library.h"
%include "gphoto2/gphoto2-port-log.h"
%include "gphoto2/gphoto2-port-portability.h"
%include "gphoto2/gphoto2-port-portability-os2.h"
%include "gphoto2/gphoto2-port-result.h"
%include "gphoto2/gphoto2-port-version.h"
%include "gphoto2/gphoto2-result.h"
%include "gphoto2/gphoto2-setting.h"
%include "gphoto2/gphoto2-version.h"
%include "gphoto2/gphoto2-widget.h"


%inline %{

/* copied from gphoto2-context.c so that swig knows what the type is */
struct _GPContext
{
        GPContextIdleFunc     idle_func;
        void                 *idle_func_data;

        GPContextProgressStartFunc  progress_start_func;
        GPContextProgressUpdateFunc progress_update_func;
        GPContextProgressStopFunc   progress_stop_func;
        void                       *progress_func_data;

        GPContextErrorFunc    error_func;
        void                 *error_func_data;

        GPContextQuestionFunc question_func;
        void                 *question_func_data;

        GPContextCancelFunc   cancel_func;
        void                 *cancel_func_data;

        GPContextStatusFunc   status_func;
        void                 *status_func_data;

        GPContextMessageFunc  message_func;
        void                 *message_func_data;

        unsigned int ref_count;
};

/* copied from gphoto2-abilities-list.c so that swig knows what the type is */
struct _CameraAbilitiesList {
        int count;
        CameraAbilities *abilities;
};

/* copied from gphoto2-port-info-list.c so that swig knows what the type is */
struct _GPPortInfoList {
        GPPortInfo *info;
        unsigned int count;
};


Camera *
dereference_p_p__Camera( Camera **pp_C ) {
    /* Given a pointer to a pointer to a Camera, return just the pointer to a Camera */
    return (pp_C==NULL) ? NULL : *pp_C;
}

CameraAbilitiesList *
dereference_p_p__CameraAbilitiesList( CameraAbilitiesList **pp_CAL ) {
    /* Given a pointer to a pointer to a CAL, return just the pointer to a CAL */
    return (pp_CAL==NULL) ? NULL : *pp_CAL;
}

GPPortInfoList *
dereference_p_p__GPPortInfoList( GPPortInfoList **pp_GPPIL ) {
    /* Given a pointer to a pointer to a GPPIL, return just the pointer to a GPPIL */
    return (pp_GPPIL==NULL) ? NULL : *pp_GPPIL;
}

GPPortInfoList **
create_null_p_p__GPPortInfoList() {
    return malloc(sizeof(GPPortInfoList **));
}

Camera **
create_null_p_p__Camera() {
    return malloc(sizeof(Camera **));
}

CameraAbilitiesList **
create_null_p_p__CameraAbilitiesList() {
    return malloc(sizeof(CameraAbilitiesList **));
}

%}
