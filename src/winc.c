
/*
 *
 * winc -- a command line program for accessing the WinCam.One digital camera.
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
 * $Header: E:/winc/RCS/winc.c 1.1 1997/03/01 03:44:14 Derek Exp Derek $
 */

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <utime.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "wincam.h"
#include "pnm.h"
#include "trace.h"

/*
 * from mergeenv.c -- there's no convenient header file in which
 * this appears.
 */
int	safeputenv(char *name, char *value);

/*
 * forward declarations
 */
void emit_wcd_format(unsigned char *, int, unsigned char *, FILE *);
void inhale_wcd_format(unsigned char *, int *, FILE *);
void snapper_func(void *timep);
void audible_snapper_func(void *timep);
void reset_mod_time(char *filename, time_t mtime);
void quitcatcher(int sig);

/*
 * global camera handle, so it can be closed/flushed from a signal handler
 */
cam_t Cam = 0;		/* camera "handle" */

/*
 * for getopt()
 */
extern char *optarg;
extern int optind, opterr, optopt;

void
usage(char *progname, int full)
{
    fprintf(stderr, 
    "usage: %s -{flags}  where:\n%s%s", progname,
    "   -h gives the usage message\n"
    "   -H gives a more complete usage message\n"
    "   -c grabs a color image\n"
    "   -g grabs a greyscale image\n"
    "   -v grabs a viewfinder image\n"
    "     (if none of -v, -g, or -c is used, no image will be grabbed,\n"
    "      but camera will still be discovered and initialized)\n"
    "   -2 to double-scan an image\n"
    "   -z N sets image reduction factor, or viewfinder zoom. (1, 2, 4, or 8)\n"
    "   -b NN - brightness (1-100, 0 for default) [50]\n"
    "   -l color-adjustment\n"
    "   -a causes an audible alert (beep) when image is exposed\n"
    "   -s SPEED sets the preferred baud rate\n"
    "   -i IDSTRING assigns a prefix for all config info looked up\n"
    "   -o FILE sets the output file to use in place of stdout\n"
    "", !full ? "" :
    "   -S NN saves a starfield image, using NN as the \"bad\" threshold\n"
    "   -e NN sets exposure in milliseconds\n"
    "   -E NN sets exposure in microseconds\n"
    "   -f don't do faster grab\n"
    "   -n leaves \"narrow\" image, rather than widening\n"
    "   -r leaves a dark image, rather than brightening corners\n"
    "	-G VAL processes with gamma function . (100 normal, 110 default)\n"
    "   -d runs camera diagnostics\n"
    "   -t STR sets tracing; arg is a string of traceflags:\n"
    "	    v just shows program and camera version information\n"
    "	    V will turn on all possible messages\n"
    "	    e to get exposure information\n"
    "	    a for simple API tracing\n"
    "	    A for full API tracing\n"
    "	    c for camera commands\n"
    "	    s for serial protocol\n"
    "	    S for all bytes of serial protocol\n"
    "	    g for graphics output routine tracing\n"
    "	    r for winc.rc file tracing\n"
    "   -R writes .wcd format data\n"
    "   -D gets .wcd format data from stdin\n"
    "\n"
    );
}

int
main(int argc, char *argv[])
{
    char *progname;
    int c;


    int do_viewfinder = 0;	/* flags three kinds of grabs */
    int do_color = 0;
    int do_greyscale = 0;
    int do_starfield = 0;
    int do_raw_output = 0, do_raw_input = 0;
    int scantype = 1;		/* images can be single- or double- scanned */
    int fraction = 1;		/* full image? what fraction? */
    int widen = 1;		/* should we widen to fix aspect ratio? */
    int vignette = 1;		/* should we fix corner darkening? */
    int gamma = 0;		/* should we do darkness lightening? */
    int faster = 1;
    int audible_alert = 0;
    int starfield_cutoff = 1;

    int goal = 0;		/* brightness goal */
    int exp = 0;		/* default exposure is 1/30th of a second */
    int zoom = 1;		/* no viewfinder zoom by default */
    int diagnostics = 0;
    struct winc_image_adjust image_adjust;
    char *color_adj_string = 0;

    char *filename = 0;		/* where to put the image if not stdout */
    FILE *outf;			/* file handle for output */
    time_t snaptime;		/* for resetting image grab time */

    int rows, cols;
    int blackadj;		/* average black value */
    unsigned char blackvals[2*512 + 2]; /* space for black values, if needed  */
    unsigned char image[640*492];	/* the biggest imagemap we'll need */
    unsigned char convimage[3*640*492]; /* the biggest converted imagemap */


    /* 
     * what program is this?
     */
    progname = strrchr(argv[0], '/');
    if (progname)
	progname++;
    else
	progname = argv[0];

    _fsetmode(stdout,"b");

    /* 
     * set up the trace and error facilities
     * register the output callbacks, and initialize the tracing class
     */
    register_error_func((vprintffunc)vfprintf, (void *)stderr);
    register_trace_func((vprintffunc)vfprintf, (void *)stderr);
    set_trace_class("");

    /* 
     * process arguments
     */
    while ((c = getopt(argc, argv, 
		    "nr2hHvfgcdmaRDo:l:G:z:b:e:E:t:f:i:s:S:"
		    )) != EOF) {
	switch (c) {
	case 'H':
	case 'h':
	    usage(progname, (c == 'H'));
	    exit(0);
	case 'e':   /* exposure (or initial exposure guess), in milliseconds */
	case 'E':   /*	or microseconds */
	    exp = atoi(optarg);
	    if (c == 'e')
		exp *= 1000;	    /* convert to microseconds */
	    break;
	case 'n':
	    widen = 0;
	    break;
	case 'b':
	    goal = atoi(optarg);
	    break;
	case 'S':
	    do_starfield = 1;
	    scantype = 2;
	    goal = 50;
	    starfield_cutoff = atoi(optarg);
	    if (safeputenv("NoStarfield", "none_at_all") == -1) {
		fprintf(stderr, "%s: couldn't set NoStarfield\n",
			progname);
		exit(1);
	    }
	    break;
	case 'r':
	    vignette = 0;
	    gamma = 100;	/* force winc_gamma() to be transparent */
	    break;
	case 'G':
	    gamma = atoi(optarg);
	    break;
	case 'z':
	    zoom = atoi(optarg);
	    fraction = zoom;
	    break;
	case 'd':
	    diagnostics = 1;
	    break;
	case 't':
	    set_trace_class(optarg);
	    break;
	case 'a':
	    audible_alert  = 1;
	    break;
	case 'v':
	    do_viewfinder = 1;
	    break;
	case 'f':
	    faster = 0;
	    break;
	case 'g':
	    do_greyscale = 1;
	    break;
	case '2':
	    scantype = 2;
	    break;
	case 'c':
	    do_color = 1;
	    break;
	case 'l':
	    color_adj_string = optarg;
	    break;
	case 'i':
	    if (safeputenv("WinCamId", optarg) == -1) {
		fprintf(stderr, "%s: couldn't set WinCamId\n", progname);
		exit(1);
	    }
	    break;
	case 'o':
	    filename = optarg;
	    break;
	case 's':
	    if (safeputenv("WinCamBaudRate", optarg) == -1) {
		fprintf(stderr, "%s: couldn't set WinCamBaudRate\n", progname);
		exit(1);
	    }
	    break;
	case 'R':
	    /* option given once, output stardot .wcd format,
		if twice, output raw image array */
	    do_raw_output++;
	    faster = 0;
	    break;
	case 'D':
	    /* if option is given once, read stardot .wcd format,
		else raw image array */
	    do_raw_input++;
	    faster = 0;
	    break;
	default:
	    fprintf(stderr, "%s: unknown option '%c'\n", progname, c);
	    usage(progname, 0);
	    exit(1);
	}
    }

    if (optind != argc) {
	fprintf(stderr, "%s: extra arguments\n", progname);
	usage(progname, 0);
	exit(1);
    }

    if (do_viewfinder +
	do_greyscale +
	do_color +
	do_starfield +
	(do_raw_output != 0) > 1) {
	fprintf(stderr, 
	    "%s: only one of -v, -g, -c, -S, or -R should be given\n",
						progname);
	usage(progname, 0);
	exit(1);
    }

    if (filename) {
	outf = fopen(filename, "wb");
	if (!outf) {
	    errormsg(__FILE__ ": can't open file \"%s\"\n", filename);
	    exit(1);
	}
    } else {
	outf = stdout;
    }

    if (audible_alert != 0)
	winc_register_alerter(audible_snapper_func, (void *)&snaptime);
    else
	winc_register_alerter(snapper_func, (void *)&snaptime);

    winc_image_adjustments(&image_adjust, color_adj_string, widen, vignette);

    if (exp)
	goal = 0;	/* goal is meaningless if manual exposure */

    /* don't actually touch the camera if we're just debugging conversions */
    if (!do_raw_input) {
	/*
	 * finally, find a camera.  the argument (1) currently doesn't
	 * do anything -- eventually we should support more than one camera,
	 * on separate serial ports.
	 */
	Cam = winc_find(1);
	if (!Cam) {
	    fprintf(stderr, "%s: couldn't find camera\n", progname);
	    exit(1);
	}

	(void) signal(SIGHUP, quitcatcher);
	(void) signal(SIGINT, quitcatcher);
	(void) signal(SIGTERM, quitcatcher);


	if (diagnostics)
	    winc_diagnose(Cam);

	if (do_viewfinder) {

	    if (winc_viewfinder_snap(Cam, zoom, -1, -1, image,
			(exp == 0) ? 15 : 1, &exp, 0, goal, 0) != OK) {
		errormsg(__FILE__ ": failed to snap initial viewfinder image\n");
		exit(0);
	    }
	    /*
	     * viewfinder images are all 48 high and 64 across, and
	     * no massaging is necessary, even aspect ratio.
	     */
	    rows = 48;
	    cols = 64;
	    (void)winc_pgm_output(image, rows, cols, 15, outf);
	    winc_unlock(Cam);
	    winc_close(Cam);
	    exit(0);
	}
    }

    /* goal is passed as negative exposure, unless we're exposing manually */
    if (!exp && goal)
	exp = -goal;

    if (faster && (do_greyscale || do_color)) {
	winc_snap_and_process(Cam, exp, image, convimage, &rows, &cols,
				do_color, scantype, fraction, &image_adjust);
	winc_gamma(gamma, convimage, rows, cols, do_color);
	if (do_color)
	    (void)winc_pnm_output(convimage, rows, cols, 255, outf);
	else
	    (void)winc_pgm_output(convimage, rows, cols, 255, outf);
	if (filename && snaptime != 0) {
	    fclose(outf);
	    reset_mod_time(filename, snaptime);
	}
	winc_unlock(Cam);
	winc_close(Cam);
	return 0;
    }

    if (do_greyscale || do_color || do_starfield || do_raw_output) {
	if (!do_raw_input) {
	    if (winc_image_snap(Cam, exp, scantype, 
		    fraction, !do_greyscale, image, &rows, &cols) != OK) {
		errormsg(__FILE__ ": failed to snap image\n");
		winc_close(Cam);
		exit(0);
	    }

	    if (winc_black_offset(Cam, &blackadj, do_raw_output ? blackvals : 0 )
								    != OK) {
		errormsg(__FILE__ ": failed to get black adjustment\n");
		winc_close(Cam);
		exit(0);
	    }
	} else {
	    int ch;
	    unsigned char *ip = image;

	    if (do_raw_input > 1) {
		/* load up the data */
		while((ch = getc(stdin)) != EOF)
		    *ip++ = ch;

		if (scantype == 2) {
		    rows = 492; cols = 512;
		} else {
		    switch(fraction) {
		    case 1: rows = 246; cols = 512; break;
		    case 2: rows = 246; cols = 256; break;
		    case 4: rows = 123; cols = 128; break;
		    default:
		    case 8: rows = 60;	cols = 64;  break;
		    }
		}

		blackadj = 0; /* can't get this back easily */
	    } else {
		inhale_wcd_format(image, &blackadj, stdin);
		rows = 492;
		cols = 512;
		scantype = 2;
	    }
	}

#if USE_MY_GREY
	if (do_greyscale) {
	    winc_grey_convert(image, convimage, &rows, &cols, blackadj,
				scantype, fraction, &image_adjust);
	    if (vignette)
		winc_vignette_fix(convimage, rows, cols, 0);
	    if (widen)
		cols = winc_aspect_fix(convimage, rows, cols, 0);
	    (void)winc_pgm_output(convimage, rows, cols, 255, outf);
	} else if (do_color) {
	    winc_color_convert(image, convimage, &rows, &cols, blackadj,
				do_color, scantype, fraction, &image_adjust);
	    if (vignette)
		winc_vignette_fix(convimage, rows, cols, do_color);
	    if (widen)
		cols = winc_aspect_fix(convimage, rows, cols, do_color);
	    if (do_color)
		(void)winc_pnm_output(convimage, rows, cols, 255, outf);
	    else
		(void)winc_pgm_output(convimage, rows, cols, 255, outf);
	} 
#else
	if (do_color || do_greyscale) {
	    winc_color_convert(image, convimage, &rows, &cols, blackadj,
				do_color, scantype, fraction, &image_adjust);
	    winc_gamma(gamma, convimage, rows, cols, do_color);
	    if (do_color)
		(void)winc_pnm_output(convimage, rows, cols, 255, outf);
	    else
		(void)winc_pgm_output(convimage, rows, cols, 255, outf);
	} 
#endif
	else if (do_raw_output) {
	    if (do_raw_output > 1) {
		/* simple write of the image array, suitable for sucking
		    in with -D */
		(void)fwrite(image, 1, rows*cols, outf);
	    } else {
		emit_wcd_format(image, scantype, blackvals, outf);
	    }
	} else if (do_starfield) {
	    (void)winc_pbm_output(image, convimage, rows, cols,
	    		starfield_cutoff, outf);
	}
	if (filename && snaptime != 0) {
	    fclose(outf);
	    reset_mod_time(filename, snaptime);
	}
    }

    if (Cam) {
        winc_unlock(Cam);
	winc_close(Cam);
    }

    return 0;
}


/* 
 * two routines to read and write  StarDot's .wcd format:
 *  246 even rows (they call them odd; historical)
 *  10	rows of zero
 *  246 odd rows (again, they call these even)
 *  8	rows of zero
 *  2	rows of black values
 * all rows are 512 bytes across
 */

void
emit_wcd_format(
    unsigned char *image,
    int scantype,
    unsigned char *blackvals,
    FILE *outf)
{
    /* this won't contain interesting data if the scan
     *	wasn't a full height scan
     */
    unsigned char empty[512];
    unsigned char *p;
    int i;
    memset(empty, 0, sizeof(empty));

    /* write the even rows */
    p = image;
    for (i = 0; i < 246; i++) {
	(void)fwrite(p, 1, 512, outf);
	p += 512;
	if (scantype == 2)
	    p += 512;
    }

    /* ten empty rows */
    for (i = 0; i < 10; i++)
	(void)fwrite(empty, 1, 512, outf);

    /* write the odd rows, or write the even rows again */
    p = (scantype == 2) ? &image[512] : image;
    for (i = 0; i < 246; i++) {
	(void)fwrite(p, 1, 512, outf);
	p += 512;
	if (scantype == 2)
	    p += 512;
    }

    /* eight empty rows */
    for (i = 0; i < 8; i++)
	(void)fwrite(empty, 1, 512, outf);

    /* two black rows */
    (void)fwrite(blackvals, 1, 2*512, outf);
}

void
inhale_wcd_format(
    unsigned char *image,
    int *blackadjp,
    FILE *inf)
{
    int i, sum;
    unsigned char *p;
    unsigned char scratch[512];

    /* read the even rows */
    p = image;
    for (i = 0; i < 246; i++) {
	if (fread(p, 1, 512, inf) != 512)
	    goto errout;
	p += 2*512;
    }

    /* ten empty rows */
    for (i = 0; i < 10; i++) {
	if (fread(scratch, 1, 512, inf) != 512)
	    goto errout;
    }

    /* read the odd rows */
    p = &image[512];
    for (i = 0; i < 246; i++) {
	if (fread(p, 1, 512, inf) != 512)
	    goto errout;
	p += 2*512;
    }

    /* eight empty rows */
    for (i = 0; i < 8; i++) {
	if (fread(scratch, 1, 512, inf) != 512)
	    goto errout;
    }


    /* two black rows to average */
    sum = 0;
    if (fread(scratch, 1, 512, inf) != 512)
	    goto errout;
    for (i = 0; i < 246; i++)
	sum += scratch[i];

    if (fread(scratch, 1, 512, inf) != 512)
	    goto errout;
    for (i = 0; i < 246; i++)
	sum += scratch[i];

    sum /= (2 * 246);
    *blackadjp = sum;

    return;

 errout:
    fprintf(stderr, "Error reading .wcd format input\n");
    exit(1);
}


/*
 * the snapper funcs are "called back" by the winc library when the
 * picture is actually snapped.  this lets us a) make a noise, and b)
 * record the time
 */
void
snapper_func(void *timep)
{
    *(time_t *)timep = time(0);
}

void
audible_snapper_func(void *timep)
{
    putc('\a', stderr);
    *(time_t *)timep = time(0);
}

/*
 * attempt to reset the modification time of the given file to mtime.
 * this is used to set the time on the output file to the time at which
 * the image was snapped, rather than when it was written, since processing
 * can take some time.
 */
void 
reset_mod_time(char *filename, time_t mtime)
{
    struct stat stb;
    struct utimbuf utb;
    if (stat(filename, &stb) == -1)
    	return;

    utb.actime = stb.st_atime;
    utb.modtime = mtime;
    (void)utime(filename, &utb);
}

void
quitcatcher(int sig)
{
    if (Cam)
    	winc_close(Cam);
    exit(1);
}

