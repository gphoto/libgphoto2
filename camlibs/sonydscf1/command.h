#ifndef __COMMAND_H__
#define __COMMAND_H__

#include "common.h"

void	F1setfd P__((int));
int	F1getfd  P__((void));
int	F1reset P__((void));
void    wbyte P__((u_char));
u_char rbyte P__((void));
void    wstr P__((u_char *, int));
void    rstr P__((u_char *, int));
u_char checksum(u_char addr, u_char *cp, int len);
int     F1ok P__((void));
long F1getdata P__((char *, u_char *, int));
void sendcommand(u_char *p, int len);
void Abort(void);
int recvdata(u_char *p, int len);
int	F1status P__((int));
char	F1newstatus P__((int, char *));
int	F1howmany P__((void));
int     F1fopen P__((char *));
long F1fread(u_char *data, long len);
long F1fwrite(u_char *data, long len, u_char b);
long     F1fseek P__((long, int));
u_long     F1finfo P__((char *));
int	F1fclose P__((void));

int	F1deletepicture P__((int));
int F1ffs P__((void));

#endif /* __COMMAND_H__ */
