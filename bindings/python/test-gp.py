#!/usr/bin/python

import sys
import os
from pprint import pprint
import unittest
import modulefinder


class TestRunner(unittest.TestCase):

    def setUp(self):
        # FIXME: Find gphoto2.so file
        sys.path.append(os.path.abspath('test-install/usr/lib/python2.4/site-packages'))
        import gphoto2

    def test_000_version_short(self):
        import gphoto2
        #print "gphoto2 version", gphoto2.version()

    def test_010_ports(self):
        import gphoto2
        self.ports = gphoto2.ports()
        self.ports.load()
        print self.ports.count(), " ports found:"
        for port in range(0, self.ports.count()):
            print self.ports[port]

    def xtest_020_cameras(self):
        import gphoto2
        self.a_l = gphoto2.abilities_list()
        self.a_l.load()
        print self.a_l.count(), " cameras found:"
        for i in range(0, self.a_l.count()):
            print self.a_l[i]

    def test_030_camera(self):
        import gphoto2
        print("Creating camera...")
        self.cam = gphoto2.camera()

    def test_007_long(self):
        import gphoto2
        pprint(gphoto2.library_version(False))
        pprint(gphoto2.library_version(True))


if __name__ == '__main__':
    unittest.main()
