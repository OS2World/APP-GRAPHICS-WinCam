/*
 *
 * simple tracing control, to isolate most of the code from the vagaries
 * of where this stuff should end up.  clients of the library should
 * register their preferred output routines using the "register_...()"
 * functions.  these routines should have semantics similar to vfprintf(),
 * (though return codes are ignored).  for a command line app, you might
 * register vfprintf itself, with a cookie of "stderr".  for a graphical
 * app, you would do something more reasonable.
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
 * $Header: E:/winc/RCS/trace.c 1.1 1997/03/01 03:44:14 Derek Exp Derek $
 */


#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include "trace.h"


/*
 * errors
 */
int errorlevel;
static vprintffunc errorfunc;
static void *errorcookie;

/* hook in the user's callback routine */
void
register_error_func(vprintffunc f, void * cookie)
{
    errorfunc = f;
    errorcookie = cookie;
}

/* set the current output level:  higher is more detail */
void
set_error_level(int new)
{
    errorlevel = new;
}

/* emit a message to the right place */
void
errormsg(char *fmt, ...)
{
    va_list ap;

    if (!errorfunc)
	return;

    va_start(ap, fmt);
    (*errorfunc)(errorcookie, fmt, ap);
    va_end(ap);
}

/*
 * tracing
 */
char tracechars[30];
static int fulltrace;
static vprintffunc tracefunc;
static void *tracecookie;

/* hook in the user's callback routine */
void
register_trace_func(vprintffunc f, void * cookie)
{
    tracefunc = f;
    tracecookie = cookie;
}


/* set the current output level:  higher is more detail */
void
set_trace_class(char *newclass)
{
    strncpy(tracechars, newclass, sizeof(tracechars));
    tracechars[sizeof(tracechars)-1] = '\0';

    /* a 'V' anywhere in the global set turns on all tracing */
    fulltrace = (strchr(tracechars, 'V') != 0);

}

/* emit a message to the right place */
void
tracemsg(char *class, char *fmt, ...)
{
    va_list ap;
    char *cp;

    if (!tracefunc)
	return;

    /* if the same character appears in both the global selector
	string and in the string attached to the tracepoint itself,
	then we print it.  if the global selector string has an
	uppercase char, it will match a lowercase tracepoint, but not
	vice-versa. (so a user asking for "A" messages will get both
	"a" and "A" messages, but a user asking for just "a" messages
	will get only those. */

    for (cp = class; *cp; cp++) {
	if (fulltrace || strchr(tracechars, *cp) ||
		(islower(*cp) && strchr(tracechars, toupper(*cp)))) {
	    va_start(ap, fmt);
	    (*tracefunc)(tracecookie, fmt, ap);
	    va_end(ap);
	    return;
	}
    }
}
