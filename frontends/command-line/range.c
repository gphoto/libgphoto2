#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "main.h"
#include "range.h"

/* 
  str_grab_nat() - Grab positive decimal integer (natural number) from str 
  	starting with pos byte. On return pos points to the first byte after 
	grabbed integer.
*/

int str_grab_nat(const char *str, int *pos) {
	
	int	result, old_pos = *pos;
	char 	*buf;

	if (!(buf = (char*)strdup(str))) /* we can not modify str */
		return -1;
	
	*pos += strspn(&buf[*pos], "0123456789");
	buf[*pos] = 0;
	result = atoi(&buf[old_pos]);
	free(buf);
	return result;
}

/*
  parse_range() - Intentionally, parse list of images given in range. Syntax
  	of range is:
		( m | m-n ) { , ( m | m-n ) }
	where m,n are decimal integers with 1 <= m <= n <= MAX_IMAGE_NUMBER.
	Ranges are XOR (exclusive or), so that 
		1-5,3,7
	is equivalent to
		1,2,4,5,7	
	Conversion from 1-based to 0-based numbering is performed, that is, 
	n-th image corresponds to (n-1)-th byte in index. The value of 
	index[n] is either 0 or 1. 0 means not selected, 1 selected. 
	index passed to parse_range() must be an array of MAX_IMAGE_NUMBER 
	char-s set to 0 (use memset() or bzero()).
*/
		
int parse_range(const char *range, char *index) {
	
	int	i = 0, l = -1, r = -1, j;
	
	do {
		switch (range[i]) {
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
				l = str_grab_nat(range, &i);
				if (l <= 0 || MAX_IMAGE_NUMBER < l)
					return (GP_ERROR); 
				break;
			
			case '-' :
				if (i == 0)
					return (GP_ERROR); /* '-' begins range */
				i++;		
				r = str_grab_nat(range, &i);		
				if (r <= 0 || MAX_IMAGE_NUMBER < r)
					return (GP_ERROR); 
				break;
				
			case '\0' :
				break; 
				
			case ',' :
				break; 
		
			default :
				return (GP_ERROR); /* unexpected character */
		}
	} while (range[i] != '\0' && range[i] != ',');	/* scan range till its end or ',' */
	
	if (i == 0)
		return (GP_ERROR); /* empty range or ',' at the begining */
	
	if (0 < r) { /* update range of bytes */ 
		if (r < l)
			return (GP_ERROR); /* decreasing ranges are not allowed */
		
		for (j = l; j <= r; j++)
			index[j - 1] ^= 1; /* convert to 0-based numbering */
	} 
	else  /* update single byte */
		index[l - 1] ^= 1; /* convert to 0-based numbering */

	if (range[i] == ',') 
		return parse_range(&range[i + 1], index); /* parse remainder */
	
	return (GP_OK);
}
