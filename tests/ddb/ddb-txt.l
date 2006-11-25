/** \file ddb-txt.l
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

%option nounput
%option noyywrap
%option yylineno
%option nobackup

%{
#include <stdlib.h>
#include <errno.h>
#define _GNU_SOURCE
#include <string.h>
#include "ddb-common.h"
#include "ddb-txt.tab.h"

//#if !(YYDEBUG)
# define ECHO do {} while (0)
//#endif

extern char *strndup(const char *s, size_t n);

static char *filename = NULL;

YYSTYPE yylval = { str_val: NULL };

static void lexer_error(const char *msg);

%}

%%
\#.*$            { /* ignore comment */ }
"interface"	 { ECHO; return TOK_INTERFACE; }
"{"		 { ECHO; return TOK_BEGIN; }
"driver"	 { ECHO; return TOK_DRIVER; }
"device"	 { ECHO; return TOK_DEVICE; }
"}"              { ECHO; return TOK_END; }
"serial"         { ECHO; return TOK_SERIAL; }
"usb"            { ECHO; return TOK_USB; }
"disk"           { ECHO; return TOK_DISK; }
"ptpip"          { ECHO; return TOK_PTPIP; }
"class"          { ECHO; return TOK_CLASS; }
"subclass"       { ECHO; return TOK_SUBCLASS; }
"protocol"       { ECHO; return TOK_PROTOCOL; }
"vendor"         { ECHO; return TOK_VENDOR; }
"product"        { ECHO; return TOK_PRODUCT; }
"speeds"         { ECHO; return TOK_SPEEDS; }
"operations"     { ECHO; return TOK_OPERATIONS; }
"driver_options" { ECHO; return TOK_DRIVER_OPTIONS; }
"option"         { ECHO; return TOK_OPTION; }
"file_operations" { ECHO; return TOK_FILE_OPERATIONS; }
"folder_operations" { ECHO; return TOK_FOLDER_OPERATIONS; }
"driver_status"  { ECHO; return TOK_DRIVER_STATUS; }
"device_type"    { ECHO; return TOK_DEVICE_TYPE; }
("none"|"config"|"capture_image"|"capture_video"|"capture_audio"|"capture_preview"|"delete"|"preview"|"raw"|"audio"|"exif"|"delete_all"|"put_file"|"make_dir"|"remove_dir"|"production"|"testing"|"experimental"|"deprecated"|"still_camera"|"audio_player") {
                  ECHO;
                  yylval.str_val = strndup(yytext, yyleng);
                  return VAL_FLAG;
                }
";"             { ECHO; return TOK_SEP; }
","             { ECHO; return TOK_COMMA; }
\"[A-Za-z0-9\.\+\-<>\|_/()!:,;=\\\$\'@&%\{\}\[\]+#~\* ]+\"    { ECHO; 
			yylval.str_val = strndup(yytext, yyleng);
			return TOK_STRING;
		}
(0x)?[0-9a-fA-F]+    {ECHO;
			yylval.ui_val = strtoul(yytext, NULL, 0);
			if ((ERANGE == errno) && 
			    (ULONG_MAX == yylval.ui_val)) {
				lexer_error("Number out of range");
			} else if (EINVAL == errno) {
				lexer_error("Invalid number");
			}
			return TOK_NUMBER;
		}
\n              { ECHO; /* ignore newlines */ }
[ \t\r]+        { ECHO; /* ignore whitespace */ }
.               {
			static char msg[] = "unexpected character (in string?) '?'";
			msg[strlen(msg)-2] = yytext[0];
			lexer_error(msg);
		}
%%

void lexer_reset(const char *_filename)
{
  filename = (char *) _filename;
}

static void lexer_error(const char *msg)
{
  fprintf(stderr, "%s:%d: lexer error: %s\n",
		  filename, yylineno, msg);
  exit(4);
}
