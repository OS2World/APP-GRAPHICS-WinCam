/*
 * configuration tool -- takes care of reading in the config file, and
 * handles the defaulting of requested information.
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
 */
#define NDEBUG 1

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "trace.h"

extern int mergeenv(char *file, int replace);

char *
winc_configvar(char *var, char *defaultval)
{
    char *cp = 0;
    char *s;
    char var_with_id[256];
    static env_inited = 0;
    static char *id = 0;
    char fname[512];

    if (!env_inited++) {
	int i;
        char *tempbuff;
        tempbuff = getenv("WINC_RC");
	if(tempbuff) 
          strcpy(fname, tempbuff);
        else
         {
            tempbuff = getenv("ETC");
            strcpy(fname, tempbuff);
	    strcat(fname, "\\winc.rc");
         }

	TRACE("r", __FILE__ ": reading config from %s\n", fname);

	i = mergeenv(fname, 0);
	if (i < 0) {
	    errormsg(__FILE__ ": hard error reading \"%s\"\n", fname);
	    return NULL;
	}
	if (i)
	    errormsg(__FILE__ ": syntax error at line %d of \"%s\"\n",
		i, fname);

	id = getenv("WinCamId");
	TRACE("r", __FILE__ ": camera id is %s\n", id ? id:"null");
    }

    s = "configured";
    if (id) {
	/* i wish snprintf were standard... */
	strncpy(var_with_id, id, sizeof(var_with_id));
	strncat(var_with_id, var, sizeof(var_with_id) - strlen(id));
	cp = getenv(var_with_id);
	if (cp)
	    var = var_with_id;  /* for the TRACE statement below */
    }

    /* if there was no id, or if looking up the id'ed version failed, look
     * for the non-id'ed version.
     */
    if (!cp) {
	cp = getenv(var);
	if (!cp) {
	    assert(defaultval);
	    cp = defaultval;
	    s = "default";
	}
    }

    TRACE("r", __FILE__ ": returning %s %s value: %s\n", var, s, cp);
    return cp;
}

int
cfg_number(char *s)
{
    return strtol(s, 0, 0);
}

int
cfg_boolean(char *s)
{
    int n = strlen(s);
    if (!strncmp("ON", s, n) ||
	!strncmp("TRUE", s, n) ||
	!strncmp("YES", s, n) ||
	!strncmp("on", s, n) ||
	!strncmp("true", s, n) ||
	!strncmp("yes", s, n) ||
	!strncmp("1", s, n))
	return 1;

    return 0;
}
