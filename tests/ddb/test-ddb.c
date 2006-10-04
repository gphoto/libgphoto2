/** \file test-ddb.c
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gphoto2/gphoto2-abilities-list.h>

#include "ddb-common.h"
#include "ddb-txt.tab.h"

static int
compare_camera_abilities(const CameraAbilities *a, 
			 const CameraAbilities *b);

CameraAbilitiesList *
read_abilities_list(const char *filename);
CameraAbilitiesList *
read_abilities_list(const char *filename)
{
  CameraAbilitiesList *al = NULL;
  int retval;
  if (0!=(retval = gp_abilities_list_new(&al))) { return NULL; }
  if (0!=(retval = yyparse(filename, al))) { goto error; }
  return al;
 error:
  if (al!=NULL)
    gp_abilities_list_free(al);
  return NULL;
}


static int
compare_lists (void)
{
  unsigned int errors = 0;
  CameraAbilitiesList *ddb = read_abilities_list(NULL);
  CameraAbilitiesList *clb;
  unsigned int i, j, n_ddb, n_clb;
  gp_abilities_list_new(&clb);
  gp_abilities_list_load(clb, NULL);
  n_ddb = gp_abilities_list_count(ddb);
  n_clb = gp_abilities_list_count(clb);
  fprintf(stderr, "## Comparing ddb with clb ##\n");
  for (i=0, j=0; (i<n_ddb) && (j<n_clb); ) {
    CameraAbilities a, b;
    gp_abilities_list_get_abilities(ddb, i, &a);
    gp_abilities_list_get_abilities(clb, j, &b);
    if ((b.port & GP_PORT_USB) && (b.usb_class == 666)) {
      /* ignore this camera */
      j++;
      continue;
    }
    if (compare_camera_abilities(&a, &b)) {
      errors++;
    }
    i++, j++;
  }
  gp_abilities_list_free(clb);
  gp_abilities_list_free(ddb);
  return (errors == 0)? 0 : 1;
}


int main(int argc, char *argv[])
{
  int i;
  int retval;
  CameraAbilitiesList *al;
#if YYDEBUG
  yydebug = 1;
#endif
  if (0!=(retval=gp_abilities_list_new(&al))) {
    fprintf(stderr, "Could not create CameraAbilitiesList\n");
    return 6;
  }
  if (argc > 1) {
    for (i=1; i<argc; i++) {
      if (0==strcmp("--compare", argv[i])) {
	return compare_lists();
      } else {
	const char *filename = argv[i];
	yyin = fopen(filename, "r");
	if (NULL == yyin) {
	  fprintf(stderr, "File %s does not exist. Aborting.\n", filename);
	  return 5;
	}
	retval = yyparse(filename, al);
	fclose(yyin);
	if (retval != 0) {
	  return retval;
	}
      }
    }
    return 0;
  } else {
    yyin = stdin;
    return yyparse("<stdin>", al);
  }
}




#define CMP_RET_S(cmp_func, field)					\
  do {									\
    int retval = cmp_func(a->field, b->field);				\
    if (retval != 0) {							\
      fprintf(stderr, "    difference in .%s: <<%s>> vs <<%s>>\n",		\
	      #field, a->field, b->field);				\
      errors++;								\
    }									\
  } while (0)

#define CMP_RET_UI(cmp_func, field)					\
  do {									\
    int retval = cmp_func((unsigned int) (a->field),			\
			  (unsigned int) (b->field));			\
    if (retval != 0) {							\
      fprintf(stderr, "    difference in .%s: 0x%x vs 0x%x\n",		\
	      #field, (unsigned int) (a->field),			\
	      (unsigned int) (b->field));				\
      errors++;								\
    }									\
  } while (0)


static int
uicmp(unsigned int a, unsigned int b)
{
  if (a < b) return -1;
  else if (a > b) return 1;
  else return 0;
}

static int
strcmp_colon(const char *a, const char *b)
{
  unsigned int i = 0;
  while (1) {
    if ((a[i] == '\0') && (b[i] == '\0'))
      return 0;
    if (a[i] == '\0')
      return -1;
    if (b[i] == '\0')
      return 1;
    if (a[i] == b[i])
      goto next;
    if (((a[i] == ' ') && (b[i] == ':')) ||
	((a[i] == ':') && (a[i] == ' ')))
      goto next;
    if (a[i] < b[i])
      return -1;
    if (a[i] > b[i])
      return 1;
    fprintf(stderr, "Internal error in strcmp_colon\n");
    exit(13);
  next:
    ++i;
  }
}


static int
compare_camera_abilities(const CameraAbilities *a, 
			 const CameraAbilities *b)
{
  unsigned int errors = 0;
  int i;
  CMP_RET_S(strcmp_colon, model);
  CMP_RET_S(strcmp, library);
  /* CMP_RET_S(strcmp, id); */
  CMP_RET_UI(uicmp, port);
  if ((a->port & GP_PORT_SERIAL)) {
    for (i=0; (a->speed[i] != 0) && (a->speed[i] != 0); i++) {
      CMP_RET_UI(uicmp, speed[i]);
    }
  }
  CMP_RET_UI(uicmp, operations);
  CMP_RET_UI(uicmp, file_operations);
  CMP_RET_UI(uicmp, folder_operations);
  if ((a->port & GP_PORT_USB)) {
    CMP_RET_UI(uicmp, usb_vendor);
    CMP_RET_UI(uicmp, usb_product);
    CMP_RET_UI(uicmp, usb_class);
    if (a->usb_class != 0) {
      CMP_RET_UI(uicmp, usb_subclass);
      CMP_RET_UI(uicmp, usb_protocol);
    }
  }
  CMP_RET_UI(uicmp, device_type);
  if (errors == 0) {
    return 0;
  } else {
    fprintf(stderr, "Difference for <<%s>>\n", a->model);
    return 1;
  }
}
