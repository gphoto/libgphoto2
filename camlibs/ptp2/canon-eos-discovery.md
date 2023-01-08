# Documenting Canon EOS PTP commands

Laurent Clevy

## Introduction

Hi,
I wrote this python tool as a toy to learn PyUSB abd PTP protocol
https://github.com/lclevy/miniPtp

Gphoto2 is a great source of inspiration, so I decided to contribute back via describing Canon PTP discoveries via this markdown file.

## describing PTP exchanges using Python syntax

I suggest to use a summarized way to describe PTP exchanges usage:
- code and request (PTP type 1) parameters
- if dataphase (type 2) sends or receives data
- if response (type 3) has any parameters

If you agree, in the following section description transaction function:
- *req_params* are request parameters 
- *code* is PTP command
- *data_phase* is true when type 2 is used
- *senddata* is None when dataphase is receiving data
- *ResponseCode* is responde code, like 0x2001
- *Data* is data received during data phase is any
- *Parameter* is a list of 32bits integers received during response phase if any (up to 5)

such description is also Python working code, but one can use ptpcam -R syntax
https://github.com/leirf/libptp/blob/master/src/ptpcam.c#L145
or plain ascii like [canon-eos-customfunc.txt](canon-eos-customfunc.txt) or [canon-eos-olc.txt](canon-eos-olc.txt).

## miniPtp transaction function

Internally to miniPtp, this function is used to initiate transaction:
```python
  def transaction(self, code, req_params, data_phase=True, senddata=None):
    ...
  return { 'ResponseCode': ptp_header.code, 'Data':bytes(data), 'Parameter':respParams }
```
In the following example, we open the session (code=0x1002) with this request which has one parameter, the session id. False means there is no data phase:
```python
self.transaction( 0x1002, [ self.session_id ], False )
```
In the following, we have got 3 parameters during request, and a list of 1 parameter (type int) inside response (type 3):
```python
  def get_num_objects( self, storage_id, object_format=0, association=0xffffffff ):
    data = self.transaction( 0x1006, [ storage_id, object_format, association ], False )['Parameter'] # all_formats, all_handles
    return data[0]
```

## Documented Canon PTP commands

### 0x9033 : EOS_GetMacAddress

```python
mac_address = ptp_obj.transaction( 0x9033, [] )['Data'][12:]
```

### 0x913f : EOS_GetSupport

```python
model_id = ptp_obj.transaction( 0x913f, [], False )
```
0x8000453 for R6

Model Ids are described here [ExifTool](https://exiftool.org/TagNames/Canon.html#CanonModelID)  

### 0x911f : EOS_UpdateFirmware

This command can used to update a FIR file to the camera, which then itself reboot into update mode. The user has to confirm the update on screen. As a side effect, it uploads any file to the root of the sdcard.

https://github.com/lclevy/miniPtp/blob/main/update_canon.py

