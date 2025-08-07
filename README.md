# libgphoto2

Hello and welcome to the wonderful world of gphoto! This is libgphoto2, the successor of gphoto with lots of new features and additional camera drivers.

If you miss a feature, would like to report success or failure, or have any questions, please don't hesitate to contact our [mailing list](http://www.gphoto.org/mailinglists/).


## What is libgphoto2?

libgphoto2 is a library that can be used by applications to access various digital cameras. For more information on gphoto, please visit the [gphoto project website](http://www.gphoto.org/).

You can find information there about our mailing lists, a list of [supported cameras](https://gphoto.sourceforge.io/proj/libgphoto2/support.php), and the availability of gphoto2. Another source of information is [gphoto github project page](https://github.com/gphoto) where you can fetch the source code of gphoto2, libgphoto2, gtkam and other related projects. 


## What libgphoto2 is NOT

Unlike the original gphoto, libgphoto2 is not a standalone graphical user interface (GUI) application. Instead, it is a backend library. There are several GUI frontends for the gphoto2 library, such as [gtkam](http://www.gphoto.org/proj/gtkam/), a reference implementation for a graphical libgphoto2 client in GTK2.

libgphoto2 can only communicate with cameras that use protocols it understands. If a camera uses a proprietary protocol that has not been publicly documented or reverse-engineered, libgphoto2 will not be able to communicate with it.

## USB Communication Protocols

Cameras typically use one of two main protocols for communication via USB:

* [USB Mass Storage protocol](https://en.wikipedia.org/wiki/USB_mass_storage_device_class): This protocol treats the camera's internal storage (e.g., an SD card) as a standard disk drive. Your operating system will automatically recognize it as an external drive that you can mount to access files, just like a flash drive. Because there are already built-in OS drivers for this protocol, you do not need libgphoto2 for these cameras.

* [PTP (Picture Transfer Protocol)](https://en.wikipedia.org/wiki/Picture_Transfer_Protocol): Developed by Kodak and others, PTP is a dedicated protocol for transferring images. libgphoto2 fully supports PTP. Almost all modern cameras that are not USB Mass Storage devices use this protocol, including models from Nikon, Canon, Fuji, Sony, and Panasonic. Cameras labeled as "PictBridge" also work, as this is an extension of PTP. If a PTP camera is unknown to libgphoto2, it will be detected as a generic PTP camera and will typically still work without any special configuration.

* [MTP (Media Transfer Protocol)](https://en.wikipedia.org/wiki/Media_Transfer_Protocol): MTP is a Microsoft-developed extension of PTP. MTP-capable devices are also accessible through libgphoto2.

For an up-to-date list of supported cameras and their protocol status, please consult the
[camera list with support status](https://gphoto.sourceforge.io/proj/libgphoto2/support.php).


## Platforms

libgphoto2 is designed to compile and run on most Unix-like platforms. Support for operating systems from Microsoft is not currently available.

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

Since [f5bb6c875c143c53bc8ba725e9d34726b31c47af](https://github.com/gphoto/libgphoto2/commit/f5bb6c875c143c53bc8ba725e9d34726b31c47af), you can also build the library using Meson:

```
cd /path/to/source/root
meson setup builddir && cd builddir
meson compile
meson test
```

### Getting the sources

The sources are available at the following places:

 - The [gphoto github repository](https://github.com/gphoto/libgphoto2/) (this page) at https://github.com/gphoto/libgphoto2
 - The [gphoto project website](http://www.gphoto.org/)
 - In the SVN module gphoto2-manual

The gphoto2 Manual includes important information about setting up USB hotplugging. If you encounter problems, you may also consult the FAQ, which is included in the manual.


## Frontends

[gphoto2](https://github.com/gphoto/gphoto2) is a command line frontend which is quite powerful, especially in combination with scripts. See [The gphoto2 Manual](http://www.gphoto.com/doc/manual/) for more information.

For the GUI lovers, there are many applications that use libgphoto2 as a backend. Examples include:

* [digiKam](https://www.digikam.org/) (KDE)
* [gThumb](https://wiki.gnome.org/Apps/Gthumb) (GNOME)
* [Shotwell](https://wiki.gnome.org/Apps/Shotwell) (GNOME)
* [Darktable](https://www.darktable.org/) (a virtual lighttable and darkroom for photographers)
* [Entangle](https://entangle-photo.org/) (a GUI for tethered shooting)
* [f-spot](https://github.com/f-spot/f-spot) (GNOME / Mono)
* [GIMP](https://www.gimp.org/) (via the [gtkam-gimp](https://github.com/gphoto/gtkam/blob/master/src/gtkam-gimp.c) plugin)

We also have a GTK2 reference GUI called gtkam, which is unmaintained. Its primary special feature is its capture ability. Additionally, there are plugins for other programs available like kio_camera (which integrates camera support into KDE's file manager, Konqueror) and a FUSE plugin [gphotofs](https://github.com/gphoto/gphotofs).


## Reporting Bugs

Every piece of software contains errors and flaws. So does libgphoto2. When you encounter something that does not work, please do the following:

1. Find out whether this is a known problem by searching the [mailing lists](http://www.gphoto.com/mailinglists/) and the [Github issues](https://github.com/gphoto/libgphoto2/issues).

2. Reproduce the problem with debug output enabled and the language set to English, so that the development team will understand the messages. You can do this by running:

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
