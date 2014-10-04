/*
 *
 * device specific code.  with any luck, when porting to a new
 * platform, this may be the only module that needs changing.
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
 * $Header: E:/winc/RCS/comms.c 1.1 1997/03/01 03:44:14 Derek Exp Derek $
 */

#define INCL_DOSFILEMGR
#define INCL_DOSDEVIOCTL
#define INCL_DOSDEVICES

#include <os2.h>


#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <signal.h>
#include "private.h"
#include "trace.h"

/* static int comms_token2index(speed_t token); */
/* static int comms_rate2index(int rate); */

static DCBINFO CommInfo;

static int
get_input_fd(int ofd)
{
    return ofd;
}

/* 
 * static jmp_buf timed_read_jmpbuf;
 *
 * static void
 * alarmcatcher(int sig)
 * {
 *   (void) signal(SIGALRM, alarmcatcher);
 *   longjmp(timed_read_jmpbuf, 1);
 * }
 */

/*
 * initialize the camera's serial port
 */
result
comms_initialize_port( cam_t cam )
{
   APIRET rc;
   ULONG ulAction;
   LINECONTROL linctrl;
   ULONG ulParmLen;
    
    TRACE("ac",__FILE__ ": opening %s\n", cam->ttyname);

    rc = DosOpen(cam->ttyname,
                 &cam->ofd,
                 &ulAction,
                 0,
                 FILE_NORMAL,
                 FILE_OPEN,
                 OPEN_ACCESS_READWRITE | OPEN_SHARE_DENYREADWRITE,
                 (PEAOP2) NULL);


    if (rc != 0) {
	errormsg(__FILE__ ": couldn't open tty %s\n", cam->ttyname);
	return NotOK;
    }

    cam->ifd = get_input_fd(cam->ofd);
    if (cam->ifd == -1) {
	errormsg(__FILE__ ": couldn't re-open tty %s\n", cam->ttyname);
	close(cam->ofd);
	return NotOK;
    }

   /* disable parity, both in and out */
   /* two stop bits on transmit */
   /* no modem control, 8bit chars, */
   /* receiver enabled, */
   /* ignore break, ignore parity errors */

   ulParmLen = 4;
   linctrl.bDataBits = 8;
   linctrl.bParity = 0;
   linctrl.bStopBits = 2;
   linctrl.fTransBreak = 0;

   rc = DosDevIOCtl(cam->ofd,
                    IOCTL_ASYNC,
                    ASYNC_SETLINECTRL,
                    (PULONG) &linctrl,
                    sizeof(linctrl),
                    (PULONG) &ulParmLen,
                    (PULONG) NULL,
                    0,
                    (PULONG) NULL);
   rc = DosDevIOCtl(cam->ofd,
                    IOCTL_ASYNC,
                    ASYNC_SETLINECTRL,
                    (PULONG) &linctrl,
                    sizeof(linctrl),
                    (PULONG) &ulParmLen,
                    (PULONG) NULL,
                    0,
                    (PULONG) NULL);
   rc = DosDevIOCtl(cam->ofd,
                    IOCTL_ASYNC,
                    ASYNC_SETLINECTRL,
                    (PULONG) &linctrl,
                    sizeof(linctrl),
                    (PULONG) &ulParmLen,
                    (PULONG) NULL,
                    0,
                    (PULONG) NULL);

    if (rc != 0) {
	comms_close_port(cam);
	return NotOK;
    }

    /* Fill out the CommInfo DCBINFO block - we'll need this for */
    /* setting the timeouts for reads                            */

    rc = DosDevIOCtl(cam->ofd,
                     IOCTL_ASYNC,
                     ASYNC_GETDCBINFO,
                     (PULONG) NULL,
                     0,
                     (PULONG) NULL,
                     (PULONG) &CommInfo,
                     sizeof(CommInfo),
                     (PULONG) &ulParmLen);

    if (rc != 0) {
	comms_close_port(cam);
	return NotOK;
    }
    return OK;
}


/*
 * close the camera's serial port.
 */
void
comms_close_port(cam_t cam)
{
   APIRET rc; 

    TRACE("ac",__FILE__ ": closing %s\n", cam->ttyname);
    rc = DosClose(cam->ifd);
    rc = DosClose(cam->ofd);
}

/*
 * the bauds[] table is a mapping of system baudrate tokens to
 * bits/sec values.
 */
struct {
    speed_t token;
    int integer;
} bauds[] = {
    {0x0013,	460800},
    {0x0012,	230400},
    {0x0011,	115200},
    {0x0010,	57600},
    {B38400,	38400},
    {B19200,	19200},
    {B9600,	9600},
    {B4800,	4800},
    {B2400,	2400},
    {B1200,	1200},
};
#define NUMBAUDS (sizeof(bauds)/sizeof(bauds[0]))

/*
 * convert a baudrate, represented by a token,
 * to an index in the bauds[] table.
 */
/* int
 * comms_token2index(speed_t token)
 * {
 *   int i = 0;
 *   while (i < NUMBAUDS && token != bauds[i].token)
 *	i++;
 *
 *   if (i == NUMBAUDS)
 *	return -1;
 *
 *   return i;
 * }
 */
/*
 * convert a baudrate, in bits/sec,
 * to an index in the bauds[] table.
 */
/* int
 * comms_rate2index(int rate)
 * {
 *   int i = 0;
 *   while (i < NUMBAUDS && rate < bauds[i].integer)
 *	i++;
 *
 *   
 *   if (i == NUMBAUDS)
 *	return NUMBAUDS - 1;
 *
 *    return i;
 * }
 */
/*
 * return the actual baudrate corresponding to an index
 */
/* int 
 * comms_index2rate(int bindex)
 * {
 *   return bauds[bindex].integer;
 * }
 */
/*
 * choose the baud rate equal or less than the configured default,
 * and return its index.
 */
int
comms_first_baud(void)
{
    int wantbaud;

    /* the following default be at least as fast as the fastest in table */
    wantbaud = cfg_number(winc_configvar("WinCamBaudRate", "115200"));
    return wantbaud;
}

/*
 * given a baudrate index, return index of next, or -1 if there are no more
 */
int 
comms_next_baud(int bindex)
{
    if (bindex < NUMBAUDS-1)
	return ++bindex;
    return -1;
}

/*
 * set port to given baud, based on index to bauds[] array 
 */
result
comms_set_baud(cam_t cam, int rate)
{
  APIRET rc;
  ULONG ulParmLen;
  ULONG usBPS;


   ulParmLen = 2;
   usBPS = rate;
   TRACE("ac", __FILE__ ": baud rate (%ld): \n", usBPS);

   if(usBPS <= 19200)
    {
      rc = DosDevIOCtl(cam->ofd,
                       IOCTL_ASYNC,
                       ASYNC_SETBAUDRATE,
                       (PULONG) &usBPS,
                       ulParmLen,
                       (PULONG) &ulParmLen,
                       (PULONG) NULL,
                       0,
                       (PULONG) NULL);
    }
   else
    {
      ulParmLen = 4;
      rc = DosDevIOCtl(cam->ofd,
                       IOCTL_ASYNC,
                       ASYNC_EXTSETBAUDRATE,
                       (PULONG) &usBPS,
                       ulParmLen,
                       (PULONG) &ulParmLen,
                       (PULONG) NULL,
                       0,
                       (PULONG) NULL);
    }

    if(rc != 0)
       {
	TRACE("ac", __FILE__ ": couldn't set baud rate (%d): %d\n",
	    rate, errno);
	return NotOK;
    }
    cam->baud_index = rate;
    return OK;
}

/*
 * sets camera to given baud rate 
 */
int
winc_set_baud(cam_t cam, int rate)
{
    TRACE("ac",__FILE__ ": setting baud rate %d\n", rate);
    return comms_set_baud(cam, rate);
}

/*
 * gets camera's current baud rate 
 */
int
winc_get_baud(cam_t cam)
{
    return (cam->baud_index);
}

/*
 * read a buffer's worth of chars from the camera, taking at
 * with a guaranteed maximum timeout.
 */
int
comms_timed_read(cam_t cam, char *buf, int len, int millisec)
{

        USHORT sec;
	int ret, totret = 0;
	int cps;
        ULONG bytesread;
        ULONG ulParmLen;

	sec = (millisec + 999)/1000;
	cps = cam->baud_index/10; /* assume 10 bits/char */
	CommInfo.usReadTimeout = (sec + (len/cps + 1)) * 100;
        CommInfo.fbTimeout     = 0xD4;     /* SIO defaults + MODE_WAIT_READ_TIMEOUT */
        DosDevIOCtl(cam->ofd,
                    IOCTL_ASYNC,
                    ASYNC_SETDCBINFO,
                    (PULONG) &CommInfo,
                    sizeof(CommInfo),
                    (PULONG) &ulParmLen,
                    (PULONG) NULL,
                    0,
                    (PULONG) NULL);

	while(totret < len)
         {
	    ret = DosRead(cam->ifd, buf + totret, len - totret, &bytesread);
	    if (ret == -1)
             {
		errormsg(__FILE__ ": read failed: %s\n", strerror(errno));
		totret = -1;
		break;
	     }
            else if (bytesread == 0)
             {
               	TRACE("ac", __FILE__ ": timeout waiting for response\n");
        	return -2;
             }

	    totret += bytesread;
	 }
	return totret;
}

/*
 * comms_write:  send content of buffer to camera
 */
int
comms_write(cam_t cam, char *buf, int n)
{
    ULONG byteswritten; 

    return DosWrite(cam->ofd, buf, n, &byteswritten);
}


result
comms_send_break(cam_t cam, int milli)
{
  APIRET rc;
  ULONG  packet;
  ULONG  ulParmLen;

      TRACE("ac", __FILE__ ": sending break\n");

      ulParmLen = 2;
      rc = DosDevIOCtl(cam->ofd,
                       IOCTL_ASYNC,
                       ASYNC_SETBREAKON,
                       (PULONG) NULL,
                       0,
                       (PULONG) NULL,
                       (PULONG) &packet,
                       ulParmLen,
                       (PULONG) &ulParmLen);

      _sleep2(1000);

      ulParmLen = 2;
      rc = DosDevIOCtl(cam->ofd,
                       IOCTL_ASYNC,
                       ASYNC_SETBREAKOFF,
                       (PULONG) NULL,
                       0,
                       (PULONG) NULL,
                       (PULONG) &packet,
                       ulParmLen,
                       (PULONG) &ulParmLen);
 

    return OK;
}
