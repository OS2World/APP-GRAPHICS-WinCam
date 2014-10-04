/*
 *
 * Public header describing interfaces to the wincam lib
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
 * $Header: E:/winc/RCS/wincam.h 1.1 1997/03/01 03:44:14 Derek Exp Derek $
 */

/* opaque handle with which to reference the camera */
typedef struct camera_s *cam_t;

/* really just a boolean:  okay/not okay */
typedef enum {
    NotOK    = 0,
    OK	     = 1
} result;


#if LATER /* multi-camera per serial port support */
/* the camera has the ability to disable and enable itself, using its
 * id value as an address.  supposedly this lets you put more than one
 * camera on a single serial port.  i only have one camera.
 */
result	winc_disable_all( cam_t cam );
result	winc_enable( cam_t cam );
#endif

/*
 * cam_configvar() will look for the specified variable:
 *  - in the caller's environment
 *  - in /usr/local/etc/winc.rc
 *  - failing those, will return the default passed in as the second arg.
 */
char *winc_configvar(char *variable, char *defaultval);

/*
 * winc_find() looks for a camera, currently on the configured device;
 * the default is "/dev/ttyCamera".  (i set that up as a symlink to the real
 * special device.)  the "cam_t" handle which is returned will be used
 * in all subsequent communications with the camera.
 */
cam_t	winc_find( int which );

/*
 * winc_close() closes the camera, and releases the handle.
 */
void winc_close(cam_t cam);

/*
 * sets camera to given baud rate 
 */
int winc_set_baud(cam_t cam, int rate);

/*
 * gets camera's current baud rate 
 */
int winc_get_baud(cam_t cam);

/*
 * winc_getstatus() isn't normally needed, since it's called internally
 * by winc_find(), but it can be used as a "ping" of sorts.  it fills
 * in version and id information in the cam_t handle.
 */
result	winc_getstatus( cam_t cam );

/*
 * the camera can respond to several diagnostic commands, and this does
 * all three of them.  there's a) an echo check, where the camera echoes
 * back a short set of bytes sent to it, b) a transmit check, where it
 * transmits all 256 possible unsigned char values, and c) an internal
 * RAM diagnostic, which does some sort of check on its internals.
 */
result	winc_diagnose( cam_t cam );


/* 
 * these three are not not normally needed -- they're called internally with,
 * defaults, but are available if needed.
 */

/*
 * set no. of extra stop bits that the camera should send when
 * transmitting to us.
 */
result	winc_extrastop( cam_t cam, int extra_stop);

/*
 * set true for flourescent flicker control -- synchronizes camera with
 * the flicker or typical indoor flourescent lighting.
 */
result	winc_linesync( cam_t cam, int sync_60hz);	

/*
 * set light sensitivity (0-15)
 */
result	winc_sensitivity( cam_t cam, int sensitivity);

/*
 * the viewfinder grab can be used to align and focus the camera, as well
 * as to determine an appropriate exposure interval.  if called with
 * "autoexpose" set, it will seek (doing multiple grabs) to a useful
 * exposure value, using average pixel intensity as a guide.
 */
#define DEFAULT_EXPOSURE 33333	    /* 1/30th of a second, in microseconds */

result winc_viewfinder_snap( cam_t cam,
    int zoom,	    /* 1, 2, 4, or 8 */
    int row,	    /* when zoomed: starting row: 0 to 197 */
    int col,	    /* when zoomed: starting col: 0 to 448 */
    unsigned char *image, /* room for 1536 bytes, 0 to suppress */
    int exposeiters, /* iterations to find correct exposure */
    int *exposurep, /* pointer to exposure guess (or null) 
	    (in microseconds -- real granularity is 50,000ths) */
    int *avgp,	    /* place to put average pixel value */
    int goal,	    /* desired brightness, 1 to 100 */
    int *ok_p);	    /* boolean: did we achieve that brightness? */

/*
 * "expose" a picture, in one of several modes.
 */
result winc_image_expose( cam_t cam,
    int scantype,   /* single(1), double(2), or noninterlaced(0) */
    int fraction,   /* 1, 2, 4, or 8 for full, half, 1/4 and 8th size */
    int u_exp, 	    /* exposure (in microseconds), 0 to download only */
    unsigned char *image);    /* room for expected no. bytes */

/*
 * fetch a previously exposed picture from the camera
 */
result winc_image_fetch( cam_t cam,
    int scantype,   /* single(1), double(2), or noninterlaced(0) */
    int fraction,   /* 1, 2, 4, or 8 for full, half, 1/4 and 8th size */
    int color,	    /* image destined for color processing */
    unsigned char *image,   /* room for expected no. bytes */
    int *rows_p,    /* no. of rows in resulting image */
    int *cols_p);	/* no. of cols in resulting image */

/*
 * winc_image_snap() takes a "full-size" picture, in one of several modes.
 * this is a simple combination of winc_image_expose() and winc_image_fetch().
 */
result winc_image_snap( cam_t cam,
    int u_exp, 	    /* exposure (in microseconds), -1 to autoexpose,
			and 0 to suppress exposure, just download */
    int scantype,   /* single(1), double(2), or noninterlaced(0) */
    int fraction,   /* 1, 2, 4, or 8 for full, half, 1/4 and 8th size */
    int color,	    /* image destined for color processing */
    unsigned char *image,   /* room for expected no. bytes */
    int *rows_p,    /* no. of rows in resulting image */
    int *cols_p);   /* no. of cols in resulting image */

/*
 * register a routine with the library which will be called when
 * a snapshot is actually being done.  can be used as an audible alert,
 * for instance.  the function is eventually called with the supplied
 * argument.
 */
void winc_register_alerter(
    void (*alert_func)(void *), /* function called when an image is "snapped" */
    void *cookie);		/* single argument passed to that function */

/*
 * the camera will report some residual light level even for purely dark
 * scenes.  this level can be detected via some dark "sidebands" that the
 * camera makes available, and then factored out (subtracted) from an entire
 * image to reduce "haziness".
 */
result	winc_black_offset(cam_t id, int *blackadj, unsigned char *blackp);

/*
 * structure to hold color balance information.
 * the values are the percent of "standard" that that color
 * should be adjusted to, i.e. 100, 100, 100 means no changes.
 * default standard values for a structure like this are returned by
 * winc_lighting_adjustments()
 */
struct winc_image_adjust {
    int red;			/* percent of "standard" red */
    int green;			/* percent of "standard" green */
    int blue;			/* percent of "standard" green */
    int widen;			/* should image be stretched? */
    int vignette_correction;	/* should edges be brightened? */
};

/*
 * take a values, presumably supplied by the user, and turn it into
 * a set of appropriate color adjustment values in the supplied structure.
 * given a null string, will provide defaults for color balance.
 */
void winc_image_adjustments(
    struct winc_image_adjust *ca,
    char *cs,		/* string describing color balance */
    int widen,		/* boolean: should image be stretched? */
    int vignette	/* boolean: shoudl corners be brightened? */
);

/*
 * to get proper greyscale, we average every pair of pixes from the
 * camera.  "convimage" should be at least as big as "image".
 */
void winc_grey_convert(
    unsigned char *image,	/* raw data from camera */
    unsigned char *convimage,	/* data with adjacent pixels averaged */
    int *rows_p,	    /* no. of rows in image */
    int *cols_p,	    /* no. of cols in image */
    int offs,		/* amount (black value) to subtract from every pixel */
    int scantype,	/* double or single scan */
    int fraction,	/* fraction of orig dimension */
    struct winc_image_adjust *ia);	/* image adjustment info */

/*
 * color is a more complicated.
 * "convimage" should be 3 times the size of "image", to contain R,G,B
 * values.
 */
void winc_color_convert(
    unsigned char *image,	/* raw data from camera */
    unsigned char *convimage,	/* data with adjacent pixels averaged */
    int *rows_p,		/* no. of rows in image */
    int *cols_p,		/* no. of cols in image */
    int offs,		/* amount (black value) to subtract from every pixel */
    int color,		/* do we want 24 bit color or 8 bit grey ? */
    int scantype,	/* double or single scan */
    int fraction,	/* fraction of orig dimension */
    struct winc_image_adjust *ia);	/* image adjustment info */


/*
 * winc_snap_and_process combines the actions of winc_image_snap() and
 *  winc_color_convert().  it overlaps the serial i/o with the color
 *  processing, so it's quite a bit faster than doing the two steps
 *  separately
 */
result winc_snap_and_process(
    cam_t cam,
    int u_exp, 		/* exposure (in microseconds) */
    unsigned char *image, /* image from winc_image_snap()/winc_image_fetch() */
    unsigned char *convimage,	/* where to put the rgb result */
    int *rows_p,		/* no. of rows in image */
    int *cols_p,		/* no. of cols in image */
    int color,		/* do we want 24 bit color or 8 bit grey ? */
    int scantype,	/* double or single scan */
    int fraction,	/* fraction of orig dimension */
    struct winc_image_adjust *ia);	/* image adjustment info */

/*
 * stretch an image horizontally, by 25%, to fix the WinCam's inherent
 * aspect ratio problem.  this takes a 512 column image to 640.
 * returns the new width.
 */
int winc_aspect_fix(
    unsigned char *image,	/* the old narrow image */
    int rows,		/* no. of rows.  remains unchanged */
    int origcols,	/* no. of cols in original image */
    int rgb);		/* is this a color image? */

/*
 * the corners of a raw image are darker than the middle, by quite
 * a bit.  this routine tries to correct for this. 
 */
void winc_vignette_fix(
    unsigned char *image,	/* the image */
    int rows,		/* no. of rows */
    int cols,		/* no. of cols */
    int rgb);		/* is this a color image? */

/*
 * performa a gamma function on the image.  gamma of 1.0 leaves the
 * image unchanges -- less dims, greater brightens.  the first arg.
 * is the gamma times 1000.
 */
void
winc_gamma(
    int gam,		/* gamma value, times 1000 */
    unsigned char * image,
    int rows,
    int cols,
    int rgb);

/*
 * compare two images, returning a simple boolean:  true if they "match",
 * false if they don't.
 */
int winc_compare_image(
    unsigned char *image1,  /* the two images being */
    unsigned char *image2,  /*	compared */
    int rows,		    /* rows of pixels */
    int cols,		    /* columns of pixels */
    int maxdiff,	    /* percent pixels changed for image to be diff */
    int thresh, 	    /* amount pixel val must, as percentage of max */
    int maxval,		    /* maximum pixel value */
    int rgb);		    /* boolean -- three bits per pixel, or one */


/*
 * get and release locks on the camera, to allow multiple programs
 * to attempt to use it at once.
 */
void winc_lock(cam_t cam);
void winc_unlock(cam_t cam);

