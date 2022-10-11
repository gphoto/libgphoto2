#ifndef LIBGPHOTO2_GPHOTO2_PORT_MUTEX_H
#define LIBGPHOTO2_GPHOTO2_PORT_MUTEX_H

#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#ifdef _GPHOTO2_INTERNAL_CODE

extern pthread_mutex_t gpi_libltdl_mutex;

#endif /* defined(_GPHOTO2_INTERNAL_CODE) */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* defined(LIBGPHOTO2_GPHOTO2_PORT_MUTEX_H) */
