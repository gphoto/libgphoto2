/*
	Defines some PrintMacros. They are need to disable much
	outputs they were used during development.
*/
#ifndef DEFINE_PRINT_H
#define DEFINE_PRINT_H
#include <stdio.h>

/* Message or Errors from the gphoto API */
#define printAPINote			if (1) printf 
#define printAPIError		if (1) printf

/* only for debugging : The FunktionCall */
#define printFnkCall			if (0) printf

/* Message or Errors from the Core Layer */
#define printCoreNote 		if (1) printf
#define printCoreError		if (1) printf

/* CError are Error Messages below */
/* the Core of this driver */
#define printCError			if (1) printf


#endif

