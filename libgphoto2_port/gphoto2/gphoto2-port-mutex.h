#ifndef __GPHOTO2_MUTEX_H__
#define __GPHOTO2_MUTEX_H__

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

#endif /* __GPHOTO2_MUTEX_H__ */
