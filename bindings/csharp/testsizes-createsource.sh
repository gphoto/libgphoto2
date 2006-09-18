#!/bin/sh

case "$1" in
c) 
	lang="c" ;;
cs|csharp) 
	lang="cs" ;;
*) 
	echo "Invalid output language: $1." >&2
	echo "Valid output languages: c csharp" >&2
	exit 1
	;;
esac

# Common comment
cat<<EOF
/* size check output for language $lang */
/* DO NOT MODIFY THIS FILE - MODIFY $0 and testsizes-typelist.txt instead */

EOF

# Beginning of file
case "$lang" in
c)
cat <<EOF
#include <stdio.h>

#include <gphoto2/gphoto2-port.h>

#include <gphoto2/gphoto2-camera.h>
#include <gphoto2/gphoto2-file.h>
#include <gphoto2/gphoto2-filesys.h>
#include <gphoto2/gphoto2-abilities-list.h>

int
main(void)
{
EOF
;;
cs)
cat<<EOF
using LibGPhoto2;

class TestSizes {

  public static int Main() {
EOF
;;
esac

# Print type size
while read type type2; do
    if test "x$type" = "x"; then continue; fi
    if echo "$type" | grep '^#' > /dev/null; then continue; fi
    case "$lang" in
	c)
	    if test "x$type2" != "x"; then ctype="$type2"; else ctype="$type"; fi
	    echo "  printf(\"%s %d\\n\", \"${ctype}\", sizeof(${ctype}));"
	    ;;
	cs)
	    var="__var_${type}"
	    echo "    ${type} ${var} = new ${type}();"
	    echo "    System.Console.WriteLine(\"${type} \" + System.Runtime.InteropServices.Marshal.SizeOf(${var}));"
	    echo
	    ;;
    esac
done

# End of file
case "$lang" in
c)
cat <<EOF
  return 0;
}
EOF
;;
cs)
cat<<EOF
    return 0;
  }

}
EOF
;;
esac
