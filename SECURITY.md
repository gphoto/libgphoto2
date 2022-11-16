# Security overview

## General

libgphoto2 is a software library to allow accessing USB devices (cameras,
media players), allowing file system operations (list, download,
upload, delete), and control operations (get and set settings, and
remote control).

It consists of a core library, and drivers for both "port" protocols
like USB, serial, IP and devices (camera drivers).

libgphoto2 only processes images to provide standard formats. For JPEG
images the libexif library is used for extraction of EXIF data.

Callers of the library can be assumed trusted, also input coming into
the library via API calls is considered trusted.

Data coming from port drivers (USB, serial, IP, etc) is considered untrusted.

Historically the primary development goals was "make it work", without
security in mind.

## Attack Surface

The primary attack scenario is a kiosk style photo access computer, where people
can plug in USB devices in an unattended fashion.

Attack impacts are achieving control over this computer, or blocking its use.

## Bugs considered security issues

(Mostly for CVE assigments rules.)

In scope of a security attack are the autodetecting protocols support,
like USB.

Also IP (TCP and UDP) based drivers are considered in scope, with
malicious target devices or man in the middle attacks.

Current day camera drivers are in scope (e.g. all drivers not marked as "outdated").

Triggering memory corruption is considered in scope.
Triggering endless loops is considered in scope. (would block kiosk style operation)

## Bugs not considered security issues

Serial cameras are not in scope, as they cannot be autodetected and need
special configuration which makes other attack vectors likely.

Outdated drivers... We have classified a number of older drivers as
"outdated", and do not recommend to build them by default anymore.

Denial of service attacks of class "crash" or "resource consumption
(disk)" are not in scope.

- Frontends should auto-recover (restart) after crashes.
- Resource consumption in terms of diskspace is not in scope, as the
  library is meant to download large amounts of data (Gigabytes) in
  regular operation.

Information disclosure is not a relevant attack scenario.

## Bugreports

Bugreports can be filed as github issues.

If you want to report an embargoed security bug report, reach out to marcus@jet.franken.de
or [Report a vulnerability](https://github.com/gphoto/libgphoto2/security/advisories/new)
on Github.
