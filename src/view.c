/*
 *
 * get "viewfinder" images from the wincam
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
 * $Header: E:/winc/RCS/view.c 1.1 1997/03/01 03:44:14 Derek Exp Derek $
 */
#define NDEBUG 1

#define BINARY_SEARCH 1

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <termios.h>
#include <assert.h>
#include "private.h"
#include "trace.h"

/* the row and column values at which to start the zoom, to keep the
 * image centered:
 *  zoom 0 gives a 512x246 image.
 *  zoom 1 gives a 256x147 image.
 *  zoom 2 gives a 192x98  image.
 *  zoom 3 gives a  64x49. image.
 */
int centered_col[4] = { 0, (512-256)/2, (512-192)/2, (512-64)/2 };
int centered_row[4] = { 0, (246-147)/2, (246-98)/2, (246-49)/2 };

result
winc_viewfinder_snap( cam_t cam,
	    int zoom,	    /* 1, 2, 4, or 8 */
	    int row,	    /* when zoomed: starting row: 0 to 197 */
	    int col,	    /* when zoomed: starting col: 0 to 448 */
	    byte *image,    /* room for resulting 1536 bytes, 0 to suppress */
	    int exposeiters, /* should we iterate to find correct exposure? */
	    int *exposurep, /* pointer to exposure guess (or null) 
		    (in microseconds -- real granularity is 50,000ths) */
	    int *avgp,	    /* place to put average pixel value */
	    int goal,	    /* desired brightness, 1 to 100 */
	    int *ok_p)	    /* boolean: did we achieve that brightness? */
{
    unsigned long sum;
    byte csum;
    int avg;
    unsigned int u_exp;
    unsigned int exp;
    unsigned int new_exp;

    assert(cam);

    if (exposurep && *exposurep)  /* don't allow first guess of zero */
	u_exp = (unsigned)*exposurep;
    else
	u_exp = DEFAULT_EXPOSURE;

    /* provide default goal */
    if (!goal) {
	static configured_goal = 0;
	if (!configured_goal) {  /* don't read from config every time */
	    configured_goal =
	    	cfg_number(winc_configvar("WinCamBrightness", "50"));
	}
	goal = configured_goal;
	if (goal > 100 || goal < 1)
	    goal = 42;
    }
    goal = (goal * 16 ) / 10;   /* we want numbers out of 160, not 100 */

    if (avgp)
	avg = *avgp;
    else
	avg = 0;

    switch(zoom) {
	case 8: zoom = 3;   break;
	case 4: zoom = 2;   break;
	case 2: zoom = 1;   break;
	case 1: zoom = 0;   break;
	default: zoom = 0; row = col = -1; break;
    }
    if (row == -1)
	row = centered_row[zoom];
    if (col == -1)
	col = centered_col[zoom];

    while (1) {

	TRACE("AE", __FILE__ ": exposing %d usec, avg was %d, goal is %d\n",
	    u_exp, avg, goal);

	exp = u_exp / 20;	/* microseconds to 50,000ths */
	/* the minimum exposure (0) is actually 1/100,000 */
	if (exp > 65535) exp = 65535;	/* 1.3 seconds max */

	if (winc_send_cmd(cam, Cmd_snap_view, 
			zoom, row, col/2, exp%256, exp/256, 
			((image ? -1 : -4) & 0xff), -1) != OK)
	    return NotOK;

	/* get 1536 bytes plus a checksum byte */
	if (winc_get_resp_wait(cam, 1536, image, 500 + u_exp/1000,
				&sum) != OK)
	    return NotOK;
	if (winc_get_resp_wait(cam, 1, &csum, 200, 0) != OK)
	    return NotOK;

	if ((sum & 0xff) != csum)
	    errormsg( __FILE__ ": warning viewfinder checksum incorrect\n");

	/* get viewfinder average from packet checksum,
	 *  taking into account that half the nibbles are weighted by
	 *  16, since we summed as bytes.  weight by 10, so we can look
	 *  at fractions */
	avg = 10 * sum / (1536 * 17);

#define RANGE 4

	if ((goal-RANGE <= avg && avg <= goal+RANGE) || exposeiters-- <= 0)
	    break;

	if (avg == 0 || u_exp == 0) {
	    u_exp = DEFAULT_EXPOSURE;	/* safety */
	    /* it might be 0 for legitimate reasons, like not enough light */
	    if (avg == 0)
		u_exp *= 10;
	} else {
	    new_exp = (u_exp * goal) / avg;

	    /* if we're far away, just guess linearly, otherwise, if we're already
		close, average our new guess with the current value */
	    if ( (avg < goal - RANGE*2) || (avg > goal + RANGE*2) )
		u_exp = new_exp;
	    else
		u_exp = (new_exp + u_exp) / 2;
	}
    }

    /* stretch the results from nibbles to bytes */
    {
	    byte *ip, *ep;
	    ip = &image[1536];
	    ep = &image[3072];
	    while (ip-- > image) {
		*--ep = *ip & 0xf;
		*--ep = *ip >> 4;
	    }
    }

    TRACE("a", __FILE__ ": final viewfinder avg is %d\n", avg );
    TRACE("e", __FILE__ ": exposure is %d\n", u_exp);

    if (exposurep)
	*exposurep = u_exp;

    if (avgp)
	*avgp = avg;

    if (ok_p)
	*ok_p = (avg >= goal-RANGE && avg <= goal+RANGE);

    return OK;

}

