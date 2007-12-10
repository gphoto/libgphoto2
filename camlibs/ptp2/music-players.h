/**
 * \file music-players.h
 * List of music players as USB ids.
 *
 * Copyright (C) 2005-2007 Richard A. Low <richard@wentnet.com>
 * Copyright (C) 2005-2007 Linus Walleij <triad@df.lth.se>
 * Copyright (C) 2006-2007 Marcus Meissner
 * Copyright (C) 2007 Ted Bullock
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * This file is supposed to be included within a struct from both libmtp
 * and libgphoto2.
 */

/*
 * MTP device list, trying real bad to get all devices into
 * this list by stealing from everyone I know.
 */

  /*
   * Creative Technology
   * Initially the Creative devices was all we supported so these are
   * the most thoroughly tested devices. Presumably only the devices
   * with older firmware (the ones that have 32bit object size) will
   * need the DEVICE_FLAG_BROKEN_MTPGETOBJPROPLIST_ALL flag. This bug
   * manifest itself when you have a lot of folders on the device,
   * some of the folders will start to disappear when getting all objects
   * and properties.
   */
  { "Creative", 0x041e, "ZEN Vision", 0x411f, DEVICE_FLAG_BROKEN_MTPGETOBJPROPLIST_ALL },
  { "Creative", 0x041e, "Portable Media Center", 0x4123, DEVICE_FLAG_BROKEN_MTPGETOBJPROPLIST_ALL },
  { "Creative", 0x041e, "ZEN Xtra (MTP mode)", 0x4128, DEVICE_FLAG_BROKEN_MTPGETOBJPROPLIST_ALL },
  { "Dell", 0x041e, "DJ (2nd generation)", 0x412f, DEVICE_FLAG_BROKEN_MTPGETOBJPROPLIST_ALL },
  { "Creative", 0x041e, "ZEN Micro (MTP mode)", 0x4130, DEVICE_FLAG_BROKEN_MTPGETOBJPROPLIST_ALL },
  { "Creative", 0x041e, "ZEN Touch (MTP mode)", 0x4131, DEVICE_FLAG_BROKEN_MTPGETOBJPROPLIST_ALL },
  { "Dell", 0x041e, "Dell Pocket DJ (MTP mode)", 0x4132, DEVICE_FLAG_BROKEN_MTPGETOBJPROPLIST_ALL },
  { "Creative", 0x041e, "ZEN Sleek (MTP mode)", 0x4137, DEVICE_FLAG_BROKEN_MTPGETOBJPROPLIST_ALL },
  { "Creative", 0x041e, "ZEN MicroPhoto", 0x413c, DEVICE_FLAG_BROKEN_MTPGETOBJPROPLIST_ALL },
  { "Creative", 0x041e, "ZEN Sleek Photo", 0x413d, DEVICE_FLAG_BROKEN_MTPGETOBJPROPLIST_ALL },
  { "Creative", 0x041e, "ZEN Vision:M", 0x413e, DEVICE_FLAG_BROKEN_MTPGETOBJPROPLIST_ALL },
  // Reported by marazm@o2.pl
  { "Creative", 0x041e, "ZEN V", 0x4150, DEVICE_FLAG_BROKEN_MTPGETOBJPROPLIST_ALL },
  // Reported by danielw@iinet.net.au
  // This version of the Vision:M needs the no release interface flag,
  // unclear whether the other version above need it too or not.
  { "Creative", 0x041e, "ZEN Vision:M (DVP-HD0004)", 0x4151, 
      DEVICE_FLAG_NO_RELEASE_INTERFACE | DEVICE_FLAG_BROKEN_MTPGETOBJPROPLIST_ALL},
  // Reported by Darel on the XNJB forums
  { "Creative", 0x041e, "ZEN V Plus", 0x4152, DEVICE_FLAG_NONE },
  { "Creative", 0x041e, "ZEN Vision W", 0x4153, DEVICE_FLAG_NONE },
  // Reported by Paul Kurczaba <paul@kurczaba.com>
  { "Creative", 0x041e, "ZEN 8GB", 0x4157, DEVICE_FLAG_IGNORE_HEADER_ERRORS },
  // Reported by Ringofan <mcroman@users.sourceforge.net>
  { "Creative", 0x041e, "ZEN V 2GB", 0x4158, DEVICE_FLAG_NONE },

  /*
   * Samsung
   * We suspect that more of these are dual mode.
   * We suspect more of these might need DEVICE_FLAG_NO_ZERO_READS
   * YP-NEU, YP-NDU, YP-20, YP-800, YP-MF Series, YP-100, YP-30
   * YP-700 and YP-90 are NOT MTP, but use a Samsung custom protocol.
   */
  // From anonymous SourceForge user, not verified
  { "Samsung", 0x04e8, "YP-900", 0x0409, DEVICE_FLAG_NONE },
  // From Soren O'Neill
  { "Samsung", 0x04e8, "YH-920", 0x5022, DEVICE_FLAG_UNLOAD_DRIVER },
  // Contributed by aronvanammers on SourceForge
  { "Samsung", 0x04e8, "YH-925GS", 0x5024, DEVICE_FLAG_NONE },
  // From libgphoto2, according to tests by Stephan Fabel it cannot
  // get all objects with the getobjectproplist command..
  { "Samsung", 0x04e8, "YH-820", 0x502e, DEVICE_FLAG_BROKEN_MTPGETOBJPROPLIST_ALL },
  // Contributed by polux2001@users.sourceforge.net
  { "Samsung", 0x04e8, "YH-925(-GS)", 0x502f, DEVICE_FLAG_UNLOAD_DRIVER | DEVICE_FLAG_BROKEN_MTPGETOBJPROPLIST_ALL },
  // Contributed by anonymous person on SourceForge
  { "Samsung", 0x04e8, "YH-J70J", 0x5033, DEVICE_FLAG_UNLOAD_DRIVER },
  // From XNJB user
  { "Samsung", 0x04e8, "YP-Z5", 0x503c, DEVICE_FLAG_UNLOAD_DRIVER },
  // From XNJB user
  { "Samsung", 0x04e8, "YP-Z5 2GB", 0x5041, DEVICE_FLAG_NONE },
  // Contributed by anonymous person on SourceForge
  { "Samsung", 0x04e8, "YP-T7J", 0x5047, DEVICE_FLAG_NONE },
  // Reported by cstrickler@gmail.com
  { "Samsung", 0x04e8, "YP-U2J (YP-U2JXB/XAA)", 0x5054, DEVICE_FLAG_UNLOAD_DRIVER },
  // Reported by Andrew Benson
  { "Samsung", 0x04e8, "YP-F2J", 0x5057, DEVICE_FLAG_UNLOAD_DRIVER },
  // Reported by Patrick <skibler@gmail.com>
  { "Samsung", 0x04e8, "YP-K5", 0x505a, DEVICE_FLAG_NO_ZERO_READS },
  // From dev.local@gmail.com - 0x4e8/0x507c is the UMS mode, don't add this.
  // From m.eik michalke
  { "Samsung", 0x04e8, "YP-U3", 0x507d, DEVICE_FLAG_NONE },
  // Reported by Matthew Wilcox <matthew@wil.cx>
  { "Samsung", 0x04e8, "YP-T9", 0x507f, DEVICE_FLAG_NONE },
  // From Paul Clinch
  { "Samsung", 0x04e8, "YP-K3", 0x5081, DEVICE_FLAG_NONE },
  // From XNJB user
  { "Samsung", 0x04e8, "YP-P2", 0x5083, DEVICE_FLAG_NO_ZERO_READS },
  // From Paul Clinch
  { "Samsung", 0x04e8, "YP-T10", 0x508a, DEVICE_FLAG_OGG_IS_UNKNOWN },
  // From a rouge .INF file,
  // this device ID seems to have been recycled for the Samsung SGH-A707 Cingular cellphone
  { "Samsung", 0x04e8, "YH-999 Portable Media Center / SGH-A707", 0x5a0f, DEVICE_FLAG_NONE },
  // From Lionel Bouton
  { "Samsung", 0x04e8, "X830 Mobile Phone", 0x6702, DEVICE_FLAG_NONE },
  // From James <jamestech@gmail.com>
  { "Samsung", 0x04e8, "U600 Mobile Phone", 0x6709, DEVICE_FLAG_UNLOAD_DRIVER },
  // From Charlie Todd  2007-10-31
  { "Samsung", 0x04e8, "Juke (SCH-U470)", 0x6734, DEVICE_FLAG_UNLOAD_DRIVER},

  /*
   * Intel
   */
  { "Intel", 0x045e, "Bandon Portable Media Center", 0x00c9, DEVICE_FLAG_NONE },

  /*
   * JVC
   */
  // From Mark Veinot
  { "JVC", 0x04f1, "Alneo XA-HD500", 0x6105, DEVICE_FLAG_NONE },

  /*
   * Philips
   */
  { "Philips", 0x0471, "HDD6320/00 or HDD6330/17", 0x014b, DEVICE_FLAG_BROKEN_MTPGETOBJPROPLIST_ALL },
  // Anonymous SourceForge user
  { "Philips", 0x0471, "HDD1630/17", 0x014c, DEVICE_FLAG_NONE },
  // from discussion forum
  { "Philips", 0x0471, "HDD085/00 or HDD082/17", 0x014d, DEVICE_FLAG_NONE },
  // from XNJB forum
  { "Philips", 0x0471, "GoGear SA9200", 0x014f, DEVICE_FLAG_NONE },
  // From John Coppens <jcoppens@users.sourceforge.net>
  { "Philips", 0x0471, "SA1115/55", 0x0164, DEVICE_FLAG_NONE },
  // From Gerhard Mekenkamp
  { "Philips", 0x0471, "GoGear Audio", 0x0165, DEVICE_FLAG_NONE },
  // from David Holm <wormie@alberg.dk>
  { "Philips", 0x0471, "Shoqbox", 0x0172, DEVICE_FLAG_ONLY_7BIT_FILENAMES },
  // from npedrosa
  { "Philips", 0x0471, "PSA610", 0x0181, DEVICE_FLAG_NONE },
  // From libgphoto2 source
  { "Philips", 0x0471, "HDD6320", 0x01eb, DEVICE_FLAG_NONE },
  // From Detlef Meier <dm@emlix.com>
  { "Philips", 0x0471, "SA6014/SA6015/SA6024/SA6025/SA6044/SA6045", 0x084e, DEVICE_FLAG_UNLOAD_DRIVER },
  // from XNJB user
  { "Philips", 0x0471, "PSA235", 0x7e01, DEVICE_FLAG_NONE },


  /*
   * SanDisk
   * several devices (c150 for sure) are definately dual-mode and must 
   * have the USB mass storage driver that hooks them unloaded first.
   * They all have problematic dual-mode making the device unload effect
   * uncertain on these devices. All except for the Linux based ones seem
   * to need DEVICE_FLAG_BROKEN_MTPGETOBJPROPLIST_ALL.
   */
  // Reported by Brian Robison
  { "SanDisk", 0x0781, "Sansa m230/m240", 0x7400, 
    DEVICE_FLAG_UNLOAD_DRIVER | DEVICE_FLAG_BROKEN_MTPGETOBJPROPLIST_ALL |
    DEVICE_FLAG_NO_RELEASE_INTERFACE },
  // Reported by tangent_@users.sourceforge.net
  { "SanDisk", 0x0781, "Sansa c150", 0x7410, 
    DEVICE_FLAG_UNLOAD_DRIVER | DEVICE_FLAG_BROKEN_MTPGETOBJPROPLIST_ALL |
    DEVICE_FLAG_NO_RELEASE_INTERFACE },
  // From libgphoto2 source
  // Reported by <gonkflea@users.sourceforge.net>
  // Reported by Mike Owen <mikeowen@computerbaseusa.com>
  { "SanDisk", 0x0781, "Sansa e200/e250/e260/e270/e280", 0x7420, 
    DEVICE_FLAG_UNLOAD_DRIVER |  DEVICE_FLAG_BROKEN_MTPGETOBJPROPLIST_ALL |
    DEVICE_FLAG_NO_RELEASE_INTERFACE },
  // Reported by XNJB user
  { "SanDisk", 0x0781, "Sansa e280", 0x7421, 
    DEVICE_FLAG_UNLOAD_DRIVER | DEVICE_FLAG_BROKEN_MTPGETOBJPROPLIST_ALL |
    DEVICE_FLAG_NO_RELEASE_INTERFACE },
  // Reported by XNJB user
  { "SanDisk", 0x0781, "Sansa e280 v2", 0x7422, 
    DEVICE_FLAG_UNLOAD_DRIVER | DEVICE_FLAG_BROKEN_MTPGETOBJPROPLIST_ALL |
    DEVICE_FLAG_NO_RELEASE_INTERFACE },
  // Reported by XNJB user
  { "SanDisk", 0x0781, "Sansa m240", 0x7430, 
    DEVICE_FLAG_UNLOAD_DRIVER |  DEVICE_FLAG_BROKEN_MTPGETOBJPROPLIST_ALL |
    DEVICE_FLAG_NO_RELEASE_INTERFACE },
  // Reported by Eugene Brevdo <ebrevdo@princeton.edu>
  { "SanDisk", 0x0781, "Sansa Clip", 0x7432, DEVICE_FLAG_UNLOAD_DRIVER |
    DEVICE_FLAG_BROKEN_MTPGETOBJPROPLIST },
  // Reported by anonymous user at sourceforge.net
  { "SanDisk", 0x0781, "Sansa c240/c250", 0x7450, 
    DEVICE_FLAG_UNLOAD_DRIVER |  DEVICE_FLAG_BROKEN_MTPGETOBJPROPLIST_ALL |
    DEVICE_FLAG_NO_RELEASE_INTERFACE },
  // Reported by Troy Curtis Jr.
  { "SanDisk", 0x0781, "Sansa Express", 0x7460, 
    DEVICE_FLAG_UNLOAD_DRIVER | DEVICE_FLAG_BROKEN_MTPGETOBJPROPLIST | 
    DEVICE_FLAG_NO_RELEASE_INTERFACE },
  // Reported by XNJB user, and Miguel de Icaza <miguel@gnome.org>
  // This has no dual-mode so no need to unload any driver.
  // This is a Linux based device!
  { "SanDisk", 0x0781, "Sansa Connect", 0x7480, DEVICE_FLAG_NONE },
  // Reported by anonymous SourceForge user
  { "SanDisk", 0x0781, "Sansa View", 0x74b0, 
    DEVICE_FLAG_UNLOAD_DRIVER |  DEVICE_FLAG_BROKEN_MTPGETOBJPROPLIST_ALL |
    DEVICE_FLAG_NO_RELEASE_INTERFACE },  

  /*
   * iRiver
   * we assume that PTP_OC_MTP_GetObjPropList is essentially
   * broken on all iRiver devices, meaning it simply won't return
   * all properties for a file when asking for metadata 0xffffffff. 
   * Please test on your device if you believe it isn't broken!
   * Some devices from http://www.mtp-ums.net/viewdeviceinfo.php
   */
  { "iRiver", 0x1006, "Portable Media Center", 0x4002, 
    DEVICE_FLAG_BROKEN_MTPGETOBJPROPLIST | DEVICE_FLAG_NO_ZERO_READS | 
    DEVICE_FLAG_IRIVER_OGG_ALZHEIMER },
  { "iRiver", 0x1006, "Portable Media Center", 0x4003, 
    DEVICE_FLAG_BROKEN_MTPGETOBJPROPLIST | DEVICE_FLAG_NO_ZERO_READS | 
    DEVICE_FLAG_IRIVER_OGG_ALZHEIMER },
  // From an anonymous person at SourceForge
  { "iRiver", 0x4102, "iFP-880", 0x1008, 
    DEVICE_FLAG_BROKEN_MTPGETOBJPROPLIST | DEVICE_FLAG_NO_ZERO_READS | 
    DEVICE_FLAG_IRIVER_OGG_ALZHEIMER },
  // From libgphoto2 source
  { "iRiver", 0x4102, "T10", 0x1113, 
    DEVICE_FLAG_BROKEN_MTPGETOBJPROPLIST | DEVICE_FLAG_NO_ZERO_READS | 
    DEVICE_FLAG_IRIVER_OGG_ALZHEIMER },
  { "iRiver", 0x4102, "T20 FM", 0x1114, 
    DEVICE_FLAG_BROKEN_MTPGETOBJPROPLIST | DEVICE_FLAG_NO_ZERO_READS | 
    DEVICE_FLAG_IRIVER_OGG_ALZHEIMER },
  // This appears at the MTP-UMS site
  { "iRiver", 0x4102, "T20", 0x1115, 
    DEVICE_FLAG_BROKEN_MTPGETOBJPROPLIST | DEVICE_FLAG_NO_ZERO_READS | 
    DEVICE_FLAG_IRIVER_OGG_ALZHEIMER },
  { "iRiver", 0x4102, "U10", 0x1116, 
    DEVICE_FLAG_BROKEN_MTPGETOBJPROPLIST | DEVICE_FLAG_NO_ZERO_READS | 
    DEVICE_FLAG_IRIVER_OGG_ALZHEIMER },
  { "iRiver", 0x4102, "T10a", 0x1117, 
    DEVICE_FLAG_BROKEN_MTPGETOBJPROPLIST | DEVICE_FLAG_NO_ZERO_READS | 
    DEVICE_FLAG_IRIVER_OGG_ALZHEIMER },
  { "iRiver", 0x4102, "T20", 0x1118, 
    DEVICE_FLAG_BROKEN_MTPGETOBJPROPLIST | DEVICE_FLAG_NO_ZERO_READS | 
    DEVICE_FLAG_IRIVER_OGG_ALZHEIMER },
  { "iRiver", 0x4102, "T30", 0x1119, 
    DEVICE_FLAG_BROKEN_MTPGETOBJPROPLIST | DEVICE_FLAG_NO_ZERO_READS | 
    DEVICE_FLAG_IRIVER_OGG_ALZHEIMER },
  // Reported by David Wolpoff
  { "iRiver", 0x4102, "T10 2GB", 0x1120, 
    DEVICE_FLAG_BROKEN_MTPGETOBJPROPLIST | DEVICE_FLAG_NO_ZERO_READS | 
    DEVICE_FLAG_IRIVER_OGG_ALZHEIMER },
  // Rough guess this is the MTP device ID...
  { "iRiver", 0x4102, "N12", 0x1122, 
    DEVICE_FLAG_BROKEN_MTPGETOBJPROPLIST | DEVICE_FLAG_NO_ZERO_READS | 
    DEVICE_FLAG_IRIVER_OGG_ALZHEIMER },
  // Reported by Philip Antoniades <philip@mysql.com>
  // Newer iriver devices seem to have shaped-up firmware without any
  // of the annoying bugs.
  { "iRiver", 0x4102, "Clix2", 0x1126, DEVICE_FLAG_NONE },
  // Reported by Adam Torgerson
  { "iRiver", 0x4102, "Clix", 0x112a, 
    DEVICE_FLAG_NO_ZERO_READS | DEVICE_FLAG_IRIVER_OGG_ALZHEIMER },
  // Reported by Douglas Roth <dougaus@gmail.com>
  { "iRiver", 0x4102, "X20", 0x1132, 
    DEVICE_FLAG_BROKEN_MTPGETOBJPROPLIST | DEVICE_FLAG_NO_ZERO_READS | 
    DEVICE_FLAG_IRIVER_OGG_ALZHEIMER },
  // Reported by Robert Ugo <robert_ugo@users.sourceforge.net>
  { "iRiver", 0x4102, "T60", 0x1134, 
    DEVICE_FLAG_BROKEN_MTPGETOBJPROPLIST | DEVICE_FLAG_NO_ZERO_READS | 
    DEVICE_FLAG_IRIVER_OGG_ALZHEIMER },
  // Reported by Scott Call
  { "iRiver", 0x4102, "H10 20GB", 0x2101, 
    DEVICE_FLAG_BROKEN_MTPGETOBJPROPLIST | DEVICE_FLAG_NO_ZERO_READS | 
    DEVICE_FLAG_IRIVER_OGG_ALZHEIMER },
  { "iRiver", 0x4102, "H10", 0x2102, 
    DEVICE_FLAG_BROKEN_MTPGETOBJPROPLIST | DEVICE_FLAG_NO_ZERO_READS | 
    DEVICE_FLAG_IRIVER_OGG_ALZHEIMER },


  /*
   * Dell
   */
  { "Dell, Inc", 0x413c, "DJ Itty", 0x4500, DEVICE_FLAG_NONE },
  
  /*
   * Toshiba
   */
  { "Toshiba", 0x0930, "Gigabeat MEGF-40", 0x0009, DEVICE_FLAG_NONE },
  { "Toshiba", 0x0930, "Gigabeat", 0x000c, DEVICE_FLAG_NONE },
  // Reported by Nicholas Tripp
  { "Toshiba", 0x0930, "Gigabeat P20", 0x000f, DEVICE_FLAG_NONE },
  // From libgphoto2
  { "Toshiba", 0x0930, "Gigabeat S", 0x0010, DEVICE_FLAG_BROKEN_MTPGETOBJPROPLIST_ALL },
  // Reported by Rob Brown
  { "Toshiba", 0x0930, "Gigabeat P10", 0x0011, DEVICE_FLAG_NONE },
  // Reported by solanum@users.sourceforge.net
  { "Toshiba", 0x0930, "Gigabeat V30", 0x0014, DEVICE_FLAG_NONE },
  // Reported by Michael Davis <slithy@yahoo.com>
  { "Toshiba", 0x0930, "Gigabeat U", 0x0016, DEVICE_FLAG_NONE },
  // Reported by Rolf <japan (at) dl3lar.de>
  { "Toshiba", 0x0930, "Gigabeat T", 0x0019, DEVICE_FLAG_NONE },
  
  /*
   * Archos
   * These devices have some dual-mode interfaces which will really
   * respect the driver unloading, so DEVICE_FLAG_UNLOAD_DRIVER
   * really work on these devices!
   */
  // Reported by Alexander Haertig <AlexanderHaertig@gmx.de>
  { "Archos", 0x0e79, "Gmini XS100", 0x1207, DEVICE_FLAG_UNLOAD_DRIVER },
  // Added by Jan Binder
  { "Archos", 0x0e79, "XS202 (MTP mode)", 0x1208, DEVICE_FLAG_NONE },
  // Reported by gudul1@users.sourceforge.net
  { "Archos", 0x0e79, "104 (MTP mode)", 0x120a, DEVICE_FLAG_NONE },
  // Reported by Etienne Chauchot <chauchot.etienne@free.fr>
  { "Archos", 0x0e79, "504 (MTP mode)", 0x1307, DEVICE_FLAG_UNLOAD_DRIVER },
  // Reported by Kay McCormick <kaym@modsystems.com>
  { "Archos", 0x0e79, "704 mobile dvr", 0x130d, DEVICE_FLAG_UNLOAD_DRIVER },
  // Reported by Joe Rabinoff
  { "Archos", 0x0e79, "605 (MTP mode)", 0x1313, DEVICE_FLAG_UNLOAD_DRIVER },

  /*
   * Dunlop (OEM of EGOMAN ltd?) reported by Nanomad
   * This unit is falsely detected as USB mass storage in Linux
   * prior to kernel 2.6.19 (fixed by patch from Alan Stern)
   * so on older kernels special care is needed to remove the
   * USB mass storage driver that erroneously binds to the device
   * interface.
   */
  { "Dunlop", 0x10d6, "MP3 player 1GB / EGOMAN MD223AFD", 0x2200, DEVICE_FLAG_UNLOAD_DRIVER},
  
  /*
   * Microsoft
   */
  // Reported by Farooq Zaman
  { "Microsoft", 0x045e, "Zune", 0x0710, DEVICE_FLAG_NONE }, 
  
  /*
   * Sirius
   */
  { "Sirius", 0x18f6, "Stiletto", 0x0102, DEVICE_FLAG_NONE },

  /*
   * Canon
   * This is actually a camera, but it has a Microsoft device descriptor
   * and reports itself as supporting the MTP extension.
   */
  { "Canon", 0x04a9, "PowerShot A640 (PTP/MTP mode)", 0x3139, DEVICE_FLAG_NONE },

  /*
   * Nokia
   */
  // From: DoomHammer <gaczek@users.sourceforge.net>
  { "Nokia", 0x0421, "3110c Mobile Phone", 0x005f, DEVICE_FLAG_NONE },
  // From: Mitchell Hicks <mitchix@yahoo.com>
  { "Nokia", 0x0421, "5300 Mobile Phone", 0x04ba, DEVICE_FLAG_NONE },
  // From Christian Arnold <webmaster@arctic-media.de>
  { "Nokia", 0x0421, "N73 Mobile Phone", 0x04d1, DEVICE_FLAG_UNLOAD_DRIVER },
  // From Swapan <swapan@yahoo.com>
  { "Nokia", 0x0421, "N75 Mobile Phone", 0x04e1, DEVICE_FLAG_NONE },
  // From Anonymous Sourceforge User
  { "Nokia", 0x0421, "N95 Mobile Phone", 0x04ef, DEVICE_FLAG_NONE },
  // From: Pat Nicholls <pat@patandannie.co.uk>
  { "Nokia", 0x0421, "N80 Internet Edition (Media Player)", 0x04f1, DEVICE_FLAG_UNLOAD_DRIVER },


  /*
   * LOGIK
   * Sold in the UK, seem to be manufactured by CCTech in China.
   */
  { "Logik", 0x13d1, "LOG DAX MP3 and DAB Player", 0x7002, DEVICE_FLAG_UNLOAD_DRIVER },

  /*
   * RCA / Thomson
   */
  // From kiki <omkiki@users.sourceforge.net>
  { "Thomson", 0x069b, "EM28 Series", 0x0774, DEVICE_FLAG_NONE },
  { "Thomson / RCA", 0x069b, "Opal / Lyrca MC4002", 0x0777, DEVICE_FLAG_NONE },
  // From Svenna <svenna@svenna.de>
  // Not confirmed to be MTP.
  { "Thomson", 0x069b, "scenium E308", 0x3028, DEVICE_FLAG_NONE },
  
  /*
   * NTT DoCoMo
   */
  { "FOMA", 0x04c5, "F903iX HIGH-SPEED", 0x1140, DEVICE_FLAG_NONE },

  /*
   * Palm device userland program named Pocket Tunes
   * Reported by Peter Gyongyosi <gyp@impulzus.com>
   */
  { "Palm / Handspring", 0x1703, "Pocket Tunes", 0x0001, DEVICE_FLAG_NONE },
  // Reported by anonymous submission
  { "Palm Handspring", 0x1703, "Pocket Tunes 4", 0x0002, DEVICE_FLAG_NONE },

  /*
   * TrekStor devices
   * Their datasheet claims their devices are dualmode so probably needs to
   * unload the attached drivers here.
   */
  // Reported by Stefan Voss <svoss@web.de>
  // This is a Sigmatel SoC with a hard disk.
  { "TrekStor", 0x066f, "Vibez 8/12GB", 0x842a, 
      DEVICE_FLAG_UNLOAD_DRIVER | DEVICE_FLAG_BROKEN_MTPGETOBJPROPLIST },
  // Reported by Cristi Magherusan <majeru@gentoo.ro>
  { "TrekStor", 0x0402, "i.Beat Sweez FM", 0x0611, 
      DEVICE_FLAG_UNLOAD_DRIVER },
  
  /*
   * Disney (have had no reports of this actually working.)
   */
  // Reported by XNJB user
  { "Disney", 0x0aa6, "MixMax", 0x6021, DEVICE_FLAG_NONE },

  /*
   * Cowon Systems, Inc.
   * The iAudio audiophile devices don't encourage the use of MTP.
   */
  // Reported by Patrik Johansson <Patrik.Johansson@qivalue.com>
  { "Cowon", 0x0e21, "iAudio U3 (MTP mode)", 0x0701, DEVICE_FLAG_NONE },
  // Reported by Roberth Karman
  { "Cowon", 0x0e21, "iAudio 7 (MTP mode)", 0x0751, DEVICE_FLAG_NONE },
  // Reported by TJ Something <tjbk_tjb@users.sourceforge.net>
  { "Cowon", 0x0e21, "iAudio D2 (MTP mode)", 0x0801, 
   DEVICE_FLAG_UNLOAD_DRIVER | DEVICE_FLAG_BROKEN_MTPGETOBJPROPLIST_ALL },

  /*
   * Insignia, dual-mode.
   */
  { "Insignia", 0x19ff, "NS-DV45", 0x0303, DEVICE_FLAG_UNLOAD_DRIVER },
  // Reported by Rajan Bella <rajanbella@yahoo.com>
  { "Insignia", 0x19ff, "Sport Player", 0x0307, DEVICE_FLAG_UNLOAD_DRIVER },
  // Reported by "brad" (anonymous, sourceforge)
  { "Insignia", 0x19ff, "Pilot 4GB", 0x0309, DEVICE_FLAG_UNLOAD_DRIVER },

  /*
   * LG Electronics
   */
  // Not verified - anonymous submission
  { "LG", 0x043e, "UP3", 0x70b1, DEVICE_FLAG_NONE },

  /*
   * Sony
   */
  // Reported by Endre Oma <endre.88.oma@gmail.com>
  // (possibly this is for the A-series too)
  { "Sony", 0x054c, "Walkman S-series", 0x0327, DEVICE_FLAG_UNLOAD_DRIVER },

  /*
   * SonyEricsson
   */
  // Reported by Øyvind Stegard <stegaro@users.sourceforge.net>
  { "SonyEricsson", 0x0fce, "K850i", 0x0075, DEVICE_FLAG_NONE },
  // Reported by Michael Eriksson
  { "SonyEricsson", 0x0fce, "W910", 0x0076, DEVICE_FLAG_NONE },

  /*
   * Motorola
   * Assume DEVICE_FLAG_BROKEN_SET_OBJECT_PROPLIST on all of these.
   */
  // Reported by Marcus Meissner to libptp2
  { "Motorola", 0x22b8, "K1", 0x4811, DEVICE_FLAG_BROKEN_SET_OBJECT_PROPLIST },
  // Reported by Hans-Joachim Baader <hjb@pro-linux.de> to libptp2
  { "Motorola", 0x22b8, "A1200", 0x60ca, DEVICE_FLAG_BROKEN_SET_OBJECT_PROPLIST },
  // Reported by anonymous user
  { "Motorola", 0x22b8, "RAZR2 V8", 0x6415, DEVICE_FLAG_BROKEN_SET_OBJECT_PROPLIST },

  /*
   * Media Keg
   */
  // Reported by Rajan Bella <rajanbella@yahoo.com>
  { "Kenwood", 0x0b28, "Media Keg HD10GB7 Sport Player", 0x100c, DEVICE_FLAG_UNLOAD_DRIVER},

  /*
   * Other strange stuff.
   */
  { "Isabella", 0x0b20, "Her Prototype", 0xddee, DEVICE_FLAG_NONE }
