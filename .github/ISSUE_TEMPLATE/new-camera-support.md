---
name: New camera support
about: Support a new camera
title: ''
labels: ''
assignees: ''

---

Fill in fields below. If you don't know how, leave it free.

**Name of the camera*

Use the Marketing name.

**USB IDs**
e.g. by running:   lsusb 

**camera summary output**
run

  gphoto2 --summary > summary.txt

attach summary.txt here

**camera configuration output**

gphoto2 --list-all-config > list-all-config.txt

and attach list-all-config.txt here

**test capture**

test if capture perhaps already works:

  gphoto2 --capture-image-and-download
 
  gphoto2 --capture-preview
