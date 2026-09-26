# Canon EOS R7 with libgphoto2: findings and xcframework update notes

These results come from testing a Canon EOS R7 (firmware 1.3.0) with an RF-S18-150mm F3.5-6.3 IS STM lens. The camera was connected over USB to a Mac mini (Apple silicon), running libgphoto2 2.5.34.1 from branch `claude/optimistic-fermi-730u8o` of `Victor-Ixaru-01/libgphoto2` (now merged into that fork's `master`).

---

## 1. What to change in your `libgphoto2.xcframework`

### 1.1 Source changes to pull in

There are two commits on top of upstream `master` at `77a6c7008`:

| Commit | File | Change |
|---|---|---|
| `18da1ae68` | `camlibs/ptp2/ptp-pack.c` | Fixes the CustomFuncEx length check and the bogus ImageFormat options |
| `f95ed3191` | `camlibs/ptp2/cameras/canon-eos-r7.txt` | Updates the R7 reference dump. Documentation only, not compiled |

Only `ptp-pack.c` affects the binary. It is not compiled on its own: `ptp.c` (line 193) and `olympus-wrap.c` both `#include` it. **Rebuild the whole `ptp2` camlib target**, not just one object file. If your xcframework links `ptp2` statically, rebuild the static archive and every slice (macOS arm64/x86_64, iOS device, iOS simulator). Nothing changed in `libgphoto2`, `libgphoto2_port` or the public headers, so your Swift or C bridging code needs no changes.

The full diff:

```diff
--- a/camlibs/ptp2/ptp-pack.c
+++ b/camlibs/ptp2/ptp-pack.c
@@ ptp_unpack_EOS_CustomFuncEx
 	if (*size < sizeof(uint32_t))
 		return strdup("bad length");

+	/* s is the total length of the blob, including this length field itself */
 	s = dtoh32a( *data );
 	n = s/4;

-	if (*size < 4+s)
+	if (*size < s)
 		return strdup("bad length");
@@ ptp_unpack_EOS_events, case PTP_DPC_CANON_EOS_ImageFormat*
 				for (j=0;j<dpd_count;j++) {
+					const uint8_t *prev = xdata;
+
 					dpd->FORM.Enum.SupportedValue[j].u16 = ptp_unpack_EOS_ImageFormat( params, &xdata, &xsize );
+					/* parser does not advance xdata on failure, e.g. EOS R7 in movie mode sends count 1 with no data */
+					if (xdata == prev) {
+						dpd->FORM.Enum.NumberOfValues = j;
+						break;
+					}
```

### 1.2 What the fixes change in your app

| Symptom before | After |
|---|---|
| The `customfuncex` setting always reads `"bad length"`. This affects every Canon EOS body on libgphoto2 2.5.34 | Reads the real data, e.g. `20,1,6,14,1,102,1,0,` |
| In movie mode, `imageformat` offers a fake `"L + L"` choice (value `0x0000`). Selecting it sends a meaningless value to the camera | The fake choice is gone, and only the current value is listed |

### 1.3 How the fixes were checked

- A parser test fed R7-style events into `ptp_unpack_EOS_events`. It fails on the old code and passes on the new code, with no AddressSanitizer or UndefinedBehaviorSanitizer errors.
- On the R7 itself, `customfuncex` decoded all six groups and the fake option was gone.

---

## 2. Identifying the R7

| Field | Value |
|---|---|
| USB vendor:product | `0x04a9:0x32f7` |
| Model string | `Canon EOS R7` |
| Device version | `3-1.3.0` (firmware 1.3.0) |
| libgphoto2 driver flags | `PTP_CAP \| PTP_CAP_PREVIEW` (capture and live-view preview) |
| Storage IDs | `0x00010001` = SD1, `0x00020001` = SD2 |
| Folder paths | `/store_00010001/DCIM/100EOSR7`, `/store_00020001/DCIM/100EOSR7` |
| File names | e.g. `117A0463.CR3`, `117A0463.JPG` |
| New in firmware 1.3.0 | PTP operation `0x9088` (SetModeDialDisable) |

---

## 3. Photo mode vs movie mode

The R7's power switch has a separate **movie** position. The setting `eosmovieswitch` reads **1** in movie mode and **0** in photo mode.

In movie mode, the camera itself reports only one option (or none) for most stills settings. This is camera behaviour, not a driver bug:

| Setting | Movie mode | Photo mode |
|---|---|---|
| `imageformat` | current value only | 23 choices |
| `drivemode` | Single | Single, Continuous low, Timer 10 sec, Timer 2 sec, Continuous timer |
| `meteringmode` | current value only | Evaluative, Partial, Spot, Center-weighted average |
| `picturestyle` | current value only | 11 choices |
| `focusmode` | One Shot | One Shot, AI Servo |
| `aspectratio` | 3:2 | 3:2, 1:1, 4:3, 16:9 |
| `aeb` | off | off, and ±1/3 to ±3 |
| `colorspace` | sRGB | sRGB, AdobeRGB |
| `shutterspeed` (M mode) | 1/8 to 1/4000 | 30 s to 1/8000 |
| `iso` (M mode) | Auto, 100 to 12800 | Auto, 100 to 12800 on the test body (see note) |

**App recommendation:** read `eosmovieswitch` after connecting. If it is `1`, tell the user to move the power switch to photo mode before showing stills settings.

Note: the ISO list stopped at 12800 even in photo mode. The camera sent exactly 23 values, so the camera's own "ISO speed range" menu setting was most likely limiting it.

In Av mode (and other semi-automatic modes), the camera only lists the values the user can actually change. For example, ISO shows `Auto` plus the current value, and shutter speed shows only its current value. The full lists appear in **M** mode.

---

## 4. Settings reference (photo mode, M mode)

These are gphoto2 setting names, which you pass to `gp_camera_get_single_config` / `gp_camera_set_single_config`.

| Name | Type | Values seen on the R7 |
|---|---|---|
| `imageformat` | RADIO | `L`, `cL`, `M`, `cM`, `S1`, `cS1`, `S2`, `cRAW + L`, `cRAW + cL`, `RAW + L`, `RAW + cL`, `cRAW + M`, `cRAW + cM`, `RAW + M`, `RAW + cM`, `cRAW + S1`, `cRAW + cS1`, `RAW + S1`, `RAW + cS1`, `cRAW + S2`, `RAW + S2`, `RAW`, `cRAW` |
| `imageformatsd`, `imageformatcf` | RADIO | Same list. Always equal to `imageformat` (see §5.3) |
| `iso` | RADIO | `Auto`, `100` … `12800` in 1/3-stop steps |
| `shutterspeed` | RADIO | `30` … `1/8000` |
| `aperture` | RADIO | `3.5` … `22` with this lens |
| `exposurecompensation` | RADIO | `-3` … `3` in 1/3 steps |
| `whitebalance` | RADIO | `Auto`, `AWB White`, `Daylight`, `Shadow`, `Cloudy`, `Tungsten`, `Fluorescent`, `Flash`, `Manual`, `Color Temperature` |
| `colortemperature` | RADIO | `2500` … `10000` |
| `drivemode` | RADIO | See §3. High-speed modes (H, H+) were not offered by the camera during testing |
| `focusmode` | RADIO | `One Shot`, `AI Servo` |
| `afmethod` | RADIO | `LiveSpotAF`, `Live`, `LiveSingleExpandCross`, `LiveSingleExpandSurround`, `FlexibleZoneAF1..3`, `WholeAreaAF` |
| `meteringmode` | RADIO | `Evaluative`, `Partial`, `Spot`, `Center-weighted average` |
| `picturestyle` | RADIO | `Standard`, `Portrait`, `Landscape`, `Neutral`, `Faithful`, `Monochrome`, `Auto`, `Fine detail`, `User defined 1..3` |
| `aspectratio` | RADIO | `3:2`, `1:1`, `4:3`, `16:9` |
| `aeb` | RADIO | `off`, `+/- 1/3` … `+/- 3` |
| `alomode` | RADIO | Only `x3` offered. The camera restricts it, probably because Highlight Tone Priority is on |
| `capturetarget` | RADIO | `Internal RAM` (download only), `Memory card` |
| `storageid` | TEXT | Selected card: `00010001` (SD1) or `00020001` (SD2) |
| `lensname` | TEXT, read-only | `RF-S18-150mm F3.5-6.3 IS STM` |
| `eosmovieswitch` | TEXT, read-only | `0` = photo, `1` = movie |
| `availableshots` | TEXT, read-only | Remaining shots on the selected card, for the current format |
| `customfuncex` | TEXT | Fixed by this branch (§1.2) |
| `manualfocusdrive` | RADIO | `Near 1..3`, `None`, `Far 1..3` |
| `autofocusdrive`, `cancelautofocus`, `viewfinder` | TOGGLE | Actions |
| `eosremoterelease` | RADIO | `Press Half AF`, `Press Full AF`, `Release Full`, and others |

Values that look odd but are not bugs:
- `liveviewsize` reads `val 1` when live view isn't being streamed to the computer. Other Canon bodies do the same.
- `bracketmode` reads `Unknown value 0000`. 11 other Canon dumps in the repo show the same.
- `evfmode` reads `1` in photo mode and `Unknown value 0002` in movie mode. Its meaning isn't known.
- `batterylevel` appears twice (for example 87% and 100%). One is probably a second battery field.

---

## 5. Dual card slots (SD1 + SD2)

### 5.1 What works

| Feature | Status |
|---|---|
| Both cards visible as separate storages | ✅ |
| List, download and delete files on either card | ✅ |
| Capture to card, RAW + JPEG (CR3 and JPG both downloaded) | ✅ tested |
| Burst capture (`-F 3 -I 5`) | ✅ tested |
| "Rec. to multiple": one shot, one copy on each card, both downloaded | ✅ tested (RAW) |
| Read which card is selected (`storageid`) | ✅ |
| Read or set **Record func** (Standard / Auto switch / Rec. separately / Rec. to multiple) | ❌ Not reported over USB. Must be set on the camera |
| Read the format of each card in "Rec. separately" | ⚠️ Only the **selected** card's format is visible |
| Choose the recording card from the computer | ❓ Untested |

### 5.2 Card-related camera properties

| Property | gphoto2 name | Values |
|---|---|---|
| `0xd11e` CurrentStorage | `storageid` | `0x00010001` SD1, `0x00020001` SD2 |
| `0xd11c` CaptureDestination | (used internally by `capturetarget`) | `1` = card 1, `2` = card 2, `4` = computer. The camera also offers `5` and `6` (meaning unconfirmed; probably card + computer) |
| `0xd11f` CurrentFolder | none | `0x51900000` on SD1, `0x91900000` on SD2 |
| `0xd11b` AvailableShots | `availableshots` | Follows the selected card and its format |

### 5.3 "Rec. separately"

`imageformat`, `imageformatcf` and `imageformatsd` (properties `0xd120` to `0xd122`) always hold **the same value**: the format of whichever card is selected.
- On a two-CF/SD body, CF and SD hold one format each. On the R7 they don't.
- To read the other card's format, the selected card has to change. Doing that from the computer is untested (see §5.1).

### 5.4 "Rec. to multiple": duplicate file names

One shot creates two **identically named** files (e.g. `117A0463.CR3`), one per card. How to handle them:
- `gp_camera_capture()` returns the path of the first copy (SD1 in testing).
- The second copy arrives afterwards as a `GP_EVENT_FILE_ADDED` from `gp_camera_wait_for_event()`, with a path on the other storage.
- **Save each file under a unique local name.** Include the storage folder or a counter, or the second download overwrites the first. With the gphoto2 command-line tool, use `--filename "%f_%n.%C"`.

### 5.5 gphoto2 and card selection

With `capturetarget` = `Memory card`, the driver requests CaptureDestination `1` (card 1): it always picks the first card value the camera lists. With card 2 selected on the R7, the camera still reported `2` and kept recording to card 2, so no harm was seen. Don't rely on this. To be sure where files go, read `storageid` after connecting and after each capture.

---

## 6. Using it from your app (C API)

The same calls work from Swift through a bridging header.

```c
#include <gphoto2/gphoto2.h>

/* Read a setting as a string */
static int get_text(Camera *cam, GPContext *ctx, const char *name, const char **out) {
    CameraWidget *w = NULL;
    int ret = gp_camera_get_single_config(cam, name, &w, ctx);
    if (ret < GP_OK) return ret;
    ret = gp_widget_get_value(w, out);   /* RADIO/TEXT/MENU give const char* */
    /* copy *out before freeing w */
    gp_widget_free(w);
    return ret;
}

/* Set a RADIO/TEXT setting, e.g. set_text(cam, ctx, "imageformat", "RAW + L") */
static int set_text(Camera *cam, GPContext *ctx, const char *name, const char *val) {
    CameraWidget *w = NULL;
    int ret = gp_camera_get_single_config(cam, name, &w, ctx);
    if (ret < GP_OK) return ret;
    ret = gp_widget_set_value(w, val);
    if (ret >= GP_OK) ret = gp_camera_set_single_config(cam, name, w, ctx);
    gp_widget_free(w);
    return ret;
}
```

A capture sequence that also picks up the extra files from RAW + JPEG and "Rec. to multiple":

```c
CameraFilePath path;
gp_camera_capture(cam, GP_CAPTURE_IMAGE, &path, ctx);
download(cam, ctx, path.folder, path.name);          /* first file */

for (;;) {                                           /* drain the remaining files */
    CameraEventType type; void *data = NULL;
    gp_camera_wait_for_event(cam, 2000, &type, &data, ctx);
    if (type == GP_EVENT_FILE_ADDED) {
        CameraFilePath *p = data;
        download(cam, ctx, p->folder, p->name);      /* e.g. JPG, or the SD2 copy */
    } else if (type == GP_EVENT_TIMEOUT || type == GP_EVENT_CAPTURE_COMPLETE) {
        free(data);
        break;
    }
    free(data);
}
```

Here `download()` is your own helper: `gp_file_new` → `gp_camera_file_get(..., GP_FILE_TYPE_NORMAL, file, ctx)` → save. Build the local file name from `folder` + `name` so the SD1 and SD2 copies don't collide.

Recommended checks after connecting:
1. Read `eosmovieswitch`. If it is `1`, ask the user to switch to photo mode (§3).
2. Read `storageid` to know which card is active.
3. Only show the entries that the camera's own choice lists contain. The camera narrows them depending on mode and settings.

### macOS notes

- While a session is open, the camera **locks its buttons and menus**. Close the camera (`gp_camera_exit`) to let the user change on-camera settings such as Record func.
- The camera stores up property-change events while no session is open. The next session receives them all at once when it connects. Use the values that arrive last.
- macOS's `ptpcamerad` process (used by Photos/Image Capture) can grab the camera. If the camera isn't found, it may need to be stopped.
- In testing, a session left open too long sometimes met a camera that had fallen asleep (Auto power off was 60 s). Either keep the session active or tell the user to raise Auto power off.

### iOS notes

- iOS apps cannot access USB cameras through libusb. **On iOS only the `ptpip` (Wi-Fi) port driver is usable.**
- **Nothing in this document was tested over Wi-Fi or on iOS.** Test the R7's Wi-Fi remote mode before relying on it.
- iOS apps can't load separate driver files. `ptp2` and the `ptpip` port driver must be linked into the app statically, which stock libgphoto2 doesn't support out of the box (see §7).

---

## 7. Open issues

| Issue | Status |
|---|---|
| One capture (JPEG L, card 2 selected, Rec. to multiple) hung. The camera sent nothing for 31 s, then dropped off USB (`PTP No Device`) | Not reproduced yet; the cause could be an autofocus failure, sleep, or the cable. Retry with MF, or with a contrasty subject |
| Driver requests CaptureDestination `1` even when card 2 is selected | Harmless in testing. Possible fix: prefer the camera's current card value |
| `storageid` is a raw hex text field | Could become an `SD1` / `SD2` choice |
| Record func not controllable | Not reported by the camera over USB |
| Drive mode lacks H / H+; ALO fixed at `x3`; ISO capped at 12800 | Limited by camera settings, not the driver |
| Static camlib linking for iOS | Stock libgphoto2 loads `ptp2` and the port drivers at runtime (`lt_dlopenext` in `gphoto2-abilities-list.c` and `gphoto2-port-info-list.c`). An iOS build needs them linked in statically, e.g. with libltdl's preloaded-symbols mechanism or a small patch to register them directly |
