dir=`mktemp -d /tmp/camera.XXXXXX`
cd $dir
echo -n "Attach Canon EOS 100D and press return: "
read dummy 
set -x -v
echo "***  standard actions"
gphoto2 -l
gphoto2 -L
gphoto2 --summary
echo "***  single capture"
gphoto2 --set-config capturetarget=0
gphoto2 --set-config imageformat=0 --capture-image-and-download
rm capt0000.jpg
gphoto2 --set-config imageformat=8 --capture-image-and-download
rm capt0000.jpg capt0001.cr2
gphoto2 --set-config capturetarget=1
gphoto2 --set-config imageformat=0 --capture-image-and-download
gphoto2 --set-config imageformat=8 --capture-image-and-download
gphoto2 --set-config imageformat=8 --capture-image-and-download --keep-raw

echo "single capture done, press return"
read dummy

echo "***  trigger capture"
gphoto2 --set-config capturetarget=0
gphoto2 --set-config imageformat=0 --trigger-capture --wait-event-and-download=5s
gphoto2 --set-config imageformat=0 --trigger-capture --trigger-capture --trigger-capture --wait-event-and-download=10s
rm capt0000.jpg
gphoto2 --set-config imageformat=8 --trigger-capture --wait-event-and-download=5s
gphoto2 --set-config imageformat=8 --trigger-capture --trigger-capture --trigger-capture --wait-event-and-download=10s
rm capt0000.jpg capt0001.cr2
gphoto2 --set-config capturetarget=1
gphoto2 --set-config imageformat=0 --trigger-capture --wait-event-and-download=5s
gphoto2 --set-config imageformat=0 --trigger-capture --trigger-capture --trigger-capture --wait-event-and-download=10s
gphoto2 --set-config imageformat=8 --trigger-capture --wait-event-and-download=5s
gphoto2 --set-config imageformat=8 --trigger-capture --trigger-capture --trigger-capture --wait-event-and-download=10s

echo "trigger capture done, press return"
read dummy

echo "***  interval capture"
gphoto2 --set-config capturetarget=0
gphoto2 --set-config imageformat=0 --capture-image-and-download -F 3 -I 5
rm capt000*.*
gphoto2 --set-config imageformat=8 --capture-image-and-download -F 3 -I 5
rm capt000*.*
gphoto2 --set-config capturetarget=1
gphoto2 --set-config imageformat=0 --capture-image-and-download -F 3 -I 5
gphoto2 --set-config imageformat=8 --capture-image-and-download -F 3 -I 5
gphoto2 --set-config imageformat=0

echo "***  timing capture"
gphoto2 --set-config capturetarget=0
rm capt000*.*
time gphoto2 --set-config imageformat=0 --capture-image-and-download
rm capt0000.jpg
gphoto2 --set-config capturetarget=1
time gphoto2 --set-config imageformat=0 --capture-image-and-download

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

echo "*** capture and wait_event  - jpg/sdram - 10s"
gphoto2 --set-config capturetarget=0
gphoto2 --set-config imageformat=0
gphoto2 --wait-event-and-download=10s
gphoto2 --set-config imageformat=8
gphoto2 --wait-event-and-download=10s
echo "*** capture and wait_event  - jpg/card - 50 events"
gphoto2 --set-config capturetarget=1
gphoto2 --set-config imageformat=0
gphoto2 --wait-event-and-download=200
gphoto2 --set-config imageformat=8
gphoto2 --wait-event-and-download=200
gphoto2 --wait-event-and-download=200 --keep-raw

echo "*** config "
gphoto2 --list-config
gphoto2 --list-all-config

gphoto2 --set-config ownername="Markus Meissner"
gphoto2 --get-config ownername|grep Markus.Meissner
gphoto2 --set-config ownername="Marcus Meissner"
gphoto2 --get-config ownername|grep Marcus.Meissner

gphoto2 --get-config datetime
date
gphoto2 --set-config datetime=now
gphoto2 --get-config datetime

echo "*** DONE"
echo rm -rf $dir
