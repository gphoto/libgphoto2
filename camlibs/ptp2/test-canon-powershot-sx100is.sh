dir=`mktemp -d /tmp/camera.XXXXXX`
cd $dir
echo -n "Attach Canon Powershot SX100IS and press return: "
read dummy 
set -x -v
echo "***  single capture"
gphoto2 --set-config capturetarget=0
gphoto2 --capture-image-and-download
rm capt0000.jpg
gphoto2 --set-config capturetarget=1
gphoto2 --capture-image-and-download
rm capt0000.jpg
echo -n "Remove SD Card and press Enter: "
read dummy
gphoto2 --set-config capturetarget=0
gphoto2 --capture-image-and-download
echo -n "Replugin SD Card and press Enter: "
read dummy

echo "***  interval capture"
gphoto2 --set-config capturetarget=0
gphoto2 --capture-image-and-download -F 3 -I 5
rm capt000*.*
gphoto2 --set-config capturetarget=1
gphoto2 --capture-image-and-download -F 3 -I 5
rm capt000*.*

echo "***  timing capture"
gphoto2 --set-config capturetarget=0
rm capt000*.*
time gphoto2 --capture-image-and-download
rm capt0000.jpg
gphoto2 --set-config capturetarget=1
time gphoto2 --capture-image-and-download

echo "*** testing preview"
rm capture_preview.jpg || true
gphoto2 --capture-preview
gwenview capture_preview.jpg
rm capture_preview.jpg || true
gphoto2 --set-config capturetarget=0
gphoto2 --capture-preview --capture-image-and-download --capture-preview --capture-image-and-download
rm capt0*.*
gwenview capture_preview.jpg
gphoto2 --set-config capturetarget=1
gphoto2 --capture-preview --capture-image-and-download --capture-preview --capture-image-and-download
gwenview capture_preview.jpg
rm capture_preview.jpg

rm movie.mjpg || true
gphoto2 --capture-movie=10s
rm movie.mjpg

echo "*** wait_event 10s"
gphoto2 --wait-event-and-download=10s

echo "*** config "
gphoto2 --list-config
gphoto2 --list-all-config

echo "*** capture switching"
gphoto2 --get-config capture
gphoto2 --set-config capture=1
gphoto2 --get-config capture
gphoto2 --set-config capture=0
gphoto2 --get-config capture
gphoto2 --set-config capture=1

echo "*** zoom"
gphoto2 --get-config zoom
gphoto2 --set-config zoom=20
gphoto2 --get-config zoom
gphoto2 --get-config d02a
gphoto2 --set-config d02a=10
gphoto2 --get-config d02a

echo "*** ownername"
gphoto2 --set-config ownername="Markus Meissner"
gphoto2 --get-config ownername|grep Markus.Meissner
gphoto2 --set-config ownername="Marcus Meissner"
gphoto2 --get-config ownername|grep Marcus.Meissner

echo "*** MTP property"
gphoto2 --get-config d402

echo "*** Date & Time"
gphoto2 --get-config datetime
date
gphoto2 --set-config datetime=now
gphoto2 --get-config datetime


echo "*** DONE"
echo rm -rf $dir
