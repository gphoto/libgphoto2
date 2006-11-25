/*

  Packet Structure
  --------------------------------------------------------------
  Command Packet:  CMD 00 [4 byte field] 00 1A
  Camera Response: [variable width field] CHECKSUM

  Responses
  --------------------------------------------------------------

	00	Command completed
	10	Command ready
	d1	Command received (ACK)
	d2	Correct packet
	e1 	Command incorrect (NAK)
	e2	Command execution error
	e3	Illegal packet
	e4	Cancel


  Commands & Byte field descriptions
  --------------------------------------------------------------
  These are ones relavent to gPhoto

  * Album #0 is the root folder (directory/album/etc...)

  List albums (folders)
	Packet: 44 00 00 00 00 00 00 1A
	
	Response:
	ALBUMNAME1\0\0\0\0\0ALBUMNAME2\0\0\0\0\0ALBUMNAME3\0\0\0\0\0 [...] CS

	(258 bytes, 15 byte fixed-with fields in multiple packets)
	CS (checksum)	

  Picture count in album
	Packet: 45 LOC 00 00 AN 00 00 1A
	
		LOC (location): 00 (memory), 01 (card)
		AN (album #)

	Response: NP4 NP3 NP2 NP1 [11 reserved bytes] CS
	
		NP4 - NP1 (number of pictures): MSB to LSB number of pictures
		CS (checksum)

  Set Active album for next taken picture
	Packet: 49 00 AN 00 00 00 00 1A
	
		AN (album #)

  Set Image Quality
	Packet:	71 00 QQ 00 00 00 00 1A

		QQ (quality): 00 (no comp), 01 (best), 02 (better), 03 (good)

  Take picture
	Packet:	CMD 00 00 00 00 00 00 1A

		CMD:
		77	Take a picture to FLASH
		7C	Take a picture to card

  Picture Transfer/Deletion
	Packet:	CMD AA PU PL AL 00 00 1A

		CMD:
		4A	Send filename in album
		51	Send picture in memory
		52	Send TIFF info in memory
		54	Send TIFF in memory
		55	Send picture info in memory
		64	Send TIFF on card
		76 	Copy image from memory to card
		7B	Erase picture from card

		AA (access mode): 00 (sequential), 01 (album)
		PU (picture # upper)
		PL (picture # lower)
		AL (album #)

*/

#define COMM1	(unsigned char)0x00
#define READY	(unsigned char)0x10
#define ACK		(unsigned char)0xd1
#define PACK1	(unsigned char)0xd2

/* nice. errors all have upper nibble of 'e' */
#define	NAK		(unsigned char)0xe1
#define	COMM0	(unsigned char)0xe2
#define PACK0	(unsigned char)0xe3
#define CANCL	(unsigned char)0xe4

#define TIMEOUT		5000
#define SLEEP_TIMEOUT 	50
#define RETRIES		5
