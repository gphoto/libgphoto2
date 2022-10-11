#ifndef LIBGPHOTO2_GPHOTO2_PORT_LOCKING_H
#define LIBGPHOTO2_GPHOTO2_PORT_LOCKING_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#ifdef _GPHOTO2_INTERNAL_CODE

/** lock libltdl before calling lt_*() (libltdl is not thread safe) */
extern void gpi_libltdl_lock(void);

/** unlock libltdl after calling lt_*() (libltdl is not thread safe) */
extern void gpi_libltdl_unlock(void);

#endif /* defined(_GPHOTO2_INTERNAL_CODE) */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* defined(LIBGPHOTO2_GPHOTO2_PORT_LOCKING_H) */
