void	F1setfd P__((int));
int	F1getfd  P__((void));
int	F1reset P__((void));
void    wbyte P__((u_char));
u_char rbyte P__((void));
void    wstr P__((u_char *, int));
void    rstr P__((u_char *, int));
u_char  checksum P__((u_char));
int     F1ok P__((void));
long F1getdata P__((char *, u_char *, int));

int	F1status P__((int));
char	F1newstatus P__((int, char *));
int	F1howmany P__((void));
int     F1fopen P__((char *));
long     F1fread P__((u_char *, int));
long     F1fseek P__((long, int));
u_long     F1finfo P__((char *));
int	F1fclose P__((void));

int	F1deletepicture P__((int));
int F1ffs P__((void));

long     F1fwrite P__((u_char *, int, u_char ));

