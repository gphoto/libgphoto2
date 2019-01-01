# How to prepare a release

## sync translations from translationproject

   In the toplevel directory (of which po/ is a subdirectory)

   rsync -Lrtvz  translationproject.org::tp/latest/libgphoto2/  po
   cd libgphoto2\_port
   rsync -Lrtvz  translationproject.org::tp/latest/libgphoto2\_port/  po
   cd ..

   if gphoto2 is released too: 
   rsync -Lrtvz  translationproject.org::tp/latest/gphoto2/  po

## sync music-players.h from libmtp

   (We ship this to give additional device support for distributions
    that might update libmtp less often.)

   Copy over, disable the GoPro entries.

## test cameras

   Run: make check

   In the gphoto2 checkout I am running <code>perl tests/testcamera.pl</code> 
   for several cameras that might have been affected by the current release.

## update NEWS

   git diff libgphoto2-2\_5\_21-release.. | less

   summarize changes

## tag release

   bump version in configure.ac, NEWS, commit and push.

   make clean
   make
   sudo make install

## build and sign release ŧarballs

   make distcheck

   this builds .gz and .bz2 tarballs currently.

   GPG detach sign

   gpg --detach-sign -a libgphoto2-2.5.22.tar.bz2
   gpg --detach-sign -a libgphoto2-2.5.22.tar.gz

   this will create .asc files alongside the tarballs.

   Similar when doing the gphoto2 release.

## tag git

   git tag libgphoto2-2\_5\_22-release
   git push --tags

## create a README for sourceforge

   paste the NEWS blob from this release into README for uploading

## SF upload

   Create a new folder below libgphoto2 with the release number in it (Add Folder)

   Inside that "Add File" of the two tarballs, the two .asc files and the README.

   After upload, select "Info" on the .tar.bz2 tarball, and "Select All" for the Default download.
   This will make it point the "default download" link to this tarball.
 
   If you are uploading gphoto2, do the same ... but do not select it for the default download.

## Github tagging

   https://github.com/gphoto/libgphoto2/releases

   "Draft a new release"

   Select the new release tag. Name is "2.5.22 release",
   paste in the NEWS blob into the textfield.

## Edit the website

### index.php

   Use a entry similar to the current ones. Drop older ones occasionaly.

### news/index.php

   Paste in the NEWS blob, htmlify with h2 - h3 and ul/li lists, same as the ones before

### proj/libgphoto2/support.php

   Adjust the date (LANG=C date).

   Adjust the gphoto2 --version blob (LANG=C gphoto2 --version). Make sure you have the correct versions in there.

   Replace the table with fresh output of:

	/usr/lib64/libgphoto2/print-camera-list html

### Upload via sftp

   see upload.sh ... html tree starts below htdocs/ on the server.

   Do not forget to "svn ci" the changes.

### Verify website.

   Check the pages render OK and no HTML error was introduced.

## Announce to mailing list

   Write a summary email to gphoto-user@lists.sourceforge.net, gphoto-devel@lists.sourceforge.net

## Announce to freshcode.club

   https://freshcode.club/, look for libgphoto2

   Fill out the "New release" form. It is pretty straight forward. As text use the NEWS blob again.

## Announce to twitter / facebook / etc.

   e.g.:

   I have just released libgphoto2 2.5.22 ( http://gphoto.org/news/  )
   ... Fixed Canon EOS M and PowerShot SX capture, added Sony Alpha RX0
   and RX100M5A, added Canon EOS R, and some bugfixes.
 
## Make git ready for further development

   Rev NEWS and configure.ac versions to 2.5.22.1 devel release.
