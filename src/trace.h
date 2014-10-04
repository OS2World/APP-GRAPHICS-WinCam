/*
 *
 * simple tracing control, to isolate most of the code from the vagaries
 * of where this stuff should end up.
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
 * $Header: E:/winc/RCS/trace.h 1.1 1997/03/01 03:44:14 Derek Exp Derek $
 */

#include <stdarg.h>

/*
 * If we're not using gcc, turn off "attribute" stuff.
 */
#ifndef __GNUC__
#define __attribute__(x)
#endif


extern int errorlevel;
extern char tracechars[];

typedef int (*vprintffunc)( void *, const  char  *,  va_list );
void register_error_func(vprintffunc, void *);
void errormsg(char *fmt, ...)
    __attribute__ ((format (printf, 1, 2)));


void register_trace_func(vprintffunc, void *);
void set_trace_class(char *newclass);
void tracemsg(char *class, char *fmt, ...)
    __attribute__ ((format (printf, 2, 3)));

/* if there's any tracing at all going on, invoke tracemsg() */
#define TRACE if (tracechars[0]) tracemsg
#define TRACE_IS_ACTIVE()   (tracechars[0])

/* if there's any tracing at all going on, invoke tracemsg() */
#define ERROR if (errorlevel) errormsg


