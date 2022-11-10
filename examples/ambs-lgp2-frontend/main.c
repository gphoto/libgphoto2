#include <stdio.h>

#include <gphoto2/gphoto2-version.h>

/* The purpose of this code is to use things defined in gphoto2 header
 * files and then link to libgphoto2. */

static
struct {
  const char *const title;
  GPVersionFunc version_func;
} versions[] = {
  {"libgphoto2_port", gp_port_library_version},
  {"libgphoto2",      gp_library_version}
};

int main(void)
{
  for (size_t i=0; i<(sizeof(versions)/sizeof(versions[0])); i++) {
    const char *const title          = versions[i].title;
    const GPVersionFunc version_func = versions[i].version_func;
    const char **version_data = version_func(GP_VERSION_VERBOSE);
    printf("%s\n", title);
    for (size_t j=0; version_data[j]; j++) {
      printf("  %s\n", version_data[j]);
    }
  }
  return 0;
}
