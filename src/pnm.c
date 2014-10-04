/*
 *
 * routines to output imagemaps to PGM/PNM style output
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
 * $Header: E:/winc/RCS/pnm.c 1.1 1997/03/01 03:44:14 Derek Exp Derek $
 */

#include <stdio.h>
#include "trace.h"

/*
 * write a greyscale imagemap in PGM format
 */
int
winc_pgm_output(
    unsigned char *image,
    int rows,
    int cols,
    int maxgrey,
    FILE *out)
{

    TRACE("a", __FILE__ ": emitting PGM format\n");
    fprintf(out, "P5\n"
		"%d %d\n"
		"%d\n", cols, rows, maxgrey);

    if (ferror(out))
	return EOF;

    (void)fwrite(image, 1, rows*cols, out);

    if (ferror(out))
	return EOF;

    return 0;
}


/*
 * write an imagemap consisting of R,G,B bytes in PNM format
 */
int
winc_pnm_output(
    unsigned char *image,
    int rows,
    int cols,
    int maxcolor,
    FILE *out)
{
    TRACE("a", __FILE__ ": emitting PNM format\n");
    fprintf(out, "P6\n"
		"%d %d\n"
		"%d\n", cols, rows, maxcolor);

    if (ferror(out))
	return EOF;

    (void)fwrite(image, 3, rows*cols, out);

    if (ferror(out))
	return EOF;

    return 0;
}

/*
 * write a monochrome bitmap imagemap in PBM format
 */

static 
unsigned char bit[8] = { 0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01 };

int
winc_pbm_output(
    unsigned char *image,
    unsigned char *tmpout,  /* at least imagesize/8, to hold output bits */
    int rows,
    int cols,
    int cutoff,		/* above cutoff is white, below or equal is black */
    FILE *out)
{
    int i;
    unsigned char *op = tmpout;

    TRACE("a", __FILE__ ": emitting PBM format\n");
    fprintf(out, "P4\n"
		"%d %d\n",
		cols, rows);

    if (ferror(out))
	return EOF;

    *op = 0;
    for (i = 0; i < rows * cols; i++, image++) {
	if (*image > cutoff)
	    *op |= bit[i % 8];
	if (i % 8 == 7) {
	    op++;
	    *op = 0;
	}
    }

    (void)fwrite(tmpout, 1, (rows*cols + 7)/8, out);

    if (ferror(out))
	return EOF;

    return 0;
}

