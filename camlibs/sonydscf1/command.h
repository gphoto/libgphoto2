#ifndef __COMMAND_H__
#define __COMMAND_H__

#include "common.h"

void	F1setfd (GPPort*,int);
int	F1getfd  (GPPort*);
int	F1reset (GPPort*);
u_char checksum(u_char addr, u_char *cp, int len);
int     F1ok (GPPort*);
long F1getdata (GPPort*,char *, u_char *, int);
int	F1status (GPPort*,int);
char	F1newstatus (GPPort *port, int, char *);
int	F1howmany (GPPort *);
int     F1fopen (GPPort *,char *);
long F1fread(GPPort *,u_char *data, long len);
long F1fwrite(GPPort *,u_char *data, long len, u_char b);
long     F1fseek (GPPort *,long, int);
u_long     F1finfo (GPPort *,char *);
int	F1fclose (GPPort *);

int	F1deletepicture (GPPort*,int);
int F1ffs (GPPort*);

#endif /* __COMMAND_H__ */
