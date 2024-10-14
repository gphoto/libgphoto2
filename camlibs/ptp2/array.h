/* array.h
 *
 * Copyright (C) 2024 Axel Waggershauser <awagger@web.de>
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
 */

#ifndef CAMLIBS_PTP2_ARRAY_H
#define CAMLIBS_PTP2_ARRAY_H

#define ARRAYSIZE(ARRAY) (sizeof(ARRAY) / sizeof(ARRAY[0]))

#define move(dst, src) do { \
	dst = src; \
	memset(&(src), 0, sizeof(src)); \
} while(0)

/* The follow set of macros implements a generic array or list of TYPE.
 * This is basically a TYPE* pointer and a length integer. This structure
 * together with the typical use-cases repeats regularly throughout the
 * codebase. It raises the level of abstraction and improves code
 * readabilty. axxel is a c++ developer and misses his STL ;)
 *
 * A typical life-cycle lookes like this:
 *
 * typedef ARRAY_OF(PTPObject) PTPObjects;
 * PTPObjects objects;
 * array_push_back(&objects, some_value);
 * PTPObject *o;
 * array_push_back_empty(&objects, &o);
 * o->some_prop = 1;
 * for_each(PTPObject*, pobj, objects)
 *     pobj->call_some_func();
 * free_array_recursive(&objects, ptp_free_object);
 */

#define ARRAY_OF(TYPE) struct ArrayOf##TYPE \
{ \
	TYPE *val; \
	uint32_t len; \
}

/* TODO: with support for C23, we can improve the for_each macro by dropping the TYPE argument
 *     #define for_each(PTR, ARRAY) for (typeof(ARRAY.val) PTR = ARRAY.val; PTR != ARRAY.val + ARRAY.len; ++PTR)
 */
#define for_each(TYPE, PTR, ARRAY) \
	for (TYPE PTR = ARRAY.val; PTR != ARRAY.val + ARRAY.len; ++PTR)

#define free_array(ARRAY) do { \
	free ((ARRAY)->val); \
	(ARRAY)->val = 0; \
	(ARRAY)->len = 0; \
} while (0)

#define free_array_recusive(ARRAY, DESTRUCTOR) do { \
	for (uint32_t _i = 0; _i < (ARRAY)->len; ++_i) \
		DESTRUCTOR (&(ARRAY)->val[_i]); \
	free_array (ARRAY); \
} while (0)

#define array_extend_capacity(ARRAY, LEN) do { \
	(ARRAY)->val = realloc((ARRAY)->val, ((ARRAY)->len + (LEN)) * sizeof((ARRAY)->val[0])); \
	if (!(ARRAY)->val) { \
		GP_LOG_E ("Out of memory: 'realloc' of %ld bytes failed.", ((ARRAY)->len + (LEN)) * sizeof((ARRAY)->val[0])); \
		return GP_ERROR_NO_MEMORY; \
	} \
	memset((ARRAY)->val + (ARRAY)->len, 0, LEN * sizeof((ARRAY)->val[0])); \
} while(0)

#define array_push_back_empty(ARRAY, PITER) do { \
	array_extend_capacity(ARRAY, 1); \
	*PITER = &(ARRAY)->val[(ARRAY)->len++]; \
} while(0)

#define array_push_back(ARRAY, VAL) do { \
	array_extend_capacity(ARRAY, 1); \
	move((ARRAY)->val[(ARRAY)->len++], VAL); \
} while(0)

#define array_append_copy(DST, SRC) do { \
	array_extend_capacity(DST, (SRC)->len); \
	memcpy((DST)->val + (DST)->len, (SRC)->val, (SRC)->len * sizeof((SRC)->val[0])); \
	(DST)->len += (SRC)->len; \
} while(0)

#define array_append(DST, SRC) do { \
	if ((DST)->len) { \
		array_append_copy(DST, SRC); \
		free_array (SRC); \
	} else { \
		free_array (DST); \
		*(DST) = *(SRC); \
	} \
} while(0)

#define array_remove(ARRAY, ITER) do { \
	if (ITER < (ARRAY)->val || ITER >= (ARRAY)->val + (ARRAY)->len) \
		break; \
	memmove (ITER, ITER + 1, ((ARRAY)->len - (ITER + 1 - (ARRAY)->val)) * sizeof((ARRAY)->val[0])); \
	(ARRAY)->len--; \
} while(0)

#define array_pop_front(ARRAY, VAL) do { \
	*VAL = (ARRAY)->val[0]; \
	array_remove(ARRAY, (ARRAY)->val); \
} while(0)

#endif
