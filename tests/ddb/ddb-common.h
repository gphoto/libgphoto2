#ifndef __DDB_TXT_H__
#define __DDB_TXT_H__

#define _GPHOTO2_INTERNAL_CODE

#include <gphoto2/gphoto2-abilities-list.h>

typedef union {
  char *str_val;
  unsigned int ui_val;
} symtype;

#define YYSTYPE symtype

extern int yylineno;

void
yyerror (const char *filename, CameraAbilitiesList *al, 
	 const char *str);

int
yyparse (const char *filenname, CameraAbilitiesList *al);

#define YYLTYPE my_yyltype

typedef struct {
  int lineno;
  int column;
  const char *filename;
} FilePosition;

typedef struct YYLTYPE {
  FilePosition begin, end;
} YYLTYPE;


#define YYLLOC_DEFAULT(Current, Rhs, N)                                \
  do									\
    if (N)								\
      {									\
	(Current).begin.lineno = YYRHSLOC(Rhs, 1).begin.lineno;		\
	(Current).begin.column = YYRHSLOC(Rhs, 1).begin.column;		\
	(Current).end.lineno   = YYRHSLOC(Rhs, N).end.lineno;		\
	(Current).end.lineno   = YYRHSLOC(Rhs, N).end.column;		\
      }									\
    else								\
      {									\
	(Current).begin.lineno = (Current).end.lineno =		\
	  YYRHSLOC(Rhs, 0).end.lineno;					\
	(Current).begin.column = (Current).end.column =		\
	  YYRHSLOC(Rhs, 0).end.column;					\
      }									\
  while (0)

#define YY_DECL int yylex (void)
YY_DECL;
/* #define YYLEX_PARAM void */

// #define YYDEBUG 1

void lexer_reset(const char *_filename);

#endif /* !__DDB_TXT_H__ */
