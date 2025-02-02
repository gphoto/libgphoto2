# libgphoto2

Hello and welcome to the wonderful world of gphoto! This is libgphoto2, the
successor of gphoto with lots of new features and additional camera
drivers.

If you miss a feature, would like to report success or failure, or have any
questions, please don't hesitate to contact our mailing list.


## What is libgphoto2?

libgphoto2 is a library that can be used by applications to access various
digital cameras.  

For more information on gphoto, see [gphoto project home page].

There, you can also get information on mailing lists, supported cameras,
and availability of gphoto2. Another source of information is [gphoto github project page].

where you can access our SVN server to fetch the source code of
gphoto2, gtkam and GnoCam (see below).


## What is libgphoto2 not?

libgphoto2 itself is not a GUI application, opposed to gphoto. There are
GUI frontends for the gphoto2 library, however, such as gtkam for
example.

libgphoto2 can only talk to cameras the language of those it understands.
That is, if you own a camera that speaks a language that isn't published
anywhere and nobody has been able to figure out the meaning of the sentences,
libgphoto2 cannot communicate with those cameras.

Then, there are cameras supporting the so-called USB Mass Storage protocol.
This is a protocol that has been published and lets you access any storage
device, be it a camera or a disk connected via USB to your computer. As there
are already drivers for this protocol out there, you don't need an additional
library like libgphoto2. The list of camera that use USB Mass Storage is getting
longer everyday, so we won't publish it.

For a more up to date list, you can consult [camera list with support status].

Your operating system will tell you about that because it is likely to recognize
the device as a SCSI disk that you can mount (for Linux 'dmesg' will tell you).
Again, those cameras *cannot* be accessed through libgphoto2. Some of them can
be switched to use a different communication protocol and might be in that case
usable with libgphoto2.

Other camera support a protocol called PTP or USB Imaging Devices that has
been developed by Kodak and other. libgphoto2 does support PTP. Also working
will be cameras labeled as "PictBridge", which is a extension to PTP.

Almost all cameras that are not mass storage support it these days, including
but not limited to all Nikon, Canon, Fuji, Sony, Panasonic, HP and more.

PTP camera unknown to libgphoto2 will be detected as a generic PTP camera and
will work as-is without any changes.

MTP (Microsoft Transfer Protocol) capable devices will also be accessible,
since MTP is based on PTP.


## Platforms

libgphoto2 should compile and run on pretty much all Unix-like platforms.
libgphoto2 has _not_ (yet?) been ported to any operating system from MicroSoft.


## Bindings

- Java: [gphoto2-java](https://github.com/mvysny/gphoto2-java), [libgphoto2-jna](https://github.com/angryelectron/libgphoto2-jna)
- Python: [python-gphoto2](https://github.com/jim-easterbrook/python-gphoto2), [gphoto2-cffi](https://github.com/jbaiter/gphoto2-cffi)
- C#: [libgphoto2-sharp](https://github.com/gphoto/libgphoto2-sharp)
- Go: [gphoto2-go](https://github.com/weebney/gphoto2-go)
- Rust: [gphoto-rs](https://github.com/dcuddeback/gphoto-rs)
- Node.js: [node-gphoto2](https://github.com/lwille/node-gphoto2)
- Ruby: [ffi-gphoto2](https://github.com/zaeleus/ffi-gphoto2)
- Crystal: [gphoto2.cr](https://github.com/sija/gphoto2.cr)


## How do I build it?

If you want to have 'libgphoto2' installed  into `$HOME/.local`, keep the `PKG_CONFIG_PATH=` and `--prefix=` arguments to `configure`. Otherwise adapt or remove them.

```
autoreconf -is  # if using a git clone
./configure PKG_CONFIG_PATH="$HOME/.local/lib/pkgconfig${PKG_CONFIG_PATH+":${PKG_CONFIG_PATH}"}" --prefix="$HOME/.local"
make
make install
```

Out-of-tree builds are supported. `./configure --help` may help.

### using Meson build system

Since [f5bb6c875c143c53bc8ba725e9d34726b31c47af](https://github.com/gphoto/libgphoto2/commit/f5bb6c875c143c53bc8ba725e9d34726b31c47af), you can use meson to build the library :

```
cd /path/to/source/root
meson setup builddir && cd builddir
meson compile
meson test
```

### Where to get the sources

The sources are available at the following places:

 - At gphoto's github repository (this page) : https://github.com/gphoto/libgphoto2
 - At the gphoto website: [gphoto project home page]
 - In the SVN module gphoto2-manual

The gphoto2 Manual includes information about setting up USB
hotplugging.

If you run into problems, you may also consult the FAQ (also included
in The gphoto2 Manual).


## Frontends

gphoto2 is a command line frontend which is quite powerful,
especially in combination with scripts. See The gphoto2 Manual
for a short description.

For the GUI lovers, there are for example digikam (KDE), gthumb (GNOME),
f-spot (GNOME / Mono) and more. We also have a GTK2 reference GUI
called gtkam, which is unmaintained, its only special features are
capture ability.
Additionally, there are plugins for other programs available like
kio_camera (KDE - Konqueror) and a fuse plugin, gphotofs.


## Reporting Bugs

Every piece of software contains errors and flaws. So does
libgphoto2. When you encounter something that does not work, please do
the following:

1. Find out whether this is a known problem.

2. Reproduce the problem with debug output enabled and the language
    set to English, so that the development team will understand the
    messages. You can do this by running:

        env LC_ALL=C gtkam

    if you're using the gtkam frontend or running:

        env LC_ALL=C gphoto2 --debug-logfile=logfile.log --debug <options>

    using the gphoto2 command line interface.

3. Report the problem on the mailing list with the corresponding debug
    output if it is small. If it is more than a few kilobytes, please
    don't post the complete debug output on the list.

## Links

* [gphoto project home page](http://www.gphoto.org/)
* [gphoto github project page](https://github.com/gphoto)
* [gphoto camera list](http://gphoto.org/proj/libgphoto2/support.php)
* [gphoto camera remote control list and doc](http://gphoto.org/doc/remote/)
* [jphoto home page](http://jphoto.sourceforge.net/)
* [Information about using USB mass storage](http://www.linux-usb.org/USB-guide/x498.html)
* [gphoto development mailing list](mailto:gphoto-devel@lists.sourceforge.net)
* [gphoto user mailing list](mailto:gphoto-user@lists.sourceforge.net)
* [gphoto translation mailing list](mailto:gphoto-translation@lists.sourceforge.net)
