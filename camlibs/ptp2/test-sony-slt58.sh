#!/bin/sh -x
dir=`mktemp -d /tmp/camera.XXXXXX`
cd $dir
echo -n "Attach Sony SLT and press return: "
read dummy 
set -x -v
echo "***  standard actions"
gphoto2 -l
gphoto2 -L
gphoto2 --summary
echo "***  single capture"
gphoto2 --set-config imagequality=0 --capture-image-and-download
rm capt0000.jpg
gphoto2 --set-config imagequality=2 --capture-image-and-download
rm capt0000.jpg capt0000.arw

echo "single capture done, press return"
read dummy

echo "***  interval capture"
gphoto2 --set-config imagequality=0 --capture-image-and-download -F 3 -I 5
rm capt000*.*
gphoto2 --set-config imagequality=2 --capture-image-and-download -F 3 -I 5
rm capt000*.*

echo "***  timing capture"
rm capt000*.*
time gphoto2 --set-config imagequality=0 --capture-image-and-download
rm capt0000.jpg

echo "*** trigger movie"
gphoto2 --set-config movie=1 --wait-event=10s --set-config movie=0

echo "*** capture and wait_event  - jpg/sdram - 10s"
gphoto2 --set-config imagequality=0
gphoto2 --wait-event-and-download=10s
gphoto2 --set-config imagequality=2
gphoto2 --wait-event-and-download=10s

echo "*** config "
gphoto2 --list-config
gphoto2 --list-all-config

echo "*** DONE"
echo rm -rf $dir
