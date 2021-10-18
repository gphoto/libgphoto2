#ifndef LIBGPHOTO2_I18N_H
#define LIBGPHOTO2_I18N_H

#include "config.h"

#ifdef ENABLE_NLS
#  include <libintl.h>
#  undef _
#  define _(String) dgettext (GETTEXT_PACKAGE_LIBGPHOTO2, String)
#  ifdef gettext_noop
#      define N_(String) gettext_noop (String)
#  else
#      define N_(String) (String)
#  endif
#else
#  define N_(String) (String)
#  define _(String) (String)
#  define bind_textdomain_codeset(Domain, codeset) (codeset)
#  define bindtextdomain(Domain, Directory) (Domain)
#  define dcgettext(Domain, Message, Type) (Message)
#  define dgettext(Domain, Message) (Message)
#  define gettext(String) (String)
#  define ngettext(String1, String2, Count) ((Count==1)?String1:String2)
#  define textdomain(String) (String)
#endif

#endif /* !defined(LIBGPHOTO2_I18N_H) */
