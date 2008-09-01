#ifndef __COMMAND_H__
#define __COMMAND_H__

int	F1ok (GPPort*);
long	F1getdata (GPPort*,char *, unsigned char *);
int	F1status (GPPort*);
char	F1newstatus (GPPort *port, int, char *);
int	F1howmany (GPPort *);
int	F1fopen (GPPort *,char *);
long	F1fread(GPPort *,unsigned char *data, long len);
long	F1fwrite(GPPort *,unsigned char *data, long len, unsigned char b);
long	F1fseek (GPPort *,long, int);
unsigned long	F1finfo (GPPort *,char *);
int	F1fclose (GPPort *);
int	F1deletepicture (GPPort*,int);
#endif /* __COMMAND_H__ */
