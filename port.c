#include <string.h>
#include <gpio.h>
#include <gphoto2.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

int gp_port_count()
{
        return (gpio_get_device_count());
}

int gp_port_info(int port_number, CameraPortInfo *info)
{
        gpio_device_info i;

        if (gpio_get_device_info(port_number, &i)==GPIO_ERROR)
                return (GP_ERROR);

        /* Translate to gPhoto types */
        switch (i.type) {
                case GPIO_DEVICE_SERIAL:
                        info->type = GP_PORT_SERIAL;
                        break;
                case GPIO_DEVICE_PARALLEL:
                        info->type = GP_PORT_PARALLEL;
                        break;
                case GPIO_DEVICE_USB:
                        info->type = GP_PORT_USB;
                        break;
                case GPIO_DEVICE_IEEE1394:
                        info->type = GP_PORT_IEEE1394;
                        break;
                case GPIO_DEVICE_NETWORK:
                        info->type = GP_PORT_NETWORK;
                        break;
                default:
                        info->type = GP_PORT_NONE;
        }
        strcpy(info->name, i.name);
        strcpy(info->path, i.path);

        return (GP_OK);
}
