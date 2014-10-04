/*
 *
 * get real images from the camera
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
 * $Header: E:/winc/RCS/image.c 1.1 1997/03/01 03:44:14 Derek Exp Derek $
 */
#define NDEBUG 1

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <termios.h>
#include <assert.h>
#include "private.h"
#include "trace.h"

#define ROW_BY_ROW 1

static void (*snap_alert_func)(void *cookie);
static void *snap_alert_cookie;

void
winc_register_alerter(
    void (*alert_func)(void *),
    void *cookie)
{
    snap_alert_func = alert_func;
    snap_alert_cookie = cookie;
}


result
winc_image_expose( cam_t cam,
	    int scantype,   /* single(1), double(2), or noninterlaced(0) */
	    int fraction,   /* 1, 2, 4, or 8 for full, half, 1/4 and 8th size */
	    int u_exp, /* exposure (in microseconds) */
	    byte *image /* room for expected no. bytes */
	)
{
    Cmd_byte_t cmd;
    int exp;
    char *s;
    int goal = 0;

    assert(cam);

    /*
     * negative exposure is really a brightness goal
     */
    if (u_exp < 0) {
	goal = -u_exp;
	u_exp = 0;
    }

    /*
     * exposure of zero means autoexpose
     */
    if (u_exp == 0) {
	int avg  = 0;
	u_exp = 0;
	if (winc_viewfinder_snap(cam, 1, -1, -1, image, 15, &u_exp, 
						&avg, goal, 0) != OK) {
	    errormsg(__FILE__ ": failed to get autoexposure\n");
	    return NotOK;
	}
	
    }

    /* 
     * viewfinder exposures are brighter than real images.  the
     * docs say "4 times", but changing the exposure that much seems
     * like too much.
     */
    u_exp *= 2;

    /*
     * can snap single or double-interelaced scans, but that only
     * makes sense for full-sized images
     *
     * treat the interlaces as twice the lines */
    if (fraction != 1)
	scantype = 1;

    switch(scantype) {
    case 1:	cmd = Cmd_snap_single;	    s = "single";   break;
    case 2:	cmd = Cmd_snap_interlaced;  s = "double";   break;
    case 3:	/* cmd = Cmd_snap_non;		s = "non";	break; */
    default:
	errormsg( __FILE__ ": bad scantype attempted (%d)\n", scantype);
	return NotOK;
    }


    TRACE("a", __FILE__ ": snapping \"%s\" image\n", s);

    exp = u_exp / 20;	    /* microseconds to 50,000ths */
    /* the minimum exposure (0) is actually 1/100,000 */
    if (exp > 65535) exp = 65535;   /* 1.3 seconds max */

    if (winc_send_cmd(cam, cmd, exp%256, exp/256, -1) != OK)
	return NotOK;

    /* call the function the user registered as an alert function
     * i think it's better to call it just after taking the picture,
     * and not before...
     */
    if (snap_alert_func)
	(*snap_alert_func)(snap_alert_cookie);

    return OK;

}


/*
 * set up things that are determined by what fraction of an image
 * we are downloading:  repeat counts needed by the row download commands,
 * and row and column counts.
 */
result
get_repeats(int fraction, 
    int *startp, int *sendp, int *skipp, int *repeatp,
    int *colsp, int *rowsp)
{
    /* these numbers are somewhat magic.  they tell the camera that
     * for the requested row, it should send some subset of the available
     * data.  this is done because color sensors need to be picked up
     * in pairs, not singly.
     * start  - the offset into the image, divided by 2
     * send   - length of a chunk of bytes to send
     * skip   - lenght of a chunk of bytes to skip (not send)
     * repeat - repeat count for send and skip
     * send*repeat pixels are actually transferred
     */
    switch(fraction) {
    case 1: *startp=0; *sendp=1; *skipp=0;  *repeatp=512; *colsp = 512; break;
    case 2: *startp=0; *sendp=2; *skipp=2;  *repeatp=128; *colsp = 256; break;
    case 4: *startp=0; *sendp=2; *skipp=6;  *repeatp=64;  *colsp = 128; break;
    case 8: *startp=0; *sendp=2; *skipp=14; *repeatp=32;  *colsp = 64;	break;
    default:
	errormsg( __FILE__ ": bad fractional image attempted (%d)\n", fraction);
	return NotOK;
    }
    *rowsp = 492/fraction;
    return OK;
}


result
winc_image_fetch( cam_t cam,
	    int scantype,   /* single(1), double(2), or noninterlaced(0) */
	    int fraction,   /* 1, 2, 4, or 8 for full, half, 1/4 and 8th size */
	    int color,	    /* image destined for color processing */
	    byte *image,    /* room for expected no. bytes */
	    int *rows_p,    /* no. of rows in resulting image */
	    int *cols_p)	/* no. of cols in resulting image */
{
    int pairflag;
    int start, send, skip, repeat;
    int rows, cols;
    int camrow;
    byte *imagep;
    byte csum;
    unsigned long sum;
    byte star_line[512];

    if (get_repeats(fraction, 
	    &start, &send, &skip, &repeat, &cols, &rows) != OK)
	return NotOK;


    /* the camera actually has 492 rows available.  the docs say to trim
	it to 480 by taking 10 off the top, and two off the bottom.  no
	big deal either way, and all the numbers come out nice if i do so.
    */

    pairflag = 0;
    imagep = image;
    camrow = 0;
    while (camrow < 492) {
	
	TRACE("A", __FILE__ ": requesting camera row %d\n", camrow);

	if (winc_send_cmd(cam, Cmd_send_row, camrow>>1, (camrow & 1), 
			start, send, skip, repeat/2,
			-1) != OK)
	    return NotOK;

	/* get 512 bytes plus a checksum byte */
	if (winc_get_resp_wait(cam, cols, imagep,
				2000, &sum) != OK)
	    return NotOK;

	if (winc_get_resp_wait(cam, 1, &csum, 200, 0) != OK)
	    return NotOK;

	if ((sum & 0xff) != csum)
	    errormsg( __FILE__ 
		": warning: camera row %d checksum incorrect\n", camrow);

	if (winc_starfield_line(star_line, cam, camrow,
    		start, send, skip, repeat) == OK)
	    winc_starfield_fix(imagep, star_line, cols);

	imagep += cols;

	/* of the 492 possible rows (in two banks) the camera can give us,
	 * which do we _really_ want? */
	if (scantype == 2) {	/* double scan -- all of them, both banks */
	    camrow++;
	} else if (fraction <= 2) { /* full/half height -- every row in bank */
	    camrow += 2;
	} else if (!color) {	/* greyscale -- simple alternation */
	    camrow += fraction/2;
	} else {
	    /* to satisfy color processing req'ts we want to take rows from
	     * the camera in pairs.  so rather than taking alternate rows, we
	     * take 2, skip 2, or take 2, skip 6, etc.
	     */
	    camrow += 2 * ((pairflag++ & 1) ? fraction - 1 : 1);
	}
	
    }

    if (rows_p) {
	if (scantype == 2) {
	    *rows_p = 492;
	} else if (fraction <= 2) {
	    *rows_p = 246;
	} else {
	    *rows_p = 492 / fraction;
	}
    }
    if (cols_p)
	*cols_p = cols;


    TRACE("a", __FILE__ ": done snapping\n");

    return OK;
}

/* on exit from winc_image_snap, *rows_p is the no. of rows in the pixel
 * map, but _not_ necessarily the no. in the image. */
result
winc_image_snap(
	    cam_t cam,
	    int u_exp,	    /* exposure (in microseconds), 0 to download only */
	    int scantype,   /* single(1), double(2), or noninterlaced(0) */
	    int fraction,   /* 1, 2, 4, or 8 for full, half, 1/4 and 8th size */
	    int color,	    /* image destined for color processing */
	    byte *image,    /* room for expected no. bytes */
	    int *rows_p,    /* no. of rows in resulting image */
	    int *cols_p)	/* no. of cols in resulting image */
{
    result r;
    r = winc_image_expose(cam, scantype, fraction, u_exp, image);
    if (r != OK)
	return r;

    return winc_image_fetch(cam, scantype, fraction, color, 
		    image, rows_p, cols_p);
}


result
winc_black_offset(cam_t cam, int *adjp, byte *blacks)
{
    byte black[513];	/* room for checksum byte, which we'll ignore */
    unsigned long sum;
    int blackadj;
    int blackrow;

    assert(cam && adjp);

    TRACE("a", __FILE__ ": getting black adjustment\n");

    blackadj = 0;
    for (blackrow = 0; blackrow <= 1; blackrow++) {

	if (winc_send_cmd(cam, Cmd_send_row, blackrow + 510, 1, 
					0, 1, 0, 0, -1) != OK)
	    return NotOK;

	/* there are 512 bytes plus a checksum byte, or 513.
	 * we get them in several chunks, so we can use the pre-calculated
	 * sums.
	 * if the caller wants the values, we use their buffer.
	 */
	/* bytes 0 through 245 */
	if (winc_get_resp(cam, 246, 
		blacks ? &blacks[0 + 256 * blackrow] : black,
		&sum) != OK)
	    return NotOK;

	blackadj += sum;

	/* bytes 246 through 512 */
	if (winc_get_resp(cam, 267,
		blacks ? &blacks[246 + 256 * blackrow] : black,
		&sum) != OK)
	    return NotOK;

    }

    blackadj /= (2 * 246);

    *adjp = blackadj;

    TRACE("a", __FILE__ ": black adjustment is %d\n", blackadj);

    return OK;
}
