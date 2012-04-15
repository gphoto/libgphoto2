dir=`mktemp -d /tmp/camera.XXXXXX`
cd $dir
echo -n "Attach Canon EOS 1000D and press return: "
read dummy 
set -x -v
echo "***  single capture"
gphoto2 --set-config capturetarget=0
gphoto2 --set-config imageformat=0 --capture-image-and-download
rm capt0000.jpg
gphoto2 --set-config imageformat=6 --capture-image-and-download
rm capt0000.jpg capt0000.cr2
gphoto2 --set-config capturetarget=1
gphoto2 --set-config imageformat=0 --capture-image-and-download
gphoto2 --set-config imageformat=6 --capture-image-and-download

echo "***  interval capture"
gphoto2 --set-config capturetarget=0
gphoto2 --set-config imageformat=0 --capture-image-and-download -F 3 -I 5
rm capt000*.*
gphoto2 --set-config imageformat=6 --capture-image-and-download -F 3 -I 5
rm capt000*.*
gphoto2 --set-config capturetarget=1
gphoto2 --set-config imageformat=0 --capture-image-and-download -F 3 -I 5
gphoto2 --set-config imageformat=6 --capture-image-and-download -F 3 -I 5
gphoto2 --set-config imageformat=0

echo "***  timing capture"
gphoto2 --set-config capturetarget=0
rm capt000*.*
time gphoto2 --set-config imageformat=0 --capture-image-and-download
rm capt0000.jpg
gphoto2 --set-config capturetarget=1
time gphoto2 --set-config imageformat=0 --capture-image-and-download

echo "*** testing preview"
rm preview.jpg || true
gphoto2 --capture-preview
gwenview preview.jpg
rm preview.jpg || true
gphoto2 --set-config capturetarget=0
gphoto2 --capture-preview --capture-image-and-download --capture-preview --capture-image-and-download
rm capt*.*
gwenview preview.jpg
gphoto2 --set-config capturetarget=1
gphoto2 --capture-preview --capture-image-and-download --capture-preview --capture-image-and-download
gwenview preview.jpg
rm preview.jpg

rm movie.mjpg || true
gphoto2 --capture-movie=10s
rm movie.mjpg

echo "*** capture and wait_event  - jpg/sdram - 10s"
gphoto2 --set-config capturetarget=0
gphoto2 --set-config imageformat=0
gphoto2 --wait-event-and-download=10s
gphoto2 --set-config imageformat=6
gphoto2 --wait-event-and-download=10s
echo "*** capture and wait_event  - jpg/card - 50 events"
gphoto2 --set-config capturetarget=1
gphoto2 --set-config imageformat=0
gphoto2 --wait-event-and-download=100
gphoto2 --set-config imageformat=6
gphoto2 --wait-event-and-download=100

echo "*** config "
gphoto2 --list-config
gphoto2 --list-all-config

gphoto2 --set-config ownername="Markus Meissner"
gphoto2 --get-config ownername|grep Markus.Meissner
gphoto2 --set-config ownername="Marcus Meissner"
gphoto2 --get-config ownername|grep Marcus.Meissner

echo "*** DONE"
echo rm -rf $dir
