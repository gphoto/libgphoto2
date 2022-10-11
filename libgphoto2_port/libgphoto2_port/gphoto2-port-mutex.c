#include <pthread.h>

#include <gphoto2/gphoto2-port-mutex.h>

static
pthread_mutex_t gpi_libltdl_mutex = PTHREAD_MUTEX_INITIALIZER;

void gpi_libltdl_lock(void)
{
  pthread_mutex_lock(&gpi_libltdl_mutex);
}

void gpi_libltdl_unlock(void)
{
  pthread_mutex_unlock(&gpi_libltdl_mutex);
}
