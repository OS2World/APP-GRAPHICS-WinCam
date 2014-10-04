/*
 *
 * routine to correct for corner darkening that the camera does for us.
 *
 * Copyright (C) 1996, Paul G. Fox
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * This software was created with the help of proprietary information
 * belonging to StarDot Technologies.
 *
 * $Header: E:/winc/RCS/vignette.c 1.1 1997/03/01 03:44:14 Derek Exp Derek $
 */



#include "private.h"
#include "trace.h"

#include <math.h>
#include <string.h>


/*
 * the ccd sensors are wider than they are tall (that's why the 512x496
 * image needs to stretch to 640 wide).  the exact ratio is 7.5uM to 9.6uM,
 * or a factor of 1.28.  that is, the 4th pixel from the center in the
 * horizontal direction is really 4*1.28 vertical units from the center.
 */

/* 
 * the wincam folks tell me that the intensity falls as the square of
 * the distance from the center, and that the drop in intensity is 50% 
 * at the right edge of the screen, relative to the center.
 * the right edge is maxX (= 256 * 1.28) units away.  so, assuming
 *	    i = 1 - (d**2 * factor)
 * substituting maxX and 1/2, we get:
 *	    1/2 = 1 - (maxX ** 2) * factor
 * or
 *	    1/2 = maxX**2 * factor
 * or
 *	    factor = 1 / (2 * (maxX)**2)
 *
 * back to the beginning, we get:
 *	    i = 1 - (d**2)/(2*maxX**2))
 *
 * to brighten a pixel, we need to multiply by the inverse of that:
 *	    newpix = pix * (2*maxX**2)/(2*maxX**2 - d**2)
 *  
 */


/* find the absolute value of an integer */
static inline int absolute(int i)
{
    return (i < 0) ? -i : i;
}

/*
 * the corners of a raw image are darker than the middle, by quite
 * a bit.  this routine tries to correct for this. 
 */
void
winc_vignette_fix(
    byte *image,
    int rows,
    int cols,
    int rgb)
{
    int r, c;
    int x, y;
    int dsq, twomaxXsq, newpix;

    /* calculate 2*maxX**2 -- 
     *	 it's 256, corrected for pitch, squared, and doubled */
    twomaxXsq = (256 * 128) / 100;
    twomaxXsq *= twomaxXsq * 2;

    for (r = 0; r < rows; r++) {
	/* distance from center */
	y = absolute((rows/2) - r);

	/* adjust for image size */
	y *= 512;    /* should really be 492, but this seems close enough */
	y /= rows;

	for (c = 0; c < cols; c++) {
	    /* distance from center */
	    x = absolute((cols/2) - c);

	    /* adjust for image size */
	    x *= 512;
	    x /= cols;

	    /* correct for pitch */
	    x *= 128;
	    x /= 100;


	    /* distance is sqrt(x**2 + y**2), so this is d**2 */
	    dsq = x * x + y * y;

	    newpix = (twomaxXsq * *image) / (twomaxXsq - dsq);
	    *image++ = (newpix > 255) ? 255 : newpix;
	    if (rgb) {
		newpix = (twomaxXsq * *image) / (twomaxXsq - dsq);
		*image++ = (newpix > 255) ? 255 : newpix;
		newpix = (twomaxXsq * *image) / (twomaxXsq - dsq);
		*image++ = (newpix > 255) ? 255 : newpix;
	    }
	}
    }
}

/*
 * apply a gamma function to the image.  the parameter to a gamma function
 * is normally a number near 1.0, where 1.0 exactly leaves the image
 * unchanges.  we use 100 as the base, to avoid forcing floating point
 * on the callers.
 *
 * thanks to John Bradley's xv program for the correct equation.
 */
static byte gammamap[256];	/* the current gamma function map */
static int last_gam;		/* the gamma value which produced it */

void
winc_gamma(
    int gam,		/* gamma value, times 100 */
    byte * image,
    int rows,
    int cols,
    int rgb)
{
    double d, one_over_gam;
    int i, j;
    byte *ip;

    if (gam == 0) /* fetch default */
	gam = cfg_number(winc_configvar("WinCamGamma", "110"));

    /* 100 is "normal", i.e. image is unchanges.  0 is undefined */
    if (gam == 0 || gam == 100)
	return;

    if (gam != last_gam) {	/* recalculate the map */
	one_over_gam = 100.0 / absolute(gam);
	for (i = 0; i < 256; i++) {
	    d = pow(((double) i / 255.0), one_over_gam) * 255.0;
	    j = (int) floor(d + 0.5);
	    if (j < 0) j = 0;
	    else if (j > 255) j = 255;
	    if (gam > 0)
		gammamap[i] = j;
	    else
		gammamap[255 - i] = j;	/* entries reversed */
	}
	last_gam = gam;
    }

    for (ip = image + rows * cols * (rgb ? 3:1); ip >= image; ip--)
	*ip = gammamap[*ip];
}
