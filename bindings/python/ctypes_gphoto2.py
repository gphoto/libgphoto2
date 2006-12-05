#!/usr/bin/python
########################################################################
# ctypes_gphoto2.py - demonstrate libgphoto2 bindings using ctypes
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


########################################################################

import ctypes
import ctypes.util


########################################################################

def __char_p_p_to_string_list(value):
    """Turn char** into ["foo", "bar", "bla"]"""
    array_of_c_char_p = ctypes.c_char_p * 200 # FIXME: OUCH!
    blubb = array_of_c_char_p.from_address(value)
    retval = []
    for x in blubb:
        if not x:
            break
        retval.append(x)
    return retval
        

########################################################################

__gpp = ctypes.cdll.LoadLibrary("libgphoto2_port.so")
assert(__gpp)
__gp = ctypes.cdll.LoadLibrary("libgphoto2.so")
assert(__gp)


########################################################################

# __all__ contains all python symbols to be exported.
#
# You may append more symbols later in this file.
#
# An automatic test case may compare this __all__ to the
# libgphoto2.sym and libgphoto2_port.sym files.
__all__ = [
    "gp_library_version",
    "gp_port_library_version",
    "GPVersionVerbosity",
    "GP_VERSION_SHORT",
    "GP_VERSION_VERBOSE",
    ]


########################################################################

# enum GPVersionVerbosity { GP_VERSION_SHORT, GP_VERSION_VERBOSE }
class GPVersionVerbosity(ctypes.c_int):
    pass
GP_VERSION_SHORT = GPVersionVerbosity(0)
GP_VERSION_VERBOSE = GPVersionVerbosity(1)

# char **gp_library_version(GPVersionVerbosity verbose);
gp_library_version = __gp.gp_library_version
gp_library_version.argtypes = ( GPVersionVerbosity, )
gp_library_version.restype = __char_p_p_to_string_list
        
# char **gp_port_library_version(GPVersionVerbosity verbose);
gp_port_library_version = __gpp.gp_port_library_version
gp_port_library_version.argtypes = ( ctypes.c_int, )
gp_port_library_version.restype = __char_p_p_to_string_list


########################################################################
