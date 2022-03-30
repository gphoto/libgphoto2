# gp-set.m4 - implement configure time set arithmetic          -*- Autoconf -*-
# serial 13
dnl | Increment the above serial number every time you edit this file.
dnl | When it finds multiple m4 files with the same name,
dnl | aclocal will use the one with the highest serial.
dnl
dnl ######################################################################
dnl Set operations using shell
dnl ######################################################################
dnl
dnl This implements a set of set operations ('set' in the sense of
dnl 'set theory') as shell code for use at configure time.  At m4 time
dnl (aka autoreconf time), there already exists a set of m4_set_*
dnl macros, but we cannot use those for user specified and
dnl autodetected changes at configure time.
dnl
dnl Not all set operations are implemented, but a few basic ones we
dnl need for gp-camlibs.m4 are.
dnl
dnl   * Basic definitions:
dnl
dnl       * GP_SET_DEFINE
dnl       * GP_SET_UNDEFINE
dnl
dnl   * Operations on sets:
dnl
dnl       * GP_SET_ADD
dnl       * GP_SET_ADD_ALL
dnl       * GP_SET_REMOVE
dnl
dnl       * GP_SET_CARDINALITY
dnl       * GP_SET_DIFFERENCE
dnl       * GP_SET_UNION
dnl
dnl       * GP_SET_FOREACH
dnl
dnl   * Conditionals:
dnl
dnl       * GP_SET_CONTAINS_IFELSE
dnl       * GP_SET_EMPTY_IFELSE
dnl       * GP_SET_EQUAL_IFELSE
dnl
dnl       * GP_SET_CLEAN_FILES
dnl
dnl   * Internals and debugging help:
dnl
dnl       * GP_SET_CANONICALIZE
dnl       * GP_SET_DUMP_ALL
dnl       * GP_SET_TESTSUITE
dnl
dnl   * Fill shell variables with set contents:
dnl
dnl       * GP_SET_DEBUG_VAR
dnl       * GP_SET_SPACE_VAR
dnl
dnl   * Output AC_MSG_* for GP_SET_* sets:
dnl
dnl       * GP_SET_DEBUG_MSG
dnl       * GP_SET_MSG_DEBUG_RESULT
dnl
dnl       * GP_SET_SPACE_MSG
dnl       * GP_SET_MSG_SPACE_RESULT
dnl
dnl ######################################################################
dnl Remarks on usage and implementation of GP_SET_*
dnl
dnl   * Sets are stored in text files, one element per line. An empty
dnl     file is an empty set, and empty lines mean empty elements.
dnl     Non-empty files not ending with a newline are undefined behaviour.
dnl
dnl   * At this time, GP_SET_* macros work with set elements
dnl     consisting of a limited set of characters, but the storage
dnl     format allows for a future code cleanup to allow for handling
dnl     more arbitrarily called elements.
dnl
dnl   * Set names given to GP_SET_ macros must be defined at m4
dnl     (autoreconf) time, not at sh (configure) time.
dnl
dnl   * Every set must be declared before use with GP_SET_DEFINE so we
dnl     can create an empty file for them and keep track of the name of
dnl     that file for cleaning up later.
dnl
dnl   * Use GP_SET_CLEAN_FILES after your last set operations to remove
dnl     all left over set files.
dnl
dnl   * Set text files may contain set elements in any order, and in
dnl     any number. There is no difference between a set text file
dnl     containing the same line seven times and one which only contains
dnl     it one time.
dnl
dnl   * Some set operations require or work better on sorted files, so
dnl     sometimes the implementation sorts the set text files. If you
dnl     need a set text file sorted for some another reason, use
dnl     GP_SET_CANONICALIZE([set-name]).
dnl
dnl   * Some set GP_SET_* operations are implemented using shell
dnl     functions to reduce the size of the emitted sh code. The goal
dnl     is for the shell functions to be as compatible with different
dnl     shells as possible, but if necessary, we can revert to avoiding
dnl     shell functions and just emitting the same sh code 40 times.
dnl
dnl
dnl ######################################################################
dnl
dnl FIXME: Do we need a sh-based replacement for ${COMM} if it is not present?
dnl FIXME: Do we need to stop using shell functions?
dnl
dnl
dnl ######################################################################
dnl Try catching unexpanded macros in the output.
dnl ######################################################################
m4_pattern_forbid([GP_SET_])dnl
m4_pattern_forbid([_GP_SET_])dnl
dnl
dnl
dnl ######################################################################
dnl _GP_SET_INIT
dnl   Called internally before any set operation.
dnl ######################################################################
AC_DEFUN_ONCE([_GP_SET_INIT], [dnl
AC_REQUIRE([AC_PROG_GREP])dnl
AC_REQUIRE([GP_PROG_SORT])dnl
AC_REQUIRE([GP_PROG_UNIQ])dnl
m4_set_empty([gp_set_all_sets], [], [dnl
  m4_set_foreach([gp_set_all_sets], [elt], [dnl
    m4_errprintn(__file__: __line__:[ Set gp_set_all_sets contains element ]elt)dnl
  ])dnl
  m4_errprintn(__file__:__line__:[ Error: gp_set_all_sets already defined])dnl
  dnl m4_exit(1)dnl
])dnl
])dnl
dnl
dnl
dnl ######################################################################
dnl _GP_SET_FILENAME([set_name])
dnl _GP_SET_FILENAME([set_name], [extra-extension])
dnl   Convert set name to set file name. If present, add the given
dnl   extra-extension to the file name in some position.
dnl ######################################################################
AC_DEFUN([_GP_SET_FILENAME], [dnl
[gp-set-file--][$1]m4_if([$2],[],[],[.$2])])dnl
dnl
dnl
dnl ######################################################################
dnl _GP_SET_CHECK_INIT
dnl   Implement the gp_set_shfn_check() shell function which checks that the
dnl   given set exists and is in general working order.
dnl ######################################################################
AC_DEFUN_ONCE([_GP_SET_CHECK_INIT], [dnl
AC_REQUIRE([_GP_SET_INIT])dnl
gp_set_shfn_check ()
{
  if test -f "[$]1"; then :; else
    AC_MSG_ERROR(["Error: set [$]1 has not been defined yet"])
  fi
} # gp_set_shfn_check
])dnl
dnl
dnl
dnl ######################################################################
dnl _GP_SET_CHECK([set_name])
dnl   Check that the given set exists and is in general working order.
dnl   Works by calling the gp_set_shfn_check() shell function.
dnl ######################################################################
AC_DEFUN([_GP_SET_CHECK], [dnl
AC_REQUIRE([_GP_SET_INIT])dnl
AC_REQUIRE([_GP_SET_CHECK_INIT])dnl
gp_set_shfn_check "_GP_SET_FILENAME([$1])"
])dnl
dnl
dnl
dnl ######################################################################
dnl GP_SET_DUMP_ALL
dnl   Dump all sets to stdout. Intended for helping with debugging.
dnl ######################################################################
AC_DEFUN([GP_SET_DUMP_ALL], [dnl
AC_REQUIRE([AC_PROG_SED])dnl
AC_REQUIRE([_GP_SET_INIT])dnl
for setfile in gp-set-file--*
do
  AS_ECHO(["Set file: ${setfile}"])
  ${SED} 's/^/  * <</; s/$/>>/' "${setfile}"
done
])dnl
dnl
dnl
dnl ######################################################################
dnl GP_SET_DEFINE([set_name])
dnl   Declare a set of given name, initializing it to an empty set.
dnl   Not a shell function, as the emitted shell code is shorter than
dnl   calling a shell function.
dnl ######################################################################
AC_DEFUN([GP_SET_DEFINE], [dnl
AC_REQUIRE([_GP_SET_INIT])dnl
m4_set_add([gp_set_all_sets], [$1])dnl
: > "_GP_SET_FILENAME([$1])"
])dnl
dnl
dnl
dnl ######################################################################
dnl GP_SET_UNDEFINE([set_name])
dnl   Undefine a set of given name, removing its file.
dnl   Not a shell function, as the emitted shell code is shorter than
dnl   calling a shell function.
dnl ######################################################################
AC_DEFUN([GP_SET_UNDEFINE], [dnl
AC_REQUIRE([_GP_SET_INIT])dnl
_GP_SET_CHECK([$1])dnl
rm -f "_GP_SET_FILENAME([$1])"
])dnl
dnl
dnl
dnl ######################################################################
dnl _GP_SET_ADD_INIT
dnl   Implement the gp_set_shfn_set_add() shell function which adds a given
dnl   element to a given set.
dnl ######################################################################
AC_DEFUN_ONCE([_GP_SET_ADD_INIT], [dnl
AC_REQUIRE([_GP_SET_INIT])dnl
AC_REQUIRE([_GP_SET_CHECK_INIT])dnl
gp_set_shfn_set_add ()
{
  gp_set_shfn_check "[$]1"
  AS_ECHO(["[$]2"]) >> "[$]1"
} # gp_set_shfn_set_add
])dnl
dnl
dnl
dnl ######################################################################
dnl GP_SET_ADD([set_name], [element])
dnl   Add the given element to the given set. Implemented by calling the
dnl   gp_set_shfn_set_add() shell function.
dnl   Note: Unlike m4_set_add, this does not handle IF-UNIQ and IF-DUP
dnl   macro parameters.
dnl ######################################################################
AC_DEFUN([GP_SET_ADD], [dnl
AC_REQUIRE([_GP_SET_INIT])dnl
AC_REQUIRE([_GP_SET_ADD_INIT])dnl
gp_set_shfn_set_add "_GP_SET_FILENAME([$1])" "$2"
])dnl
dnl
dnl
dnl ######################################################################
dnl GP_SET_ADD_ALL([set_name], [element]...)
dnl   Add the given elements to the given set, similar to m4_set_add_all.
dnl   This can be very useful for initializing a GP_SET_* set from a
dnl   m4_set_* in one go.
dnl   Not a shell function, as the emitted shell code is shorter than
dnl   calling a shell function.
dnl ######################################################################
AC_DEFUN([GP_SET_ADD_ALL], [dnl
AC_REQUIRE([_GP_SET_INIT])dnl
_GP_SET_CHECK([$1])dnl
cat>>"_GP_SET_FILENAME([$1])"<<EOF
m4_foreach([myvar], [m4_shift($@)], [dnl
myvar
])dnl
EOF
])dnl
dnl
dnl
dnl ######################################################################
dnl _GP_SET_REMOVE_INIT
dnl   Implement the gp_set_shfn_remove() shell function removing a given
dnl   element from a given set.
dnl ######################################################################
AC_DEFUN_ONCE([_GP_SET_REMOVE_INIT], [dnl
AC_REQUIRE([_GP_SET_INIT])dnl
gp_set_shfn_remove ()
{
  gp_set_shfn_check "[$]1"
  ${GREP} -v "^[$]2$" < "[$]1" > "[$]1.tmp"
  mv -f "[$]1.tmp" "[$]1"
} # gp_set_shfn_remove
])dnl
dnl
dnl
dnl ######################################################################
dnl GP_SET_REMOVE([set_name], [element])
dnl   Remove a given element from a given set.
dnl   Calls the gp_set_shfn_remove() shell function for the actual work.
dnl ######################################################################
AC_DEFUN([GP_SET_REMOVE], [dnl
AC_REQUIRE([_GP_SET_INIT])dnl
AC_REQUIRE([_GP_SET_REMOVE_INIT])dnl
gp_set_shfn_remove "_GP_SET_FILENAME([$1])" "$2"
])dnl
dnl
dnl
dnl ######################################################################
dnl _GP_SET_UNION_INIT
dnl   Define the gp_set_shfn_union() shell function which defines a
dnl   result set as the union of 0 or more sets.
dnl ######################################################################
AC_DEFUN_ONCE([_GP_SET_UNION_INIT], [dnl
AC_REQUIRE([_GP_SET_INIT])dnl
dnl This functions uses the shell builtin 'shift', so it needs to store
dnl the original $1 in a local variable.
gp_set_shfn_union ()
{
  local result_fname="[$]1"
  gp_set_shfn_check "[$]result_fname"
  if shift; then
    cat "[$]@" > "[$]{result_fname}.tmp"
    mv -f "[$]{result_fname}.tmp" "[$]{result_fname}"
  fi
} # gp_set_shfn_union
])dnl
dnl
dnl
dnl ######################################################################
dnl GP_SET_UNION([result_set], [setname]...)
dnl   Define result_set as the union of 0 or more setnames.
dnl   Calls the gp_set_shfn_union() shell function for the actual work.
dnl ######################################################################
AC_DEFUN([GP_SET_UNION], [dnl
AC_REQUIRE([_GP_SET_INIT])dnl
AC_REQUIRE([_GP_SET_UNION_INIT])dnl
m4_set_add([gp_set_all_sets], [$1])dnl
gp_set_shfn_union m4_foreach([setname], [$@], [ "_GP_SET_FILENAME(setname)"])
])dnl
dnl
dnl
dnl ######################################################################
dnl _GP_SET_DIFFERENCE_INIT([result_set_fname], [set_fname]...)
dnl   This uses comm(1), which is a POSIX command. We can always
dnl   re-implement comm(1) with a lot of speed penalties in a bunch
dnl   of nested sh loops if we run into a system which does not have
dnl   comm(1) installed or where comm(1) does not work.
dnl ######################################################################
AC_DEFUN_ONCE([_GP_SET_DIFFERENCE_INIT], [dnl
AC_REQUIRE([_GP_SET_INIT])dnl
AC_REQUIRE([_GP_SET_CHECK_INIT])dnl
AC_REQUIRE([_GP_SET_CANONICALIZE_INIT])dnl
AC_REQUIRE([GP_PROG_COMM])dnl
cat>confset_a.txt<<EOF
x
y
EOF
cat>confset_b.txt<<EOF
y
z
EOF
cat>confset_res_23.txt<<EOF
x
EOF
AC_MSG_CHECKING([whether comm -23 works])
m4_pattern_allow([GP_SET_xyz])dnl mentioned in message strings
AS_IF([${COMM} -23 confset_a.txt confset_b.txt > confset_diff_23.txt], [dnl
  AS_IF([${CMP} confset_diff_23.txt confset_res_23.txt > /dev/null 2>&1], [dnl
    AC_MSG_RESULT([yes])
    rm -f confset_a.txt confset_b.txt confset_diff_23.txt confset_res_23.txt
  ], [dnl
    AC_MSG_RESULT([no (wrong result)])
    AC_MSG_ERROR([comm -23 must work for GP_SET_xyz difference calculations])
  ])
], [dnl
  AC_MSG_RESULT([no (does not run)])
  AC_MSG_ERROR([comm -23 must work for GP_SET_xyz difference calculations])
])
dnl This functions uses the shell builtin 'shift', so it needs to store
dnl the original $1 and $2 in local variables.
gp_set_shfn_difference ()
{
  local result_fname="[$]1"
  gp_set_shfn_check "[$]result_fname"
  if shift; then
    gp_set_shfn_canonicalize "[$]1"
    cat "[$]1" > "[$]result_fname"
    if shift; then
      for gp_s
      do
        gp_set_shfn_canonicalize "[$]gp_s"
        ${COMM} -23 "[$]{result_fname}" "[$]gp_s" > "[$]{result_fname}.tmp"
        mv -f "[$]{result_fname}.tmp" "[$]{result_fname}"
      done
    fi
  fi
} # gp_set_shfn_difference
])dnl
dnl
dnl
dnl ######################################################################
dnl GP_SET_DIFFERENCE([result_set], [setname]...)
dnl   Define result_set as the first setname with the element of all
dnl   the other sets removed from it.
dnl ######################################################################
AC_DEFUN([GP_SET_DIFFERENCE], [dnl
AC_REQUIRE([_GP_SET_INIT])dnl
AC_REQUIRE([_GP_SET_DIFFERENCE_INIT])dnl
m4_set_add([gp_set_all_sets], [$1])dnl
gp_set_shfn_difference m4_foreach([setname], [$@], [ "_GP_SET_FILENAME(setname)"])
])dnl
dnl
dnl
dnl ######################################################################
dnl _GP_SET_CARDINALITY_INIT
dnl   Define gp_set_shfn_cardinality() shell function which writes
dnl   set cardinality to stdout.
dnl ######################################################################
AC_DEFUN_ONCE([_GP_SET_CARDINALITY_INIT], [dnl
AC_REQUIRE([_GP_SET_INIT])dnl
gp_set_shfn_cardinality ()
{
  gp_set_shfn_check "[$]1"
  wc -l < "[$]1"
} # gp_set_shfn_cardinality
])dnl
dnl
dnl
dnl ######################################################################
dnl GP_SET_CARDINALITY([set_name])
dnl   Write set cardinality to stdout.
dnl ######################################################################
AC_DEFUN([GP_SET_CARDINALITY], [dnl
AC_REQUIRE([_GP_SET_INIT])dnl
AC_REQUIRE([_GP_SET_CARDINALITY_INIT])dnl
gp_set_shfn_cardinality "_GP_SET_FILENAME([$1])"])dnl
dnl
dnl
dnl ######################################################################
dnl GP_SET_EMPTY_IFELSE([set_name], [if-empty], [if-not-empty])
dnl   Run if-empty block or if-not-empty block, depending on whether
dnl   the named set is empty or not.
dnl ######################################################################
AC_DEFUN([GP_SET_EMPTY_IFELSE], [dnl
AC_REQUIRE([_GP_SET_INIT])dnl
_GP_SET_CHECK([$1])dnl
AS_IF([test "0" -eq "$(wc -l < "_GP_SET_FILENAME([$1])")"], m4_shift($@))
])dnl
dnl
dnl
dnl ######################################################################
dnl GP_SET_EQUAL_IFELSE([set_a], [set_b], [if-equal], [if-not-equal])
dnl   When set_a and set_b are equal, run the if-equal block. Otherwise,
dnl   run the if-not-equal block.
dnl ######################################################################
AC_DEFUN([GP_SET_EQUAL_IFELSE], [dnl
gp_set_shfn_canonicalize "_GP_SET_FILENAME([$1])"
gp_set_shfn_canonicalize "_GP_SET_FILENAME([$2])"
AS_IF([${CMP} "_GP_SET_FILENAME([$1])" "_GP_SET_FILENAME([$2])" > /dev/null 2>&1],
      m4_shift2($@))
])dnl
dnl
dnl
dnl ######################################################################
dnl GP_SET_CONTAINS_IFELSE([set_name], [element], [yes-block], [no-block])
dnl   If set_name contains element, run the yes-block. Otherwise,
dnl   run the no-block.
dnl ######################################################################
AC_DEFUN([GP_SET_CONTAINS_IFELSE], [dnl
_GP_SET_CHECK([$1])dnl
AS_IF([test "0" -lt "$(${GREP} -c "^$2\$" < "_GP_SET_FILENAME([$1])")"],
      m4_shift2($@))
])dnl
dnl
dnl
dnl ######################################################################
dnl _GP_SET_DEBUG_VAR_INIT
dnl   Define gp_set_shfn_debug_var() shell function which sets a shell
dnl   variable to a string representing a human readable form for of
dnl   the given set.
dnl ######################################################################
AC_DEFUN_ONCE([_GP_SET_DEBUG_VAR_INIT], [dnl
AC_REQUIRE([_GP_SET_INIT])dnl
gp_set_shfn_debug_var ()
{
  gp_set_shfn_check "[$]1"
  gp_set_shfn_canonicalize "[$]1"
  local element
  local gp_set_is_first=:
  local gp_set_saved_ifs="$IFS"
  IFS=""
  while read element
  do
    if "$gp_set_is_first"
    then
      eval "[$]2=\"{ \""
      gp_set_is_first=false
    else
      eval "[$]2=\"\${[$]2}, \""
    fi
    if test "x$element" = "x"
    then
      eval "[$]2=\"\${[$]2}''\""
    else
      eval "[$]2=\"\${[$]2}'\$element'\""
    fi
  done < "[$]1"
  IFS="$gp_set_saved_ifs"
  if "$gp_set_is_first"
  then
    eval "[$]2=\"{ }\""
  else
    eval "[$]2=\"\${[$]2} }\""
  fi
} # gp_set_shfn_debug_var
])dnl
dnl
dnl
dnl ######################################################################
dnl GP_SET_DEBUG_VAR([set_name], [shell_var_name])
dnl   Set shell variable shell_var_name to a string representing a human
dnl   readable for of the given set.
dnl ######################################################################
AC_DEFUN([GP_SET_DEBUG_VAR], [dnl
AC_REQUIRE([_GP_SET_INIT])dnl
AC_REQUIRE([_GP_SET_DEBUG_VAR_INIT])dnl
gp_set_shfn_debug_var "_GP_SET_FILENAME([$1])" "$2"
])dnl
dnl
dnl
dnl ######################################################################
dnl GP_SET_DEBUG_MSG([set-name])
dnl   For a given set, expand to a simple pair of AC_MSG_CHECKING and
dnl   AC_MSG_RESULT showing the GP_SET_DEBUG_VAR, mostly to help with
dnl   debugging.
dnl ######################################################################
AC_DEFUN([GP_SET_DEBUG_MSG], [dnl
AC_REQUIRE([_GP_SET_INIT])dnl
AC_MSG_CHECKING([value of set ]$1)
GP_SET_MSG_DEBUG_RESULT([$1])
])dnl
dnl
dnl
dnl ######################################################################
dnl GP_SET_SPACE_MSG([set-name])
dnl   For a given set, expand to a simple pair of AC_MSG_CHECKING and
dnl   AC_MSG_RESULT showing the GP_SET_SPACE_VAR, both to help with
dnl   debugging gp-set.m4 and possibly for produnction use.
dnl ######################################################################
AC_DEFUN([GP_SET_MSG], [dnl
AC_REQUIRE([_GP_SET_INIT])dnl
AC_MSG_CHECKING([value of set ]$1)
GP_SET_MSG_RESULT([$1])
])dnl
dnl
dnl
dnl ######################################################################
dnl GP_SET_SPACE_VAR([set-name], [var-name])
dnl ######################################################################
AC_DEFUN([GP_SET_SPACE_VAR], [dnl
AC_REQUIRE([_GP_SET_INIT])dnl
$2=""
GP_SET_FOREACH([$1], [element], [dnl
AS_IF([test "x[$]$2" = "x"], [dnl
  $2="[$]element"
], [dnl
  $2="[$]$2 [$]element"
])
])
])dnl
dnl
dnl
dnl ######################################################################
dnl GP_SET_MSG_RESULT([set-name])
dnl   Shortcut for setting a shell variable to a space separated list of
dnl   elements, and then expanding to AC_MSG_RESULT([$shellvar]).
dnl ######################################################################
AC_DEFUN([GP_SET_MSG_RESULT], [dnl
AC_REQUIRE([_GP_SET_INIT])dnl
GP_SET_SPACE_VAR([$1], [gp_set_msg_result_var])dnl
AC_MSG_RESULT([${gp_set_msg_result_var}])
])dnl
dnl
dnl
dnl ######################################################################
dnl GP_SET_MSG_DEBUG_RESULT([set-name])
dnl   Shortcut for setting a shell variable to a human readable list of
dnl   elements, and then expanding to AC_MSG_RESULT([$shellvar]).
dnl ######################################################################
AC_DEFUN([GP_SET_MSG_DEBUG_RESULT], [dnl
AC_REQUIRE([_GP_SET_INIT])dnl
GP_SET_DEBUG_VAR([$1], [gp_set_msg_result_var])dnl
AC_MSG_RESULT([${gp_set_msg_result_var}])
])dnl
dnl
dnl
dnl ######################################################################
dnl _GP_SET_CANONICALIZE_INIT
dnl   Implement the gp_set_shfn_canonicalize() shell function.
dnl ######################################################################
AC_DEFUN_ONCE([_GP_SET_CANONICALIZE_INIT], [dnl
AC_REQUIRE([_GP_SET_INIT])dnl
gp_set_shfn_canonicalize ()
{
  gp_set_shfn_check "[$]1"
  ( set -e
    [$]{SORT} < "[$]1" | [$]{UNIQ} > "[$]1.tmp"
    mv -f "[$]1.tmp" "[$]1"; )
} # gp_set_shfn_canonicalize
])dnl
dnl
dnl
dnl ######################################################################
dnl GP_SET_CANONICALIZE([set_name])
dnl   Convert the set file for set_name into a canonical form.
dnl
dnl   Implementation detail: Sorts the lines, and makes them unique.
dnl ######################################################################
AC_DEFUN([GP_SET_CANONICALIZE], [dnl
AC_REQUIRE([_GP_SET_INIT])dnl
AC_REQUIRE([_GP_SET_CANONICALIZE_INIT])dnl
gp_set_shfn_canonicalize "_GP_SET_FILENAME([$1])"
])dnl
dnl
dnl
dnl ######################################################################
dnl GP_SET_FOREACH([set_name], [shell-var], [shell-block])
dnl   For each element in set_name, run the shell-block with the given
dnl   shell variable set to every element in turn.
dnl ######################################################################
AC_DEFUN([GP_SET_FOREACH], [dnl
_GP_SET_CHECK([$1])dnl
gp_set_saved_ifs="$IFS"
IFS=""
while read $2
do
  IFS="$gp_set_saved_ifs"
  $3
done < "_GP_SET_FILENAME([$1])"
IFS="$gp_set_saved_ifs"
])dnl
dnl
dnl
dnl ######################################################################
dnl GP_SET_CLEAN_FILES
dnl   Remove all the files used for storing sets. Invoke this macro near
dnl   the end of configure.ac after you have finished doing anything with
dnl   GP_SET_* sets.
dnl ######################################################################
AC_DEFUN([GP_SET_CLEAN_FILES], [dnl
AC_BEFORE([_GP_SET_INIT], [$0])dnl
AC_BEFORE([GP_SET_DEFINE], [$0])dnl
AC_BEFORE([GP_SET_UNDEFINE], [$0])dnl
AC_BEFORE([GP_SET_ADD], [$0])dnl
AC_BEFORE([GP_SET_ADD_ALL], [$0])dnl
AC_BEFORE([GP_SET_REMOVE], [$0])dnl
AC_BEFORE([GP_SET_EMPTY_IFELSE], [$0])dnl
AC_BEFORE([GP_SET_EQUAL_IFELSE], [$0])dnl
AC_BEFORE([GP_SET_CONTAINS_IFELSE], [$0])dnl
AC_BEFORE([GP_SET_DEBUG_VAR], [$0])dnl
AC_BEFORE([GP_SET_SPACE_VAR], [$0])dnl
dnl
dnl GP_SET_DUMP_ALL()dnl
dnl
dnl m4_set_foreach([gp_set_all_sets], [set_name], [dnl
dnl AS_ECHO(["Set defined somewhere: set_name"])
dnl ])dnl
dnl
[rm -f]m4_set_foreach([gp_set_all_sets], [setname],
                      [ "_GP_SET_FILENAME(setname)"])dnl
])dnl
dnl
dnl
dnl ######################################################################
dnl GP_SET_TESTSUITE()
dnl   Does a number of tests for the GP_SET_* macros.
dnl
dnl   Should be mostly useful while debugging gp-set.m4 itself.
dnl ######################################################################
AC_DEFUN([GP_SET_TESTSUITE], [dnl
# BEGIN $0
_GP_SET_INIT

GP_SET_DEFINE([foo])
GP_SET_DEFINE([bar])
GP_SET_DEFINE([foobar])
GP_SET_DEFINE([everything])

GP_SET_DEBUG_MSG([foo])

GP_SET_DEFINE([bax])
GP_SET_UNDEFINE([bax])

GP_SET_UNDEFINE([bar])
GP_SET_DEFINE([bar])

dnl GP_SET_ADD([moo], [meh])
GP_SET_ADD_ALL([everything], [foo], [bar], [bar ], [bla])

GP_SET_DUMP_ALL

m4_set_add_all([m4testset], [a], [b c], [d e], [f], [g])
AC_MSG_CHECKING([for value of m4 testset])
AC_MSG_RESULT([m4_set_contents([m4testset], [, ])])

GP_SET_ADD_ALL([everything]m4_set_listc([m4testset]))

GP_SET_DUMP_ALL

GP_SET_ADD([foo], [barfoo])
GP_SET_ADD([foo], [foobar])
GP_SET_ADD([foo], [fox])
GP_SET_ADD([foo], [fux])
GP_SET_ADD([foo], [fox])
GP_SET_ADD([foo], [])
GP_SET_ADD([foo], [fox])
GP_SET_ADD([foo], [barfoo])
GP_SET_ADD([foo], [ barfoo])
GP_SET_ADD([foo], ["barfoo "])
GP_SET_ADD([foo], [])
GP_SET_REMOVE([foo], [fox])
GP_SET_ADD([foo], [barfoo])

GP_SET_ADD([foobar], [bar])
GP_SET_ADD([foobar], [foobar])
GP_SET_ADD([foobar], [barfoo])

GP_SET_DEBUG_MSG([foo])

GP_SET_MSG([bar])
GP_SET_MSG([foo])
GP_SET_MSG([foobar])
GP_SET_MSG([everything])

GP_SET_DEFINE([union])
GP_SET_UNION([union], [foo], [bar], [foobar])
AC_MSG_CHECKING([value of union of foo bar foobar])
GP_SET_MSG_RESULT([union])

GP_SET_DEFINE([difference])
GP_SET_DIFFERENCE([difference], [foo], [bar], [foobar])
GP_SET_MSG([difference])

AC_MSG_CHECKING([whether sets foo and bar are equal])
GP_SET_EQUAL_IFELSE([foo], [bar], [AC_MSG_RESULT([yes])], [AC_MSG_RESULT([no])])

AC_MSG_CHECKING([whether set foo is empty])
GP_SET_EMPTY_IFELSE([foo], [AC_MSG_RESULT([yes])], [AC_MSG_RESULT([no])])

AC_MSG_CHECKING([whether set bar is empty])
GP_SET_EMPTY_IFELSE([bar], [AC_MSG_RESULT([yes])], [AC_MSG_RESULT([no])])

AC_MSG_CHECKING([whether set foo contains element bar])
GP_SET_CONTAINS_IFELSE([foo], [bar], [AC_MSG_RESULT([yes])], [AC_MSG_RESULT([no])])

AC_MSG_CHECKING([whether set foobar contains element bar])
GP_SET_CONTAINS_IFELSE([foobar], [bar], [AC_MSG_RESULT([yes])], [AC_MSG_RESULT([no])])

GP_SET_FOREACH([foobar], [gp_set_var], [dnl
  AC_MSG_CHECKING([foobar element])
  AC_MSG_RESULT([${gp_set_var}])
])dnl
GP_SET_FOREACH([foobar], [gp_set_var], [dnl
  AS_ECHO(["  * element ${gp_set_var}"])
])dnl
GP_SET_FOREACH([foobar], [gp_set_var], [echo "  * ELEMENT ${gp_set_var}"])dnl
AS_ECHO(["Moo."])

GP_SET_DUMP_ALL
# END $0
])dnl
dnl
dnl
dnl ####################################################################
dnl
dnl Local Variables:
dnl mode: autoconf
dnl End:
