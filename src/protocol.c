/*
 *
 * handle the serial command structure for getting data to and from the camera.
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
 * $Header: E:/winc/RCS/protocol.c 1.1 1997/03/01 03:44:14 Derek Exp Derek $
 */
#define NDEBUG 1

/* routines for shuffling bytes to and from the camera */

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <assert.h>
#include "private.h"
#include "trace.h"

result
winc_reset( cam_t cam )
{
    assert(cam);

    TRACE("p", __FILE__ ": resetting camera\n");

    if (comms_send_break(cam, 300) != OK)
	return NotOK;
    _sleep2(100);	/* wait 100 milliseconds */
    return OK;
}

result
winc_flush(cam_t cam)
{
    byte buf[2048];
    static byte nops[] = { Cmd_nop, Cmd_nop, Cmd_nop, Cmd_nop, Cmd_nop };

    assert(cam);


    if (!cam->havelock)
	return OK;

    TRACE("a", __FILE__ ": had lock, flushing commands, timeouts are ok\n");

    if (comms_write(cam, nops, 5) == -1) {
	errormsg(__FILE__ ": couldn't send nops\n");
	return NotOK;
    }
    (void)comms_timed_read(cam, buf, sizeof(buf), 1000);
    return OK;
}

result
winc_send_cmd( cam_t cam, Cmd_byte_t cmd, ... )
{

    byte b;
    int i;
    va_list ap;
    int retry = 2;
    result res = OK;

    assert(cam && cam->havelock);

    TRACE("p", __FILE__ ": sending camera command 0x%x\n", cmd);


    while (retry--) {

    	b = (byte)cmd;

	if (comms_write(cam, &b, 1) == -1) {
	    errormsg(__FILE__ ": couldn't send cmd byte: %s\n", strerror(errno));
	    return NotOK;
	}

	va_start (ap, cmd);
	while ((i = va_arg(ap, int)) != -1) {
	    b = (byte)i;
	    TRACE("p", __FILE__ ": sending 0x%x\n", b);
	    if (comms_write(cam, &b, 1) == -1) {
		errormsg(__FILE__ ": couldn't send arg byte: %s\n", strerror(errno));
		return NotOK;
	    }
	}
	va_end (ap);

	res = OK;

	/* almost all commands come back with an ACK */
	if (cmd != Cmd_nop && cmd != Cmd_disable_all) {
	    i = comms_timed_read(cam, &b, 1, 4000);
	    if (i == -1)
		return NotOK;
	    else if (i == -2)	/* timeout */
		res = NotOK;
	    else if (b != Resp_Ack) {
		TRACE("P", __FILE__ ": expected 0x%x, got 0x%x\n",
					Resp_Ack, i);
		res = NotOK;
	    }
	}
	if (res == OK)
	    break;
	else if (retry) {
	    winc_flush(cam);
	    errormsg(__FILE__ ": retrying command %x\n", cmd);
	}
    }

    return res;
}

result
winc_get_resp( cam_t cam,
    int nbytes,
    byte *bp,
    unsigned long *sump)
{
    return winc_get_resp_wait( cam, nbytes, bp, 200, sump);
}

result
winc_get_resp_wait( cam_t cam,
    int nbytes,
    byte *buf,
    int millisec,
    unsigned long *sump)
{
    int i;
    int tcnt = 16;
    byte *bp;

    TRACE("p", __FILE__ ": getting camera response, want %d bytes\n", nbytes);

    TRACE("P", __FILE__ ": receiving:");
    if (sump)
	*sump = 0;
    i = comms_timed_read(cam, buf, nbytes, millisec);
    if (i == -1) {
	errormsg(__FILE__ ": response failed\n");
	return NotOK;
    } else if (i == -2) {
	errormsg(__FILE__ ": response timed out\n");
	return NotOK;
    }
    if (TRACE_IS_ACTIVE()) {
	bp = buf;
	for (i = 0; i < nbytes; i++) {
	    if (tcnt == 16) {
		TRACE("P", "\n	" __FILE__ ": got");
		tcnt = 0;
	    }
	    if ((tcnt % 4) == 0)
		TRACE("P", " ");
	    TRACE("P", " %02x", *bp++);
	    tcnt++;
	}

    }

    if (sump) {
	bp = buf;
	for (i = 0; i < nbytes; i++)
	    *sump += *bp++;
    }

    TRACE("P", "\ngot %d bytes\n", nbytes);

    return OK;
}

result
winc_check_for_modem( cam_t cam )
{
    char buf[10];
    int i;

    assert(cam);

    TRACE("p", __FILE__ ": checking for modem\n");
    if (comms_write(cam, "AT\r", 3) == -1) {
	errormsg(__FILE__ ": couldn't send AT cmd: %s\n", strerror(errno));
	return NotOK;
    }

    i = comms_timed_read(cam, buf, 1, 4000);
    if (i < 0)
	return NotOK;

    if (buf[0] != 'A') /* then it's not a modem */
    	return NotOK;

    TRACE("P", __FILE__ ": looking for modem got A\n");

    /* we should get "OK" within 8 characters */
    i = comms_timed_read(cam, buf, 8, 4000);
    if (i < 0)
	return NotOK;

    buf[7] = 0;
    if (i > 1 && buf[0] == 'T' && strstr(buf, "OK")) {
	TRACE("p", __FILE__ ": probably have modem\n");
	return OK;
    }
    TRACE("p", __FILE__ ": not modem: '%s'\n", buf);

    return OK;
}

