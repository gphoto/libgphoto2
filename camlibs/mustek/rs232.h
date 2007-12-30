#ifndef _RS232_H
#define _RS232_H

int mdc800_rs232_sendCommand(GPPort*,unsigned char* , unsigned char * , int );
int mdc800_rs232_waitForCommit(GPPort*,char commandid);
int mdc800_rs232_receive(GPPort*,unsigned char * , int );
int mdc800_rs232_download(GPPort*,unsigned char *, int);

#endif
