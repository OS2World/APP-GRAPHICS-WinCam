
/* this struct defines the interface to the color processing code in
 * color.c.  please go there for documentation.
 *	(c) Copyright 1996 StarDot Technologies
 *	(c) 1996 by daniel lawton
 */

struct colordata {
    int linenum;
    int mode;
#define COLOR_NONINTERLACED_24	0
#define COLOR_INTERLACED_24 1
#define GREY_NONINTERLACED_8	2
#define GREY_INTERLACED_8   3
#define GREY_NONINTERLACED_24	4
#define GREY_INTERLACED_24  5
#define RESET_LINESTATS 0x80
#define GET_LINESTAT_FOR_LINE 0x81

    int bufwidth;
    int bufheight;
    int redlev;
    int greenlev;
    int bluelev;
    int intensity;
    int pixlimits[2];
    int hazelevel;
    int diflimit;
    int options;
#define OPT_COLOR_AVG	    0x01
#define OPT_COLOR_CROP	    0x02
#define OPT_VIGNETTE_CORRECTION 0x04
#define OPT_SIMPLE_SUBTRACTION	0x08
#define OPT_HORIZ_STRETCH   0x10
#define OPT_COLOR_TRACING   0x20
#define OPT_EDGE_CORRECTION 0x40

    byte    blacklevs[2];
    byte    *input_off;
    byte    *temp_off;
    byte    *result_off;
};

extern void sharpcolor(struct colordata *);
