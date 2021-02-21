HACKING
=======

If you want to hack on gphoto2 (or a camera driver for gphoto2),
please follow some rules to make our work easier.

[ Note from 2019-10: Some of these rules are from the early 2000s and
  may make less sense now than they did fifteen years plus earlier. ]


Licensing
=========

We urge you to license your contributions under the LGPL so we can
distribute gphoto2 under the LGPL as well.


Patches
=======

... are always welcome.

We prefer patches against the current git master very much over
patches against old released versions, but these are also welcome :-)


Source Repository Directory Structure
=====================================

The source repository structure as of libgphoto2-2.5.23 (2019-10):

  * [`libgphoto2_port/gphoto2/*.h`](libgphoto2_port/gphoto2/)

    The header files with the interface to `libgphoto2_port`.

  * [`libgphoto2_port/libgphoto2_port/*.[ch]`](libgphoto2_port/libgphoto2_port/)

    Implementation of the `libgphoto2_port` library used internally by
    `libgphoto2`.

  * [`libgphoto2_port/{disk,libusb1,ptpip,serial,usb,usbdiskdirect,usbscsi,vusb}/*.[ch]`](libgphoto2_port/)

    The port driver code for the `iolibs`. Dynamically loaded by `libgphoto2_port`.

  * [`libgphoto2/*.[ch]`](libgphoto2/)

    The source code of the actual `libgphoto2` library.

  * [`gphoto2/*.h`](gphoto2/)

    The header files with the interface to `libgphoto2`.

  * [`camlibs/<CAMLIB>/*.[ch]`](camlibs/)

    The camera drivers (`camlibs`). Dynamically loaded by `libgphoto2`.

The buildsystem is based on automake, i.e. `configure.ac`,
`Makefile.am` and `Makefile-files` have their standard functions.


Source Files
============

Every source file (`file_name.c`) should have the following layout:

    /* Licence, author, etc. */
    #include "config.h"
    #include "file_name.h"

    #include <std*.h> (i.e. stdlib, stdio, string, etc.)

    #include <gphoto2/gphoto2-*.h> (what you need)

    /* Source code here. */


Header Files
============

Please use the following layout:

    /* Licence, author, etc. */
    #ifndef __FILE_NAME_H__
    #define __FILE_NAME_H__

    #include <whatever is needed for the _header_>

    /* Declarations */

    #endif /* __FILE_NAME_H__ */


Camlib
======

Camera libraries are often very sensitive to changes. Therefore,
before you commit any changes, double check with the author that your
changes won't break the driver.

If you want to write a driver for gphoto2, the easiest way to do so
would be to copy over the contents of camlibs/template and fill in
your code.

Use something like `CHECK_RESULT` (see for example
`libgphoto2/filesys.c`).

Let's say you write a driver called sillycam. Please set up a file
called `library.c` containing all gphoto2-specific code (like
`camera_init`) and another two files called `sillycam.c` and
`sillycam.h` containing the "magic", that is all
`gp_port_[read,write]` functions and the basic logic of the
communication. This makes it easier for us to adapt your code if
anything should change in the gphoto2-API.

Use the port provided by `camera->port`.

Use the filesystem provided by `camera->fs` and set up the callbacks so
that libgphoto2 can cache listings and file-information.

Please keep track of the changes to the sillycam camlib in the file
`camlib/sillycam/ChangeLog`.

For test compiling and installing only the sillycam and doofuscam
camlibs, you can use

    make -C camlibs sillycam.la doofuscam.la

and

    make CAMLIBS="sillycam.la doofuscam.la" install-camlibs

respectively.

Compiling all camlibs is sped up considerably on *N* CPU core
computers using `make -j`*N*.


libgphoto2
==========

If you add code, use the coding style of that file. This is, code like

```
int
my_func (int arg)
{
	int var, res;

	/*
	 * This is a multiline
	 * comment. Use TAB for 
	 * indentation!
	 */
	res = gp_some_action (var);

	/* This is a simple one-line comment */
	if (res < 0) {
		gp_log (GP_LOG_DEBUG, "Error happened!");

		/* Note that we _don't_ indent the case statements */
		switch (res) {
		case GP_ERROR:
			gp_log (GP_LOG_DEBUG, "Generic error");
			break;
		default:
			gp_log (GP_LOG_DEBUG, "Not a generic error");
		}
		return (res);
	}

	return (GP_OK);
}
```

Please always check the return value of `gp_`-functions! We defined some
handy macros all over the place (like `CHECK_RESULT`) - by using those,
you'll avoid lots of `if () {} else {}`.

Emacs users may define and use a `gphoto-c-mode` by putting the
following stuff into their `.emacs` file:


    ;;* gphoto-c-mode
    ;;*=====================================================================
    (defun gphoto-c-mode ()
      "C mode with adjusted defaults for gphoto hacking"
      (interactive)
      (c-mode)
      (c-set-style "linux")
      (setq indent-tabs-mode t)
      (font-lock-mode))
    
    ;;
    (setq auto-mode-alist (cons '("/home/user/src/gphoto.*\\.[ch]$" . gphoto-c-mode)
                           auto-mode-alist))



Compatibility
=============

Once gphoto2 has been officially launched as a 2.0 version, it is important
that the API does not change frequently.  It is annoying to users when they
have to upgrade all sort of libraries and applications just to get an
upgraded camera driver.  Once the 2.0 release has been made, try to
follow these guidelines:

  - New versions of the gphoto2 core libraries in the 2.0 series shall
    work with all older 2.0 camera libraries

  - New versions of the gphoto2 camera libraries in the 2.0 series shall
    work with all older 2.0 core libraries (this one may be relaxed)

  - Before fixing 2.0 in stone, look for weaknesses in the current
    APIs that may preclude enhancements in the future.  For instance,
    there should be some filler entries added to the end of `struct
    _CameraFunctions` so that a newer camera library that tries to
    fill in an entry that is added in gphoto2 ver. 2.1 that doesn't
    exist there now won't overwrite something important.

  - Try to work using the existing API instead of making minor
    changes here and there; think about whether the change you're proposing
    will demonstratively benefit gphoto2 or it's just a "nice to have"

  - Instead of adding a new parameter to a function, create a new function
    that enhances the other one (where appropriate)

  - If a new parameter is absolutely needed in an existing function
    call, rename the function with the new parameter, but leave the
    existing function as is, calling the new function with a default value
    for the new parameter

  - When adding entries to a common structure, add them to the end of the
    `struct` so that the other members aren't shifted around.  Consider what
    will happen if that member is not filled in by an older application.

  - Don't delete entries from a common struct; instead, just rename them
    into filler entries.

  - If there is a very good reason to break compatibility, wait until
    release 3.0 to make those changes

  - Delay the release of 3.0 until the benefits outweigh the consequences
    to the user of an incompatible library version.


Portability
===========

Please remember that people will be running gphoto2 on a wide variety
of operating systems and CPUs and will be compiling it on a wide
variety of C compilers.  As you write your code, be sure not to make
any assumptions in your code that aren't in the ANSI C89 standard.
If your code absolutely needs some feature or header file that isn't
available everywhere, write an autoconf test so that the configure
script will detect if that feature is available at compile time and
provide an alternative for those compilers that don't support it.

There are lots of subtle portability issues that you should keep in
the back of your mind.  Following are some of the major ones that affect
gphoto2.  See the paper "Notes on Writing Portable Programs in C" at
<URL:http://www.literateprogramming.com/portableC.pdf> or "Writing
Portable C with GNU Autotools" at
<URL:http://sources.redhat.com/autobook/autobook/autobook_110.html#SEC110>
for more details.


* A char can be signed or unsigned.

  Use `signed char` or `unsigned char`, or `int8_t` or `uint8_t` from
  `stdint.h`, to be sure to get the type you want when it is
  important.


* A pointer is not necessarily an `int`.

  Don't cast a pointer to an `int` and vice-versa.


* An `int` can be almost any width.

  Don't assume that it's 32 or 16 bits or any other value.  Instead,
  if you need a variable of a certain size, include the header file
  `stdint.h` (or `gphoto2-endian.h`) and use the C99-style fixed-width
  types declared therein.  If you don't really care about the size of a
  variable (e.g. as the index variable in a small for loop), you can
  still use an `int` as it's often the most efficient type for each
  processor.  It's usually the case that a char is 8 bits, `int` is at
  least 16 bits, and a long is at least 32 bits. Never assume that
  `int` or `long` (or `char`, for that matter) have a specific size, or
  that they will overflow at a particular point. Use a size-specific
  type (e.g. `uint32_t`) if necessary.


* The sequence of bytes received from a camera isn't necessarily the
  same as how those bytes are stored in a C `struct` in memory.

  Many compilers will place padding bytes between the elements of a C
  `struct` to improve run-time efficiency on the target processor.
  This means that if you read a camera packet directly into such a
  struct, the bytes will not line up and your program will use the
  wrong values.

  A similar problem occurs even if you read just a couple of bytes
  into an `int`, if gphoto is running on a processor with a different
  "endianness" than the camera.

  The most portable solution is to use the macros available in the
  `gphoto2-endian.h` header file.  When you read a packet from a
  camera (writing is similar), use a `uint8_t` array or heap space to
  store the raw packet.  Extract each member of the packet out of the
  array one at a time using a macro like `be32atoh` from
  `gphoto2-endian.h`.  Those macros take care of both those problems
  at once.

  The macros have the form `AANN[a]toh` or `htoAANN[a]`, where `AA` is
  `le` (little-endian) or `be` (big-endian), `NN` is `16` or `32`
  (bits in the word) and `a`, if present, means that the preceding
  type is located in a byte array, not an integer. `h` refers to
  `host`, and could be big or little-endian depending on the current
  CPU.  Upper-case versions of these macros (where appropriate) do the
  conversion in place.  Do a `man ntohl` to find out more about why
  these are needed, and the generated `gphoto2-endian.h` file for more
  descriptions.


* Your code won't necessarily be compiled with gcc.

  gcc is a great C compiler, but it isn't installed on everybody's
  systems (yet!).  Avoid use of proprietary gcc language extensions
  and features that aren't available in ANSI C89 compilers.  Sure,
  there's probably some code that would be more elegant using a gcc
  language extension, but somebody, somewhere will be denied the use
  of the best digital camera application in existence because of it.

  This also applies to new standard C features that appeared in the
  C99 specification of the language.  There is not much support yet in
  the installed base of C compilers to allow unrestricted use, but
  this will change as time goes by.  In the meantime, use `autoconf`
  to detect if the feature is available at run-time and act
  appropriately.

  Although one-line comments starting with `//` have been available in
  most compilers for several years, they were only officially added to
  the ANSI C99 spec and some compilers out there still don't support
  them.  The `inline` keyword also falls into the same category, but
  configure tests for this feature at compile time and supplies the
  appropriate inline keyword as a macro.  Finally, don't add
  compiler-specific flags to make files directly, as many of them are
  specific to one compiler and will cause the build to fail when using
  another.
