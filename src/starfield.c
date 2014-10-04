/*
 *
 * correct the star-studded appearance of low-light images
 *
 * Copyright (C) 1996, Paul G. Fox
 *
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
 * $Header: E:/winc/RCS/starfield.c 1.1 1997/03/01 03:44:14 Derek Exp Derek $
 */
#define NDEBUG 1

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <assert.h>
#include "private.h"
#include "trace.h"

/*
 * the starfield image should be created by saving a double-scan full-height
 * image taken with the lens cap on, in PBM (monochrome bitmap) format.
 */

result
winc_read_starfield(cam_t cam)
{
    char *fn;
    FILE *f;
    int pbm, cols, rows;
    char newl;

    if (cam->starfield)
    	return OK;

    /* give the starfield snapper way to suppress this stuff */
    fn = winc_configvar("NoStarfield", "");
    if (fn && *fn)
	return NotOK;

    /* get the name of the starfield file */
    fn = winc_configvar("WinCamStarfield", "");
    if (!fn || *fn  == 0)
    	return NotOK;

    f = fopen(fn, "r");
    if (!f) {
	TRACE("a", __FILE__ ": cannot open %s\n", fn);
    	return NotOK;
    }

    TRACE("a", __FILE__ ": reading starfield from %s\n", fn);

    if (4 != fscanf(f, "P%d %d %d%c", &pbm, &cols, &rows, &newl) ||
	    pbm != 4 || newl != '\n' ||
	    cols != 512 || rows != 492) {
	TRACE("a", __FILE__ ": bad stuff in starfield file\n");
    	return NotOK;
    }

    cam->starfield = (byte *)malloc(512 * 492 / 8);
    if (!cam->starfield) {
	TRACE("a", __FILE__ ": couldn't malloc starfield\n");
	fclose(f);
	return NotOK;
    }

    if (fread(cam->starfield, 512 * 492 / 8, 1, f) != 1) {
	TRACE("a", __FILE__ ": didn't read starfield\n");
	fclose(f);
	free(cam->starfield);
	cam->starfield  = 0;
	return NotOK;
    }
    return OK;
}

static 
unsigned char bit[8] = { 0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01 };
/*
 * return 0 or 1 depending on whether bit N is set in a bit array
 * (the bit order matches the pbm monochrome bit ordering)
 */
#define bit2byte(bits,n) (bits[n/8] & bit[n%8]) ? 1 : 0;

result
winc_starfield_line(byte *oline, cam_t cam, int row,
    int start, int send, int skip, int repeat)
{
    int rep, i, o, snd;
    byte *starf_bits;
    result ret;

    ret = winc_read_starfield(cam);
    if (ret != OK)
    	return ret;

    /*  the starfield is always 512 bits across by 492 bits high */
    starf_bits = &cam->starfield[row * (512 / 8)];

    i = start;	/* first input byte */
    o = 0;  	/* first output byte */
    for (rep = 0; rep < repeat; rep++) {
	for (snd = 0; snd < send; snd++) {
	    oline[o++] = bit2byte(starf_bits,i);
	    i++;
	}
	i += skip;
    }

    assert(o == repeat * send);

    return OK;
}

/*
 * correct an image row for starfield effect by averaging "bad" pixels
 * with their neighbors.  the "neighbor" is two distant, since not all
 * pixels are the same in a raw image (i.e. red/blue/red/blue or
 * yel/grn/yel/grn).  or something like that
 */
void
winc_starfield_fix(byte *line, byte *starf_line, int len)
{
    int i;

    /* make sure the first two and last two bytes have valid values */
    if (starf_line[0]) {
	for (i = 2; i < len && starf_line[i]; i += 2) /* loop */ ;
	if (i >= len)
	    return;  /* whole bad row */
	line[0] = line[i];
    }
    if (starf_line[1]) {
	for (i = 3; i < len && starf_line[i]; i += 2) /* loop */ ;
	if (i >= len)
	    return;  /* whole bad row */
	line[1] = line[i];
    }
    if (starf_line[len-1]) {
	for (i = len-3; i >= 0 && starf_line[i]; i -= 2) /* loop */ ;
	if (i < 0)
	    return;  /* whole bad row */
	line[len-1] = line[i];
    }
    if (starf_line[len-2]) {
	for (i = len-4; i >= 0 && starf_line[i]; i -= 2) /* loop */ ;
	if (i < 0)
	    return;  /* whole bad row */
	line[len-2] = line[i];
    }
    	
    /* now average the rest */
    for (i = 2; i < len - 2; i++) {
	if (starf_line[i]) {
	    int val;
	    /* use the rearward neighbor */
	    val = line[i-2];  /* we know this will work */
	    /* look forward, use the forward neighbor if it's okay */
	    if (!starf_line[i+2]) {
		val += line[i+2];
		val /= 2;
	    }
	    line[i] = val;
	}
    }
}

