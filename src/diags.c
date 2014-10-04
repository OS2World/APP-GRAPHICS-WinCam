/*
 *
 * run wincam diagnostics
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
 * $Header: E:/winc/RCS/diags.c 1.1 1997/03/01 03:44:14 Derek Exp Derek $
 */
#define NDEBUG 1

#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <termios.h>
#include <assert.h>
#include <string.h>
#include "private.h"
#include "trace.h"

#define SENDBYTES 0x55, 0xaa, 0x80, 0x01, 0xff, 0x00 

result
winc_diagnose( cam_t cam )
{
    int i;
    byte retbytes[256];
    byte sendbytes[6] = { SENDBYTES };

    assert(cam);

    TRACE("da", "Running camera diagnostics...\n");
    TRACE("dA", __FILE__ ": running receive test\n");
    if (winc_send_cmd(cam, Cmd_rx_test, SENDBYTES, -1 ) != OK)
	return NotOK;

    if (winc_get_resp(cam, 6, retbytes, 0) != OK)
	return NotOK;

    if (memcmp(retbytes, sendbytes, 6) != 0) {
	errormsg(__FILE__ ": receive test failed\n");
	return NotOK;
    }
    TRACE("dA", __FILE__ ": receive test passed\n");

    TRACE("dA", __FILE__ ": running transmit test\n");
    if (winc_send_cmd(cam, Cmd_tx_test, -1) != OK)
	return NotOK;

    memset(retbytes, 0, 256);
    if (winc_get_resp(cam, 256, retbytes, 0) != OK)
	return NotOK;

    for (i = 0; i < 256; i++) {
	if (retbytes[i] != i) {
	    errormsg(__FILE__ ": receive test failed\n");
	    return NotOK;
	}
    }
    TRACE("dA", __FILE__ ": transmit test passed\n");

    TRACE("dA", __FILE__ ": running dram test\n");
    if (winc_send_cmd(cam, Cmd_test_dram, -1) != OK)
	return NotOK;

    if (winc_get_resp_wait(cam, 1, retbytes, 10000, 0) != OK)
	return NotOK;

    if (retbytes[0] == '*') {
	TRACE("dA", __FILE__ ": dram test passed\n");
    } else if (retbytes[0] == '-') {
	errormsg(__FILE__ ": dram test failed\n");
    } else {
	errormsg(__FILE__ ": dram test -- unexpected result\n");
    }

    TRACE("da", " ...diagnostics passed\n");
    return OK;
}

