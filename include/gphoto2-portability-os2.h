#ifndef GPHOTO_OS2_INCLUDED
#include <db.h>
#include <sys/param.h>
#define GPHOTO_OS2_INCLUDED

#define CAMLIBS getenv("CAMLIBS")
#define RTLD_LAZY       0x001
#define VERSION "2"
#define usleep(t)          _sleep2(((t)+500)/ 1000)
//typedef u_short unsigned int;
#endif
