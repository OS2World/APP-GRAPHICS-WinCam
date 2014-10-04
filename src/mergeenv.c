
/*
 * merge a file with the environment
 */

/*
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
 */

/*
 * read the specified file, parse out environment-like variables (i.e.
 * things that look like FOO=BAR, or MyVerySpecialVariable=frewfrew)
 * and put them in the environment.  if "replace" is true, replace
 * existing variables, otherwise skip them.  variables cannot be removed
 * from the environment -- only added or replaced, possibly being set to null.
 *
 *  - lines whose first non-whitespace character is '#' are skipped.
 *  - end-of-line comments are not supported.
 *  - whitespace surrounding the '=' sign itself is removed, as is
 *	whitespace at the end of the line.  thus no values can ever
 *	begin or end in whitespace.
 *  - no other modifications are made to the value part.
 *  - no quoting mechanisms are supported.
 *  - line continuations are not supported.
 */ 
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>

/*
 * wrap putenv in something that allocates the necessary memory
 * for us.
 */
int
safeputenv(char *name, char *value)
{
    char *cp;
    cp = malloc(strlen(name) + strlen(value) + 2);
    if (!cp)
	return -1;

    sprintf(cp, "%s=%s", name, value);
    if (putenv(cp) == -1) {
	free(cp);
	return -1;
    }
    return 0;
}

int
mergeenv( char *filename, int replace)
{
    FILE *fp;
    int c, nlen;
    int lineno = 0;
    char *cp, *np, *vp;
    char line[160];

    fp = fopen(filename, "r");
    if (!fp)
	return 0;

    while (fgets(line, sizeof(line), fp) != NULL) {
	lineno++;
	cp = strchr(line, '\n');
	if (!cp) { /* didn't get a whole line */
	    /* skip the _rest_ of that line */
	    while ((c = fgetc(fp)) != '\n' && c != EOF)
		;
	    continue;
	}
	*cp = '\0'; /* nuke the newline */

	cp = line;
	while (isspace(*cp)) cp++;  /* eat leading whitespace */

	/* nothing of interest on line? */
	if (!*cp || *cp == '#')
	    continue;

	np = cp;    /* name starts here */

	while (isalnum(*cp) || *cp == '_') cp++; /* the name part */

	/* name part is this long */
	nlen = cp - np;

	while (isspace(*cp)) cp++;  /* eat whitespace */

	if (nlen == 0 || *cp++ != '=') { /* syntax error */
	    fclose(fp);
	    return lineno;
	}
	np[nlen] = '\0'; /* terminate the name part */

	/* we're not supposed to replace existing, and it exists */
	if (!replace && getenv(np))
	    continue;

	while (isspace(*cp)) cp++;  /* eat whitespace */

	vp = cp;    /* value starts here */

	cp = strchr(vp, '\0'); /* find end of line */
	while(isspace(*--cp))	/* shrink to non-whitespace */
	    *cp = '\0';

	if (safeputenv(np, vp) == -1) {
	    fclose(fp);
	    return -1;
	}
    }

    fclose(fp);
    return 0;
}

#if TESTING

#include <unistd.h>

int
main()
{
    int r;
    r = mergeenv("foo",1);
    if (r == 0)
	execl("/bin/sh", "sh", 0);
    if (r == -1) {
	fprintf(stderr, "error reading file\n");
	exit(1);
    }
    fprintf(stderr, "syntax error at line %d\n", r);
    return 1;
}
#endif
