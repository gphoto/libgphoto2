/** \file ddb-common.h
 * \author Copyright (C) 2006 Hans Ulrich Niedermann
 *
 * \note
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * \note
 * This library is distributed in the hope that it will be useful, 
 * but WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details. 
 *
 * \note
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

#ifndef __DDB_TXT_H__
#define __DDB_TXT_H__

#include <stdio.h>
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

/* #define YYDEBUG 1 */

extern FILE *yyin;

void lexer_reset(const char *_filename);

#endif /* !__DDB_TXT_H__ */
