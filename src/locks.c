/*
 *
 * lock/unlock the wincam tty
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
 * $Header: E:/winc/RCS/locks.c 1.1 1997/03/01 03:44:14 Derek Exp Derek $
 */
#define NDEBUG 1

#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include "private.h"
#include "trace.h"

void
winc_locks_init( cam_t cam )
{
}

void
winc_lock(cam_t cam)
{
}

void
winc_unlock(cam_t cam)
{
}
