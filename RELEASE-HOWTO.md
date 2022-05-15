# How to prepare a release

## update the locally maintained translation(s)

   Run `make update-po`, then find which `**/po/*.po` files are
   maintained locally (see the `MAINTAINERS` file), then edit and
   commit those.

## sync translations from translationproject

   Run `make pull-translations` or manually run in the toplevel
   directory (of which `po/` is a subdirectory)

    rsync -Lrtvz  translationproject.org::tp/latest/libgphoto2/  po
    rsync -Lrtvz  translationproject.org::tp/latest/libgphoto2_port/  libgphoto2_port/po

   If gphoto2 is released too:

    rsync -Lrtvz  translationproject.org::tp/latest/gphoto2/  po

   For each updated '**/po/*.po' file, use

    git commit LANG.po --author="Last translator from the po file"

   to make it clear who did the change.

## sync music-players.h from libmtp

   We ship the `music-players.h` file from libmtp to give additional
   device support for distributions that might update libmtp less
   frequently.

   Copy over from libmtp and commit.

## test cameras

   Run: `make check`

   In the gphoto2 checkout I am running `perl tests/testcamera.pl` for
   several cameras that might have been affected by the current
   release.

## update libtool library version(s), hopefully keeping the soname(s)

   Ensure the libtool library version (from which the soname is derived)
   have been updated relative to the last release.

  * LIBGPHOTO2_{AGE,REVISION,CURRENT} in configure.ac and
  * LIBGPHOTO2_PORT_{AGE,REVISION,CURRENT} in libgphoto2_port/configure.ac

   If no functions have been added since the last release,
   incrementing *_REVISION will do.

   If functions have been added since the last release, update
   _AGE and _CHANGES and reset _REVISION to 0.

   If functions have been removed, a lot of other work results besides
   updating the libtool library version.

## update NEWS

    git diff libgphoto2-2\_5\_21-release.. | less

   summarize changes

## tag release

   bump version in `configure.ac`, `NEWS`, commit and push.

    make clean
    make
    sudo make install

## build and sign release ŧarballs

    make distcheck

   this builds `.gz`, `.bz2` and `.xz` tarballs currently.

   GPG detach sign:

    gpg --detach-sign -a libgphoto2-2.5.27.tar.bz2
    gpg --detach-sign -a libgphoto2-2.5.27.tar.gz
    gpg --detach-sign -a libgphoto2-2.5.27.tar.xz

   this will create `.asc` files alongside the tarballs.

   Similar when doing the gphoto2 release.

## tag git

   Do old style and new style GIT tagging (new style for github largely)

    git tag libgphoto2-2\_5\_27-release
    git tag v2.5.27
    git push --tags

## create a README for sourceforge

   Paste the `NEWS` blob from this release into `README` for uploading.

## SF upload

   Create a new folder below `libgphoto2` with the release number in it (Add Folder).

   Inside that "Add File" of the tarballs, the `.asc` files and the `README`.

   After upload, select "Info" on the `.tar.bz2` tarball, and "Select All" for the Default download.
   This will make it point the "default download" link to this tarball.
 
   If you are uploading gphoto2, do the same ... but do not select it for the default download.

## Github tagging

   https://github.com/gphoto/libgphoto2/releases

   "Draft a new release"

   Select the new release tag. Name is "2.5.27 release",
   paste in the `NEWS` blob into the textfield.

## Edit the website

### index.php

   Use a entry similar to the current ones. Drop older ones occasionally.

### news/index.php

   Paste in the `NEWS` blob, htmlify with h2 - h3 and ul/li lists, same as the ones before

### proj/libgphoto2/support.php

   Adjust the date (`env LANG=C date`).

   Adjust the `gphoto2 --version` blob (`env LANG=C gphoto2 --version`). Make sure you have the correct versions in there.

   Replace the table with fresh output of:

    /usr/lib64/libgphoto2/print-camera-list html

### Upload via sftp

   see `upload.sh` ... html tree starts below `htdocs/` on the server.

   Do not forget to "svn ci" the changes.

### Verify website.

   Check the pages render OK and no HTML error was introduced.

## Announce to mailing list

   Write a summary email to `gphoto-user@lists.sourceforge.net`, `gphoto-devel@lists.sourceforge.net`.

   Add download links to either SF or GITHUB.

## Announce to freshcode.club

   https://freshcode.club/, look for libgphoto2

   Fill out the "New release" form. It is pretty straight forward. As text use the NEWS blob again.

   Same for gphoto2.

## Announce to twitter / facebook / etc.

   e.g.:

   I have just released libgphoto2 2.5.22 ( http://gphoto.org/news/  )
   ... Fixed Canon EOS M and PowerShot SX capture, added Sony Alpha RX0
   and RX100M5A, added Canon EOS R, and some bugfixes.

## change IRC topic of #gphoto in libera.chat

   /msg Chanserv topic #gphoto Digital cameras for Linux/UNIX/MacOS | http://gphoto.org/ | just ask | no webcams | newest 2.5.27

## Announce to translation project

   See https://translationproject.org/html/maintainers.html

   Write an email to coordinator@translationproject.org

   Subject: libgphoto2-2.5.28.pot


   	Hi,
	
	libgphoto2 and libgphoto2_port translations domains are updated to 2.5.28.

	Download location: https://sourceforge.net/projects/gphoto/files/libgphoto/2.5.28/libgphoto2-2.5.28.tar.bz2/download


	Also gphoto2 was updated to 2.5.28:
	https://sourceforge.net/projects/gphoto/files/gphoto/2.5.28/gphoto2-2.5.28.tar.bz2/download

	Ciao, Marcus

	Ciao, Marcus
   

## Prepare git repo for further development

   Rev `NEWS` and `configure.ac` versions to 2.5.27.1 devel release.
