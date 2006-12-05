#!/usr/bin/python
########################################################################
# test_ctypes_gphoto2.py - test ctypes demonstration libgphoto2 bindings
# Copyright (C) 2006 Hans Ulrich Niedermann <gp@n-dimensional.de>
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
########################################################################

import sys


########################################################################

from ctypes_gphoto2 import *


########################################################################

def main(args, progname=None):
    for f in [
        gp_library_version(GP_VERSION_SHORT),
        gp_port_library_version(GP_VERSION_SHORT),
        gp_library_version(GP_VERSION_VERBOSE),
        gp_port_library_version(GP_VERSION_VERBOSE),
        ]:
        print f


########################################################################

if __name__ == '__main__':
    main(sys.argv[1:], progname=sys.argv[0])


########################################################################
