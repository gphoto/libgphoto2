/* Type definitions */
typedef struct {
	CameraPortInfo port_settings;
        gpio_device *device;
        gpio_device_settings *device_settings;
	gboolean image_id_long;
        gboolean debug_flag;
        CameraFilesystem *filesystem;
} konica_data_t;

