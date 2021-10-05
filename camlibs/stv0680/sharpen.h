#ifndef CAMLIBS_STV0680_SHARPEN_H
#define CAMLIBS_STV0680_SHARPEN_H

void  sharpen(int width, int height,
	unsigned char *src_region, unsigned char *dest_region,
	int sharpen_percent
);

#endif /* !defined(CAMLIBS_STV0680_SHARPEN_H) */
