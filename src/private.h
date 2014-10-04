/*
 *
 * Misc stuff for the wincam lib internals.  Clients of the library
 *  should not need to include this.
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
 * $Header: E:/winc/RCS/private.h 1.1 1997/03/01 03:44:14 Derek Exp Derek $
 */

#define INCL_DOSFILEMGR
#define INCL_DOSDEVIOCTL
#define INCL_DOSDEVICES

#include <os2.h>

#include "wincam.h"
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>

#define CAMERA_TTY "/dev/ttyCamera"

/*
 * it's tempting to move these lines to a separate header, so we don't need
 * to rebuild everything when we bump the version, but that's probably good
 * practice in any case...
 */
#define VERSION     "winc library version 36"
#define DATE	    "(released October 16, 1996)"
#define COPYRIGHT   "Copyright 1996 by Paul G. Fox" \
		    "\nsome portions (c) Copyright 1996 StarDot Technologies"

/*
 * i use this typedef for unsigned char, since it's not usually
 * in system header files, and because "unsigned char" is too much
 * to type.  :-)
 */
typedef unsigned char byte;

/*
 * camera firmware version information
 */
struct camera_ver_s {
    byte major;
    byte minor;
    byte chiptype;
};
typedef struct camera_ver_s camera_ver_t;

/* we store the camera id as bytes because a) it's more portable,
 * and b) it's really a glom of date information anyway -- see code in
 * winc_find() to interpret it.
 */
struct camera_id_s {
    byte id[4];
};
typedef struct camera_id_s camera_id_t;

/*
 * this is everything we store concerning a single camera.  this could
 * be augmented with "last useful exposure", "last zoom factor", or that
 * sort of thing.
 */
struct camera_s {
    char *ttyname;	    /* the name of the device, e.g. "/dev/ttyS1" */
    HFILE ifd;		    /* the opened file for that device (for input) */
    HFILE ofd;		    /* the opened file for that device (for output) */
    struct termios termios; /* the terminal properties for the open device */
    int baud_index;	    /* the current baud rate */
    camera_id_t id;	    /* the id bytes returned by the camera */
    camera_ver_t version;   /* the version info returned by the camera */
    char *lockfile;	    /* the name of the lockfile */
    int lockfd;		    /* the descriptor for the above lock file */
    int havelock;	    /* do we hold the lock? */
/*    struct flock flock;	    for doing camera locking */
    byte *starfield;	    /* bitmap for starfield elimination */
}; 

/*
 * WinCam.One command definitions 
 *
 * the commands are one-byte commands.	note that all valid commands
 * end in binary 01, which is how the camera autodetects baud rate.
 * some commands are followed by parameters.
 *
 * the fundamental acknowledgement response is one byte of 0x21.  this
 * may be followed by a lot of data in some cases, none in others.
 */

#define Resp_Ack	    0x21    /* '!' */

typedef enum {
/* is it there, and what sort of camera is it? (find.c) */
    Cmd_nop		= 0x1,
/* multi-camera operations */
    Cmd_disable_all	= 0x3d,
    Cmd_enable_camera	= 0x45,
/* probe/status */
    Cmd_at_command	= 0x41,
    Cmd_send_version	= 0x5,
    Cmd_send_id 	= 0x35,
/* set some camera parameters */
    Cmd_extra_stop	= 0x25,
    Cmd_set_light	= 0x31,
    Cmd_sync_mode	= 0x39,


/* is it working properly? (diag.c) */
    Cmd_tx_test 	= 0x9,
    Cmd_rx_test 	= 0x4d,
    Cmd_test_dram	= 0x0d,
    Cmd_send_black	= 0x1d, /* (used for manual camera calibration) */

/* snap a "viewfinder" image, and get it (viewfinder.c) */
    Cmd_snap_view	= 0x21,
    Cmd_download_view	= 0x29,

/* snap various image styles, and get image data (images.c) */
/* snap: */
    Cmd_snap_single	= 0x2d,
    Cmd_snap_interlaced = 0x11,
    Cmd_snap_non	= 0x19,
/* get: */
    Cmd_send_image	= 0x49,
    Cmd_send_row	= 0x15,


} Cmd_byte_t;


/*
 * protocol level routines: protocol.c
 */
result	winc_send_cmd(cam_t cam, Cmd_byte_t cmd, ...);
result	winc_get_resp(cam_t cam, int nbytes, byte *bp, unsigned long *sump);
result	winc_get_resp_wait(cam_t cam, int nbytes, byte *bp,
		    int millisec, unsigned long *sump);
result	winc_reset( cam_t cam );
result	winc_flush(cam_t cam);

/*
 * serial-port level routines:  comms.c
 */
result	comms_initialize_port( cam_t cam );
void	comms_close_port( cam_t cam );

int	comms_first_baud(void);
int	comms_next_baud(int bindex);
int	comms_index2rate(int bindex);
result	comms_set_baud(cam_t cam, int bindex);

result	comms_send_break( cam_t cam, int millisec );

int	comms_timed_read( cam_t cam, char *buf, int len, int millisec);
int	comms_write(cam_t cam, char *buf, int n);



/*
 * configuration helpers: config.c and mergeenv.c
 */
int	cfg_number(char *s);
int	cfg_boolean(char *s);

/*
 * return "repeats" parameters, given image fraction: image.c
 */
result get_repeats(int fraction, 
    int *startp, int *sendp, int *skipp, int *repeatp,
    int *colsp, int *rowsp);


/*
 * initialize the winc_lock/winc_unlock module: locks.c
 */
void winc_locks_init( cam_t cam );

/*
 * check for presence of modem on port, vs. camera.  OK --> modem.
 */
result winc_check_for_modem( cam_t cam );

/*
 * fix up raw data to the camera by averaging out "bad" CCD pixels.  (very
 * low light levels show some CCD bits as bad) these are "starfield"
 * routines because a dark image appears like a starry sky due to the bad
 * white pixels.
 */
void winc_starfield_fix(byte *line, byte *starf_line, int len);

/*
 * fetch a line from the starfield reference image.  we return the same
 * pixels the camera does for any given request.
 */
result winc_starfield_line(byte *oline, cam_t cam, int row, int start, int
    send, int skip, int repeat);
