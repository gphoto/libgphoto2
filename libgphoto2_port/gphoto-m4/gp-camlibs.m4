# gp-camlibs.m4 - define camlibs and camlib infrastructure     -*- Autoconf -*-
# serial 14
dnl | Increment the above serial number every time you edit this file.
dnl | When it finds multiple m4 files with the same name,
dnl | aclocal will use the one with the highest serial.
dnl
dnl ####################################################################
dnl
dnl GP_CAMLIB & Co.
dnl
dnl ####################################################################
dnl
dnl Redundancy free definition of libgphoto2 camlibs.
dnl
dnl Removes the redundancy from the camlib lists, and executes
dnl additional consistency checks on the build system.
dnl
dnl Every camlib belongs to one of the following (disjunct) sets of
dnl camlibs:
dnl
dnl   * the 'standard' set
dnl
dnl     The set of camlibs which is most likely to support the most
dnl     cameras. This is the default when --with-camlibs=... is not
dnl     given. This used to be called 'all', even though it
dnl     definitively does not comprise all camlibs.
dnl
dnl   * the 'outdated' set
dnl
dnl     These cameras are outdated in some way. The cameras may have
dnl     fallen out of use, the camlib might have been superseded by
dnl     another one, or other reasons. Not built by default.
dnl
dnl   * the 'unlisted' set
dnl
dnl     These camlibs will not be listed by './configure --help', but
dnl     they can still be added to the set of camlibs to be built.
dnl
dnl Example usage:
dnl
dnl   GP_CAMLIB([canon])
dnl   GP_CAMLIB([casio])
dnl   [...]
dnl   AM_COND_IF([HAVE_LIBCURL], [dnl
dnl   AM_COND_IF([HAVE_LIBXML2], [dnl
dnl     GP_CAMLIB([lumix])dnl
dnl   ])
dnl   ])
dnl   [...]
dnl   GP_CAMLIB([ptp],[unlisted])
dnl   GP_CAMLIB([ptp2])
dnl   [...]
dnl   GP_CAMLIB([toshiba])
dnl   GP_CAMLIBS_DEFINE()
dnl
dnl
dnl ####################################################################
dnl Implementation remarks
dnl ####################################################################
dnl
dnl Note that at this time, the set names are hard coded, but could be
dnl moved to a m4 time automatic set of set names at a later time.
dnl
dnl A few notes on macro and variable naming:
dnl
dnl   * GP_* are the macros to be called from configure.ac
dnl   * GP_CAMLIBS_* are also shell variables exported to configure.ac
dnl     shell code and Makefile.am files
dnl   * gp_m4_* are the macros used inside those macros as m4 variables
dnl   * gp_sh_* are the shell variables used inside the GP_ shell code
dnl   * The old, inconsistent names of the as untouched variables
dnl     exported to configure.ac and Makefile.am are still inconsistent.
dnl
dnl
dnl ####################################################################
dnl Forbid everything first, allow specific variable names later
dnl ####################################################################
m4_pattern_forbid([GP_CAMLIBS_])dnl
m4_pattern_forbid([GP_CAMLIB_])dnl
dnl
dnl
dnl ####################################################################
dnl _GP_CAMLIBS_INIT
dnl   Called internally when required.
dnl ####################################################################
AC_DEFUN_ONCE([_GP_CAMLIBS_INIT],[dnl
dnl # BEGIN $0($@)
AC_BEFORE([$0],[GP_CAMLIB])dnl
AC_REQUIRE([AC_PROG_GREP])dnl
AC_REQUIRE([AC_PROG_SED])dnl
AC_REQUIRE([GP_PROG_CMP])dnl
AC_REQUIRE([GP_PROG_SORT])dnl
AC_REQUIRE([GP_PROG_TR])dnl
AC_REQUIRE([GP_PROG_UNIQ])dnl
m4_foreach_w([var], [everything standard unlisted outdated], [dnl
  m4_set_empty([gp_m4s_camlib_set_]var, [], [dnl
    m4_errprintn([Error: non-empty set gp_m4s_camlib_set_]var[ already defined])dnl
    m4_exit(1)dnl
  ])dnl
])dnl
GP_SET_DEFINE([camlib-set-standard])dnl
GP_SET_DEFINE([camlib-set-unlisted])dnl
GP_SET_DEFINE([camlib-set-outdated])dnl
dnl # END $0($@)
])dnl
dnl
dnl
dnl ####################################################################
dnl GP_CAMLIB([mycamlib])
dnl GP_CAMLIB([mycamlib], [outdated])
dnl   Add the camlib 'mycamlib' to the "standard" set of camlibs or
dnl   the "outdated" set of camlibs, respectively.
dnl ####################################################################
m4_pattern_forbid([GP_CAMLIB])dnl
AC_DEFUN([GP_CAMLIB],[dnl
dnl # BEGIN $0($@)
AC_REQUIRE([_GP_CAMLIBS_INIT])dnl
AC_BEFORE([$0],[GP_CAMLIBS_DEFINE])dnl
m4_case([$#],[2],[dnl
  m4_case([$2], [unlisted], [dnl
    GP_SET_ADD([camlib-set-unlisted], [$1])
    m4_set_add([gp_m4s_camlib_set_unlisted], [$1])dnl
  ], [outdated], [dnl
    GP_SET_ADD([camlib-set-outdated], [$1])
    m4_set_add([gp_m4s_camlib_set_outdated], [$1])dnl
  ], [dnl
    m4_errprintn(__file__:__line__:[ Error: Wrong second argument to GP_CAMLIB])dnl
    m4_exit(1)dnl
  ])dnl
], [1], [dnl
  GP_SET_ADD([camlib-set-standard], [$1])
  m4_set_add([gp_m4s_camlib_set_standard], [$1])dnl
], [dnl
  m4_errprintn(__file__:__line__:[ Error: Wrong number of arguments to GP_CAMLIB])dnl
  m4_exit(1)dnl
])dnl m4_case $#
m4_set_add([gp_m4s_camlib_set_everything], [$1], [], [dnl
m4_errprintn(__file__:__line__:[Duplicate declaration of camlib $1])dnl
m4_exit(1)dnl
])dnl
dnl # END $0($@)
])dnl AC_DEFUN GP_CAMLIB
dnl
dnl
dnl ####################################################################
dnl GP_CAMLIBS_WARNING
dnl   Print warning about building a non-"standard" set of camlibs.
dnl ####################################################################
AC_DEFUN([GP_CAMLIBS_WARNING],[dnl
AC_MSG_WARN([

  #=====================================================================#
  # Caution: You have chosen to build a non-standard set of camlibs.    #
  #          You may have disabled the camlib required for your camera, #
  #          or enabled a camlib which does *not* work and overrides    #
  #          the camlib which *does* work. Consequently,                #
  #          YOUR CAMERA MAY NOT WORK!                                  #
  #                                                                     #
  # Many cameras of several brands are supported by a camlib with a     #
  # name different from the name of the camera brand or model. If you   #
  # are unsure, please                                                  #
  #   * enable at least the 'ptp2' camlib                               #
  #   * or even better, just build the standard set of camlibs.         #
  #=====================================================================#
])
GP_SLEEP_IF_TTY([5])
])
dnl
dnl
dnl ####################################################################
dnl GP_CAMLIBS_CONDITIONAL_WARNING
dnl   Call GP_CAMLIBS_WARNING when required.
dnl ####################################################################
AC_DEFUN([GP_CAMLIBS_CONDITIONAL_WARNING], [dnl
AS_VAR_IF([gp_sh_with_camlibs], [standard], [dnl
], [dnl
GP_CAMLIBS_WARNING
])
])dnl
dnl
dnl
dnl ####################################################################
dnl GP_CAMLIBS_DEFINE
dnl   Determine the set of camlibs to build from the --with-camlibs
dnl   parameter, and set the build variables accordingly.
dnl ####################################################################
AC_DEFUN([GP_CAMLIBS_DEFINE],[dnl
# BEGIN $0($@)
AC_REQUIRE([_GP_CAMLIBS_INIT])dnl
AC_REQUIRE([GP_PROG_EXPR])dnl

dnl GP_SET_MSG([camlib-set-standard])
dnl GP_SET_MSG([camlib-set-outdated])
dnl GP_SET_MSG([camlib-set-unlisted])

dnl Convert sets defined at m4 time (i.e. autoreconf time)
dnl into the same format as the sh time (i.e. configure time)
dnl defined sets.
m4_foreach_w([var], [everything standard outdated unlisted], [dnl
GP_SET_DEFINE([m4-camlib-set-]var)
GP_SET_ADD_ALL([m4-camlib-set-]var[]m4_set_listc([gp_m4s_camlib_set_]var))
])dnl

dnl GP_SET_MSG([m4-camlib-set-standard])
dnl GP_SET_MSG([m4-camlib-set-unlisted])
dnl GP_SET_MSG([m4-camlib-set-outdated])

GP_SET_DEFINE([camlib-set-everything])dnl
GP_SET_UNION([camlib-set-everything], [camlib-set-standard], [camlib-set-outdated], [camlib-set-unlisted])

dnl GP_SET_MSG([camlib-set-everything])
dnl GP_SET_MSG([m4-camlib-set-everything])

dnl Yes, that help output won't be all that pretty, but we at least
dnl do not have to edit it by hand.
AC_ARG_WITH([camlibs],[AS_HELP_STRING(
	[--with-camlibs=<list>],
	[Compile camera drivers (camlibs) in <list>. ]dnl
	[Camlibs may be separated with commas. ]dnl
	[CAUTION: DRIVER NAMES AND CAMERA NAMES MAY DIFFER. ]dnl
	['standard' is the default and is a standard set of camlibs: ]dnl
	m4_set_contents(gp_m4s_camlib_set_standard, [ ]).
	['outdated' is a set of camlibs for very old cameras: ]dnl
	m4_set_contents(gp_m4s_camlib_set_outdated, [ ]).dnl
	['everything' is the set including every camlib. ]dnl
	[You can add or remove camlibs or camlib sets by appending ]dnl
	[them to the list with a + or - sign in front.])],
	[gp_sh_with_camlibs="${withval}"],
	[gp_sh_with_camlibs="standard"])dnl

dnl For backwards compatibility, accept --with-camlibs='all' and
dnl interpret it as --with-camlibs='standard'.
AS_VAR_IF([gp_sh_with_camlibs], [all], [dnl
  gp_sh_with_camlibs="standard"
])dnl

dnl Gentoo mode... if user just requested "canon",
dnl add "ptp2" to save support requests.
AS_VAR_IF([gp_sh_with_camlibs], [canon], [dnl
	gp_sh_with_camlibs="${gp_sh_with_camlibs} ptp2"
	AC_MSG_WARN([

    #==============================================================#
    # You have selected only the old 'canon' driver. However, most #
    # current Canon camera models require the 'ptp2' driver.       #
    #                                                              #
    # Autoselecting the 'ptp2' driver in addition to the 'canon'   #
    # driver to prevent unnecessary support requests.              #
    #==============================================================#
])
	GP_SLEEP_IF_TTY([5])
])dnl

dnl set -x

AC_MSG_CHECKING([with-camlibs requested])
AS_VAR_IF([gp_sh_with_camlibs], [standard], [dnl
	AC_MSG_RESULT([standard set])
], [dnl
	gp_sh_with_camlibs="$(AS_ECHO(["${gp_sh_with_camlibs}"]) | ${TR} ',' ' ')"
	AC_MSG_RESULT([${gp_sh_with_camlibs}])
])

dnl AC_MSG_CHECKING([for nothing])
dnl AC_MSG_RESULT([nihil])

dnl Iterate over the list of given camlibs.
dnl
dnl Replace 'standard', 'outdated', 'unlisted', and 'everything' with
dnl the respective set of camlibs, and make sure any camlibs specified
dnl explicitly are actually valid defined camlibs.
GP_SET_DEFINE([m4-camlib-set])
GP_SET_DEFINE([camlib-set])
for gp_camlib in ${gp_sh_with_camlibs}
do
    case "X$gp_camlib" in #(
    X-*)
	operator=remove
	gp_camlib="$(AS_ECHO(["Y${gp_camlib}"]) | ${SED} 's/^Y.//')"
	;; #(
    X+*)
	operator=add
	gp_camlib="$(AS_ECHO(["Y${gp_camlib}"]) | ${SED} 's/^Y.//')"
	;; #(
    X[[A-Za-z0-9]]*)
	operator=add
	;; #(
    *)
	AC_MSG_ERROR([Invalid name given for camlib set or camlib: '${gp_camlib}'])
	;;
    esac
    dnl AC_MSG_CHECKING([with-camlibs operator])
    dnl AC_MSG_RESULT([${operator}])
    dnl AC_MSG_CHECKING([with-camlibs camlib])
    dnl AC_MSG_RESULT([${gp_camlib}])

    dnl Convert deprecated "all" parameter to "standard".
    case "$gp_camlib" in #(
    all)
	AC_MSG_WARN([

    #=============================================================#
    # You have used 'all' in the argument to the configure script #
    # --with-camlibs= parameter.  'all' is a deprecated name for  #
    # the 'standard' camlib set.                                  #
    #                                                             #
    # Please change your call to the configure script to use      #
    # 'standard' instead.                                         #
    #=============================================================#
])
	GP_SLEEP_IF_TTY([5])
	gp_camlib="standard"
	;;
    esac

    dnl Now gp_camlib contains the camlib string, and operator 'add' or 'remove'.
    case "$operator" in #(
    add)
        case "$gp_camlib" in #(
        standard)
            GP_SET_UNION([m4-camlib-set], [m4-camlib-set], [m4-camlib-set-standard])
            GP_SET_UNION([camlib-set], [camlib-set], [camlib-set-standard])
            ;; #(
        outdated)
            GP_SET_UNION([m4-camlib-set], [m4-camlib-set], [m4-camlib-set-outdated])
            GP_SET_UNION([camlib-set], [camlib-set], [camlib-set-outdated])
            ;; #(
        unlisted)
            GP_SET_UNION([m4-camlib-set], [m4-camlib-set], [m4-camlib-set-unlisted])
            GP_SET_UNION([camlib-set], [camlib-set], [camlib-set-unlisted])
            ;; #(
        everything)
            GP_SET_UNION([m4-camlib-set], [m4-camlib-set], [m4-camlib-set-everything])
            GP_SET_UNION([camlib-set], [camlib-set], [camlib-set-everything])
            ;; #(
        *)
	    GP_SET_CONTAINS_IFELSE([m4-camlib-set-everything], ["${gp_camlib}"], [dnl
                GP_SET_ADD([m4-camlib-set], ["$gp_camlib"])
                GP_SET_ADD([camlib-set], ["$gp_camlib"])
            ], [dnl
                AC_MSG_ERROR([Unknown camlib found in --with-camlibs: '${gp_camlib}'])
            ])
            ;;
        esac
	;; #(
    remove)
        case "$gp_camlib" in #(
        standard)
            GP_SET_DIFFERENCE([camlib-set], [camlib-set], [camlib-set-standard])
            GP_SET_DIFFERENCE([m4-camlib-set], [m4-camlib-set], [m4-camlib-set-standard])
            ;; #(
        outdated)
            GP_SET_DIFFERENCE([camlib-set], [camlib-set], [camlib-set-outdated])
            GP_SET_DIFFERENCE([m4-camlib-set], [m4-camlib-set], [m4-camlib-set-outdated])
            ;; #(
        unlisted)
            GP_SET_DIFFERENCE([camlib-set], [camlib-set], [camlib-set-unlisted])
            GP_SET_DIFFERENCE([m4-camlib-set], [m4-camlib-set], [m4-camlib-set-unlisted])
            ;; #(
        everything)
            GP_SET_DIFFERENCE([camlib-set], [camlib-set], [camlib-set-everything])
            GP_SET_DIFFERENCE([m4-camlib-set], [m4-camlib-set], [m4-camlib-set-everything])
            ;; #(
        *)
	    GP_SET_CONTAINS_IFELSE([m4-camlib-set-everything], ["${gp_camlib}"], [dnl
                GP_SET_REMOVE([camlib-set], ["$gp_camlib"])
	        GP_SET_CONTAINS_IFELSE([m4-camlib-set], ["$gp_camlib"], [dnl
                    GP_SET_REMOVE([m4-camlib-set], ["$gp_camlib"])
		], [dnl
		    AC_MSG_WARN([Removing camlib ${gp_camlib} from m4-camlib-set which does not contain ${gp_camlib}])
		])
            ], [dnl
                AC_MSG_ERROR([Unknown camlib found in --with-camlibs: '${gp_camlib}'])
            ])
            ;;
        esac
	;;
    esac
done
AS_UNSET([gp_camlib])
AC_MSG_CHECKING([with-camlibs parsing])
AC_MSG_RESULT([finished])

dnl GP_SET_MSG([m4-camlib-set])
dnl GP_SET_MSG([camlib-set])

dnl Camlibs requested, but cannot be built.
GP_SET_DEFINE([camlib-set-diff-skipping])
GP_SET_DIFFERENCE([camlib-set-diff-skipping], [m4-camlib-set], [camlib-set])
GP_SET_MSG([camlib-set-diff-skipping])

dnl Camlibs added over the standard set
GP_SET_DEFINE([camlib-set-diff-over-standard])
GP_SET_DIFFERENCE([camlib-set-diff-over-standard], [camlib-set], [camlib-set-standard])
GP_SET_MSG([camlib-set-diff-over-standard])

dnl Camlibs missing from the standard set
GP_SET_DEFINE([camlib-set-diff-from-standard])
GP_SET_DIFFERENCE([camlib-set-diff-from-standard], [camlib-set-standard], [camlib-set])
GP_SET_MSG([camlib-set-diff-from-standard])

GP_SET_DEBUG_MSG([camlib-set-diff-skipping])
GP_SET_DEBUG_MSG([camlib-set-diff-from-standard])
GP_SET_DEBUG_MSG([camlib-set-diff-over-standard])

GP_SET_SPACE_VAR([camlib-set-diff-skipping], [gp_camlib_set_skipping])dnl

dnl We could use the cardinality of the difference sets to determine
dnl how far we are from the standard set.  If we do not differ too
dnl much, we can still show the camlib set as "standard plus these
dnl minus those skipping that" instead of just listing the camlib
dnl names.

dnl gp_camlibs_from_standard="$(GP_SET_CARDINALITY([camlib-set-diff-from-standard]))"
dnl AC_MSG_CHECKING([camlibs removed from standard set])
dnl AC_MSG_RESULT([${gp_camlibs_from_standard}])

dnl gp_camlibs_over_standard="$(GP_SET_CARDINALITY([camlib-set-diff-over-standard]))"
dnl AC_MSG_CHECKING([camlibs added over standard set])
dnl AC_MSG_RESULT([${gp_camlibs_over_standard}])

dnl gp_camlibs_non_standard="$(${EXPR} ${gp_camlibs_from_standard} + ${gp_camlibs_over_standard})"
dnl AC_MSG_CHECKING([total number of camlibs differing from standard set])
dnl AC_MSG_RESULT([${gp_camlibs_non_standard}])

AC_MSG_CHECKING([whether skipping some requested camlibs])
AS_IF([test "x$gp_camlib_set_skipping" = "x"], [dnl
    AC_MSG_RESULT([no])
], [dnl
    AC_MSG_RESULT([yes (${gp_camlib_set_skipping})])
    AC_DEFINE_UNQUOTED([GP_CAMLIB_SET_SKIPPING], ["${gp_camlib_set_skipping}"],
                       [If defined, the camlibs which are skipped due to missing dependencies])
    AC_MSG_WARN([

    #=========================================================#
    # We are skipping building one or more camlibs, probably  #
    # due to missing dependencies.  Check the dependencies if #
    # you insist on building these camlibs.                   #
    #=========================================================#
])
    GP_SLEEP_IF_TTY([5])
])

GP_SET_SPACE_VAR([camlib-set], [gp_camlib_set])
AC_MSG_CHECKING([camlib set to build in detail])
AC_MSG_RESULT([${gp_camlib_set}])

GP_CAMLIBS_CONDITIONAL_WARNING

dnl Whether user has requested a non-standard set of camlibs
GP_SET_EQUAL_IFELSE([camlib-set], [camlib-set-standard], [dnl
    AS_IF([test "x$gp_camlib_set_skipping" = "x"], [dnl
        GP_CONFIG_MSG([Camlibs],
                      [standard set (${gp_camlib_set})])
    ], [dnl
        GP_CONFIG_MSG([Camlibs],
                      [standard set (${gp_camlib_set} SKIPPING ${gp_camlib_set_skipping})])
    ])
], [dnl
    AS_IF([test "x$gp_camlib_set_skipping" = "x"], [dnl
        GP_CONFIG_MSG([Camlibs],
                      [non-standard set (${gp_camlib_set})])
    ], [dnl
        GP_CONFIG_MSG([Camlibs],
                      [non-standard set (${gp_camlib_set} SKIPPING ${gp_camlib_set_skipping})])
    ])
    m4_pattern_allow([GP_CAMLIB_SET_IS_NONSTANDARD])dnl
    AC_DEFINE_UNQUOTED([GP_CAMLIB_SET_IS_NONSTANDARD], [1],
                       [define when the camlib set to build is non-standard])
])dnl

m4_pattern_allow([GP_CAMLIB_SET])dnl
AC_DEFINE_UNQUOTED([GP_CAMLIB_SET], ["${gp_camlib_set}"],
                   [The actually defined set of camlibs to build])

AS_UNSET([GP_CAMLIB_SET])
for f in ${gp_camlib_set}
do
    GP_CAMLIB_SET="${GP_CAMLIB_SET}${GP_CAMLIB_SET+ }${f}.la"
done
AS_UNSET([f])
AC_SUBST([GP_CAMLIB_SET])

m4_pattern_allow([GP_CAMLIB_SET_EVERYTHING])dnl
AC_SUBST([GP_CAMLIB_SET_EVERYTHING],
         ["m4_set_contents([gp_m4s_camlib_set_everything], [ ])"])

# END $0($@)
])dnl
dnl
dnl
dnl ####################################################################
dnl
dnl Local Variables:
dnl mode: autoconf
dnl End:
