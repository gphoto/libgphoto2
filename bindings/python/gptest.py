# Copyright (C) 2003-2004 David PHAM-VAN -- david@ab2r.com
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

import gphoto2

print "GPhoto version "+gphoto2.version()

#print dir(gphoto2)

print("Creating camera...")
cam=gphoto2.camera()

# ports=gphoto2.ports()
# ports.load()
# print ports.count(), " ports trouves"
# for port in range(0,ports.count()):
#   print ports[port]
#
#
# print("Setting model...")
# abl=gphoto2.abilities_list()
# abl.load()
#
# print("Detection des cameras...")
# dt=abl.detect(ports)
# print dt.count()
# for i in range(0,dt.count()):
#   print dt[i]
#
# mdl=abl.lookup_model(dt[0][0])
# ab=abl[mdl]
#
# print ab
#
# cam.set_abilities(ab)

print("Initializing camera...")
cam.init()
list = cam.list_folders_in_folder('/')
for i in range(0,list.count()):
  print list[i]
print cam.summary
print cam.abilities
print cam.capture_image()
