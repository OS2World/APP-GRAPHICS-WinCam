/*
 *
 * routines to convert raw wincam imagemaps to real greyscale or rgb
 * scaling is in here too.
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
 * $Header: E:/winc/RCS/convert.c 1.1 1997/03/01 03:44:14 Derek Exp Derek $
 */
#define NDEBUG 1

#include "private.h"
#include "trace.h"
#include "color.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

#define EVEN(i) (((i)&1) == 0)
#define ODD(i)	(((i)&1) != 0)

#define TOLOWER(c) (isupper((unsigned char)c) ? tolower((unsigned char)c) : (c))


int strncasecmp (char* a, char * b, int n)
{
	char *p =a;
	char *q =b;
	
	for(p=a, q=b;; p++, q++) {
	    int diff;
	    if (p == a+n) return 0;	/*   Match up to n characters */
	    if (!(*p && *q)) return *p - *q;
	    diff = TOLOWER(*p) - TOLOWER(*q);
	    if (diff) return diff;
	}
}



/*
 * grey is pretty easy -- every pair of pixels gets averaged horizontally
 */
void
winc_grey_convert(
    byte *image,	/* raw data from camera */
    byte *convimage,	/* data with adjacent pixels averaged */
    int *rows_p,
    int *cols_p,
    int offs,
    int scantype,
    int fraction,
    struct winc_image_adjust *ia)
{
    int b;
    int nr, nc;
    int rows, cols;
    byte *outp;

    TRACE("g", __FILE__ ": massaging grey image\n");

    rows = *rows_p;
    cols = *cols_p;

    if (fraction != 1)
	scantype = 1;

    outp = convimage;
    nr = 0;
    while (nr < rows) {
	for (nc = 0; nc < cols; nc++) {
	    b = *image++;
	    b -= offs;
	    if (nr < cols) { /* not in last column? */
		b += *image;
		b -= offs;
		b /= 2; /* average the two values */
	    }
	    if (b < 0) {
		b = 0;
		TRACE("g", __FILE__ ": clamped offset adjust at zero\n");
	    }
	    *outp++ = b;
	}

	nr++;
	/* skip every other row for full-height single-scan images */
	if (scantype == 1 && fraction == 1)
	    outp += cols;
    }

    /* need to go back and duplicate the odd rows from the even ones */
    if (scantype == 1 && fraction == 1) {
	outp = convimage;
	for (nr = 0; nr < rows; nr++) {
	    memcpy(outp + cols, outp, cols);
	    outp += 2*cols;
	}
	*rows_p = rows * 2;
    }

    *rows_p  = 492/fraction;

}

/*
 * notes on color levels.  the default values here are those suggested
 * as defaults in the comments in color.c.  the StarDot software itself
 * adjusts these values with 3 sliders, labelled in percentages, whose
 * ranges go from 0 to very high values, like 200 or 300.  the numbers
 * are labelled as a percentage, but i think they're actuall just absolute
 * values, where 128, 75, 128 are the "normal" values.
 *
 * for various lighting scenarios, the default slider values are:
 *
 *	lightsource		red	green	    blue
 *  --------------------------------------------------------
 *	sunlight		138	80 (136)    117
 *	flourescent		138	88 (149)    116
 *	tungsten		98	78 (132)    157
 *	halogen			103	96 (163)    153
 *	studio			112	77 (131)    142
 *	daylight w/ filter	104	65 (110)    150
 *
 *  green values in parentheses are divided by .59, to correct for
 *  the 75 <--> 128 aberration.  that is, a "standard" line would
 *  read:  			100	59 (100)    100
 *
 */

/*
 * fill in the image adjustment descriptor.  most of the work is for the
 * color balance entries.  note that we use 100/100/100 as the default
 * values.  i've modified color.c to do the adjustment of green down by 59%
 * to match stardot's recommendation. 
 *
 * i've normalized the numbers in the chart above so that a) they're out of
 * 100, rather than 128, b) the average of the three numbers is always 128. 
 * i don't know if that was right or not...
 */

void
winc_image_adjustments(
    struct winc_image_adjust *ia,
    char *s,
    int widen,
    int vig)
{
    char newname[40];
    char *ns;

    assert(ia);

    ia->widen = widen;
    ia->vignette_correction = vig;

    /* the input string is either the name of a predefined set
     * of adjustments, or a parenthesized triple, e.g. "(105, 76,54)"
     */
    /* if s is NULL or "", we do defaults. */
    if (!s || !*s) {
	s = winc_configvar("WinCamColor", "");
	if (!*s) /* i.e. we got back the default val */
	    goto defaults;
    }

retry:
    if (s[0] == '(') {
	char rparen[2];
	if (sscanf(s, "( %d , %d , %d %1s",
			&ia->red, &ia->green, &ia->blue, rparen) != 4 ||
		rparen[0] != ')')
	    goto error_defaults;
    } else if (strncasecmp(s, "sunlight", strlen(s)) == 0) {
	ia->red =   (136*100)/128;	/* 138; */
	ia->green = (134*100)/128;	/* 136; */
	ia->blue =  (115*100)/128;	/* 117; */
    } else if (strncasecmp(s, "flourescent", strlen(s)) == 0) {
	ia->red =   (131*100)/128;	/* 138; */
	ia->green = (142*100)/128;	/* 149; */
	ia->blue =  (111*100)/128;	/* 116; */
    } else if (strncasecmp(s, "tungsten", strlen(s)) == 0 ||
		strncasecmp(s, "incandescent", strlen(s)) == 0) {
	ia->red =   ( 98*100)/128;
	ia->green = (132*100)/128;
	ia->blue =  (157*100)/128;
    } else if (strncasecmp(s, "halogen", strlen(s)) == 0) {
	ia->red =   ( 94*100)/128;	/* 103; */
	ia->green = (149*100)/128;	/* 163; */
	ia->blue =  (140*100)/128;	/* 153; */
    } else if (strncasecmp(s, "studio", strlen(s)) == 0) {
	ia->red =   (112*100)/128;
	ia->green = (131*100)/128;
	ia->blue =  (142*100)/128;
    } else if (strncasecmp(s, "filter", strlen(s)) == 0) {
	ia->red =   (110*100)/128;	/* 104; */
	ia->green = (116*100)/128;	/* 110; */
	ia->blue =  (158*100)/128;	/* 150; */
    } else if (strncasecmp(s, "defaults", strlen(s)) == 0) {
	goto defaults;
    } else {
	/*
	 * aliases:  allow the user to set up their own color balances,
	 * by setting variables named "WinCamColor_XXXX" where XXXX is
	 * matched exactly against the passed-in string.  Most of the
	 * time the invocation of winc will be something like "winc -l
	 * Office", and we should find something like
	 *	    "WinCamColor_Office=(100,100,120)", 
	 * but we might as well allow for
	 *	    "WinCamColor_Office=flourescent"
	 * as well, so we just fully reparse the value we get.  (Note
	 * that since config variable names are case-sensitive, you
	 * can't use "winc -l office" unless define "WinCamColor_office".
	 */
	ns = s;
	sprintf(newname, "WinCamColor_%.*s", 40 - 15, ns);
	s = winc_configvar(newname, "");
	if (!s || !*s) {
	    s = newname;
	    goto error_defaults;
	}
	/* we found something.  retry the "parse". */
	goto retry;
    }
    TRACE("g", __FILE__ ": lighting r %d g %d b %d\n",
    				ia->red, ia->green, ia->blue);
    return;

error_defaults:
    if (s)
	errormsg("warning: bad color adjust: %s, using defaults\n", s);
defaults:
    ia->red = cfg_number(winc_configvar("WinCamRedLevel", "100"));
    ia->green = cfg_number(winc_configvar("WinCamGreenLevel", "100"));
    ia->blue = cfg_number(winc_configvar("WinCamBlueLevel", "100"));
    TRACE("g", __FILE__ ": default lighting r %d g %d b %d\n",
    				ia->red, ia->green, ia->blue);
}

/* set up most of the colordata struct expected by sharpcolor() */
void
setup_colordata(struct colordata *cdp, int cols, int color, int fraction,
    struct winc_image_adjust *ia)
{

    /* the sharpcolor routines wants these based on 128 */
    cdp->redlev = (ia->red * 128)/100;
    cdp->greenlev = (ia->green * 128)/100;
    cdp->bluelev = (ia->blue * 128)/100;

    cdp->bufwidth = cols;
    cdp->bufheight = (fraction == 1) ? 246 : 492/fraction;
    cdp->intensity = 336;	/* suggested 336 */
    cdp->pixlimits[0] = 3;  /* suggested 3 */
    cdp->pixlimits[1] = 8;  /* suggested 8 */
    cdp->hazelevel = 8;     /* suggested 8 */
    cdp->diflimit  = 42;	/* suggested 42 */

    cdp->options = 0
		| OPT_COLOR_AVG 
		/* | OPT_COLOR_CROP */
		| OPT_COLOR_TRACING 
		| OPT_EDGE_CORRECTION
		;
    if (ia->widen)
	cdp->options |= OPT_HORIZ_STRETCH;
    if (ia->vignette_correction)
	cdp->options |= OPT_VIGNETTE_CORRECTION;

    cdp->mode = RESET_LINESTATS;
    sharpcolor(cdp);

    cdp->mode = color ? COLOR_INTERLACED_24 : GREY_INTERLACED_8;

}

byte tempoutput[11*512*3*2];


void
winc_color_convert(
    byte *image,	/* image from winc_image_snap() or winc_image_fetch() */
    byte *convimage,	/* where to put the rgb result */
    int *rows_p,
    int *cols_p,
    int offs,
    int color,
    int scantype,
    int fraction,
    struct winc_image_adjust *ia)
{
    int rows, cols, outcols, nr, camrow;
    struct colordata cd;
    byte *inp, *outp;
    int rgb;

    TRACE("g", __FILE__ ": calling stardot code\n");

    rows = *rows_p;
    cols = *cols_p;

    rgb = (color) ? 3 : 1;

    /* full height single and double-scan images have fraction == 1 here */
    setup_colordata(&cd, cols, color, fraction, ia);

    if (fraction != 1)
	scantype = 1;

    if (cd.options & OPT_HORIZ_STRETCH)
	outcols = (cols*100)/80;
    else
	outcols = cols;

    cd.blacklevs[0] = offs;
    cd.blacklevs[1] = offs;
    cd.temp_off = tempoutput;

    nr = 0;
    camrow = 0;
    inp = image;
    outp = convimage;
    while (nr < rows) {
	/* fake up line number the color code expects, in bank/offset form */
	cd.linenum = ((camrow & 1) * 256) + (camrow >> 1);
	cd.input_off = inp;
	cd.result_off = outp;

	sharpcolor(&cd);

	nr++;
	if (scantype == 2)
	    camrow += 1;    /* "use" both banks */
	else
	    camrow += 2;    /* use first bank only */
	inp += cols;
	outp += rgb*outcols;
	/* skip every other row for full-height single-scan images */
	if (scantype == 1 && fraction == 1)
	    outp += rgb*outcols;

    }

    /* need to go back and duplicate the odd rows from the even ones */
    if (scantype == 1 && fraction == 1) {
	outp = convimage;
	for (nr = 0; nr < rows; nr ++) {
	    memcpy(outp + rgb*outcols, outp, rgb*outcols);
	    outp += 2*rgb*outcols;
	}
	*rows_p = rows * 2;
    }

    *rows_p = 492/fraction;
    *cols_p = outcols;

}


result
winc_snap_and_process(
    cam_t cam,
    int u_exp, 		/* exposure (in microseconds) */
    byte *image,	/* image from winc_image_snap() or winc_image_fetch() */
    byte *convimage,	/* where to put the rgb result */
    int *rows_p,
    int *cols_p,
    int color,
    int scantype,
    int fraction,
    struct winc_image_adjust *ia)
{
    int rows, cols, outrows, outcols, camrow, processrow;
    int start, send, skip, repeat;
    struct colordata cd;
    byte *inp, *outp;
    int blackadj;
    int pairflag;
    int rgb;
    result r;
    byte csum;
    unsigned long sum;
    int got_starfield;
    byte star_line[512];

    TRACE("g", __FILE__ ": calling stardot code\n");

    if (get_repeats(fraction, 
	    &start, &send, &skip, &repeat, &cols, &rows) != OK)
	return NotOK;

    /* full height single and double-scan images have fraction == 1 here */
    setup_colordata(&cd, cols, color, fraction, ia);

    if (fraction != 1)
	scantype = 1;

    if (cd.options & OPT_HORIZ_STRETCH)
	outcols = (cols*100)/80;
    else
	outcols = cols;

    outrows = 492/fraction;

    if (winc_black_offset(cam, &blackadj, 0 ) != OK) {
	errormsg(__FILE__ ": failed to get black adjustment\n");
	return NotOK;
    }
    r = winc_image_expose(cam, scantype, fraction, u_exp, image);
    if (r != OK)
	return r;

    rgb = (color) ? 3 : 1;

    cd.blacklevs[0] = blackadj;
    cd.blacklevs[1] = blackadj;
    cd.temp_off = tempoutput;

    pairflag = 0;
    camrow = 0;
    processrow = 0;
    inp = image;
    outp = convimage;

    /* send the initial get row command */
    if (winc_send_cmd(cam, Cmd_send_row, camrow>>1, (camrow & 1), 
		    start, send, skip, repeat/2,
		    -1) != OK)
	return NotOK;

    while (1) {

	got_starfield = (winc_starfield_line(star_line, cam, camrow,
				    start, send, skip, repeat) == OK);

	/* get 512 bytes plus a checksum byte */
	if (winc_get_resp_wait(cam, cols, inp, 2000, &sum) != OK)
	    return NotOK;

	if (winc_get_resp_wait(cam, 1, &csum, 200, 0) != OK)
	    return NotOK;

	if ((sum & 0xff) != csum)
	    errormsg( __FILE__ 
		": warning: camera row %d checksum incorrect\n", camrow);

	if (got_starfield)
	    winc_starfield_fix(inp, star_line, cols);

	/* fake up line number the color code expects, in bank/offset form */
	cd.linenum = ((processrow & 1) * 256) + (processrow >> 1);
	cd.input_off = inp;
	cd.result_off = outp;


	inp += cols;
	outp += rgb*outcols;
	/* skip every other row for full-height single-scan images */
	if (scantype == 1 && fraction == 1)
	    outp += rgb*outcols;

	/* of the 492 possible rows (in two banks) the camera can give us,
	 * which do we _really_ want? */
	if (scantype == 2) {	/* double scan -- all of them, both banks */
	    camrow++;
	    processrow++;
	} else if (fraction <= 2) { /* full/half height -- every row in bank */
	    camrow += 2;
	    processrow += 2;
	} else if (!color) {	/* greyscale -- simple alternation */
	    camrow += fraction/2;
	    processrow += 2;
	} else {
	    /* to satisfy color processing req'ts we want to take rows from
	     * the camera in pairs.  so rather than taking alternate rows, we
	     * take 2, skip 2, or take 2, skip 6, etc.
	     */
	    camrow += 2 * ((pairflag++ & 1) ? fraction - 1 : 1);
	    processrow += 2;
	}

	if (camrow >= 492)
	    break;

	if (winc_send_cmd(cam, Cmd_send_row, camrow>>1, (camrow & 1), 
			start, send, skip, repeat/2,
			-1) != OK)
	    return NotOK;

	/* while that data is arriving, process the previous row */
	sharpcolor(&cd);

    }

    /* one more row to process */
    sharpcolor(&cd);

    /* need to go back and duplicate the odd rows from the even ones */
    if (scantype == 1 && fraction == 1) {
	outp = convimage;
#if 1 || COPY
	for (camrow = 0; camrow < 246; camrow++) {
	    memcpy(outp + rgb*outcols, outp, rgb*outcols);
	    outp += 2*rgb*outcols;
	}
#else
	for (camrow = 0; camrow < 245; camrow++) {
	    int i;
	    for (i = 1; i < rgb*outcols - 1; i++) {
		*(outp + i + rgb*outcols) =  (
		*(outp + i + 0 * rgb*outcols - rgb) +
		2 * *(outp + i + 0 * rgb*outcols    ) +
		*(outp + i + 0 * rgb*outcols + rgb) +
		*(outp + i + 2 * rgb*outcols - rgb) +
		2 * *(outp + i + 2 * rgb*outcols    ) +
		*(outp + i + 2 * rgb*outcols + rgb) ) / 8;
	    }
	    outp += 2*rgb*outcols;
	}
	memcpy(outp + rgb*outcols, outp, rgb*outcols);
	outp += 2*rgb*outcols;
#endif
    }
    *rows_p = outrows;
    *cols_p = outcols;

    return OK;

}



/* 
 * the wincam images are narrow by 20%, i.e. they need to be expanded
 * by 25% (512 becomes 640) to make the scene aspect ratio right.
 * this does that, in a simple, hard-coded way.
 *
 * there are 5 new pixels for every 4 old pixels.
 *
 *    0    1	2    3	  4    5    6	 7    8    9
 *  |	 |    |    |	|    |	  |    |    |	 | original pixels
 *  --------------------------------------------------------------
 *  |	|   |	|   |	|   |	|   |	|   |	| wider image pixels
 *    0   1   2   3   4   5   6   7   8   9  10  11
 *
 * new pixel 0 is .8 * old pixel 0
 * new pixel 1 is .2 * old pixel 0 plus .6 * old pixel 1.
 * etc.
 *
 * we start at the end and work backwards so we can do it in-place.
 *
 * note that this results in a blurring of the image.  it could be
 * argued that it would be better to preserve bits 1 through 4, and just
 * create a 5th by duplicating 4, or perhaps averaging between 4 and 6.
 * indeed, that's exactly what StarDot's code does.
 */

int
winc_aspect_fix(
    byte *image,
    int rows,
    int origcols,
    int rgb)
{
    int newcols = 5 * origcols / 4;	/* new image is 125% as wide as old */
    int nc;
    byte *widerimage;

    if (!rgb) {
	widerimage = image + rows * newcols;
	image = image + rows * origcols;

	while (rows--) {
	    for (nc = newcols; nc > 0; nc -= 5) {
		image -= 4;
		widerimage -= 5;
		widerimage[4] = (4 * image[3]		    ) / 4;
		widerimage[3] = (1 * image[3] + 3 * image[2]) / 4;
		widerimage[2] = (2 * image[2] + 2 * image[1]) / 4;
		widerimage[1] = (3 * image[1] + 1 * image[0]) / 4;
		widerimage[0] = (		4 * image[0]) / 4;

	    }
	}
    } else { /* same as above, but for each of three colors */
#define B 3
	widerimage = image + rows * newcols * B;
	image = image + rows * origcols * B;

	while (rows--) {
	    for (nc = newcols; nc > 0; nc -= 5) {
		image -= 4*B;
		widerimage -= 5*B;
		widerimage[4*B+2] = (4 * image[3*B+2]			) / 4;
		widerimage[4*B+1] = (4 * image[3*B+1]			) / 4;
		widerimage[4*B+0] = (4 * image[3*B+0]			) / 4;
		widerimage[3*B+2] = (1 * image[3*B+2] + 3 * image[2*B+2]) / 4;
		widerimage[3*B+1] = (1 * image[3*B+1] + 3 * image[2*B+1]) / 4;
		widerimage[3*B+0] = (1 * image[3*B+0] + 3 * image[2*B+0]) / 4;
		widerimage[2*B+2] = (2 * image[2*B+2] + 2 * image[1*B+2]) / 4;
		widerimage[2*B+1] = (2 * image[2*B+1] + 2 * image[1*B+1]) / 4;
		widerimage[2*B+0] = (2 * image[2*B+0] + 2 * image[1*B+0]) / 4;
		widerimage[1*B+2] = (3 * image[1*B+2] + 1 * image[0*B+2]) / 4;
		widerimage[1*B+1] = (3 * image[1*B+1] + 1 * image[0*B+1]) / 4;
		widerimage[1*B+0] = (3 * image[1*B+0] + 1 * image[0*B+0]) / 4;
		widerimage[0*B+2] = (			4 * image[0*B+2]) / 4;
		widerimage[0*B+1] = (			4 * image[0*B+1]) / 4;
		widerimage[0*B+0] = (			4 * image[0*B+0]) / 4;
	    }
	}
    }
    return newcols;
}

#define absval(a) (((a) >= 0) ? (a) : -(a))

int
winc_compare_image(
    byte *image1,  /* the two images being */
    byte *image2,  /*	compared */
    int rows,	   /* rows of pixels */
    int cols,	   /* columns of pixels */
    int maxdiff,   /* percent pixels changed for image to be diff */
    int thresh,    /* amount pixel val must, as percentage of max */
    int maxval,    /* maximum pixel value */
    int rgb)	   /* boolean -- three bits per pixel, or one */
{
    int i;
    int d;
    int diffcount = 0;

    /* convert percentages to absolutes */
    maxdiff = (maxdiff * (rows * cols)) / 100;
    thresh = (thresh * (maxval+1)) / 100;

    for (i = rows * cols * (rgb ? 3 : 1); i > 0; i--) {
	d = *image1 - *image2;
	if (absval(d) > thresh) {
	    if (++diffcount > maxdiff)
		return 0;
	}
	image1++;
	image2++;

    }
    return 1;
}


#ifdef HARDLY_WORTH_SAVING  /* my early attempt at color processing */
/*
 * color is tricky.  here's how i understand it.
 * the sensors in the CCD array are arranged in clusters of four, and
 * each of the four is sensitive to a different color.
 * the pairs in the even rows can be combined simply to get red, the pairs
 * in the odd rows can be combined simply to get blue, and if you combine
 * all four, and subtract red and blue, you'll get something resembling
 * green.  :-)	that all works pretty well, and for solid color scenes, i
 * think the code below (or something simpler) would do just fine.  for
 * each output rgb pixel on the screen, it calculates the r, g, and b
 * values from the sensor cluster of which it is the upper left element.
 * the far right column and the bottom row are treated specially, and are
 * simply copies of the column (or row) previous.
 * 
 * white balance should be achieved by tuning the equations for creating
 * red, blue, and green.
 *
 * the big trouble comes when an image transition falls in the middle of
 * one of these clusters of 4 sensors.	then you calculate a red value, for
 * instance, from two sensors that are actually "sensing" unrelated parts
 * of the image, and you may get a bright (often red) spot that shouldn't
 * be there.
 *
 * my lame initial attempt at fixing this modifies the basic calculation
 * thusly:  for each pixel, calculate the red and blue values as usual from
 * the cluster to the right, but also calculate them from the cluster to
 * the left.  chose the value wiht the lower red and blue values.  this
 * gets rid of "spikes", but it also seems to remove a lot of color
 * information.  it also doesn't do anything about vertical transitions.
 * consider it an experiment-in-progress.
 *
 * oh -- the above algorithms are complicated by the fact that the data
 * is in "banks" returned from the two scans, and that their are two sets
 * of sensors interleaved.  so, taking that interleaving into account,
 * the sensors look like:
 * 
 *
 *	cyg  yemg cyg  yemg cyg  yemg ... 
 *	cyg  yemg cyg  yemg cyg  yemg ... 
 *	cymg yeg  cymg yeg  cymg yeg  ... 
 *	cymg yeg  cymg yeg  cymg yeg  ... 
 *	cyg  yemg cyg  yemg cyg  yemg ... 
 *	cyg  yemg cyg  yemg cyg  yemg ... 
 *	cymg yeg  cymg yeg  cymg yeg  ... 
 *	cymg yeg  cymg yeg  cymg yeg  ... 
 *	.
 *	.
 *	.
 *
 * the "official" equations look something like:
 *	red = yemg - .71 * cyg.
 *	blue = cymg - .66 * yeg.
 *	green = ((yemg+cyg+cymg+yeg)/2 - red - blue) / 2.
 * but as you can see below, i've played with that a lot.
 *
 * i'm not happy with this at all.  but it only took a day or two
 * so far.
 */

static void putcolors(int, int, int, int, int, int, int, int, byte *);

void
winc_color_convert(
    byte *image,
    byte *convimage,
    int rows,
    int cols,
    int offs,
    int scantype,
    int fraction,
    struct winc_image_adjust *ia)
{
    int r, c;
    int cyg = 0, yemg = 0, cymg = 0, yeg = 0;
    int cyg2 = 0, yemg2 = 0, cymg2 = 0, yeg2 = 0; 
    int u, l, ul, ur, ll, lr;

    TRACE("g", __FILE__ ": emitting PGM format\n");

    /* this could _clearly_ be sped way up.  if i thought it
     * was even close to "right", i might even bother.
     */
    for (r = 0; r < rows - 1 ; r++) {
	for (c = 0; c < cols - 1 ; c++, image++) {
	    u = *image;
	    l = *(image + 2 * cols);
	    ur = *(image + 1);
	    lr = *(image + 2 * cols + 1);
	    if (c) {
		ul = *(image - 1);
		ll = *(image + 2 * cols - 1);
	    } else {
		ul = ur;
		ll = lr;
	    }
	    if (offs) {
		u  -= offs; if (u  < 0) u  = 0;
		l  -= offs; if (l  < 0) l  = 0;
		ur -= offs; if (ur < 0) ur = 0;
		lr -= offs; if (lr < 0) lr = 0;
		ul -= offs; if (ul < 0) ul = 0;
		ll -= offs; if (ll < 0) ll = 0;
	    }
	    if (EVEN(c) && EVEN(r/2)) {
		cyg   = u; 
		cyg2  = u; 
		yemg  = ur; 
		yemg2 = ul; 
		cymg  = l; 
		cymg2 = l; 
		yeg   = lr; 
		yeg2  = ll; 
	    } else if (ODD(c) && EVEN(r/2)) {
		yemg  = u;
		yemg2 = u;
		cyg   = ur;
		cyg2  = ul;
		yeg   = l;
		yeg2  = l;
		cymg  = lr;
		cymg2 = ll;
	    } else if (EVEN(c) && ODD(r/2)) {
		cymg  = u;
		cymg2 = u;
		yeg   = ur;
		yeg2  = ul;
		cyg   = l;
		cyg2  = l;
		yemg  = lr;
		yemg2 = ll;
	    } else /* (ODD(c) && ODD(r/2)) */ {
		yeg   = u;
		yeg2  = u;
		cymg  = ur;
		cymg2 = ul;
		yemg  = l;
		yemg2 = l;
		cyg   = lr;
		cyg2  = ll;
	    }
	    putcolors(cyg, yemg, cymg, yeg, cyg2, yemg2, cymg2, yeg2, convimage);
	    convimage += 3;
	}
	/* take care of last column -- we just copy the column before */
	putcolors(cyg, yemg, cymg, yeg, cyg2, yemg2, cymg2, yeg2, convimage);
	convimage += 3;
	image++;
    }
    /* take care of last row -- it's really a copy of the row above, but
	that data is gone, so we reproduce it here */
    for (c = 0; c < cols - 1 ; c++, image++) {
#if LATER
	if (EVEN(c)) {
	    cymg = *image;
	    yeg = *(image + 1);
	    cyg = *(image - 2 * cols);
	    yemg = *(image - 2 * cols + 1);
	} else /* if (ODD(c)) */ {
	    yeg = *image;
	    cymg = *(image + 1);
	    yemg = *(image - 2 * cols);
	    cyg = *(image - 2 * cols + 1);
	}
	putcolors(cyg, yemg, cymg, yeg, convimage);
#else
	putcolors(0, 0, 0, 0, 0, 0, 0, 0, convimage);
#endif
	convimage += 3;
    }

    /* take care of last pixel -- again, it's the same as the one just
     * before */
#if LATER
    putcolors(cyg, yemg, cymg, yeg, convimage);
#else
    putcolors(0, 0, 0, 0, 0, 0, 0, 0, convimage);
#endif
    convimage += 3;

}

static void
putcolors(int cyg, int yemg, int cymg, int yeg, 
    int cyg2, int yemg2, int cymg2, int yeg2, byte *outp)
{
    int red, blue, green;
    int red2, blue2;
#if OFFICIAL
    red  = yemg - (cyg * 7)/10;
    blue = cymg - (yeg * 2)/3;
    green = ((yemg + cyg + cymg + yeg)/2 - red - blue) / 2;
#else
    red  = yemg - (cyg * 7)/10;
    if (red < 0) red = 0;
    red2  = yemg2 - (cyg2 * 7)/10;
    if (red2 < 0) red2 = 0;
    blue = cymg - (yeg * 2)/3;
    if (blue < 0) blue = 0;
    blue2 = cymg2 - (yeg2 * 2)/3;
    if (blue2 < 0) blue2 = 0;
#if 1
    if (red > red2 || blue > blue2) {
	red = red2;
	blue = blue2;
	yemg = yemg2;
	cyg = cyg2;
	cymg = cymg2;
	yeg = yeg2;
    }
#endif
    green = (((yemg + cyg + cymg + yeg)/2 - red - blue) * 4) / 9;
    red = (red * 5) / 2;
    blue = (blue * 5) / 2;
    if (red > 255) {
	red = 255;
    }
    if (green > 255) {
	green = 255;
    }
    if (blue > 255) {
	blue = 255;
    }
#endif

    TRACE("g", __FILE__ ": cyg %d yemg %d cymg %d yeg %d\n",
			    cyg, yemg, cymg, yeg);
    TRACE("g", __FILE__ ": rgb are %d %d %d\n", red, green, blue);

    *outp++ = red;
    *outp++ = green;
    *outp = blue;
}
#endif
