dnl AC_NEED_STDINT_H ( HEADER-TO-GENERATE )
dnl $Id$
dnl Copyright 2001 by Dan Fandrich <dan@conharvesters.com>
dnl This file may be copied and used freely without restrictions.  No warranty
dnl is expressed or implied.
dnl 
dnl Look for a header file that defines size-specific integer types like the
dnl ones recommended to be in stdint.h in the C99 standard.
dnl This is a dumbed-down version of the macro of the same name in the file
dnl ac_need_stdint_h.m4 which is part of the ac-archive, available at
dnl <URL:http://ac-archive.sourceforge.net/> (also, another macro
dnl AC_CREATE_STDINT_H by the same author).  This version is not as smart,
dnl but works on older autoconf versions and has a different license.

dnl AC_CHECK_DEFINED_TYPE ( TYPE, FILE, ACTION-IF-FOUND, ACTION-IF-NOT-FOUND )
dnl This is similar to _AC_CHECK_TYPE_NEW (a.k.a. new syntax version of
dnl AC_CHECK_TYPE) in autoconf 2.50 but works on older versions
AC_DEFUN(AC_CHECK_DEFINED_TYPE,
[AC_MSG_CHECKING([for $1 in $2])
AC_EGREP_CPP(changequote(<<,>>)dnl
<<(^|[^a-zA-Z_0-9])$1[^a-zA-Z_0-9]>>dnl
changequote([,]), [#include <$2>],
ac_cv_type_$1=yes, ac_cv_type_$1=no)dnl
AC_MSG_RESULT($ac_cv_type_$1)
if test $ac_cv_type_$1 = yes; then
  $3
else
  $4
fi
])

dnl Look for a header file that defines size-specific integer types
AC_DEFUN(AC_NEED_STDINT_H,
[AC_CHECK_DEFINED_TYPE(uint8_t,
stdint.h,
[cat > $1 <<EOF
/* This file is generated automatically by configure */
#ifndef __STDINT_H
#define __STDINT_H
#include <stdint.h>
#endif /*!__STDINT_H*/
EOF],
[AC_CHECK_DEFINED_TYPE(uint8_t,
inttypes.h,
[cat > $1 <<EOF
/* This file is generated automatically by configure */
#ifndef __STDINT_H
#define __STDINT_H
#include <inttypes.h>
#endif /*!__STDINT_H*/
EOF],
[AC_CHECK_DEFINED_TYPE(uint8_t,
sys/types.h,
[cat > $1 <<EOF
/* This file is generated automatically by configure */
#ifndef __STDINT_H
#define __STDINT_H
#include <sys/types.h>
#endif /*!__STDINT_H*/
EOF],
[AC_CHECK_DEFINED_TYPE(u_int8_t,
sys/types.h,
[cat > $1 <<EOF
/* This file is generated automatically by configure */
#ifndef __STDINT_H
#define __STDINT_H
#include <sys/types.h>
typedef u_int8_t uint8_t;
typedef u_int16_t uint16_t;
typedef u_int32_t uint32_t;
#endif /*!__STDINT_H*/
EOF],
AC_MSG_WARN([I can't find size-specific integer definitions on this system])
)])])])dnl
])
