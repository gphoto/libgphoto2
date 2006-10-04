/** \file ddb-txt.y
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

%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gphoto2/gphoto2-abilities-list.h>

#include "ddb-common.h"
#include "ddb-txt.tab.h"


extern char *yytext;


char *current_driver = NULL;
char *current_device_name = NULL;
CameraAbilities ca;
unsigned int speed_index = 0;

char *alloc_parse_string(char *s);

char *error_string = NULL;
     
static void reset_ca(void);


#ifdef __GNUC__
#define __unused__ __attribute__((unused))
#else
#define __unused__
#endif


static int
compare_camera_abilities(const CameraAbilities *a, 
			 const CameraAbilities *b);


%}

%defines

/* This is supposed to track linenumbers and stuff, but I don't know how to
 * make it work. */
%locations

/* Pure yylex.  */
//%pure-parser

/* More than "syntax error" */
%error-verbose

/* parameters to yyparse() and yyerror() */
%parse-param { char const *file_name };
%parse-param { CameraAbilitiesList *al };

/* parameters to yylex() */
/* %lex-param { CameraAbilitiesList *al }; */

%initial-action
{
  lexer_reset(file_name);
  yyloc.begin.filename = yyloc.end.filename = file_name;
  yyloc.begin.column = yyloc.end.column = -1;
  yyloc.begin.lineno = yyloc.end.lineno = -1;
  @$.begin.filename = @$.end.filename = file_name;
  error_string = NULL;
};

%token TOK_SEP TOK_COMMA
%token TOK_WHITESPACE TOK_NEWLINE

%token <ui_val> TOK_NUMBER
%token <str_val> TOK_STRING

%token TOK_INTERFACE
%token TOK_DRIVER
%token TOK_DEVICE
%token TOK_BEGIN
%token TOK_END

%token TOK_SERIAL
%token TOK_SPEEDS

%token TOK_USB
%token TOK_PRODUCT
%token TOK_VENDOR
%token TOK_CLASS
%token TOK_SUBCLASS
%token TOK_PROTOCOL

%token TOK_DISK
%token TOK_PTPIP

%token TOK_DEVICE_TYPE
%token TOK_DRIVER_STATUS
%token TOK_OPERATIONS
%token TOK_FILE_OPERATIONS
%token TOK_FOLDER_OPERATIONS

%token TOK_DRIVER_OPTIONS
%token TOK_OPTION

%token <str_val> VAL_FLAG

 /* %type <ui_val> interface_usb_vendor */
 /* %type <ui_val> interface_usb_product */

%start devices

%%

devices:
	/* empty */
	| devices device
	;

device:
	TOK_DEVICE TOK_STRING TOK_BEGIN
	{
	  current_device_name = alloc_parse_string($<str_val>2);
	  /* printf("BEGIN_DEVICE <<%s>>\n", current_device_name); */
	  reset_ca();
	  strncpy(ca.model, current_device_name, sizeof(ca.model));
	  ca.model[sizeof(ca.model)-1]='\0';
	}
	device_type
	driver_definition
	driver_status
	operations
	file_operations
	folder_operations
	interfaces
	driver_options
	TOK_END
	{
	  //printf("END_DEVICE <<%s>>\n", current_device_name);
	  free(current_device_name);  current_device_name = NULL;
	  if (current_driver != NULL) {
	    free(current_driver);  current_driver = NULL;
	  }
	  gp_abilities_list_append(al, ca);
	}
	TOK_SEP
	;

driver_options:
	/* empty */
	| TOK_DRIVER_OPTIONS TOK_BEGIN 
	driver_option_list
	TOK_END TOK_SEP
	;

driver_option_list:
	/* empty */
	| driver_option_list driver_option_item
	;

driver_option_item:
	TOK_OPTION TOK_STRING TOK_SEP
	| TOK_OPTION TOK_STRING TOK_STRING TOK_SEP
	;

device_type:
	/* empty */
	| TOK_DEVICE_TYPE VAL_FLAG TOK_SEP
	{
	  if (gpi_string_to_enum($<str_val>2, &ca.device_type,
				 gpi_gphoto_device_type_map)) {
	    error_string = $<str_val>2;
	    yyerror(file_name, al,
		    "illegal device_type value");
	  }
	}
	;

driver_definition:
        TOK_DRIVER TOK_STRING TOK_SEP
	{
	  const char *camlib_env = getenv(CAMLIBDIR_ENV);
	  const char *camlibs = (camlib_env != NULL)?camlib_env:CAMLIBS;
	  current_driver = alloc_parse_string($<str_val>2);
	  strncpy(ca.library, camlibs, sizeof(ca.library));
	  strncat(ca.library, "/", sizeof(ca.library)-strlen(ca.library)-1);
	  strncat(ca.library, current_driver, sizeof(ca.library)-strlen(ca.library)-1);
	  ca.library[sizeof(ca.library)-1] = '\0';
	  //printf("   DRIVER <<%s>>\n", current_driver);
	}
	;

driver_status:
	/* empty */
        | TOK_DRIVER_STATUS VAL_FLAG TOK_SEP
	{
	  if (gpi_string_to_enum($<str_val>2, &ca.status,
				 gpi_camera_driver_status_map)) {
	    error_string = $<str_val>2;
	    yyerror(file_name, al,
		    "illegal driver_status value");
	  }
	}
	;

folder_operations:
	/* empty */
	| TOK_FOLDER_OPERATIONS folder_operations_list TOK_SEP
	;

file_operations:
	/* empty */
	| TOK_FILE_OPERATIONS file_operations_list TOK_SEP
	;

operations:
	/* empty */
	| TOK_OPERATIONS operations_list TOK_SEP
	;

folder_operations_list:
        folder_operations_list TOK_COMMA folder_operation
	| folder_operation
	;

file_operations_list:
        file_operations_list TOK_COMMA file_operation
	| file_operation
	;

operations_list:
        operations_list TOK_COMMA operation
	| operation
	;

folder_operation:
	VAL_FLAG
	{
	  if (gpi_string_or_to_flags($<str_val>1, &ca.folder_operations,
				     gpi_folder_operation_map)) {
	    error_string = $<str_val>1;
	    yyerror(file_name, al, 
		    "illegal folder_operation flag");
	  }
	}
	;

file_operation:
	VAL_FLAG
	{
	  if (gpi_string_or_to_flags($<str_val>1, &ca.file_operations,
				     gpi_file_operation_map)) {
	    error_string = $<str_val>1;
	    yyerror(file_name, al, 
		    "illegal file_operation flag");
	  }
	}
	;

operation:
	VAL_FLAG
	{
	  if (gpi_string_or_to_flags($<str_val>1,
				     &ca.operations,
				     gpi_camera_operation_map)) {
	    error_string = $<str_val>1;
	    yyerror(file_name, al,
		    "illegal operation flag");
	  }
	}
	;

interfaces:
	/* empty */
	| interfaces interface TOK_SEP
	;

interface:
	interface_serial
	| interface_usb
	| TOK_INTERFACE TOK_DISK {
	  ca.port |= GP_PORT_DISK;
	}
	| TOK_INTERFACE TOK_PTPIP {
	  ca.port |= GP_PORT_PTPIP;
	}
	;

interface_serial:
	TOK_INTERFACE TOK_SERIAL
	{
	  ca.port |= GP_PORT_SERIAL;
	  /* Ideally: Prepare serial entry. */
	}
	TOK_BEGIN
	interface_serial_internal
	TOK_END
        {
	  /* Ideally: Add serial entry to database. */
        }
	;

interface_serial_internal:
	/* empty */
	| TOK_SPEEDS speed_number_list TOK_SEP
	{
	  if (speed_index < (sizeof(ca.speed)/sizeof(ca.speed[0]))) {
	    ca.speed[speed_index++] = 0;
	  } else {
	    ca.speed[(sizeof(ca.speed)/sizeof(ca.speed[0]))-1] = 0;
	  }
	}
	;

speed_number_list:
	speed_number_list TOK_COMMA speed_list_number
	| speed_list_number
	;

speed_list_number:
	TOK_NUMBER
	{
	  if (speed_index < (sizeof(ca.speed)/sizeof(ca.speed[0]))) {
	    ca.speed[speed_index++] = $<ui_val>1;
	  }
	}
	;

/* FIXME: Make sure that the CameraAbilities usb_* fields are not
 *  overwritten by multiple definitions 
 */

interface_usb:
	TOK_INTERFACE TOK_USB
	TOK_BEGIN 
	{
	  ca.port |= GP_PORT_USB;
	  /* Ideally: Prepare USB entry for this camera */
	}
	interface_usb_internal
	TOK_END
	{
	  /* Ideally: Commit prepared USB entry to database */
	}
	;

interface_usb_internal:
	interface_usb_vendprod_optional
	interface_usb_class_optional
	;

interface_usb_vendprod_optional:
	/* empty */
	| interface_usb_vendor
	  interface_usb_product
	;

interface_usb_vendor:
	TOK_VENDOR TOK_NUMBER TOK_SEP
	{
	  ca.usb_vendor = $<ui_val>2;
	}
	;

interface_usb_product:
	TOK_PRODUCT TOK_NUMBER TOK_SEP
	{
	  ca.usb_product = $<ui_val>2;
	}
	;

interface_usb_class_optional:
	/* empty */
	| TOK_CLASS TOK_NUMBER TOK_SEP
	{
	  ca.usb_class = $<ui_val>2;
	}
	interface_usb_subclass_optional
	;

interface_usb_subclass_optional:
	/* empty */
	| TOK_SUBCLASS TOK_NUMBER TOK_SEP
	{
	  ca.usb_subclass = $<ui_val>2;
	}
	interface_usb_protocol_optional
	;

interface_usb_protocol_optional:
	/* empty */
	| TOK_PROTOCOL TOK_NUMBER TOK_SEP
	{
	  ca.usb_protocol = $<ui_val>2;
	}
	;

%%


char *alloc_parse_string(char *s)
  {
    if (s[0] == '"') {
      int len = strlen(s);
      if (s[len-1] == '"') {
	char *ret = malloc(len-2+1);
	strncpy(ret, &s[1], len-2);
	ret[len-2] = '\0';
	free(s);
	return ret;
      } else {
	fprintf(stderr, "Error: string does not end with quote: <<%s>>\n", s);
	exit(2);
      }
    } else {
      fprintf(stderr, "Error: string does not start with quote: <<%s>>\n", s);
      exit(2);
    }
  }


void yyerror(const char *filename, 
	     CameraAbilitiesList __unused__ *al, 
	     const char *str)
{
  fprintf(stderr, "%s:%d: parse error: %s\n", 
	  filename, yylineno, str);
  fprintf(stderr, "%s:%d- near: `%s'\n", 
	  filename, yylineno, yytext);
  if (NULL != error_string) {
    fprintf(stderr, "%s:%d-              <<%s>>\n",
	    filename, yylineno, error_string);
    error_string = NULL;
  }
  exit(4);
}


static void 
reset_ca(void)
{
  memset(&ca, 0, sizeof(ca));
  speed_index = 0;
  /* FIXME: These defaults are of dubious quality. */
  ca.usb_subclass = -1;
  ca.usb_protocol = -1;
}
