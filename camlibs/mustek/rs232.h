#ifndef _RS232_H
#define _RS232_H

int mdc800_rs232_sendCommand(GPPort*,char* , char * , int );
int mdc800_rs232_waitForCommit(GPPort*,char commandid);
int mdc800_rs232_receive(GPPort*,char * , int );
int mdc800_rs232_download(GPPort*,char *, int);

#endif
