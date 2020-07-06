/* fswebcam - Small and simple webcam for *nix                */
/*============================================================*/
/* Copyright (C)2005-2011 Philip Heron <phil@sanslogic.co.uk> */
/*                                                            */
/* This program is distributed under the terms of the GNU     */
/* General Public License, version 2. You may use, modify,    */
/* and redistribute it under the terms of this license. A     */
/* copy should be included with this source.                  */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdint.h>
#include "fswebcam.h"
#include "src.h"

int fswc_add_image_bayer(avgbmp_t *dst, uint8_t *img, uint32_t length, uint32_t w, uint32_t h, int palette)
{
	uint32_t x = 0, y = 0;
	uint32_t i = w * h;
	
	if(length < i) return(-1);
	
	/* SBGGR8 bayer pattern:
	 * 
	 * BGBGBGBGBG
	 * GRGRGRGRGR
	 * BGBGBGBGBG
	 * GRGRGRGRGR
	 * 
	 * SGBRG8 bayer pattern:
	 * 
	 * GBGBGBGBGB
	 * RGRGRGRGRG
	 * GBGBGBGBGB
	 * RGRGRGRGRG
	 *
	 * SGRBG8 bayer pattern:
	 *
	 * GRGRGRGRGR
	 * BGBGBGBGBG
	 * GRGRGRGRGR
	 * BGBGBGBGBG
	*/
	
	while(i-- > 0)
	{
		uint8_t *p[8];
		uint8_t hn, vn, di;
		uint8_t r, g, b;
		int mode;
		
		/* Setup pointers to this pixel's neighbours. */
		p[0] = img - w - 1;
		p[1] = img - w;
		p[2] = img - w + 1;
		p[3] = img - 1;
		p[4] = img + 1;
		p[5] = img + w - 1;
		p[6] = img + w;
		p[7] = img + w + 1;
		
		/* Juggle pointers if they are out of bounds. */
		if(!y)              { p[0]=p[5]; p[1]=p[6]; p[2]=p[7]; }
		else if(y == h - 1) { p[5]=p[0]; p[6]=p[1]; p[7]=p[2]; }
		if(!x)              { p[0]=p[2]; p[3]=p[4]; p[5]=p[7]; }
		else if(x == w - 1) { p[2]=p[0]; p[4]=p[3]; p[7]=p[5]; }
		
		/* Average matching neighbours. */
		hn = (*p[3] + *p[4]) / 2;
		vn = (*p[1] + *p[6]) / 2;
		di = (*p[0] + *p[2] + *p[5] + *p[7]) / 4;
		
		/* Calculate RGB */
		if(palette == SRC_PAL_SBGGR8 ||
		   palette == SRC_PAL_SRGGB8 ||
		   palette == SRC_PAL_BAYER) {
			mode = (x + y) & 0x01;
		} else {
			mode = ~(x + y) & 0x01;
		}
		
		if(mode)
		{
			g = *img;
			if(y & 0x01) { r = hn; b = vn; }
			else         { r = vn; b = hn; }
		}
		else if(y & 0x01) { r = *img; g = (vn + hn) / 2; b = di; }
		else              { b = *img; g = (vn + hn) / 2; r = di; }
		
		if(palette == SRC_PAL_SGRBG8 ||
		   palette == SRC_PAL_SRGGB8)
		{
			uint8_t t = r;
			r = b;
			b = t;
		}
		
		*(dst++) += r;
		*(dst++) += g;
		*(dst++) += b;
		
		/* Move to the next pixel (or line) */
		if(++x == w) { x = 0; y++; }
		img++;
	}
	
	return(0);
}

static inline uint8_t clip(float v)
{
	if (v > 255.)
		return 255;
	else
		return (uint8_t)v;
}

uint8_t interpol_r(void *raw_data, unsigned int width, unsigned int height, unsigned int x, unsigned int y)
{
	uint16_t *samples = raw_data;
	float value;
	float range_max = (1 << 10) - 1;
	unsigned int xp, yp;
	unsigned int xn, yn;

	if ((x % 2 == 1) && (y % 2 == 1)) {
		value = samples[y * width + x];
		goto ret;
	}

	if (x < 2 || x >= (width - 1))
		return 0;
	if (y < 2 || y >= (height - 1))
		return 0;

	xp = (x - 2) | 1;
	yp = (y - 2) | 1;

	xn = x | 1;
	yn = y | 1;

	value = samples[yp * width + xp];

ret:
	return clip(255. * value / range_max);
}

uint8_t interpol_g(void *raw_data, unsigned int width, unsigned int height, unsigned int x, unsigned int y)
{
	uint16_t *samples = raw_data;
	float value;
	float range_max = (1 << 10) - 1;
	unsigned int xp, yp;
	unsigned int xn, yn;

	if (((x % 2 == 1) && (y % 2 == 0)) || ((x % 2) == 0 && (y % 2 == 1))) {
		value = samples[y * width + x];
		goto ret;
	}

	if (x < 2 || x >= (width - 1))
		return 0;
	if (y < 2 || y >= (height - 1))
		return 0;

	xp = x - 1;
	yp = y;

	xn = x + 1;
	yn = y;

	value = samples[yp * width + xp];

ret:
	return clip(255. * value / range_max);
}

uint8_t interpol_b(void *raw_data, unsigned int width, unsigned int height, unsigned int x, unsigned int y)
{
	uint16_t *samples = raw_data;
	float value;
	float range_max = (1 << 10) - 1;
	unsigned int xp, yp;
	unsigned int xn, yn;

	if (x % 2 == 0 && y % 2 == 0) {
		value = samples[y * width + x];
		goto ret;
	}

	if (x < 2 || x >= (width - 1))
		return 0;
	if (y < 2 || y >= (height - 1))
		return 0;

	xp = x & ~1;
	yp = y & ~1;

	xn = x & ~1;
	yn = y & ~1;

	value = samples[yp * width + xp];

ret:
	return clip(255. * value / range_max);
}

void fswc_add_image_10bitsbayer(void *raw_data, void *rgb_data, unsigned int width, unsigned int height)
{
	unsigned int x, y;
	uint16_t *samples = raw_data;
	uint16_t *pixels = rgb_data;
	float range_max = (1 << 10) - 1;
	uint8_t r = 0, g = 0, b = 0;

	// BGGR
	for (y = 0; y < height - 1; y++) {
		for (x = 0; x < width; x++) {

			r = interpol_r(raw_data, width, height, x, y);
			g = interpol_g(raw_data, width, height, x, y);
			b = interpol_b(raw_data, width, height, x, y);


			*pixels++ = r;
			*pixels++ = g;
			*pixels++ = b;

			samples++;
		}
	}
}

