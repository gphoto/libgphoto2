/******************************************************************************/
/* See lowlevel.h for licence and details.			              */
/******************************************************************************/


/*************************/
/* Included header files */
/*************************/
#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <gphoto2.h>
#include "library.h"
#include "lowlevel.h"


/***************/
/* Definitions */
/***************/
#define DEFAULT_TIMEOUT	200
#define STX	0x02	/****************/
#define ETX	0x03	/*		*/
#define ENQ	0x05	/*		*/
#define ACK	0x06	/*		*/  
#define XOFF	0x11	/* ESC quotes.	*/     
#define XON	0x13	/*		*/ 
#define NACK	0x15	/*		*/     
#define ETB	0x17	/*		*/
#define ESC	0x1b	/****************/
#define EOT	0x04       


/**************/
/* Prototypes */
/**************/
l_return_status_t l_esc_read (gpio_device *device, guchar *c);


l_return_status_t l_send (gpio_device *device, unsigned char *send_buffer, unsigned int send_buffer_size);


l_return_status_t l_receive (gpio_device *device, unsigned char **rb, unsigned int *rbs, guint timeout);


/*************/
/* Functions */
/*************/
l_return_status_t l_init (gpio_device *device)
{
	guchar c;
	gint i;

	gpio_set_timeout (device, DEFAULT_TIMEOUT);
	if (gpio_open (device) == GPIO_ERROR) return L_IO_ERROR;
	for (i = 0; ; i++) {
		/****************/
		/* Write ENQ. 	*/
		/****************/
		if (gpio_write (device, "\5", 1) == GPIO_ERROR) return L_IO_ERROR;
		if (gpio_read (device, &c, 1) < 1) {
			/******************************************************/
			/* We didn't receive anything. We'll try up to five   */
			/* time.                                              */
			/******************************************************/
			if (i == 4) {
			        gpio_close (device);
				return (L_IO_ERROR);
			}
			continue;
		}
		switch (c) {
		case ACK:
			/******************************************************/
			/* ACK received. We can proceed.                      */
			/******************************************************/
			return (L_SUCCESS);
		default:
			/******************************************************/
			/* The camera seems to be sending something. We'll    */
			/* dump all bytes we get and try again.               */
			/******************************************************/
			while (gpio_read (device, &c, 1) > 0);
			i--;
			break;
		}
	}
}


l_return_status_t l_exit (gpio_device *device)
{
	if (gpio_close (device) == GPIO_ERROR) return (L_IO_ERROR);
	return L_SUCCESS;
}


l_return_status_t l_esc_read (gpio_device *device, guchar *c)
{
	if (gpio_read (device, c, 1) < 1) return L_IO_ERROR;
	/**********************************************************************/
	/* STX, ETX, ENQ, ACK, XOFF, XON, NACK, and ETB have to be masked by  */
	/* ESC. If we receive one of those (except ETX and ETB) without mask, */
	/* we will not report an error, as it will be recovered automatically */
	/* later. If we receive ETX or ETB, we reached the end of the packet  */
	/* and report a transmission error, so that the error can be          */
	/* recovered.                                                         */
	/* If the camera sends us ESC (the mask), we will not count this byte */
	/* and read a second one. This will be reverted and processed. It     */
	/* then must be one of STX, ETX, ENQ, ACK, XOFF, XON, NACK, ETB, or   */
	/* ESC. As before, if it is not one of those, we'll not report an     */
	/* error, as it will be recovered automatically later.                */
	/**********************************************************************/
	if ((*c == STX ) || (*c == ETX) || (*c == ENQ ) || (*c == ACK) || (*c == XOFF) || (*c == XON) || (*c == NACK) || (*c == ETB)) {
		gp_debug_printf (GP_DEBUG_HIGH, "konica", "Wrong ESC masking!");
		if ((*c == ETX) || (*c == ETB)) return (L_TRANSMISSION_ERROR);

	} else if (*c == ESC) {
		if (gpio_read (device, c, 1) < 1) return L_IO_ERROR;
		*c = (~*c & 0xff);
		if ((*c != STX ) && (*c != ETX ) && (*c != ENQ) && (*c != ACK ) && (*c != XOFF) && (*c != XON) && (*c != NACK) && (*c != ETB ) && (*c != ESC))
			gp_debug_printf (GP_DEBUG_HIGH, "konica", "Wrong ESC masking!");
	}
	return (L_SUCCESS);
}


l_return_status_t l_send (gpio_device *device, guchar *send_buffer, guint send_buffer_size)
{
	guchar c;
	guchar checksum;
	/**********************************************************************/
	/*  sb: A pointer to the buffer that we will send.                    */
	/* sbs: Its size.		                                      */
	/**********************************************************************/
	guchar *sb; 	
	guint   sbs;
	gint i;

	i = 0;
	for (;;) {
		/****************/
		/* Write ENQ.	*/
		/****************/
		if (gpio_write (device, "\5", 1) == GPIO_ERROR) return (L_IO_ERROR);
		/****************/
		/* Read.	*/
		/****************/
		if (gpio_read (device, &c, 1) < 1) {
			/************************************/
			/* Let's try for a couple of times. */
			/************************************/
			i++;
			if (i == 5) return (L_IO_ERROR);
			continue;
		}
		switch (c) {
		case ACK:
			/****************************************/
			/* ACK received. We can proceed.	*/
			/****************************************/
			break;
		case NACK:
			/****************************************/
			/* NACK received. We'll start from the  */
			/* beginning.				*/
			/****************************************/
			continue;
		case ENQ:
			/******************************************************/
			/* ENQ received. It seems that the camera would like  */
			/* to send us data, but we do not want it and         */
			/* therefore simply reject it. The camera will try    */
			/* two more times with ENQ to get us to receive data  */
			/* before finally giving up and sending us ACK.       */
			/******************************************************/
			/****************/
			/* Write NACK.	*/
			/****************/
			c = NACK;
			if (gpio_write (device, &c, 1) == GPIO_ERROR) 
				return (L_IO_ERROR);
			for (;;) {
				if (gpio_read (device, &c, 1) < 1) 
					return (L_IO_ERROR);
				switch (c) {
				case ENQ: 
					/**************************************/
					/* The camera has not yet given up.   */
					/**************************************/
					continue;
				case ACK:
					/**************************************/
					/* ACK received. We can proceed.      */
					/**************************************/
					break;
				default:
					/**************************************/
					/* This should not happen.            */
					/**************************************/
					return (L_TRANSMISSION_ERROR);
				}
				break;
			}
			break;
		default:
			/******************************************************/
			/* The camera seems to send us data. We'll            */
			/* simply dump it and try again.                      */
			/******************************************************/
			continue;
		}
		break;
	}
	/********************************************************/
	/* We will write:			 		*/
	/*  - STX						*/
	/*  - low order byte of (send_buffer_size + 5)		*/
	/*  - high order byte of (send_buffer_size + 5)		*/
	/*  - 'send_buffer_size' bytes data plus ESC quotes	*/
	/*  - ETX						*/
	/*  - 1 byte for checksum plus ESC quotes		*/
	/*							*/
	/* The checksum covers all bytes after STX and before	*/
	/* the checksum byte(s).				*/
	/********************************************************/
	sbs = send_buffer_size + 5;
	sb = g_new (guchar, sbs);
	sb[0] = STX;
	sb[1] = send_buffer_size;
	sb[2] = send_buffer_size >> 8;
	checksum  = sb[1];
	checksum += sb[2];
	for (i = 3; i < (sbs - 2); i++) {
		checksum += *send_buffer;
		if (	(*send_buffer == STX ) || 
			(*send_buffer == ETX ) ||
			(*send_buffer == ENQ ) ||
			(*send_buffer == ACK ) ||	
			(*send_buffer == XOFF) ||
			(*send_buffer == XON ) ||
			(*send_buffer == NACK) ||
			(*send_buffer == ETB ) ||
			(*send_buffer == ESC )) {
			sb = g_renew (guchar, sb, ++sbs);
			sb[  i] = ESC;
			sb[++i] = ~*send_buffer;
		} else  sb[  i] =  *send_buffer;
		send_buffer++;
	}
	sb[sbs - 2] = ETX;
	checksum += ETX;
	if (	(checksum == STX ) || 
		(checksum == ETX ) || 
		(checksum == ENQ ) || 
		(checksum == ACK ) || 
		(checksum == XOFF) ||
		(checksum == XON ) || 
		(checksum == NACK) || 
		(checksum == ETB ) ||
		(checksum == ESC )) {
		sb = g_renew (guchar, sb, ++sbs);
		sb[sbs - 2] = ESC;
		sb[sbs - 1] = ~checksum;
	} else  sb[sbs - 1] =  checksum;
	for (i = 0; ; i++) {
		/************************/
		/* Write data as above.	*/
		/************************/
		if (gpio_write (device, sb, sbs) == GPIO_ERROR) {
			g_free (sb);
			return (L_IO_ERROR);
		}
		if (gpio_read (device, &c, 1) < 1) {
			g_free (sb);
			return L_IO_ERROR;
		}
		switch (c) {
		case ACK:
			/******************************************************/
			/* ACK received. We can proceed.                      */
			/******************************************************/
			g_free (sb);
			/****************/
			/* Write EOT.	*/
			/****************/
			c = EOT;
			if (gpio_write (device, &c, 1) == GPIO_ERROR) return (L_IO_ERROR);
			return (L_SUCCESS);
		case NACK:
			/******************************************************/
			/* NACK received. We'll try up to three times.        */
			/******************************************************/
			if (i == 2) {
				g_free (sb);
				return (L_TRANSMISSION_ERROR);
			} else break;
		default:
			/******************************************************/
			/* Should not happen.                                 */
			/******************************************************/
			return (L_TRANSMISSION_ERROR);
		}
	}
}


l_return_status_t l_receive (gpio_device *device, guchar **rb, guint *rbs, guint timeout)
{
	guchar c, d;
	gboolean error_flag;
	guint i, j, rbs_internal;
	unsigned char checksum;
	l_return_status_t return_status;

	for (i = 0; ; ) {
		gpio_set_timeout (device, timeout);  
		if (gpio_read (device, &c, 1) < 1) return (L_IO_ERROR);
		gpio_set_timeout (device, DEFAULT_TIMEOUT);
		switch (c) {
		case ENQ:
			/******************************************************/
			/* ENQ received. We can proceed.                      */
			/******************************************************/
			break;
		case ACK:
			/******************************************************/
			/* ACK received. We'll try again at most ten times.   */
			/******************************************************/
			if (i == 9) {
				/**********************************************/
				/* The camera hangs! Although it could be     */
				/* that the camera accepts one of the         */
				/* commands                                   */
				/*  - KONICA_CANCEL,                          */
				/*  - KONICA_GET_IO_CAPABILITY, and           */
				/*  - KONICA_SET_IO_CAPABILITY,               */
				/* we can not get the camera back to working  */
				/* correctly.                                 */
				/**********************************************/
				return (L_TRANSMISSION_ERROR);
			}
			i++;
			break;
		default:
			/******************************************************/
			/* We'll dump this data until we get ENQ (then, we'll */
			/* proceed) or nothing any more (then, we'll report   */
			/* error).                                            */
			/******************************************************/
			for (;;) {
				if (gpio_read (device, &c, 1) < 0) return (L_TRANSMISSION_ERROR); 
				if (c == ENQ) break;
			}
			break;
		}
		if (c == ENQ) break;
	}
	/**************/
	/* Write ACK. */
	/**************/
	if (gpio_write (device, "\6", 1) == GPIO_ERROR) return (L_IO_ERROR);
	for (*rbs = 0; ; ) {
		for (j = 0; ; j++) {
			if (gpio_read (device, &c, 1) < 1) return (L_IO_ERROR);
			switch (c) {
			case STX:
				/**********************************************/
				/* STX received. We can proceed.              */
				/**********************************************/
				break;
			default:
				/**********************************************/
				/* We'll dump this data and try again.        */
				/**********************************************/
				continue;
			}
			/******************************************************/
			/* Read 2 bytes for size (low order byte, high order  */
			/* byte) plus ESC quotes.                             */
			/******************************************************/
			return_status = l_esc_read (device, &c);
			if (return_status != L_SUCCESS) return (return_status);
			checksum = c;
			return_status = l_esc_read (device, &d);
			if (return_status != L_SUCCESS) return (return_status);
			checksum += d;
			rbs_internal = (d << 8) | c;
			if (*rbs == 0) *rb = g_new (guchar, rbs_internal);
			else *rb = g_renew (guchar, *rb, *rbs + rbs_internal);
			/******************************************************/
			/* Read 'rbs_internal' bytes data plus ESC quotes.    */
			/******************************************************/
			error_flag = FALSE;
			for (i = 0; i < rbs_internal; i++) {
				return_status = l_esc_read (device, &((*rb)[*rbs + i]));
				switch (return_status) {
				case L_IO_ERROR: 
					return (L_IO_ERROR);
				case L_TRANSMISSION_ERROR:
					/**************************************/
					/* We already received ETX or ETB.    */
					/* Later, we'll read the checksum     */
					/* plus ESC quotes and reject the     */
					/* packet.                            */
					/**************************************/
					error_flag = TRUE;
        	                        break;
				case L_SUCCESS:
					/**************************************/
					/* We can proceed.                    */
					/**************************************/
					checksum += (*rb)[*rbs + i];
					break;
				}
				if (error_flag) break;
			}
			if (!error_flag) {
				if (gpio_read (device, &d, 1) < 1) return (L_IO_ERROR);
				switch (d) {
				case ETX:
					/**************************************/
					/* ETX received. This is the last     */
					/* packet.                            */
					/**************************************/
					break;
				case ETB:
					/**************************************/
					/* ETB received. There are more       */
					/* packets to come.                   */
					/**************************************/
					break;
				default:
					/**************************************/
					/* We get more bytes than expected.   */
					/* Nothing serious, as we will simply */
					/* dump all bytes until we receive    */
					/* ETX or ETB. Later, we'll read the  */
					/* checksum with ESC quotes and       */
					/* reject the packet.                 */
					/**************************************/
					while ((d != ETX) && (d != ETB)) {
						if (gpio_read (device, &d, 1) < 1) return (L_IO_ERROR);
					}
					error_flag = TRUE;
					break;
				}
			}
			checksum += d;
			/******************************************************/
			/* Read 1 byte for checksum plus ESC quotes.          */
			/******************************************************/
			return_status = l_esc_read (device, &c);
			if (return_status != L_SUCCESS) return (return_status);
			if ((c == checksum) && (!error_flag)) {
				*rbs += rbs_internal;
				/****************/
				/* Write ACK.	*/
				/****************/
				if (gpio_write (device, "\6", 1) == GPIO_ERROR) return (L_IO_ERROR);
				break;
			} else {
				/**********************************************/
				/* Checksum wrong or transmission error. The  */
				/* camera will send us the data at the most   */
				/* three times.                               */
				/**********************************************/
				if (j == 2) return (L_TRANSMISSION_ERROR);
				/****************/
				/* Write NACK.	*/
				/****************/
				c = NACK;
				if (gpio_write (device, &c, 1) == GPIO_ERROR) return (L_IO_ERROR);
				continue;
			}
		}
		if (gpio_read (device, &c, 1) < 1) 
			return (L_IO_ERROR);
		switch (c) {
			case EOT:
				/**********************************************/
				/* EOT received. We can proceed.              */
				/**********************************************/
				break;
			default:
				/**********************************************/
				/* Should not happen.                         */
				/**********************************************/
				return (L_TRANSMISSION_ERROR);
		}
		/**************************************************************/
		/* Depending on d, we will either continue to receive data or */
		/* stop.						      */
		/*  - ETX:  We are all done. 				      */
		/*  - ETB:  We expect more data.			      */
		/*  - else: Should not happen.				      */
		/**************************************************************/
		switch (d) {
		case ETX:
			/******************************************************/
			/* We are all done.				      */
			/******************************************************/
			return (L_SUCCESS);
		case ETB:
			/******************************************************/
			/* We expect more data.                               */
			/******************************************************/
			/****************/
			/* Read ENQ.	*/
			/****************/
			if (gpio_read (device, &c, 1) < 1) 
				return (L_IO_ERROR);
			switch (c) {
			case ENQ:
				/**********************************************/
				/* ENQ received. We can proceed.              */
				/**********************************************/
				break;
			default:
				/**********************************************/
				/* Should not happen.                         */
				/**********************************************/
				return (L_TRANSMISSION_ERROR);
			}
			/****************/
			/* Write ACK.	*/
			/****************/
			if (gpio_write (device, "\6", 1) == GPIO_ERROR) 
				return (L_IO_ERROR);
			break;
		default:
			/******************************************************/
			/* Should not happen.                                 */
			/******************************************************/
			return (L_TRANSMISSION_ERROR);
		}
	}

}


l_return_status_t l_send_receive (gpio_device *device, guchar *send_buffer, guint send_buffer_size, guchar **receive_buffer, guint *receive_buffer_size)
{
	l_return_status_t return_status;

	/****************/
	/* Send data.	*/
	/****************/
	return_status = l_send (device, send_buffer, send_buffer_size);
	if (return_status != L_SUCCESS) return (return_status);
	/********************************/
	/* Receive control data.	*/
	/********************************/
	return_status = l_receive (device, receive_buffer, receive_buffer_size, 3000);
	if (return_status != L_SUCCESS) return (return_status);
	if (((*receive_buffer)[0] != send_buffer[0]) || 
	    ((*receive_buffer)[1] != send_buffer[1])) {
		gp_debug_printf (GP_DEBUG_HIGH, "konica", "Error: Commands differ! %i %i != %i %i.\n", 
			(*receive_buffer)[0], (*receive_buffer)[1], 
			send_buffer[0], send_buffer[1]); 
		return (L_TRANSMISSION_ERROR);
	}
	else return (return_status);
}


l_return_status_t l_send_receive_receive (
	gpio_device *device,
	guchar  *send_buffer, 
	guint    send_buffer_size, 
	guchar **image_buffer, 
	guint   *image_buffer_size, 
	guchar **receive_buffer, 
        guint   *receive_buffer_size,
	guint    timeout)
{
	l_return_status_t return_status;

	/****************/
	/* Send data.	*/
	/****************/
        return_status = l_send (device, send_buffer, send_buffer_size);
        if (return_status != L_SUCCESS) return (return_status);
	/************************/
        /* Receive data.	*/
	/************************/
        return_status = l_receive (device, image_buffer, image_buffer_size, timeout);
        if (return_status != L_SUCCESS) return (return_status);
	/********************************/
	/* Receive control data.	*/
	/********************************/
        return_status = l_receive (device, receive_buffer, receive_buffer_size, DEFAULT_TIMEOUT);
	if (return_status != L_SUCCESS) return (return_status);
	if (((*receive_buffer)[0] != send_buffer[0]) || 
	    ((*receive_buffer)[1] != send_buffer[1])) {
		gp_debug_printf (GP_DEBUG_HIGH, "konica", "Error: Commands differ! %i %i != %i %i.\n", 
			(*receive_buffer)[0], (*receive_buffer)[1], 
			send_buffer[0], send_buffer[1]); 
		return (L_TRANSMISSION_ERROR);
	}
	return (return_status);
}
