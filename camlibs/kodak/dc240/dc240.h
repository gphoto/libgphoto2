#define COMM1	(unsigned char)0x00
#define READY	(unsigned char)0x10
#define ACK	(unsigned char)0xd1
#define PACK1	(unsigned char)0xd2

/* nice. errors all have upper nibble of 'e' */
#define	NAK	(unsigned char)0xe1
#define	COMM0	(unsigned char)0xe2
#define PACK0	(unsigned char)0xe3
#define CANCL	(unsigned char)0xe4

#define TIMEOUT	        750
#define SLEEP_TIMEOUT 	50
#define RETRIES         8

#define HPBS            1024*1

