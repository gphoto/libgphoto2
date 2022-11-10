#include <iostream>

#include <gphoto2/gphoto2.h>
#include <gphoto2/gphoto2-camera.h>
#include <gphoto2/gphoto2-list.h>
#include <gphoto2/gphoto2-version.h>
#include <gphoto2/gphoto2-setting.h>
#include <gphoto2/gphoto2-file.h>
#include <gphoto2/gphoto2-filesys.h>
#include <gphoto2/gphoto2-context.h>
#include <gphoto2/gphoto2-abilities-list.h>

#include <gphoto2/gphoto2-port.h>
#include <gphoto2/gphoto2-port-info-list.h>
#include <gphoto2/gphoto2-port-log.h>
#include <gphoto2/gphoto2-port-portability.h>
#include <gphoto2/gphoto2-port-result.h>
#include <gphoto2/gphoto2-port-version.h>

static
unsigned long cxx_version = __cplusplus;

int main(void)
{
  std::cout << "cxx_version = " << cxx_version << std::endl;
  return 0;
}
