
#define STX 0x2  /* Start of data */
#define ETX 0x3  /* End of data */
#define EOT 0x4  /* End of session */
#define ENQ 0x5  /* Enquiry */
#define ACK 0x6
#define ESC 0x10
#define ETB 0x17 /* End of transmission block */
#define NAK 0x15

#define FUJI_VERSION 0x09 /* Version Info */
#define FUJI_SIZE 0x17 /* Get picture size */
#define FUJI_DELETE 0x19 /*Delete picture*/
#define FUJI_TAKE 0x27 /*Take picture*/
#define FUJI_CHARGE_FLASH 0x34 /*Charge the flash*/
#define FUJI_CMDS_VALID 0x4C /*Get list of supported commands*/
#define FUJI_PREVIEW 0x64 /*Get a preview*/

#define FUJI_MAXBUF_DEFAULT 9000000

#define DBG(x) GP_DEBUG(x)
#define DBG2(x,y) GP_DEBUG(x,y)
#define DBG3(x,y,z) GP_DEBUG(x,y,z)
#define DBG4(w,x,y,z) GP_DEBUG(w,x,y,z)

struct _CameraPrivateLibrary {
  int folders;
  int speed;
  int first_packet;
  int type;
  gp_port* dev;
  char folder[128];
  CameraFilesystem *fs;
  CameraFile *curcamfile;
  int fuji_initialized; 
  int fuji_count;
  int fuji_size;
  int maxnum;
  char has_cmd[256];
};

typedef struct {
  int folders;
  int speed;
  int first_packet;
  int type;
  gp_port* dev;
  char folder[128];
  CameraFilesystem *fs;
  CameraFile *curcamfile;
  int fuji_initialized; 
  int fuji_count;
  int fuji_size;
  int maxnum;
  char has_cmd[256];

} FujiData;
