/** \file libgphoto2_port/compiletime-assert.h
 * \brief Allow compile time assertions with or without C11.
 *
 * C11 has added static_assert(EXPR, MSG) to assert.h, but C99 only
 * knows runtime assertions using assert(EXPR). This defines
 * COMPILETIME_ASSERT(EXPR) which works with both C99 and C11 and
 * later.
 *
 * \author Copyright (C) 2010-2021 Hans Ulrich Niedermann <hun@n-dimensional.de>
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
 */

#ifndef LIBGPHOTO2_PORT_COMPILETIME_ASSERT_H
#define LIBGPHOTO2_PORT_COMPILETIME_ASSERT_H


#if (defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L))
/* C11 or later */

#include <assert.h>

/* needed for older Macs https://github.com/gphoto/libgphoto2/issues/1145 */
#ifndef static_assert
#define static_assert _Static_assert
#endif

/** Compiletime assertion for use inside a function. */
#define COMPILETIME_ASSERT(CONDITION) \
	static_assert((CONDITION), #CONDITION)

/** Compiletime assertion for use outside a function. */
#define BARE_COMPILETIME_ASSERT(CONDITION) \
	static_assert((CONDITION), #CONDITION)

#else
/* before C11 */

/** Compiletime assertion for use inside a function. */
#define COMPILETIME_ASSERT(CONDITION)				      \
	switch (0) {      /* error here means assertion has failed */ \
	case 0:           /* error here means assertion has failed */ \
	case (CONDITION): /* error here means assertion has failed */ \
		break;						      \
	}


/** \internal used by BARE_COMPILETIME_ASSERT() macro */
#define MAKE_BARE_COMPILETIME_ASSERT_NAME				\
	MAKE_BARE_COMPILETIME_ASSERT_NAME1(COMPILETIME_ASSERT_fails_in_line, \
					   __LINE__)


/** \internal used by BARE_COMPILETIME_ASSERT() macro */
#define MAKE_BARE_COMPILETIME_ASSERT_NAME1(BASE, PARAM) \
	MAKE_BARE_COMPILETIME_ASSERT_NAME2(BASE, PARAM)


/** \internal used by BARE_COMPILETIME_ASSERT() macro */
#define MAKE_BARE_COMPILETIME_ASSERT_NAME2(BASE, PARAM) \
	BASE ## _ ## PARAM


/** Compiletime assertion for use outside a function. */
#define BARE_COMPILETIME_ASSERT(CONDITION)		\
	void MAKE_BARE_COMPILETIME_ASSERT_NAME(void);	\
	void MAKE_BARE_COMPILETIME_ASSERT_NAME(void)	\
	{						\
		COMPILETIME_ASSERT(CONDITION);		\
	}

#endif /* after/before C11 */


#endif /* !defined(LIBGPHOTO2_PORT_COMPILETIME_ASSERT_H) */

/*
 * Local Variables:
 * c-file-style:"linux"
 * indent-tabs-mode:t
 * End:
 */
