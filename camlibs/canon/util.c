/****************************************************************************
 *
 * File: util.c 
 *
 * Utility functions for the gPhoto Canon PowerShot A5(Z)/A50 driver.
 *
 ****************************************************************************/

/****************************************************************************
 *
 * include files
 *
 ****************************************************************************/

#include <stdio.h>
#include <stdarg.h>

/*****************************************************************************
 *
 * dump_hex
 *
 * Dumps a memory area as hex on the screen.
 *
 * msg  - Info message for the dump
 * buf  - the memory buffer to dump
 * len  - length of the buffer
 *
 ****************************************************************************/

extern int canon_debug_driver;

#define NIBBLE(_i)    (((_i) < 10) ? '0' + (_i) : 'A' + (_i) - 10)

void dump_hex(const char *msg, const unsigned char *buf, int len)
{
    int i;
    int nlocal;
    const unsigned char *pc;
    char *out;
    const unsigned char *start;
    char c;
    char line[100];

    if (canon_debug_driver > 8) { // Only  printout with max. debug level (9)
      start = buf;
      
#if 0
      if (len > 160)
	{
	  fprintf(stderr,"dump n:%d --> 160\n", len);
	  len = 160;
	}
#endif
      
      fprintf(stderr,"%s: (%d bytes)\n", msg, len);
      while (len > 0)
	{
	  sprintf(line, "%08x: ", buf - start);
	  out = line + 10;
	  
	  for (i = 0, pc = buf, nlocal = len; i < 16; i++, pc++)
	    {
	      if (nlocal > 0)
		{
		  c = *pc;
		  
		  *out++ = NIBBLE((c >> 4) & 0xF);
		  *out++ = NIBBLE(c & 0xF);
		  
		  nlocal--;
		}
	      else
		{
		  *out++ = ' ';
		  *out++ = ' ';
		}			/* end else */
	      
	      *out++ = ' ';
	    }			/* end for */
	  
	  *out++ = '-';
	  *out++ = ' ';
	  
	  for (i = 0, pc = buf, nlocal = len;
	       (i < 16) && (nlocal > 0);
	       i++, pc++, nlocal--)
	    {
	      c = *pc;
	      
	      if ((c < ' ') || (c >= 126))
		{
		  c = '.';
		}
	      
	      *out++ = c;
	    }			/* end for */
	  
	  *out++ = 0;
	  
	  fprintf(stderr,"%s\n", line);
	  
	  buf += 16;
	  len -= 16;
	}				/* end while */
    } /* end 'if debug' */
}				/* end dump */

void debug_message(const char * msg, ...)
{
  va_list ap;
  

  if (canon_debug_driver) {
    va_start(ap, msg);
    vfprintf(stderr,msg, ap);
    va_end(ap);
  }
}
    
/****************************************************************************
 *
 * End of file: util.c
 *
 ****************************************************************************/
