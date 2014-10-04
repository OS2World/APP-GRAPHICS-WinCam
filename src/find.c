/*
 *
 * probe for and discover information about the camera
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
 * $Header: E:/winc/RCS/find.c 1.1 1997/03/01 03:44:14 Derek Exp Derek $
 */
#define NDEBUG 1


#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "private.h"
#include "trace.h"

/*
 * winc_find() looks for a camera, currently on the configured device; the
 * default is "/dev/ttyCamera".  (i set that up as a symlink to the real
 * special device.) the "cam_t" handle which is returned will be used in
 * all subsequent communications with the camera.
 */
/*
 * if we were to support multiple cameras, this
 * routine would find camera N.  i think.  in any
 * case, it would find a camera, and fill in a camera_s
 * struct, with where it lives, what id it has, etc.
 * i'm ignoring the multiple camera per tty facility for now,
 * since the one camera per tty case seems a lot more likely in
 * the near future.  ttys are cheaper than cameras.
 */
cam_t
winc_find( int which )
{
    cam_t cam;
    int baud;
    int reset = 0;
    int maybe_modem = 0;
    int is_modem = 0;
    char *chatargs;

    TRACE("va", "%s %s\n%s\n", VERSION, DATE, COPYRIGHT);

    cam = (cam_t)malloc(sizeof(struct camera_s));
    if (!cam) {
	errormsg(__FILE__ ": couldn't malloc camera_s struct\n");
	return 0;
    }

    memset(cam, 0, sizeof(*cam));

    winc_locks_init(cam);

    cam->ttyname = winc_configvar("WinCamDevice", "COM2");

    if (comms_initialize_port(cam) != OK) {
	free(cam);
	return 0;
    }

    /* see if it might be modem-connected */
    chatargs = winc_configvar("WinCamModemChat", "");
    if (chatargs && *chatargs)
    	maybe_modem++;


    /* try talking to the camera -- if we can't, reset it.  then
     * try talking again, and try successively lower baud rates.
     */

    /* start out fast */
    for (baud = comms_first_baud();
    		!is_modem && baud >= 0;
		baud = comms_next_baud(baud)) {
	TRACE("A","Trying baud rate %d on tty %s\n",
			baud, cam->ttyname);
    retry:

	if (comms_set_baud(cam, baud) != OK)
	    continue;

	if (maybe_modem) {

	    if (winc_check_for_modem( cam ) == OK) {
	    	TRACE("va", "Found modem: tty %s, baud %d, running \n  %s\n",
			cam->ttyname, cam->baud_index, chatargs);

		is_modem = 1;

		/* spawn chat program */

                system(chatargs);

		TRACE("a", __FILE__ ": chat program completed ok\n");
	    }

	}

	if (winc_getstatus( cam ) == OK) {

	    TRACE("va", "Found camera: tty %s, baud %d, "
			"version %d.%d%c, id %d\n",
		cam->ttyname, baud,
		cam->version.major, cam->version.minor, cam->version.chiptype,
		(cam->id.id[3] << 24) +
		(cam->id.id[2] << 16) +
		(cam->id.id[1] <<  8) +
		(cam->id.id[0] <<  0)  );

	    TRACE("va", "  (manufactured %02d/%02d/%02d %02d:%02d:%02d)\n",
		(cam->id.id[3] & 0x0f), 	    /* month */
		(cam->id.id[2] & 0xf8) >> 3,	    /* day */
		((cam->id.id[3] & 0xf0) >> 4) + 1996,	/* year */

		(cam->id.id[2] & 0x07) +
		((cam->id.id[1] & 0xc0) >> 6),	    /* hour */
		(cam->id.id[1] & 0x3f), 	    /* minute */
		(cam->id.id[0] & 0xfc) >> 2	    /* second */
	    );

	    /* set default useful values for basic camera parameters */
	    if (winc_extrastop(cam, 
		    cfg_number(winc_configvar("WinCamStopBits", "0"))) != OK)
		return NotOK;
	    if (winc_linesync(cam, 
		    cfg_boolean(winc_configvar("WinCamLineSync", "on"))) != OK)
		return NotOK;
	    if (winc_sensitivity(cam, 
		    cfg_number(winc_configvar("WinCamSensitivity", "15"))) != OK)
		return NotOK;

	    return cam;
	}
	if (!reset) { /* try a reset after first failure */
	    if (winc_reset(cam) != OK) {
		goto fail;
	    }
	    reset++;
	    goto retry; /* retry first failed baud rate */
	}
    }

fail:
    comms_close_port(cam);
    if (cam)
	free(cam);
    return 0;
}

void
winc_close(cam_t cam)
{
    TRACE("a", __FILE__ ": closing camera\n");
    winc_flush(cam);
    comms_close_port(cam);
    if (cam->starfield)
    	free(cam->starfield);
    free(cam);
}

result
winc_getstatus( cam_t cam )
{
    byte retbytes[8];

    assert(cam);

    TRACE("a", __FILE__ ": getting camera status\n");
    if (winc_send_cmd(cam, Cmd_send_version, -1) != OK)
	return NotOK;

    if (winc_get_resp(cam, 3, retbytes, 0) != OK)
	return NotOK;

    cam->version.minor = retbytes[0];
    cam->version.major = retbytes[1];
    cam->version.chiptype = retbytes[2];

    if (winc_send_cmd(cam, Cmd_send_id, -1) != OK)
	return NotOK;

    if (winc_get_resp(cam, 4, (byte *)&(cam->id), 0) != OK)
	return NotOK;

    return OK;
}

result
winc_extrastop( cam_t cam,
		    int extra_stop)	/* add stop bits */
{

    assert(cam);

    TRACE("A", __FILE__ ": setting %d extra stop bits\n", extra_stop);
    if (winc_send_cmd(cam, Cmd_extra_stop, extra_stop, -1) != OK)
	return NotOK;

    return OK;
}

result
winc_linesync( cam_t cam,
		    int sync_60hz)  /* for 60hz flicker */
{

    assert(cam);

    TRACE("A", __FILE__ ": turning flouresnent sync mode %s\n",
			    sync_60hz ? "on" : "off");
    if (winc_send_cmd(cam, Cmd_sync_mode, (sync_60hz != 0), -1) != OK)
	return NotOK;

    return OK;
}

result
winc_sensitivity( cam_t cam,
		    int sensitivity )	    /* light sensitivity */
{

    assert(cam);

    if (sensitivity < 0) sensitivity = 0;
    if (sensitivity > 15) sensitivity = 15;
    TRACE("A", __FILE__ ": setting light sensitivity level to %d\n",
			    sensitivity);
    if (winc_send_cmd(cam, Cmd_set_light, sensitivity, -1) != OK)
	return NotOK;

    return OK;
}

