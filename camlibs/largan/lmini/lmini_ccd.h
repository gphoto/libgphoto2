

#ifndef __LARGAN_CCD_H__
#define __LARGAN_CCD_H__

extern char BMPheader[54];

void largan_ccd2dib(char *pData, char *pDib, long dwDibRowBytes, int nCcdFactor);

#endif

