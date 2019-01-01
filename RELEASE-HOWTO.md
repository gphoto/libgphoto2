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

   bump version in configure.ac, NEWS

   make clean
   make
   sudo make install

## build relase

   make distcheck
