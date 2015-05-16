dir=`mktemp -d /tmp/camera.XXXXXX`
cd $dir
echo -n "Attach Nikon D90 and press return: "
read dummy 
set -x -v
echo "***  basics"
gphoto2 -L
gphoto2 -l
gphoto2 --summary
echo "***  single capture"
gphoto2 --set-config capturetarget=0
gphoto2 --set-config imagequality=2 --capture-image-and-download
rm capt0000.jpg
gphoto2 --set-config imagequality=6 --capture-image-and-download
rm capt0000.jpg capt0000.nef
gphoto2 --set-config imagequality=6 --capture-image-and-download --keep-raw
rm capt0000.jpg
gphoto2 --set-config capturetarget=1
gphoto2 --set-config imagequality=2 --capture-image-and-download
gphoto2 --set-config imagequality=6 --capture-image-and-download
gphoto2 --set-config imagequality=6 --capture-image-and-download --keep-raw
echo -n "remove SDCARD and press return: " 
read dummy
gphoto2 --set-config capturetarget=0
gphoto2 --set-config imagequality=2 --capture-image-and-download
rm capt0000.jpg
gphoto2 --set-config imagequality=6 --capture-image-and-download
rm capt0000.jpg capt0000.nef
echo -n "replugin SDCARD and press return: " 
read dummy

echo "***  trigger capture"
gphoto2 --set-config capturetarget=0
gphoto2 --set-config imagequality=2 --trigger-capture --wait-event-and-download=CAPTURECOMPLETE
echo FIXME: gphoto2 --set-config imagequality=2 --trigger-capture --trigger-capture --trigger-capture --wait-event-and-download=30s
rm capt*jpg
gphoto2 --set-config imagequality=6 --trigger-capture --wait-event-and-download=CAPTURECOMPLETE 
echo FIXME: gphoto2 --set-config imagequality=6 --trigger-capture --trigger-capture --trigger-capture --wait-event-and-download=30s
rm capt*
gphoto2 --set-config capturetarget=1
gphoto2 --set-config imagequality=2 --trigger-capture --wait-event-and-download=20s
gphoto2 --set-config imagequality=2 --trigger-capture --trigger-capture --trigger-capture --wait-event-and-download=30s
gphoto2 --set-config imagequality=6 --trigger-capture --wait-event-and-download=20s
gphoto2 --set-config imagequality=6 --trigger-capture --trigger-capture --trigger-capture --wait-event-and-download=30s

echo -n "trigger capture done ... press return to continue"
read dummy

echo "***  interval capture"
gphoto2 --set-config capturetarget=0
gphoto2 --set-config imagequality=2 --capture-image-and-download -F 3 -I 5
rm capt000*.*
gphoto2 --set-config imagequality=6 --capture-image-and-download -F 3 -I 5
rm capt000*.*
gphoto2 --set-config capturetarget=1
gphoto2 --set-config imagequality=2 --capture-image-and-download -F 3 -I 5
gphoto2 --set-config imagequality=6 --capture-image-and-download -F 3 -I 5
gphoto2 --set-config imagequality=2

echo -n "interval capture done ... press return to continue"
read dummy

echo "***  fast interval capture"
gphoto2 --set-config capturetarget=0
gphoto2 --set-config imagequality=2 --capture-image-and-download -F 3 -I 1
rm capt000*.*
gphoto2 --set-config imagequality=6 --capture-image-and-download -F 3 -I 1
rm capt000*.*
gphoto2 --set-config capturetarget=1
gphoto2 --set-config imagequality=2 --capture-image-and-download -F 3 -I 1
gphoto2 --set-config imagequality=6 --capture-image-and-download -F 3 -I 1
gphoto2 --set-config imagequality=2

echo -n "fast interval capture done ... press return to continue"
read dummy

echo "***  timing capture"
gphoto2 --set-config capturetarget=0
rm capt000*.*
time gphoto2 --set-config imagequality=2 --capture-image-and-download
rm capt0000.jpg
gphoto2 --set-config capturetarget=1
time gphoto2 --set-config imagequality=2 --capture-image-and-download

echo -n "timing capture done ... press return to continue"

echo "*** testing preview"
rm capture_preview.jpg || true
gphoto2 --capture-preview
gwenview capture_preview.jpg
rm capture_preview.jpg || true
gphoto2 --set-config capturetarget=0
gphoto2 --capture-preview --capture-image-and-download --capture-preview --capture-image-and-download
gwenview capture_preview.jpg
rm capt*.*
gphoto2 --set-config capturetarget=1
gphoto2 --capture-preview --capture-image-and-download --capture-preview --capture-image-and-download
gwenview capture_preview.jpg
rm capt*.*

rm movie.mjpg || true
gphoto2 --capture-movie=10s
mplayer movie.mjpg
rm movie.mjpg

gphoto2 --set-config movie=1 --wait-event=10s --set-config movie=0 --wait-event-and-download=10s
mplayer *.MOV
rm *.MOV

echo "*** capture and wait_event  - jpg/sdram - 10s"
gphoto2 --set-config capturetarget=0
gphoto2 --set-config imagequality=2
gphoto2 --wait-event-and-download=10s
gphoto2 --set-config imagequality=6
gphoto2 --wait-event-and-download=10s
gphoto2 --wait-event-and-download=10s --keep-raw
echo "*** capture and wait_event  - jpg/card - 50 events"
gphoto2 --set-config capturetarget=1
gphoto2 --set-config imagequality=2
gphoto2 --wait-event-and-download=50
gphoto2 --set-config imagequality=6
gphoto2 --wait-event-and-download=50
gphoto2 --wait-event-and-download=50 --keep-raw
gphoto2 --set-config imagequality=2

echo "*** config "
gphoto2 --list-config
gphoto2 --list-all-config

gphoto2 --set-config imagecomment="Markus Meissner"
gphoto2 --get-config imagecomment|grep Markus.Meissner
gphoto2 --set-config imagecomment="Marcus Meissner"
gphoto2 --get-config imagecomment|grep Marcus.Meissner
gphoto2 --get-config d090|grep Marcus.Meissner
gphoto2 --set-config d090="Markus Meissner"
gphoto2 --get-config d090|grep Markus.Meissner
gphoto2 --set-config d090="Marcus Meissner"
gphoto2 --get-config d090|grep Marcus.Meissner

echo "*** needs to be in A or M mode"
gphoto2 --get-config f-number
gphoto2 --set-config f-number=f/5.6

gphoto2 --get-config datetime
date
gphoto2 --set-config datetime=now
gphoto2 --get-config datetime

gphoto2 --get-config imagecommentenable
gphoto2 --set-config imagecommentenable=0
gphoto2 --get-config d091|grep 1
gphoto2 --set-config d091=0
gphoto2 --get-config d091|grep 0
gphoto2 --set-config d091=1

echo "*** DONE"
echo rm -rf $dir
