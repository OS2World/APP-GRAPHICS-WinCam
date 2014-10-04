
/*
 *
 * winc -- a minimal "daemon" that collects and saves interesting images
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
 * $Header: E:/winc/RCS/wincd.c 1.1 1997/03/01 03:44:14 Derek Exp Derek $
 */

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <utime.h>
#include <sys/stat.h>
#include "wincam.h"
#include "pnm.h"
#include "trace.h"

#define DEFAULT_POLLSECS    10
#define DEFAULT_WAITSECS    60
#define DEFAULT_THRESHOLD   25	/* percent of max pixel value */
#define DEFAULT_MAXDIFF     1	/* percent of total pixels */
#define DEFAULT_NSAVE	    6
#define DEFAULT_SCANTYPE    1
#define DEFAULT_FRACTION    1
#define DEFAULT_FILENAME    "wincd.pnm"
#define NSAVEMAX 20

/*
 * from mergeenv.c -- there's no convenient header file in which
 * this appears.
 */
int	safeputenv(char *name, char *value);

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
usage(char *progname)
{
    fprintf(stderr, 
    "usage: %s -{flags}  where:\n"
    "   -h gives usage message\n"
    "   -f FILE sets filename into which to store image [wincd.pnm]\n"
    "   -s SCRIPT - runs when file saved, FILE is only arg. [no default]\n"
    "   -p SECS - sleep interval when polling to see if image changed [10]\n"
    "   -w SECS - sleep interval to wait after image has been snapped [60]\n"
    "   -n N - how many images for which to track history [6]\n"
    "	(use \"-n 0\" for simple save-every-time operation)\n"
    "    -r N - change in pixel (as %% of max) to be called changed [25]\n"
    "    -m N - percent of pixels changed for image to be different [4]\n"
    "   -z N - image reduction factor (one of 1, 2, 4, or 8) [1]\n"
    "   -2 to double-scan an image\n"
    "   -g grabs a greyscale image\n"
    "   -c grabs a color image (default)\n"
    "   -l color-adjustment\n"
    "   -a causes an audible alert (beep) when image is exposed\n"
    "   -i IDSTRING assigns a prefix for all config info looked up\n"
    "   -t STR sets tracing: see winc usage message\n"
    "", progname);
}

/*
 * release the camera while we sleep
 */
void
camsleep(cam_t cam, int secs)
{
    winc_unlock(cam);
    sleep(secs);
    winc_lock(cam);
}

int
main(int argc, char *argv[])
{
    char *progname;
    int c;

    int grey = 0;
    int pollsecs = DEFAULT_POLLSECS;	/* seconds to pause while looping */
    int waitsecs = DEFAULT_WAITSECS;	/* seconds to pause after snapping */
    int scantype = DEFAULT_SCANTYPE;	/* single- or double- scanned */
    int fraction = DEFAULT_FRACTION;	/* full image? what fraction? */
    char *filename = DEFAULT_FILENAME;	/* where to put the image */
    char *scriptname = 0;		/* script to run on saved image */
    char *color_adj_string = 0;
    int audible_alert = 0;

    int zoom = 1;		/* no viewfinder zoom by default */
    int nsave = DEFAULT_NSAVE;
    int thresh = DEFAULT_THRESHOLD;
    int maxdiff = DEFAULT_MAXDIFF;

    int exp = 0;
    int avg = 0;
    int blackadj;
    int same, sameas = 0;
    unsigned char *saveimage[NSAVEMAX];
    unsigned char *t;
    FILE *outf;
    time_t snaptime;		/* for resetting image grab time */
    int rows, cols;
    int pid = -1;
    int i;
    struct winc_image_adjust image_adjust;

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
    while ((c = getopt(argc, argv, "2hgcae:n:w:l:p:s:z:t:f:r:m:i:")) != EOF) {
	switch (c) {
	case 'h':
	    usage(progname);
	    exit(0);
	case 'z':
	    zoom = atoi(optarg);
	    fraction = zoom;
	    break;
	case 'e':   /* exposure (or initial exposure guess), in milliseconds */
	case 'E':   /*	or microseconds */
	    exp = atoi(optarg);
	    if (c == 'e')
		exp *= 1000;	    /* convert to microseconds */
	    break;
	case 't':
	    set_trace_class(optarg);
	    break;
	case 'a':
	    audible_alert  = 1;
	    break;
	case 'f':
	    filename = optarg;
	    break;
	case 's':
	    scriptname = optarg;
	    break;
	case 'n':
	    nsave = atoi(optarg);
	    break;
	case 'g':
	    grey = 1;
	    break;
	case 'c':
	    grey = 0;	/* this is the default in any case */
	    break;
	case 'l':
	    color_adj_string = optarg;
	    break;
	case '2':
	    scantype = 2;
	    break;
	case 'p':
	    pollsecs = atoi(optarg);
	    break;
	case 'w':
	    waitsecs = atoi(optarg);
	    break;
	case 'r':
	    thresh = atoi(optarg);
	    break;
	case 'm':
	    maxdiff = atoi(optarg);
	    break;
	case 'i':
	    if (safeputenv("WinCamId", optarg) == -1) {
		fprintf(stderr, "%s: couldn't set WinCamId\n", progname);
	    }
	    break;
	default:
	    fprintf(stderr, "%s: unknown option '%c'\n", progname, c);
	    usage(progname);
	    exit(1);
	}
    }

    if (optind != argc) {
	fprintf(stderr, "%s: extra arguments\n", progname);
	usage(progname);
	exit(1);
    }


    if (!filename || !filename[0]) {
	fprintf(stderr, "%s: must give -f filename\n", progname);
	exit(1);
    }

    if (nsave > NSAVEMAX) {
	errormsg(__FILE__ ": nsave too big -- fix NSAVE and recompile\n");
	exit(1);
    }

    if (audible_alert != 0)
	winc_register_alerter(audible_snapper_func, (void *)&snaptime);
    else
	winc_register_alerter(snapper_func, (void *)&snaptime);

    winc_image_adjustments(&image_adjust, color_adj_string, 1, 1);

    for (i = 0; i < nsave; i++) {
	saveimage[i] = (unsigned char *)malloc(48 * 64);
	if (!saveimage[i]) {
	    errormsg(__FILE__ ": can't malloc saveimages\n");
	}
	memset(saveimage[i], 0, 48*64);
    }

refind:
    Cam = winc_find(1);

    if (!Cam) {
	errormsg(__FILE__ ": couldn't find camera\n");
	exit(1);
    }

    (void)signal(SIGHUP, quitcatcher);
    (void)signal(SIGINT, quitcatcher);
    (void)signal(SIGTERM, quitcatcher);

    if (winc_black_offset(Cam, &blackadj, 0) != OK) {
	errormsg(__FILE__ ": failed to get black adjustment\n");
	goto refind;
    }

    while (1) {

	winc_lock(Cam);

	/* take a viewf image, looping to get exposure */
	if (winc_viewfinder_snap(Cam, 1, -1, -1, image,
			(exp == 0) ? 8 : 1, &exp, &avg, 0, 0) != OK) {
	    errormsg(__FILE__ ": failed to snap viewfinder image\n");
	    winc_close(Cam);	/* unlocks */
	    sleep(pollsecs);
	    goto refind;
	}

	winc_unlock(Cam);

	TRACE("xe", __FILE__ ": exposure %d.%03d sec (%d usec)\n",
	    exp/1000000, (exp%1000000)/1000, exp);

	if (nsave > 0) {
	    /* compare to last few images */
	    same = 0;
	    for (i = 0; i < nsave; i++) {
		int s = winc_compare_image(image, saveimage[i], 48, 64,
			    maxdiff, thresh, 15, 0);
		if (s && !same) sameas = i;  /* overwrite the first match */
		TRACE("x", __FILE__ ": image %d compare %ssame\n", i, 
					s ? "" : "not ");
		same = same || s;
	    }


	    if (same) {
		memcpy(saveimage[sameas], image, 48 * 64);
		sleep(pollsecs);
		/* try to pick up the last script that ran, just to
		    catch zombies earlier.  we don't block, though */
		if (pid != -1) {
		    if (waitpid(pid, 0, WNOHANG) == pid)
			pid = -1;   /* got it -- quit trying */
		}
		continue;
	    }

	    /* save new image, rotate the previous */
	    t = saveimage[nsave-1];
	    for (i = nsave-1; i > 0; i--)
		saveimage[i] = saveimage[i-1];
	    saveimage[0] = t;
	    memcpy(saveimage[0], image, 48 * 64);
	}

	winc_lock(Cam);

	TRACE("x", __FILE__ ": snapping full image\n");
	winc_snap_and_process(Cam, exp, image, convimage, &rows, &cols,
				!grey, scantype, fraction, &image_adjust);

	winc_unlock(Cam);

	/* pick up the last script that ran, before overwriting the
	   file it was processing */
	if (pid != -1) {
	    TRACE("x", __FILE__ ": reaping script process %d\n", pid);
	    if (waitpid(pid, 0, 0) != pid) /* it had to be, right? */
		TRACE("x", __FILE__ ": got wrong process? %d\n", pid);
	    pid = -1;
	}

	/* process it, widen it, write it out, to a known name. spawn
		a script to deal with it, passing filname as arg. */

	outf = fopen(filename, "wb");
	if (!outf) {
	    errormsg(__FILE__ ": can't open file \"%s\"\n", filename);
	    exit(0);
	}

	winc_gamma(0, convimage, rows, cols, !grey);

	if (grey)
	    (void)winc_pgm_output(convimage, rows, cols, 255, outf);
	else
	    (void)winc_pnm_output(convimage, rows, cols, 255, outf);

	fclose(outf);
	reset_mod_time(filename, snaptime);

	if (scriptname) {
	    TRACE("x", __FILE__ ": spawning script %s on %s\n",
	    					scriptname, filename);
	    if (system(scriptname) == -1) { /* child */
		winc_close(Cam);
		execlp(scriptname, scriptname, filename, 0);
		errormsg(__FILE__ ": exec of %s failed\n", scriptname);
		exit(1);
	    } 
	}

	/* wait for specified time after snapping a picture */
	if (waitsecs != 0)
	    sleep(waitsecs);

    }
    if (Cam)
	winc_close(Cam);
    return 0;
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

