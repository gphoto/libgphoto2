/** \file camlibs/ptp2/n-param-test.c
 * \brief Test the __VA_ARGS__ and va_args related hacks for ptp_init_container()
 *
 * The ptp_init_container() function and the PTPContainer type are a
 * bit ugly (like assuming consecutive struct members are laid out
 * like an array), so this makes sure that at least the macros calling
 * ptp_init_container() actually do what they are intended to do when
 * counting the arguments and passing them on.
 *
 * \author Copyright (C) 2021 Hans Ulrich Niedermann <hun@n-dimensional.de>
 *
 * \copyright GNU Lesser General Public License 2 or later
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301  USA
 *
 * Usage:
 *   $ clang -std=c99 -pedantic -Wall -Wextra -Weverything -o n-param-test-clang n-param-test.c && ./n-param-test-clang
 *   $ gcc -std=c99 -pedantic -Wall -Wextra -o n-param-test-gcc n-param-test.c && ./n-param-test-gcc
 *
 */


#include <assert.h>
#include <stdarg.h>
#include <stdio.h>


// #define TRY_CASES_OUTSIDE_VALID_RANGE


#define N_PARAM_SEQ(DUMMY,				\
		    Pa, Pb, Pc, Pd, Pe,			\
		    P0, P1, P2, P3, P4, P5,		\
		    N, ...)				\
  N


#define N_PARAM(...)				\
  N_PARAM_SEQ(__VA_ARGS__,			\
	      "a", "b", "c", "d", "e",		\
	      5, 4, 3, 2, 1, 0, NULL)


#define FOO_INIT(PTR, ...) \
  foo_func(PTR, N_PARAM(-666, __VA_ARGS__), __VA_ARGS__)


static
void foo_func(void *ptr, int n_param, ...)
{
  va_list args;

  (void) ptr;

  printf("%s: n_param=%d\n", "foo_func", n_param);
  assert(n_param <= 5);

  va_start(args, n_param);
  int code = va_arg(args, int);
  printf("  code   = %d\n", code);

  for (int i=0; i<n_param; ++i) {
    int value = va_arg(args, int);
    printf("  Param%d = param[%d] = %d\n", i+1, i, value);
  }
  va_end(args);
}


int main(void)
{
  /* Zero to five parameters after the code are the valid use
   * cases. The CODE in these examples has the value (1000+n_param).
   */

  /*       PTR   CODE  P1   P2   P3   P4   P5                       */
  FOO_INIT(NULL, 1000);
  FOO_INIT(NULL, 1001, 201);
  FOO_INIT(NULL, 1002, 201, 202);
  FOO_INIT(NULL, 1003, 201, 202, 203);
  FOO_INIT(NULL, 1004, 201, 202, 203, 204);
  FOO_INIT(NULL, 1005, 201, 202, 203, 204, 205);

  /* from here on, we want compile time errors or at least runtime
     errors */
#ifdef TRY_CASES_OUTSIDE_VALID_RANGE
  FOO_INIT(NULL, 1006, 201, 202, 203, 204, 205, 206);
  FOO_INIT(NULL, 1007, 201, 202, 203, 204, 205, 206, 207);
  FOO_INIT(NULL, 1008, 201, 202, 203, 204, 205, 206, 207, 208);
  FOO_INIT(NULL, 1009, 201, 202, 203, 204, 205, 206, 207, 208, 209);
  FOO_INIT(NULL, 1010, 201, 202, 203, 204, 205, 206, 207, 208, 209, 210);

  FOO_INIT(NULL, 1011, 201, 202, 203, 204, 205, 206, 207, 208, 209, 210, 211);
  FOO_INIT(NULL, 1012, 201, 202, 203, 204, 205, 206, 207, 208, 209, 210, 211, 212);
  FOO_INIT(NULL, 1013, 201, 202, 203, 204, 205, 206, 207, 208, 209, 210, 211, 212, 213);
  FOO_INIT(NULL, 1014, 201, 202, 203, 204, 205, 206, 207, 208, 209, 210, 211, 212, 213, 214);
  FOO_INIT(NULL, 1015, 201, 202, 203, 204, 205, 206, 207, 208, 209, 210, 211, 212, 213, 214, 215);
  FOO_INIT(NULL, 1016, 201, 202, 203, 204, 205, 206, 207, 208, 209, 210, 211, 212, 213, 214, 215, 216);
  FOO_INIT(NULL, 1017, 201, 202, 203, 204, 205, 206, 207, 208, 209, 210, 211, 212, 213, 214, 215, 216, 217);
#endif

  return 0;
}
