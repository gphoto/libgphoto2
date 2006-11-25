#ifndef __COMMAND_H__
#define __COMMAND_H__

#include "common.h"

void	F1setfd P__((GPPort*,int));
int	F1getfd  P__((GPPort*));
int	F1reset P__((GPPort*));
u_char checksum(u_char addr, u_char *cp, int len);
int     F1ok P__((GPPort*));
long F1getdata P__((GPPort*,char *, u_char *, int));
int	F1status P__((GPPort*,int));
char	F1newstatus P__((GPPort *port, int, char *));
int	F1howmany P__((GPPort *));
int     F1fopen P__((GPPort *,char *));
long F1fread(GPPort *,u_char *data, long len);
long F1fwrite(GPPort *,u_char *data, long len, u_char b);
long     F1fseek P__((GPPort *,long, int));
u_long     F1finfo P__((GPPort *,char *));
int	F1fclose P__((GPPort *));

int	F1deletepicture P__((GPPort*,int));
int F1ffs P__((GPPort*));

#endif /* __COMMAND_H__ */
