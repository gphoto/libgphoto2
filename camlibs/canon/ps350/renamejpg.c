/* RenameJPG    Copyright © 1999 J.A. Bezemer   GPL'd */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/stat.h>
#include <unistd.h>

void main (int argc, char **argv)
{
    FILE *jpgfile;
    time_t time;
    struct tm *temptm;
    unsigned char buffer[4];
    char timestring[200];
    char tempstring[300];
    char newname[300];
    int i;
    struct stat statbuf;
    struct stat statbuf2;
    int newnamecounter;

    if (argc < 2)
    {
      fprintf (stderr, "Usage: renamejpg [filename(s)]\n\n");
      exit(1);
    }

    for (i=1; i<argc; i++)
    {    
        if ((jpgfile = fopen (argv[i], "rb")) == NULL)
        {
          fprintf (stderr, "File could not be opened. Aborted.\n\n");
          exit(2);
        }

        fseek(jpgfile, 0x32, SEEK_SET);
	fread(buffer, 1, 4, jpgfile);

	time=    (((time_t) buffer[0])       )
               | (((time_t) buffer[1]) << 8  )
	       | (((time_t) buffer[2]) << 16 )
	       | (((time_t) buffer[3]) << 24 );

        temptm=gmtime(&time);

        strftime(timestring, 200, "%Y%m%d-%H%M%S", temptm);

	fstat (fileno (jpgfile), &statbuf);

	if (statbuf.st_size < 4000)
	/* thumbnail */
	    snprintf(tempstring, 200, "thm_%s", timestring);
	else
	/* other jpg */
	    snprintf(tempstring, 200, "aut_%s", timestring);

	snprintf(newname, 200, "%s.jpg", tempstring);
	newnamecounter=1;

	/* check if file exists. Will hang if all names are used... */
	while (stat(newname, &statbuf2)==0)
	{
	    newnamecounter++;
	    snprintf(newname, 200, "%s-%d.jpg", tempstring, newnamecounter);
	}

	printf("File %s, size %ld, %s -> %s\n", argv[i], statbuf.st_size, 
               timestring, newname);

        fclose(jpgfile);

	rename(argv[i], newname);
    }
}
