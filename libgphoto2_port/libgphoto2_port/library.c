#include <sys/types.h>
#include <stdio.h>

#include "gphoto2-port.h"
#include "library.h"

extern int device_count;
extern int glob_debug_level;
extern gp_port_info device_list[];
void *device_lh;

int gp_port_library_is_valid (const char *filename) {

        void *lh;

        if ((lh = GP_SYSTEM_DLOPEN(filename))==NULL) {
                gp_port_debug_printf(GP_DEBUG_LOW, glob_debug_level,
                        "%s is not a library (%s) ", filename, GP_SYSTEM_DLERROR());
                return (GP_ERROR);
        }

        gp_port_debug_printf(GP_DEBUG_LOW, glob_debug_level, "%s is a library ", filename);
        GP_SYSTEM_DLCLOSE(lh);

        return (GP_OK);
}

int gp_port_library_list_load(const char *filename, int loaded[], gp_port_info *list, int *count) {

        void *lh;
        int type, x;
        gp_port_ptr_type lib_type;
        gp_port_ptr_list lib_list;
        int old_count = *count;

        if ((lh = GP_SYSTEM_DLOPEN(filename))==NULL)
                return (GP_ERROR);

        lib_type = (gp_port_ptr_type)GP_SYSTEM_DLSYM(lh, "gp_port_library_type");
        lib_list = (gp_port_ptr_list)GP_SYSTEM_DLSYM(lh, "gp_port_library_list");

        if ((!list) || (!lib_type)) {
                gp_port_debug_printf(GP_DEBUG_LOW, glob_debug_level,
                        "could not find type/list symbols: %s ", GP_SYSTEM_DLERROR());
                GP_SYSTEM_DLCLOSE(lh);
                return (GP_ERROR);
        }

        type = lib_type();

        if (loaded[type] == 1) {
                gp_port_debug_printf(GP_DEBUG_LOW, glob_debug_level,
                        "%s (%i) already loaded ", filename, type);
                GP_SYSTEM_DLCLOSE(lh);
                return (GP_ERROR);
        } else {
                loaded[type] = 1;
        }

        if (lib_list(list, count)==GP_ERROR)
                gp_port_debug_printf(GP_DEBUG_LOW, glob_debug_level,
                        "%s could not list devices ", filename);

        gp_port_debug_printf(GP_DEBUG_LOW, glob_debug_level,
                "Loaded these devices from %s:", filename);
        /* copy in the library path */
        for (x=old_count; x<(*count); x++) {
                gp_port_debug_printf(GP_DEBUG_LOW, glob_debug_level,
                        "\t%s path=\"%s\"", list[x].name, list[x].path);
                strcpy(list[x].library_filename, filename);
        }

        GP_SYSTEM_DLCLOSE(lh);
        return (GP_OK);
}

int gp_port_library_list (gp_port_info *list, int *count) {

        GP_SYSTEM_DIR d;
        GP_SYSTEM_DIRENT de;
        int loaded[256];
        int x;
        char buf[1024];

        *count = 0;


        for (x=0;x<256; x++)
                loaded[x]=0;

        /* Look for available camera libraries */
        d = GP_SYSTEM_OPENDIR(IOLIBS);
        if (!d) {
                gp_port_debug_printf(GP_DEBUG_LOW, glob_debug_level,
                        "couldn't open %s ", IOLIBS);
                return GP_ERROR;
        }
        do {
           /* Read each entry */
           de = GP_SYSTEM_READDIR(d);
           if (de) {
#if defined(OS2) || defined(WIN32)
                sprintf(buf, "%s\\%s", IOLIBS, GP_SYSTEM_FILENAME(de));
#else
                sprintf(buf, "%s/%s", IOLIBS, GP_SYSTEM_FILENAME(de));
#endif
		/* Don't try to open ".*" */
		if (*buf && (buf[0] == '.'))
			continue;

                if (gp_port_library_is_valid(buf) == GP_OK)
                        gp_port_library_list_load(buf, loaded, list, count);
           }
        } while (de);

        GP_SYSTEM_CLOSEDIR(d);

        return (GP_OK);
}

int gp_port_library_load (gp_port *device, gp_port_type type) {

        int x=0;
        gp_port_ptr_operations ops_func;

        for (x=0; x<device_count; x++) {
                if (device_list[x].type == type) {
                        /* Open the correct library */
                        device->library_handle = GP_SYSTEM_DLOPEN(device_list[x].library_filename);
                        if (!device->library_handle) {
                                gp_port_debug_printf(GP_DEBUG_LOW, glob_debug_level,
                                        "bad handle: %s %s ",
                                        device_list[x].library_filename, GP_SYSTEM_DLERROR());
                                return (GP_ERROR);
                        }

                        /* Load the operations */
                        ops_func = (gp_port_ptr_operations)GP_SYSTEM_DLSYM(device->library_handle, "gp_port_library_operations");
                        if (!ops_func) {
                                gp_port_debug_printf(GP_DEBUG_LOW, glob_debug_level,
                                        "can't load ops: %s %s ",
                                        device_list[x].library_filename, GP_SYSTEM_DLERROR());
                                GP_SYSTEM_DLCLOSE(device->library_handle);
                                return (GP_ERROR);
                        }
                        device->ops = ops_func();
                        return (GP_OK);
                }
        }
        return (GP_ERROR);
}

int gp_port_library_close (gp_port *device) {

        GP_SYSTEM_DLCLOSE(device->library_handle);

        return (GP_OK);
}
