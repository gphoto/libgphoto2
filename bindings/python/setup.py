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

from distutils.core import setup
from distutils.extension import Extension
from Pyrex.Distutils import build_ext

setup(
  name = "python-gphoto2",
  ext_modules=[
    Extension(
      "gphoto2", ["gphoto2.pyx"],
      libraries = ["gphoto2"],
      include_dirs = ["/usr/include/gphoto2"]
      )
    ],
  cmdclass = {'build_ext': build_ext}
)
