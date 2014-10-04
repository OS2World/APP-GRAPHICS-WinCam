/**
    This code in this module is
	(c) Copyright 1996 StarDot Technologies
	(c) 1996 by daniel lawton

    This color processing code is licensed for use only with the
    WinCam.One digital camera developed by StarDot Technologies.
    (Visit http://www.wincam.com for more information.)

    The translation from x86 assembler to C was done by Paul Fox with the
    agreement of StarDot Technologies.  Errors of translation are perhaps
    inevitable -- Paul takes all responsibility for such errors.

    Although StarDot Technologies provided the technical interface and
    processing information used in this software, StarDot Technologies
    makes no warranties nor provides any technical support regarding its
    use with WinCam.One. 

    This program is free software; you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by the
    Free Software Foundation; either version 2 of the License, or (at your
    option) any later version.

    This program is distributed in the hope that it will be useful, but
    WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    675 Mass Ave, Cambridge, MA 02139, USA.


    	[ Allowing release of this code under GPL terms is, in my
	  opinion, very generous of the folks at StarDot.  If you
	  use this software, please let them know, so that they know
	  these efforts weren't a waste of time.  Thanks!  -pgf ]

**/

/*
    If you spend much time reading this code, you'll note that some
    of the C constructs look a little unnatural -- this is probably
    a result of being translated from the assembly code original.  I'm
    open to patches and improvements.  -pgf
*/

typedef unsigned char byte;
#include <string.h>
#include <math.h>
#include "trace.h"

#include "color.h"

#ifdef THIS_IS_WHATS_IN_COLOR_H
struct colordata {
    int linenum;
    int mode;
#define COLOR_NONINTERLACED_24	0
#define COLOR_INTERLACED_24 1
#define GREY_NONINTERLACED_8	2
#define GREY_INTERLACED_8   3
#define GREY_NONINTERLACED_24	4
#define GREY_INTERLACED_24  5
#define RESET_LINESTATS     0x80
#define GET_LINESTAT_FOR_LINE	0x81

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
#endif

/** these should be commented out for a running version.  **/
/* #define MOIREDBG   1 */ /** shows map of internal moire removal logic  **/
/* #define SHOWBAD    1 */ /** shows bad pixels as black dots.	**/

#ifdef COMMENTARY
/*


 here are some recommended defaults:
    red level = 128
    blue level = 128
    green level = 75 (ccd is more sensitive to green.  take 59% of 128.
    intensity = 336
    pixlimits = 3, 6
    haze = 8
    diflimit = 42
    options = 65h

 this routine takes in pointers to lines of pixels of color
 mosiac information from the sharp lz2313b5.  it processes to make a line
 which is the color information for the bottom of each set of 2 lines.
 the initial data produced is "unrefined" but displayable.  it is later
 refined to remove moire patterns when sufficient information becomes
 available. when the data is no longer needed for moire processing on
 other lines, a final intensity adjustment is made using your selection
 of color cropping and options bits.

 thus you call 1 time and get an initial image line, but the final
 data is available later, and you poll the program to find out if
 the data is finished.	or you can subtract a fixed value from the line
 you are processing to find out where the finished lines begin.

 because this routine processes each line several times, the buffers to hold
 the input and output data must be valid for the minimum number of
 scan lines it takes to produce the moire free data.  the number is
 equal to twice the value you are using for the larger of pixlimits.

 if your program is running in flat mode, simply allocate
 a full buffer for the input data (about 256k depending on how black
 levels are stored) and a full buffer for the output data (755712 bytes).
 you can either poll to see if a line is finished, or use the same logic
 as is used when a full screen buffer is not available.

 if your program does not enough memory for a full buffer, you can
 free up the input or output buffers pixlimits*2 scan lines after you call.
 the final data will be available for these lines and you can recycle
 the data.  for instance, if you have just passed the data for scan line
 16h for the first time, and your high pixlimits value is 2,
 scan line 12h is free as a result of the call.  note that if you had
 passed the data for scan line 116h, 112h would be free and there would be
 no effect on the scan lines from 0-0f5h.  the upper screen image is not
 connected to the lower screen image.

 you can also poll the routine to find out if a scan line is free.
 use funtion 81h and look for return code 03.

 because scan lines are processed several times, before calling for the
 first time you must use the reset function, to signal to the program that
 the data in old buffers is invalid.  if you do not do this you will
 get an "overlay" effect from the last processed data, or worse yet, if
 you are running in protected mode you might crash the program when
 invalid addresses from the old buffers list are used.

 on return, the resulting bgr data is in the buffer specified by
 result_off in 24 bit bgr format if color, 8 bit grayscale
 if black and white.  your input data is not modified.	note that the
 output data is processed in degrees, as surrounding lines become available.
 the first pass is not free of moire patterns, the second is not final
 intensity adjusted.  the third pass has complete data and the buffer
 can be reused.  use function 81h to check status.

 here's a detailed description of the data structure values.

 *line number of line to be processed. 0-245 or 256-501.
	  not the screen display row!
	  add 100h for second image of interlaced.
       note: although you should present all lines for processing,
       it is generally understood that only about 480 of the 492
       possible lines are valid. you should throw out the top
       10 lines in double scan mode, 5 in single scan mode, and
       also don't use the bottom 2 lines.  the ccd takes a while
       to "warm up" and the bottom most line is invalid due
       to the method of interleaving pixels.

       also, there is a problem when presenting bad line numbers,
       and the problem seems to crash the system.  be sure to range
       check your line numbers.  don't pass an out of range
       line for the image type you are processing.


 *mode or command: 0=clr noninterlaced, 1=clr interlaced (both 24 bit bgr)
      2=b&w noninteraced  3=b&w interlaced (both 8 bit gray)
      4=b&w noninteraced  5=b&w interlaced (both 24 bit bgr)
       80h = reset color processing line statuses.
	     used before any color processing, not needed for
	     black and white.  bufheight must be valid
	     for the next image to be processed when this reset
	     call is made.  no other variables in your
	     structure are used with this command.
       81h = return status of line specified in line number entry
	     only valid during color processing.   only
	     the line number in the structure needs to be valid
	     when using this command.  also, this command has no
	     meaning when used in black and white mode.
	     status is returned in the mode/command byte of
	     the structure:
		 0=line completely unprocessed
		 1=image present, but moire patterns still exist,
		   buffers must not be altered.
		 2=image color processed, moire patterns fixed,
		   final intensity is not complete.  buffers
		   must not be altered.
		 3=final data available with final intensity.
		   buffers (input and output) are available for
		   reuse.
 *bufwidth = buffer (image) width in unprocessed pixels.  the output buffer
	 width should be the same unless stretch is enabled.  this width
	 is necessary because of the variable image size capability of
	 the camera.  for a full image it will be 512.	other possible
	 image sizes are: 384, 256, 170, 128, 102, 72, 64, 56, 46, 40, or
	 any other combination that allows alternating color pixel filters
	 of the required type.	when considering how to call the camera
	 for an image keep in mind that your image must alternate between
	 even and odd color filter pixels from the original 512 image,
	 an image that has more than 1 even or odd filter in a row cannot
	 be processed.	note: stretched images will be 1.25 times the
	 size you specify here.
 *bufheight= buffer (image) height.  in cases of double scan, this is the
	 height of each scan. for instance, on 512x492 double scan this
	 height will be 246, the height of a single scan.  likewise this
	 height will be 246 for a 256x246 image where the vertical rows
	 are not doubled visually.  the 256 mode also has a 256x143
	 size, and in this case the output data is displayed twice
	 vertically in order to acheive the correct aspect ratio.  the
	 condition of the displayed data has no bearing on what is
	 reported here, you report the actual number of scan lines you
	 will receive from the camera, regardless of whether you intend
	 to crop off top or bottom lines or intend to display each row
	 twice.  other possible buffer heights are: 164, 123, 98, 82,
	 70, 61, 54, 42, 37, 27, 22, and 19.  others may be possible,
	 but you must alternate between even and odd numbered rows from
	 the original 246 rows of pixel information.

 *redlev = final red level.   128 = 100%.  0-256. default: 128
	note: red and blue should counter balance each other
	in most cases.	if you subtract 16 from red, add it to blue.
 *greenlev = final green level. 128=100%.  recommended default is 75.  it's
	interesting to note that the ccd is more sensitive to green
	and that's why we have to adjust it down.
	[ i've modified the code to do the 128-->75 conversion.  so now
	the "recommended default is 128, just like for red and blue.  -pgf ]
 *bluelev = final blue level.  128 = 100%.  0-256. default: 128
	note: red and blue should counter balance each other
	in most cases.	if you subtract 16 from red, add it to blue.
 *intensity level for output. 128 gives 100%.  you can use from 0-4096
 *pixlimits are the limits in distance (in pixels) to average colors together
    to make the final color.  the first byte is the low limit
    (the least averaging that will take place) and the second is the
    high limit.  a value of 0 means no averaging.

    a high number greatly reduces moire patterns, but
    creates a magenta smear on the picture.

    values for pixlimits are 0-10.  3 for the low and 8 for the high
    is the recommended value.  to disable any color smoothing
    (as in astronomical applications), set both to 0.

    note: the size of pixlimits is tied to the size of the temporary
    output data buffers you provide in your entry structure. you must
    have at least (pixlimits high)*2 buffers of bufwidth*3 bytes.
    you can have more, but not less. if stretch is active, your buffer
    must be bufwidth*1.25*3 bytes.

    you can reduce the memory required by using a lower
    value of pixlimits if desired, but if memory is not a problem, just
    alot the max of 20 buffers (for pixlimits 10) and use it with any
    lower pixlimits value.
	note: you cannot change pixlimits during processing of
	an image.  it must remain consistent.  also, going above
	10 would not work with the method used here to trick
	the checking of flag bytes in the routine findline2.
 *hazelevel is the value to subtract from every final r, g, & b value.
    8 suggested.
    note: it is suggested that if your software adjust the intensity
    according to the haze.  this is needed so that the picture doesn't
    get darker or lighter with a haze change.  unfortunately,
    to do this you have to make an assumption about the image.
    you have to assume what the average overall value of the
    image is, interms of rgb intensity.  the example below shows
    how to adjust the haze value based on the assumption that the
    final output range is close to 0-128:

    intensity = (requested intensity) * 128/(128-haze)

 *diflimit is an internal calculation value.  when differences between
    adjacent color filter types exceed this value the larger of the
    pixlimit values is used to make a color.  below this value the
    smaller pixlimit value is used.  the value is a 0-128 percent.
    42 suggested.

 *options is a word of flag bits.
    bit 0001 set if input pixel averaging is enabled.  in this
	    mode, 4 adjacent pixels of the same color filter type
	    are averaged before color calculations are made. there
	    is no effect on intensity information.  this
	    should be the default.
    bit 0002 set if color cropping enabled.  in this mode,
	    when an rgb component goes over 255 the entire color
	    intensity is adjusted to make colors stay relatively
	    the same as calculated.  when off, just the offending
	    color is cropped, making the actual final color be in
	    error, but allowing brighter colors.
    bit 0004 set if vignette correction is enabled.  this
	    flattens the brightness response of the image which
	    would normally be brighter in the center by almost
	    2.5 times as opposed to the corners of the screen.
    bit 0008 clear if mathematical calculations should be used
	    to generate the color information.
	    set if the simple subtraction method the ccd
	    was designed for is desired.
    bit 0010 set if output should be mathematically stretched.
	    a 512 wide buffer will stretch to 640.  other
	    sizes will stretch to the integer of input*1.25.
	    this stretch uses information available only
	    internally and will likely be of a better quality
	    than a stretch done by windows or a graphics program.

	    here are stretch sizes for specific input sizes:

		    512 stretches to 640
		    384 stretches to 480
		    306 stretches to 382
		    256 stretches to 320
		    170 stretches to 212
		    128 stretches to 160
		    102 stretches to 127
		    72 stretches to 90
		    64 stretches to 80
		    56 stretches to 70
		    52 stretches to 65
		    46 stretches to 57
		    40 stretches to 50

    bit 0020 set means color tracing is on.  recommended default.
	    in this mode, colors averaging decisions involve
	    tracing out from the current point and stopping
	    at detectable boundries.  otherwise the area specified
	    by pixlimit is used.

    bit 0040 set means edge correction.  in color mode this is
	    primarily right side blue reduction, but the leftmost
	    pixels are also adjusted.  the ccd seems
	    to have a higher blue sensitivity on the sides of
	    the mask.  and the leading pixel seems to be
	    darker in differing amounts for the rgb levels.
	    when this bit is set, these values are adjusted.
	    bit set is the recommended default.

	    in black and white mode, the leftmost pixel is
	    brightened.

    *** clear unused bits in this options word***

 *black levels are the averages of the leading and trailing black levels
    sent by the camera.  the first byte is the average of the leading,
    the second the average of the trailing.
 *input_off is the 32 bit offset of the buffer holding the bufwidth byte data
    for the current. this offset must be suitable for indexing to
    fetch the other lines.  they must be in a position that is relative
    to the central line in the same manner as they were fetched from
    the ccd.  this means that you need a contiguous buffer
    to hold both the lower scan image and the upper scan image.

    in color mode, 3 lines above and 3 below must have valid data at the
    time of the call.  for instance, loading your value into inp, it must
    be valid to address the extreme edge line as [inp-512*3] (if your
    bufwidth is 512), and [inp+512].  this must be true for the entire
    input line width of bufwidth pixels.

    in black and white mode interlaced, only this offset is used,
    other lines are not accessed.

 *temp_off is the offset of temporary buffers to hold intermediate output
    data results.  you must point this to a block of memory that
    is large enough for your maximum supported pixlimits+1 times
    bufwidth*3*2. if you support pixlimits of 10, and a bufwidth of
    512,  this amounts to 11*512*3*2 or 33792 bytes.  stretch doesn't
    affect this buffer size, it's for intermediate (pre-stretch)
    calculations.

    you don't change this offset with each new row, just point to the
    base of this buffer.  example buffer sizes:

	    max pixlimits   memory required
		  1	    6144
		  4	    15360
		  8	    27648
		 10	    33792

	    note: this buffer is not used in black and white mode.
 *result_off is the offset of the bufwidth*3 byte buffer to hold results.
    24 bit bgr or 8 bit b&w. if b&w, output is only bufwidth bytes.
    no indexing is done to fetch lines above or below this buffer,
    the buffer can be an independant buffer in any memory location.
    the only requirement is that it remain valid for pixlimits*2
    scan lines after it has been processed.  it takes this long to
    complete the final information.  the upper and lower images in
    double scan or non-interleaved mode are independant, thus the
    buffers for each must remain for pixlimits*2 scan lines of that
    image half, regareless of the processing position of the other.
    also note that if stretch is active your buffer must be
    bufwidth*1.25*3 bytes in length (or *1 if b&w).

*/
#endif /* COMMENTARY */

/** some defines used by the program to determine the type
 * of color filter used. **/
#define CYG	4
#define CYMG	5
#define YEG	6
#define YEMG	7


/* these 3 defines determine the order of the output color bytes */
#ifdef ORIGINAL_TRANSLATION
#define BLUE	0   /* bgr output */
#define GREEN	1
#define RED	2
#else
#define RED	0   /* rgb output */
#define GREEN	1
#define BLUE	2
#endif


byte lowplimit;
byte highplimit;



int opts;   /** user's options flag word **/

int greenfix;	/** greenlev (green) base 128 percent **/
int redfix; /** red fix from redlev **/
int bluefix;	/** blue fix from bluelev **/

#if NEEDED
int graygreen;
int grayred;
int grayblue;
#endif

long bwidth;	/** buffer width as passed by caller **/
long bheight;	/** and the height **/



byte centralbw; /** black and white for pixel being processed. **/
byte centraldev; /** deviation allowed in b&w from centralbw **/

byte moirefound; /** to flag a pixel as having moire patterns. **/
	 /** (only bit 1 set if moire trouble) **/

#define FLAGMOIRE   0x01
#define UNDERFLO -7

byte *tempoff;	/** for temporary output data pointers **/


long runningred, runninggreen, runningblue; /** for adjustments to color **/

long runningcount; /** count of samples in average for averagearea **/


/** note:  surrounding the valid linestats bytes are values of 02.  these
 * flag values fake out the routines that check if lines are surrounded by
 * valid data.	by marking them done and valid and free of moire patterns,
 * the logic to free up lines has free access at the ends without range
 * checking.  the logic to remove moire also gets fooled, and the actual
 * routine that does it does some range checking before it looks at the
 * lines of information.  **/

byte statsbase[10 + 246 + 10 + 246 + 10];
byte *linestats = &statsbase[10];

/** the linestats values are as follows: 
 *   0 = nothing in the buffer. 
 *   1 = preliminary info, not final moire adjusted. 
 *   2 = final moire adjusted , but not intensity or color balance adjusted. 
 *   3 = completely done data, and it's not surrounded by any lines which 
 *	 will need to examine it.  it's been intensity adjusted according 
 *	 to the user's options. **/
#define LS_NOTHING  0
#define LS_PRELIM   1
#define LS_ALMOST   2
#define LS_DONE     3

/** offset of original data for central line of each color line. **/
byte *input_offs[512];

byte blacks[512];   /** black adjustment for each line **/

byte *out_offs[512]; /** offset of output data for each line **/

byte tracebufs[512*42]; /** trace buffers for color mode. **/

/* prototypes */
void color_interlaced(struct colordata *cdp);
void grey_interlaced(struct colordata *cdp);
byte grey_pix(struct colordata *, int, int, byte *);
void simplergb(void);
int adjustcolor(void);
int findline(int bank, int ls_level);
void moirerow(struct colordata *, int);
void moverow(int);
int grey_vignettesub(int, int, int);
void averagearea(int line, int col);
void vignettesub(int line, int col);
int goodaverage(int, int, int);
int boundryaverage(int, int, int);
void allaverage(int, int, int);
void clearaverage(void);
void next_all_average(int line, int col, int searchlimit);
void continue_all_average(int col, int searchlimit, byte *tr_off, byte *brg_p);
int next_boundry_average(int line, int col, int searchlimit);
int continue_boundry_average(int col, int searchlimit, byte *tr_off, byte *brg_p);
int next_good_average(int line, int col, int searchlimit);
int continue_good_average(int col, int searchlimit, byte *tr_off, byte *brg_p);
void fixintensity(int, int);
int bluesub(int, int, int, int, int);
int redsub(int, int, int, int, int);
int greensub(int, int, int, int, int);
int g_cymg_yeg_yemg(int, int, int, int);
int g_cyg_cymg_yeg(int, int, int, int);
int g_cyg_yeg_yemg(int, int, int, int);
int g_cyg_cymg_yemg(int, int, int, int);
int r_cymg_yeg_yemg(int, int, int, int);
int r_cyg_cymg_yeg(int, int, int, int);
int r_cyg_yeg_yemg(int, int, int, int);
int r_cyg_cymg_yemg(int, int, int, int);
int b_cymg_yeg_yemg(int, int, int, int);
int b_cyg_cymg_yeg(int, int, int, int);
int b_cyg_yeg_yemg(int, int, int, int);
int b_cyg_cymg_yemg(int, int, int, int);

#ifdef	MOIREDBG 
#define MOIREDBGSET(r,b,g)  {
    runningred = r * 255; \
    runningblue = b * 255; \
    runninggreen = g * 255; \
    }
#else
#define MOIREDBGSET(r,b,g)
#endif
/** compare cl and ch and decide if they are too different and thus an
* indication of an edge in the image.  return false if edge suspected. **/
inline int
checkedge(byte ch, byte cl, int dlimit)
{
    return ((cl * dlimit)/128) > abs(cl - ch);
}

/** adjust for blacklevel, but don't underflow */
inline byte 
bfix(byte p, int bl)
{
    int np = p - bl;
    if (np < 0)
	return 0;
    return (byte)np;
}

/** do a safe rounded division */
inline int rdiv(int n, int d)
{
    if (d) /* don't divide by zero */
	return (n + (d/2)) / d;   /* add d/2 to force rounding */
    else
	return n;
}

/** find the absolute value of an integer **/
inline int absolute(int i)
{
    return (i < 0) ? -i : i;
}

/** this routine finds the length of the third side of triangle where the
* angle between the other 2 is 90 degrees.  ax has the length of one and dx
* the other.  the length of the third side is returned in ax.  **/

inline int findlength(int a, int b)
{
#ifdef assembler_findlength
/* this is quite a bit less code than gcc generates -- perhaps
 * i should use inline assembler? */
/* on the other hand, i should really use a lookup table -- i prototyped
 * it, and it takes about 40% less time than the sqrt version below.  but
 * the table is really big -- 32K, even if you only store the upper half
 * of the 256x123 matrix of values. */
    finit 
    mov temp,ax 
    fild    temp 
    fmul    st,st(0) 

    mov temp,dx 
    fild    temp 
    fmul    st,st(0) 

    fadd 
    fsqrt 
    fistp   temp 
    mov ax,temp 
#else
#if slightly_slower
    return (int)hypot((double)a, (double)b);
#else
    return (int)sqrt((double)(a * a + b * b));
#endif
#endif
}

/** this sub will take a row number and return the correct
* offset in tracebufs.	these are used to hold b&w info for color
* tracing.  **/

inline byte *
pointtrace(int line)
{
    int bankbeg = line & 0xff00;
    line &= 0xff;	/** get the bank offset **/
    line %= 21;     /** make mod 21 **/  /* WHY 21?!?!?!  pgf */
    if (bankbeg != 0) /** if upper bank, skip 21 buffers **/
	line += 21;
    return &tracebufs[line * 512];
}


void
reset_linestats(int bh)
{
	/**	clear the line stats, leave full of LS_ALMOST's **/
	memset(statsbase, LS_ALMOST, sizeof(statsbase));

	/**	clear it for the specified height **/
	memset(linestats, LS_NOTHING, bh);

	/** pad rest with a number that allows color processing but
	* doesn't allow line to be transported (line done) **/
	memset(&linestats[bh], LS_DONE, 256-bh);

	/**	clear the line stats high image **/
	memset(&linestats[256], LS_NOTHING, bh);

	/**	now pad the rest with 3's (line done) **/
	memset(&linestats[256+bh], LS_DONE, 256-bh);
}

void
sharpcolor( struct colordata *cdp )   /* the user's parameter structure */
{

    if (cdp->mode == RESET_LINESTATS) {
	/** reset color processing line status bytes. **/
	reset_linestats(cdp->bufheight);
	return;
    }

    if (cdp->mode == GET_LINESTAT_FOR_LINE) {
	/** return status of color processed lines. **/
	cdp->mode = linestats[cdp->linenum];
	return;
    }


    TRACE("k", __FILE__ ": sharpcolor: imageline %d, inp %p, outp %p\n",
	cdp->linenum, cdp->input_off, cdp->result_off);

    /** get common stuff **/
    opts = cdp->options;
#ifdef SHOWBAD
    opts &= ~(OPT_EDGE_CORRECTION |
	  OPT_COLOR_TRACING |
	  OPT_VIGNETTE_CORRECTION |
	  OPT_COLOR_AVG);
#endif

#ifdef SHOWBAD
    lowplimit = highplimit = 0;
#else
    lowplimit = cdp->pixlimits[0];
    highplimit = cdp->pixlimits[1];
#endif

    /** get the buffer width **/
    bwidth = cdp->bufwidth;
    if (bwidth == 0 || bwidth > 512)
	bwidth = 512;

    bheight = cdp->bufheight;
    if (bheight == 0 || bheight > 246)
	bheight = 246;

    /** go to the appropriate mode for this data or execute the
    * desired command.	**/
    switch(cdp->mode) {
    case GREY_INTERLACED_24:
    case GREY_INTERLACED_8:
	grey_interlaced(cdp);
	return;

    /** color interlaced. **/
    case COLOR_INTERLACED_24:
	/* the rest of the routine takes care of this case */
	color_interlaced(cdp);
	break;

    case GREY_NONINTERLACED_24:
    case GREY_NONINTERLACED_8:
    case COLOR_NONINTERLACED_24:
    default:
	/* not implemented */
	return;

    }
}

/** remove moire pattern possibilities for any lines which
* have enough surrounding ones to be processed.  these go into
* the temporary buffers because we can't change the ones 
* still needed to remove moire patterns from other lines.	**/
void
do_other_lines(struct colordata *cdp, int line)
{
    int foundone;
    int l;

    do {
	/** set flag meaning didn't find one **/
	foundone = 0;

	/** see if any need processing **/
	l = findline(line & 0xff00, LS_PRELIM);
	if (l >= 0) {
	    line = l;
	    moirerow(cdp, line);
	    foundone = 1;
	}
	/** and look for any fully done rows which are no longer
	* needed and can have their final data moved to the output
	* location.  we have to interleave this function with
	* moverow so that we don't overwrite data at the very end
	* of processing.  we have to free up buffers as fast as we
	* fill them.  **/

	l = findline(line & 0xff00, LS_ALMOST);
	if (l >= 0) {
	    line = l;
	    moverow(line);
	    foundone = 1;
	}

    } while (foundone != 0); /** see if any were found in last pass **/
}

void
color_interlaced(struct colordata *cdp)
{
    long neg1, neg2, neg3, pos1, pos2;
    byte *traceoff;
    byte *outptr;   /** where to store output of color **/
    byte *startout; /** copy for final adjustments **/
    byte *startin;  /** copy of start pointer for final adjustments **/
    int col;	    /** index of current pixel **/
    byte *inp;	    /** pointer to current pixel **/
    int centraltype; /** type of pixel we are on **/
    int imageline = cdp->linenum;
    int line_in_bank = imageline & 0xff;
    int bl; /** black level we are subtracting  **/

    /** when we fetch pixels, we put them into the variables that are laid
     ** out like this: **/
#if ORIG
    int
    l2top2pix,	  l1top2pix,	top2pix,    r1top2pix,	  r2top2pix,	r3top2pix,
    l2top1pix,	  l1top1pix,	top1pix,    r1top1pix,	  r2top1pix,	r3top1pix,
    l2top0pix,	  l1top0pix,	top0pix,    r1top0pix,	  r2top0pix,	r3top0pix,
    l2centpix,	  l1centpix,	centpix,    r1centpix,	  r2centpix,	r3centpix,
    l2bot0pix,	  l1bot0pix,	bot0pix,    r1bot0pix,	  r2bot0pix,	r3bot0pix,
    l2bot1pix,	  l1bot1pix,	bot1pix,    r1bot1pix,	  r2bot1pix,	r3bot1pix;
#else
#if LATER_MAYBE
#define L1TOP1PIX   0
#define TOP1PIX     1
#define R1TOP1PIX   2
#define R2TOP1PIX   3
#define L1TOP0PIX   4
#define TOP0PIX     5
#define R1TOP0PIX   6
#define R2TOP0PIX   7
#define L1CENTPIX   8
#define CENTPIX     9
#define R1CENTPIX   10
#define R2CENTPIX   11
#define L1BOT0PIX   12
#define BOT0PIX     13
#define R1BOT0PIX   14
#define R2BOT0PIX   15
    int r[16];
    
#define l1top1pix r[L1TOP1PIX]
#define top1pix   r[TOP1PIX  ]
#define r1top1pix r[R1TOP1PIX]
#define r2top1pix r[R2TOP1PIX]
#define l1top0pix r[L1TOP0PIX]
#define top0pix   r[TOP0PIX  ]
#define r1top0pix r[R1TOP0PIX]
#define r2top0pix r[R2TOP0PIX]
#define l1centpix r[L1CENTPIX]
#define centpix   r[CENTPIX  ]
#define r1centpix r[R1CENTPIX]
#define r2centpix r[R2CENTPIX]
#define l1bot0pix r[L1BOT0PIX]
#define bot0pix   r[BOT0PIX  ]
#define r1bot0pix r[R1BOT0PIX]
#define r2bot0pix r[R2BOT0PIX]

#else
    int
    l1top1pix,	  top1pix,    r1top1pix,    r2top1pix,
    l1top0pix,	  top0pix,    r1top0pix,    r2top0pix,
    l1centpix,	  centpix,    r1centpix,    r2centpix,
    l1bot0pix,	  bot0pix,    r1bot0pix,    r2bot0pix;
#endif

#endif

    int centpix2; /** this is a copy of central pix for color avrg. **/
    int entrycymg, entryyeg, entryyemg, entrycyg;

    /** get passed in black levels, average 'em **/
    bl = (cdp->blacklevs[0] + cdp->blacklevs[1]) / 2;


    /**     get green level **/
    /**     adjust this way, but process for 128 **/

    /*
     * greenfix needs to be 59% of the base values for red and blue.  this
     * adjustment used to be left to the user, but i've moved it in here
     * so the user can use 128 as the base (100%) value for all three
     * colors.
     */
    greenfix = (75 * cdp->greenlev) / 128;
#if NEEDED
    /** while setting the adjustment percents **/
    /** we need to make something that will **/
    /** balance out to gray after these are **/
    /** used.  we fill in bad pixels with it. **/
    graygreen = (3*128) / ((greenfix) ? greenfix : 1);
#endif


    /** 384 = 3*128.  we want the final **/
    /** gray to come out to intensity 3. **/
    /** intensity 1 would be better; bit **/
    /** 01 is set in the blue to mark bad. **/
    redfix = cdp->redlev;
#if NEEDED
    grayred = (3*128) / ((redfix) ? redfix : 1);
#endif


    /** but we can't use 01 because a slight **/
    /** rounding error might make some = 0. **/
    /** so 3 is next odd number up. **/
    bluefix = cdp->bluelev;
#if NEEDED
    grayblue = (3*128) / ((bluefix) ? bluefix : 1);
#endif

    /** save output pointer **/
    outptr = cdp->result_off;
    /** and save a copy **/
    startout = outptr;

    /** get offset of temporary buffers **/
    tempoff = cdp->temp_off;

    /** get center line offset **/
    /** save offset of starting data **/
    startin = cdp->input_off;


    /** set pixel being done. **/
    col = 0;

    /** make indexes to fetch lines above **/
    neg1 = -bwidth;
    neg2 = -(2 * bwidth);
    neg3 = -(3 * bwidth);

    /** if closer than 3, just go 1 for 3 **/
    if (line_in_bank < 3)
	neg3 = neg1;

    /** if closer than 2, use 0 for 2 back **/
    if (line_in_bank < 2)
	neg2 = 0;

    /** in this case, we have to go down, not up. **/
    if (line_in_bank < 1)
	neg1 = neg3 = bwidth;

    /** make index to go 1 line below **/
    pos1 = bwidth;
    pos2 = bwidth * 2;

    /** see if none below **/
    if (line_in_bank == 245) {
	pos1 = -pos1; /** if can't go 1 below, go 1 above. **/
	pos2 = 0; /** and point 2 below to 0 **/
    }

    /** see if only 1 below **/
    if (line_in_bank == 244)
	pos2 = 0; /** if so, 2 below becomes zero. **/

    /** set up the b&w trace buffer **/
    traceoff = pointtrace(imageline);

    /** main pixel loop **/
    /** for starters, we'll need specific pixels no matter which color
    * cell we are on.  also, they have to be gotten differently
    * depending on where we are in the line (start, middle, end).  we
    * put these into variables.  we have to watch out for the ends of
    * the line where we can't get the pixels we would like to have
    * relative to the center pixel **/

    /** inp has the line we are on. outptr has the output.  **/
    inp = startin;
    do {
	/** get central pixel **/
	centpix = bfix(inp[0], bl);

	/** get pixel 2 above one we are on **/
	top1pix = bfix(inp[neg2], bl);

	/** get pixel 1 above one we are on **/
	top0pix = bfix(inp[neg1], bl);

	/** get pixel 1 below one we are on **/
	bot0pix = bfix(inp[pos1], bl);

	/** if we are at the end of the line and have to be careful
	* how we fetch pixels to the right of the central one. 
	* **/

	if (col >= bwidth - 3) {
	    if (col == bwidth - 3) {
		r1centpix = bfix(inp[1], bl);
		r2centpix = bfix(inp[2], bl);
		l1centpix = bfix(inp[-1], bl);

		r1top1pix = bfix(inp[neg2+1], bl);
		r2top1pix = bfix(inp[neg2+2], bl);
		l1top1pix = bfix(inp[neg2-1], bl);

		r1top0pix = bfix(inp[neg1+1], bl);
		r2top0pix = bfix(inp[neg1+2], bl);
		l1top0pix = bfix(inp[neg1-1], bl);

		r1bot0pix = bfix(inp[pos1+1], bl);
		r2bot0pix = bfix(inp[pos1+2], bl);
		l1bot0pix = bfix(inp[pos1-1], bl);

	    } else if (col == bwidth - 2) {

		r1centpix = bfix(inp[1	], bl);
		r2centpix = centpix;
		l1centpix = bfix(inp[ -1 ], bl);

		r1top1pix = bfix(inp[neg2+1  ], bl);
		r2top1pix = top1pix;
		l1top1pix = bfix(inp[neg2-1  ], bl);

		r1top0pix = bfix(inp[neg1+1  ], bl);
		r2top0pix = top0pix;
		l1top0pix = bfix(inp[neg1-1  ], bl);

		r1bot0pix = bfix(inp[pos1+1  ], bl);
		r2bot0pix = bot0pix;
		l1bot0pix = bfix(inp[pos1-1  ], bl);

	    } else /* if (col == bwidth - 1) */ {
		r1centpix = bfix(inp[-1  ], bl);
		l1centpix = bfix(inp[-1  ], bl);
		r2centpix = centpix;

		r1top1pix = bfix(inp[neg2-1  ], bl);
		l1top1pix = bfix(inp[neg2-1  ], bl);
		r2top1pix = top1pix;

		r1top0pix = bfix(inp[ neg1-1 ], bl);
		l1top0pix = bfix(inp[ neg1-1 ], bl);
		r2top0pix = top0pix;

		l1bot0pix = bfix(inp[pos1-1  ], bl);
		r1bot0pix = bfix(inp[pos1-1  ], bl);
		r2bot0pix = bot0pix;

	    }
	} else	if (col <= 1) {
	    if (col == 0) {
		r1centpix = l1centpix = bfix(inp[1 ], bl);
		r2centpix = bfix(inp[2 ], bl);

		r1top1pix = l1top1pix = bfix(inp[neg2+1  ], bl);
		r2top1pix = bfix(inp[neg2+2  ], bl);

		r1top0pix = l1top0pix = bfix(inp[neg1+1  ], bl);
		r2top0pix = bfix(inp[neg1+2  ], bl);

		r1bot0pix = l1bot0pix = bfix(inp[pos1+1  ], bl);
		r2bot0pix = bfix(inp[pos1+2  ], bl);

	    } else { /* must be 1 */
		r1centpix = bfix(inp[1	], bl);
		l1centpix = bfix(inp[-1  ], bl);
		r2centpix = bfix(inp[2	], bl);

		r1top1pix = bfix(inp[neg2+1  ], bl);
		l1top1pix = bfix(inp[neg2-1], bl);
		r2top1pix = bfix(inp[neg2+2  ], bl);

		r1top0pix = bfix(inp[neg1+1  ], bl);
		l1top0pix = bfix(inp[neg1-1  ], bl);
		r2top0pix = bfix(inp[neg1+2  ], bl);

		r1bot0pix = bfix(inp[pos1+1  ], bl);
		l1bot0pix = bfix(inp[pos1-1  ], bl);
		r2bot0pix = bfix(inp[pos1+2  ], bl);

	    }
	} else {

		/** in the middle of the line we don't have
		* to worry about were to index.  **/

		r1centpix = bfix(inp[1	], bl);
		r2centpix = bfix(inp[2	], bl);
		l1centpix = bfix(inp[-1  ], bl);

		r1top1pix = bfix(inp[neg2+1  ], bl);
		r2top1pix = bfix(inp[neg2+2  ], bl);
		l1top1pix = bfix(inp[neg2-1  ], bl);

		r1top0pix = bfix(inp[neg1+1  ], bl);
		r2top0pix = bfix(inp[neg1+2  ], bl);
		l1top0pix = bfix(inp[neg1-1  ], bl);

		r1bot0pix = bfix(inp[pos1+1  ], bl);
		r2bot0pix = bfix(inp[pos1+2  ], bl);
		l1bot0pix = bfix(inp[pos1-1  ], bl);
	}

	/** we have the pixels.  analyze to see which are probably
	* on an edge and need more moire correction.  **/

    /** l2top2pix   l1top2pix  top2pix	r1top2pix   r2top2pix  r3top2pix  **/
    /** l2top1pix   l1top1pix  top1pix	r1top1pix   r2top1pix  r3top1pix  **/
    /** l2top0pix   l1top0pix  top0pix	r1top0pix   r2top0pix  r3top0pix  **/
    /** l2centpix   l1centpix  centpix	r1centpix   r2centpix  r3centpix  **/
    /** l2bot0pix   l1bot0pix  bot0pix	r1bot0pix   r2bot0pix  r3bot0pix  **/
    /** l2bot1pix   l1bot1pix  bot1pix	r1bot1pix   r2bot1pix  r3bot1pix  **/

	/** assume moire will be found. **/
	moirefound  = 1;

	/** here's a simple black and white check for boundries. 
	* since any 2 adjacent cells make b&w info, this should
	* catch most vertical edges.  **/

	/** X 0 0 X **/
	/** X 0 0 X **/
	/** X X X X **/
	if (checkedge((top0pix + r1top0pix)/2,
		(centpix + r1centpix)/2, cdp->diflimit)) {

	    /** we passed vertically as far as intensity goes,
	    * but we need some horizontal checks.  **/

	    /**   X x x X **/
	    /**   0 x 0 X **/
	    /**   X X X X **/
	    if (checkedge(l1centpix, r1centpix, cdp->diflimit)) {

		/**   X x x X **/
		/**   X 0 x 0 **/
		/**   X X X X **/
		if (checkedge(centpix, r2centpix, cdp->diflimit)) {

		    moirefound = 0;
		}
	    }
	}
	/** done fetching and checking the color filters we will use to
	* make the color info.	but before we make color, we create b&w
	* info for this pixel.	it's used for color tracing.  **/

	centralbw = (centpix + r1centpix)/2;
	traceoff[col] = centralbw;

	/** now apply pixel color averaging, if the user has selected it. 
	* since we've detected bad moire areas, we'll only do the pixel
	* color averaging on these areas.  otherwise the good pixels will
	* get averaged into potential bad ones.  **/

    /** l2top2pix   l1top2pix  top2pix	r1top2pix   r2top2pix  r3top2pix  **/
    /** l2top1pix   l1top1pix  top1pix	r1top1pix   r2top1pix  r3top1pix  **/
    /** l2top0pix   l1top0pix *top0pix *r1top0pix   r2top0pix  r3top0pix  **/
    /** l2centpix   l1centpix *centpix *r1centpix   r2centpix  r3centpix  **/
    /** l2bot0pix   l1bot0pix  bot0pix	r1bot0pix   r2bot0pix  r3bot0pix  **/
    /** l2bot1pix   l1bot1pix  bot1pix	r1bot1pix   r2bot1pix  r3bot1pix  **/

	/** make a copy in case color averaging off **/
	centpix2 = centpix;

	if (moirefound && (opts & OPT_COLOR_AVG)) {

	    centpix2 = (centpix + r2centpix + top1pix + r2top1pix) / 4;

	    r1centpix = (r1centpix + l1centpix + r1top1pix + l1top1pix) / 4;

	    top0pix = (top0pix + r2top0pix + bot0pix + r2bot0pix) / 4;

	    r1top0pix = (r1top0pix + l1top0pix + r1bot0pix + l1bot0pix) / 4;
	}
	/** now we fetch the info according to the type of color
	* filter the central pixel is under.  **/

	/** cyg  yemg cyg  yemg cyg  yemg  (yemg)-alpha(cyg)=red **/
	/** cymg yeg  cymg yeg	cymg yeg   (cymg)-beta(yeg)=blue **/
	if ((imageline & 1) == 0) {
	    if ((col & 1) == 0) { /** cyg central pixel **/
		centraltype = CYG;
		entryyemg = r1centpix;
		entrycyg = centpix2;
		entrycymg = top0pix;
		entryyeg = r1top0pix;

	    } else { /** yemg central pixel **/
		centraltype = YEMG;
		entryyemg = centpix2;
		entrycyg = r1centpix;
		entrycymg = r1top0pix;
		entryyeg = top0pix;
	    }
	} else {
	    if ((col & 1) == 0) { /** cymg central pixel **/
		centraltype = CYMG;
		entryyemg = r1top0pix;
		entrycyg = top0pix;
		entrycymg = centpix2;
		entryyeg = r1centpix;
	    } else { /** yeg central pixel **/
		centraltype = YEG;
		entryyemg = top0pix;
		entrycyg = r1top0pix;
		entrycymg = r1centpix;
		entryyeg = centpix2;
	    }
	}

	/** now we make colors using the surrounding pixels we have
	* set up.  **/

	/** at this point, we make rgb values using the following
	* in the 3-equation calculations:
	*   entrycymg, entryyeg  (blue values) 
	*   entryyemg, entrycyg  (red values) 
	*   "centpix" = pixel we are on, 
	*   "centraltype" = cymg, yeg, yemg, or cyg.
	**/
	runningblue = bluesub(entrycyg, entrycymg,
			    entryyeg, entryyemg, centraltype);
	runningred = redsub(entrycyg, entrycymg,
			    entryyeg, entryyemg, centraltype);
	runninggreen = greensub(entrycyg, entrycymg,
			    entryyeg, entryyemg, centraltype);

	/** now runningred=red*2, runningblue=blue*2, runninggreen=green*2 
	**/

	if (opts & OPT_EDGE_CORRECTION) {
	    int lowfixup[5] = { 100, 105, 105, 110, 110 };
	    int highfixup[5] = { 100, 100, 85, 75, 70};
	    if (col <= 4) {
		runningblue = (runningblue * lowfixup[col])/128;
	    } else if (col >= 507) {
		runningblue = (runningblue * highfixup[col-507])/128;
	    }
	    
	}

	/** we scale colors down below the 1 byte range rather than
	* chop them at this point.  the final results are a user
	* selectable option, but during the intermediate state we
	* must scale them down to preserve the accurate intensity
	* information for moire removal.  for instance, if we
	* chopped off the red, then when that color was applied to
	* another cell during moire removal it's final intensity
	* would be way off because chopping off the red created a
	* phony hue which cannot be correctly recalculated for
	* intensity against the color filter reading.  **/

	adjustcolor();

	/** all done making the initial color.	now store it.  but
	* we also have to create a flag to signal if the color was
	* on an edge and in need of some moire processing.  the eye
	* is less sensitive to blue, so we use the low bit of blue
	* to signal this.  **/


	/** store blue **/
#ifdef SHOWBAD
	if (moirefound) {
	    runninggreen = runningblue = runningred = 0;
	    outptr[BLUE] = 0;
	} else {
	    outptr[BLUE] = (runningblue & 0xfe) | moirefound;
	}
#else
	outptr[BLUE] = (runningblue & 0xfe) | moirefound;
#endif

	/** store green **/
	outptr[GREEN] = runninggreen;

	/** store red **/
	outptr[RED] = runningred;

	outptr += 3;

	/** move along source **/
	inp++;
    } while (++col < bwidth);

    /** we've done the initial processing for this line.  save the
    * pointers to this line and flag it's status.  **/

    linestats[imageline] = LS_PRELIM;
    blacks[imageline] = bl;
    input_offs[imageline] = startin;
    out_offs[imageline] = startout;

    /** now process any lines that are or have become ready **/
    do_other_lines(cdp, imageline);

}

/** black and white interlaced.  double or single doesn't matter,
* the caller must put together the correct rows. 
*    
* for b&w processing we just add 2 adjacent pixels.  this gives
* virtually the same result for even or odd scan lines.  the only
* difference is 12/142 in the blue level.  we need to adjust the
* blue just a little to make it perfect.  to do that, we use the
* scan odd scan line from which it's possible to calculate the blue
* level.  we then take 4% of that blue level and subtract it from
* our value to make this line match the duller one. **/
void
grey_interlaced(struct colordata *cdp)
{
    byte *outp;
    byte *inp;
    int b2, b1;
    int col;
    int imageline = cdp->linenum;

    /** point to output **/
    outp = cdp->result_off;
    /** point to central line **/
    inp = cdp->input_off;

    /**     set counter for pixel being done. **/
    col = 0;

    /** add 2 adjacent pixels.	there's a 3.9% blue variance
     * on every other scan line but we don't do anything about
     * that for now.  */
    do {
	b1 = grey_pix(cdp, imageline, col, inp);

	/** b1 has a value to store, but if we are
	* stretching we have to do the stretch at each
	* multiple of 4.  the ratio is 5/4.  **/

	/** see if even multiple of 4, and see if stretch **/
	if ((opts & OPT_HORIZ_STRETCH) &&
		((col % 4) == 0)) {

	    /** but we don't for the very first **/
	    if (col == 0) {

		/** it's the first and stretch is
		* on.  we can't average to the last
		* one, so we average to the next
		* one.	but we have to make it now. 
		* **/

		/** store it **/
		*outp++ = b1;


		if (cdp->mode == GREY_INTERLACED_24) {
		    /** if so, that 5th needs
		    * 24 bits **/
		    *outp++ = b1;
		    *outp++ = b1;
		}

		inp++;
		/** make the next **/
		b1 = grey_pix(cdp, imageline, col, inp);

		/** get last, average with new **/
		b1 += *(outp-1);
		b1 /= 2;

		/** trick code by backing up **/
		inp--;

	    } else {

		/** not the first.  we can average
		* to the previous sample to
		* accomplish stretch.  */

		/**	get last, average with new **/
		b2 = *(outp-1);
		b2 += b1;
		b2 /= 2;

		/** add a 5th for every 4. **/
		*outp++ = b2;

		if (cdp->mode == GREY_INTERLACED_24) {
		/** if so, that 5th needs 24 bits **/
		    *outp++ = b2;
		    *outp++ = b2;
		}
	    }
	}

	/** we duped if necessary, now b1 has a new sample.  **/

	/** store result, move output pointer **/
	*outp++ = b1;

	/** if so, that 5th needs 24 bits **/
	if (cdp->mode == GREY_INTERLACED_24) {
	    *outp++ = b1;
	    *outp++ = b1;
	}

	/** move along source **/
	inp++;

	/** go till 1 before last.  we can't add 2 at end **/
    } while (++col < bwidth-1);

    /** so we just dup the end one.  **/
    *outp++ = b1;
    if (cdp->mode == GREY_INTERLACED_24) {
	*outp++ = b1;
	*outp++ = b1;
    }

}

/** this sub makes a black and white pixel from [inp].	it does
* intensity and vignetting too.  it averages 2 adjacent pixels. 
* the value is returned in b1.	**/

byte
grey_pix(struct colordata *cdp, int imageline, int col, byte *inp)
{
    int b1, b2;
    int bl = blacks[imageline];


    /** get bytes we're examining an do black level adjustment **/
    b1 = bfix(inp[0], bl);
    b2 = bfix(inp[1], bl);

    /** add the two to make 2 double ones **/
    b1 += b2;

    /** multiply by the base 128 percentage **/
    /** but double it to match the b&w **/
    /** processing method. **/
    b1 *= cdp->intensity / 8;
    b1 /= (128/2);

    /** subtract haze level **/
    b1 -= cdp->hazelevel;
    if (b1 < 0)
	b1 = 0;

    /** do vignette processing if enabled **/
    if (opts & OPT_VIGNETTE_CORRECTION)
	b1 = grey_vignettesub(imageline, col, b1);

    /** is edge color correction on? **/
    if (opts & OPT_EDGE_CORRECTION) {
	/** take 190/128 of left pixel **/
	if (col == 0)
	    b1 = (b1 * 190) / 128;
    }
    /** we have a value in ax, see if it overflowed into a word. **/
    if (b1 > 255)
	b1 = 255;

    return (byte)b1;
}



/* the user gave us 2*highplimit+1 extra rows in which to stroe
 * intermediate results.  pick one of them.
 */
byte *
pointtemp(int line)
{
    int row;
    int i;
    row = line & 0xff;
    i = row % (highplimit + 1);
    if (line >= 256)
	i += highplimit + 1;

    return &tempoff[i * 3 * bwidth];
}


/** this sub makes sure that runningred, runningblue and runninggreen are
* not above 0ffh.  if any of them is, the largest is scaled down to 0ffh
* and the rest are scaled accordingly.	**/
int
adjustcolor(void)
{
    int maxrunning;
    int factor;

    maxrunning = runningred;
    if (maxrunning < runningblue)
	maxrunning = runningblue;
    if (maxrunning < runninggreen)
	maxrunning = runninggreen;
    if (maxrunning < 256)
	return 0;

    /* figure out a safe ratio */
    factor = rdiv(255*128, maxrunning);

    runningred *= factor;
    runningred /= 128;
    if (runningred > 255) runningred = 255;

    runningblue *= factor;
    runningblue /= 128;
    if (runningblue > 255) runningblue = 255;

    runninggreen *= factor;
    runninggreen /= 128;
    if (runninggreen > 255) runninggreen = 255;

    return 1;
}



/** this sub looks for any rows that can be a) finished as far as moire
* pattern processing goes, or b) completely finished.  returns zero if
* none.  return -1 or the row number. **/

int
findline(int bank, int ls_level)
{
    int cr;
    int line = bank;	/* first line of current bank */
    int end = line + bheight;	/* last line of current bank */
    while (line < end) {
	/** check up and down for specified distance.  plimits
	* lines above and below must have valid data and the
	* central one must have 01 as it's flag (unfinished).  it's
	* ok to check above or below the buffer by up to 10 because
	* we've put extra bytes there to allow it.  **/

	if (linestats[line] == ls_level) {
	    /** we can index negative because of the statsbase[10]
	    * buffer below the start of linestats */
	    for (cr = -highplimit; cr <= highplimit; cr++) {
		if (linestats[line + cr] < ls_level) {
		    break;
		}
	    }
	    if (cr > highplimit) { /* none were < ls_level */
		return line;
	    }
	}
	line++;
    }
    return -1;
}




/** this routine moves line from it's temporary buffer to it's output
* buffer and flags it as done in linestats.  it's stretched at this time. 
* **/

void
moverow(int line)
{
    byte *from, *to;
    int count;
    int i;

    linestats[line] = LS_DONE;

    to = out_offs[line];
    from = pointtemp(line);

    TRACE("k", __FILE__ ": moving line %d from %p to %p \n",
	    line, from, to);

    /** see if the data is stretched or unstretched. **/

    if ((opts & OPT_HORIZ_STRETCH) == 0) {
	/** unstretched data. **/
	memcpy(to, from, 3 * bwidth);
	return;
    }

    /** stretched data.  the output size will be 1.25 times the input
    * size, partial pixels are not created.  for instance, 102
    * calculates as follows:  102 x 1.25 = 127.5 you get 127 pixels,
    * not 128.	**/
    /** get width in 3 color cells **/
    count = bwidth;

    i = 0;

    do {
	/** move 1 rgb value **/
	*to++ = *from++;
	*to++ = *from++;
	*to++ = *from++;

	/** see if it's the 4th pixel.	we stretch there **/
	/** if not, just loop **/
	if (i == 3) {
	    /** but if it is, we have to have some data to **/
	    /** the right of it. **/
	    if (count != 1) {
		/** average the colors of this pixel from
		* those around it **/
		*to++ = (from[0] + from[-3])/2;
		*to++ = (from[1] + from[-2])/2;
		*to++ = (from[2] + from[-1])/2;

	    } else {

		/** if we need to stretch but there's **/
		/** nothing to right, just dup it. **/
		*to++ = from[-3];
		*to++ = from[-2];
		*to++ = from[-1];
	    }
	}

	i = (i + 1) & 3; /** make counter for next pixel **/

    } while (--count > 0);
}

/** this sub will remove moire patterns for the dram row you specify in
* line and make final color adjustments.  you must verify that it's
* surrounding pixlimit rows are available before calling and that it is in
* need of final processing (linestat=1).  when the line is on the screen
* border, this routine uses whichever rows can be used (at the top it goes
* down, at the bottom it goes up). 

* the resulting data goes into the temporary buffers, not into the final
* output buffer.  this prevents probogation of the color averaging.  **/

void
moirerow(struct colordata *cdp, int line)
{

    byte *to, *from;
    byte *traceoff;
    int col;
    int centpix;
    int centtype;
    int bl;

    /** point to the temporary output buffer into which to put the
    * corrected data.  we can't move it to the real output buffer yet
    * or else it will cause color smearing when the averaging moves
    * down the screen.	**/

    to = pointtemp(line);   /** find the temporary buffer **/

    /** get input data offset **/
    from = input_offs[line];

    TRACE("k", __FILE__ ": moirerow line %d from %p to %p\n", line,
	    from, to);

    /** set image as moire fixed, but not final intensity adjusted.  **/
    linestats[line] = LS_ALMOST;

    /** get dark level of this line **/
    bl = blacks[line];

    /** clear loop counter **/
    col = 0;

    /** we'll average the pixels around this one together to smooth
    * out the apperance and minimize the effect of moire patterns.  **/

    /** first step is to get particulars of the original in order to
    * adjust the intensity when done.  we also get the b&w info of this
    * pixel as stored in tracebuf.  we use that to find boundries as we
    * average.	**/

    do {
	/** get pixel we are trying to fix, adjust for black level **/
	centpix = bfix(*from, bl);

	if ((line & 1) == 0) { /** see if on a cyg yemg line **/
	    if ((col & 1) == 0) {
		centtype = CYG; /** its cyg **/
	    } else {
		centtype = YEMG; /** its yemg **/
	    }
	} else { /** nope. it's a cymg yeg line **/
	    if ((col & 1) == 0) {
		centtype = CYMG; /** its cymg **/
	    } else {
		centtype = YEG; /** its yeg **/
	    }
	} 

	/** now get the original b&w info.	 **/
	traceoff = pointtrace(line); /** point to the trace buffer **/

	/** set central pixel b&w info **/
	centralbw = traceoff[col];

	/** create the amount this pixel can deviate **/
	/** make percent*128 [ we use half as much]  **/
	/** set for checkedge **/
	centraldev = (centralbw * cdp->diflimit)/128/2;
	if (centraldev > 255) centraldev = 255;

	/** we have the particulars so we can set an intensity
	* when done and we have b&w info for boundry checking.	now
	* we have to average neighboring pixels to make a color. 
	* **/

	/** use an averaging technique **/
	averagearea(line, col);

	/** color to use is in runningred, green, blue.  match it
	* to the intensity of the cell we are on so that the black
	* and white info is perfect.  **/

#ifndef MOIREDBG	/** for visual display of moire logic **/

	fixintensity(centpix, centtype); /** adjust the intensity to the cell type **/

	/** if edge color correction is on, we have to do the left
	* most pixel which is too dark and has to be fixed after
	* calling fixintensity.  **/

	if (col == 0 && (opts & OPT_EDGE_CORRECTION)) {

	    runningblue *= 190;
	    runningblue /= 128;

	    runninggreen *= 190;
	    runninggreen /= 128;

	    runningred *= 190;
	    runningred /= 128;
	}

#endif /** MOIREDBG **/

	/** make the final intensity adjustment.  we do that
	* before color balance so that we get finer resolution on
	* the color balance.  */

	/** multiply by the base 128 percentage intensity **/
	runningblue *= cdp->intensity;
	runningblue /= 128;

	/** multiply by the base 128 percentage intensity **/
	runninggreen *= cdp->intensity;
	runninggreen /= 128;

	/** multiply by the base 128 percentage intensity **/
	runningred *= cdp->intensity;
	runningred /= 128;

	/** if vignette correction is on we need to do that now. **/
	if (opts & OPT_VIGNETTE_CORRECTION) {
	    vignettesub(line, col);
	}

	/** now do the color balance adjustments and the haze level **/


#ifndef MOIREDBG

	/** adjust red level for white balance **/
	runningred = ((runningred * redfix) / 128) - cdp->hazelevel;
	if (runningred < 0) runningred = 0;

	/** give specified adjustment to green **/
	runninggreen = ((runninggreen * greenfix) / 128) - cdp->hazelevel;
	if (runninggreen < 0) runninggreen = 0;

	/** adjust blue level for white balance **/
	runningblue = ((runningblue * bluefix) / 128) - cdp->hazelevel;
	if (runningblue < 0) runningblue = 0;

#endif /* MOIREDBG */

	/** see what type of scaling down is enabled, cropping to
	* fit, or simply chopping it off.  **/

	/** see if color cropping is on **/
	if (opts & OPT_COLOR_CROP) {
	    adjustcolor(); /** if on, scale them down **/
	}

	/** set adjusted blue **/
	to[BLUE] = (runningblue <= 255) ? runningblue : 255;

	/** set adjusted green **/
	to[GREEN] = (runninggreen <= 255) ? runninggreen : 255;

	/** set adjusted red **/
	to[RED] = (runningred <= 255) ? runningred : 255;

	to += 3;
	from++;
	col ++;

    } while (col < bwidth);

    return;
}

/** this routine does the vignette adjustment.	the intensity of the lens
* falls off as we get further from the center.	assuming that it falls as
* the square of the distance, the measurements taken indicated a 50%
* falloff from center to right side.  since a 50% falloff is acheived by
* dividing by 2, the square root of that is 1.41.  but the falloff at the
* center is arbitrarily assigned to be zero since we desire to brighten the
* sides, not lower the brightness of the center.  thus the distance from
* center to right side, a distance of 256 pixels horizontally, is .41
* units.  we based our initial calculations on that assumption.  after that
* we twiked the numbers to balance a white piece of paper best. 
*
* to find the distance from the center we take the square root of the sum
* of the squares of the horizontal and vertical distances.  we divide by
* that square.	**/

/** on input: line has the row number, col has the pixel number, 
* and the data is in runningblue, runninggreen, and runningred. **/

/* on return: runningred, etc are changed. **/


#define HORZPITCH 624	    /** horz pitch is what you would expect
		* to be called vertical pitch.	the
		* ccd is laid on it's side image wise.	**/

#define VERTPITCH (HORZPITCH*75)/96   /** the ratio is 7.5uM to 9.6uM **/

void
vignettesub(int line, int col)
{

    int x, y, l, factor;

    /** get vertical size (horizontal to us). **/
    /** make center point of image. **/
    /** (remember!  ccd is laid on side, so vertical **/
    /** distance is really horizontal distance!) **/
    /** make absolute value of it. **/
    x = absolute((bwidth/2) - col);

    /** now adjust for image size **/
    /** full image is 512, we we make ratio **/
    x *= bwidth;
    x /= 512;

    /** correct for pitch **/
    /** divide by 128 to get result **/
    /** but another 4 to keep numbers as /4 **/
    x *= (VERTPITCH * 128)/HORZPITCH;
    x /= (128*4);

    /** make horizontal center point (vertical to us) **/
    /** remember to remove 100h which selects bank **/

    y = absolute((bheight/2) - (line & 0xff));
    y *= bheight;
    y /= (256*4);   /** we can assume 246=256 for speed. **/
	    /** since we're making /4 numbers when done, **/

    /** find distance to pixel. **/
    l = findlength(x,y);

    /** make center be 1.   **/
    l += HORZPITCH/4;

    /** make square of distance * (horzpitch/4)^2 **/
    l *= l;
    factor = (HORZPITCH/4)*(HORZPITCH/4);

    /** now l has the square of the distance and factor has the factor
    * that distance square is multiplied by.  we need to multiply the
    * intensity by that squared distance, then divide by factor.  **/

    runningblue *= l;
    runningblue /= factor;

    runninggreen *= l;
    runninggreen /= factor;

    runningred *= l;
    runningred /= factor;
}

/** this is like vignettesub but it's used for a single b&w level.
* col has the pixel number, and line the line number.  **/
int
grey_vignettesub(int line, int col, int b)
{
    int x, y, l, factor;

    /** get vertical size (horizontal to us).
    * make center point of image. 
    * (remember!  ccd is laid on side, so vertical 
    * distance is really horizontal distance!) 
    * make absolute value of it. **/
    x = absolute((bwidth/2) - col);

    /** now adjust for image size **/
    x *= bwidth;
    /** full image is 512, we we make ratio **/
    x /= (128*4);

    /** correct for pitch **/
    x *= (VERTPITCH * 128)/HORZPITCH;

    /** divide by 128 to get result **/
    /** but another 4 to keep numbers as /4 **/
    x /= (128*4);

    /** make horizontal center point (vertical to us) **/
    /** but work in byte to remove 100h **/
    /** make absolute value of vert. dif from center **/
    y = absolute((bheight/2) - (line & 0xff));

    /** we can also assume 246=256 for speed. **/
    y *= bheight;
    y /= (256*4);

    /** find distance using dx and ax. returned in ax. **/
    l = findlength(x,y);

    /** make center be 1.   **/
    l += HORZPITCH/4;

    /** make square of distance * (horzpitch/4)^2 **/
    l *= l;
    factor = (HORZPITCH/4)*(HORZPITCH/4);

    /** now l has the square of the distance and factor has the factor
    * that distance square is multiplied by.  we need to multiply the
    * intensity by that squared distance, then divide by factor.  **/

    return (b * l) / factor;
}


/** this routine averages the area surrounding the pixel specified by
* line and col.  the average is returned in runningred,
* runninggreen, and runningblue.  it assumes that the low bit of blue is
* used to flag if the lower or upper pixlimits value should be used.  **/

void
averagearea(int line, int col)
{

    byte *brg_p;
    int searchlimit;

    /** point to pixel in question **/
    brg_p = out_offs[line] + col*3;

    /** use upper limit(if pixel marked bad)? **/
    if (brg_p[BLUE] & 1) {
	searchlimit = highplimit;

	/** we're using the larger pixlimit.  experiments show we
	* should grab the whole area.  first we try just the good
	* ones, then if that fails we use it all.  **/

	/** try to use only the good ones **/
	if (goodaverage(line, col, searchlimit)) {
	    MOIREDBGSET(1,0,0); /** red: moderate amount go here. **/
	} else if ((opts & OPT_COLOR_TRACING) && 
			boundryaverage(line, col, searchlimit)) {
	    MOIREDBGSET(0,1,0); /** blue: a low amount go here. **/
	} else { /** no good. use all of them. **/
	    allaverage(line, col, searchlimit);
	    MOIREDBGSET(0,0,0); /** black: a moderately to low amount **/
	}

    } else {
	searchlimit = lowplimit;

	/** we're using the smaller pixlimit.  try to use only good
	* pixels first.  **/

	/** try to use only the good ones **/
	if (goodaverage(line, col, searchlimit)) {
	    MOIREDBGSET(1,1,1); /** white: a high amount go here. **/
	} else if ((opts & OPT_COLOR_TRACING) &&
			boundryaverage(line, col, searchlimit)) {
	    /** try just boundry tracing **/
	    MOIREDBGSET(1,0,1); /** yellow: extreme low amount here **/
	} else { /** no good. use all of them. **/
	    allaverage(line, col, searchlimit);
	    MOIREDBGSET(0,0,1); /** green: extreme low amount go here. **/
	}
    }
#ifndef MOIREDBG 
    /** now average the colors we added. **/
    runningred = rdiv(runningred, runningcount);
    runninggreen = rdiv(runninggreen, runningcount);
    runningblue = rdiv(runningblue, runningcount);
#endif
}

inline void
clearaverage(void)
{
    runningred = runninggreen = runningblue = runningcount = 0;
}


/** this sub makes the average for the area specified in searchlimit.  it
* uses all pixels, there are no boundry or good pixel checks, and it always
* returns an averaged value.  **/

void
allaverage(int line, int col, int searchlimit)
{
    int i;
    int bankbeg = line & 0xff00;
    int bankend = bankbeg + bheight;
    int searchpos;

    /** clear the color averages **/
    clearaverage();

    /** do this row **/
    next_all_average(line, col, searchlimit);

    searchpos = 0;

    while (++searchpos <= searchlimit) {

	/** go 1 up and then 1 down and repeat until we've done
	* pixlimit above and below **/

	/** try to go 1 up **/
	i = line - searchpos;
	if (i >= bankbeg)
	    next_all_average(i, col, searchlimit);

	/** try to go 1 down **/
	i = line + searchpos;
	if (i < bankend)
	    next_all_average(i, col, searchlimit);
    }

}

/** this routine takes a row number in bx and sets up to call
* continue_all_average.  it uses all pixels.  **/

/** next_all_average: **/
void
next_all_average(int line, int col, int searchlimit)
{
    byte *traceoff;

    /** set up the trace buffer pointer **/
    traceoff = pointtrace(line);
    
    continue_all_average(col, searchlimit, &traceoff[col],
	out_offs[line] + col*3);
}


/** entry:  brg_p points to the bgr pixel data, tr_off points to the
* b&w trace buffer entries for the pixel data, col is the number of
* the pixel on the scan line, centraltype and centpix have the original
* type of pixel we are trying to match and it's value.	centralbw has the
* black and white rendition of the cell you are on.  you have called
* clearaverage 1 time before starting your accumulated average.  **/

/** on return, runningred, etc.  have runningcount samples in them.  **/

/** call here with a new scan line after calling clearaverage and it will
* add the new samples to the average **/

void
continue_all_average(int col, int searchlimit, byte *tr_off, byte *brg_p)
{
    int counter;
    byte *p, *trp;

    p = brg_p; /** starting position of rgb **/
    trp = tr_off; /** and of bw info **/

    /** look left on the same scan line for a pixel we can use. **/

    /** this is the count of pixels to the left **/
    counter = col;
    /** skip if only doing 1 we are on **/
    if (counter != 0 && searchlimit != 0) {
	if (counter > searchlimit)
	    counter = searchlimit;

	/** average the ones to the left **/
	do {
	    p -= 3;

	    /** average the new color **/
	    runningblue += p[BLUE];
	    runninggreen += p[GREEN];
	    runningred += p[RED];

	    ++runningcount;

	} while (--counter > 0);

    }

    /** now average the colors to the right of this pixel, including
    * the one we are on.  **/

    p = brg_p;
    trp = tr_off;

    /** make count to right, including one we are on **/
    counter = bwidth - col;

    if (counter > searchlimit + 1)
	counter = searchlimit + 1;

    do { 
	/** average the new color **/
	runningblue += p[BLUE];
	runninggreen += p[GREEN];
	runningred += p[RED];

	++runningcount;

	p += 3;

    } while (--counter > 0);

}


/** this sub makes the average for the area specified in searchlimit.  it
* uses all pixels until it hits a boundry.  use this only if the options
* bits have boundry tracing on.  this routine does not check for bad
* pixels.  since boundry checking mean there aren't enough, this routine
* returns a flag:  non-zero means there were enough, zero means there
* weren't enough.  **/

/** boundryaverage: **/
int
boundryaverage(int line, int col, int searchlimit)
{
    int i;
    int bankbeg = line & 0xff00;
    int bankend = bankbeg + bheight;
    int searchpos;

    /** clear the color averages **/
    clearaverage();

    /** do this row **/
    (void)next_boundry_average(line, col, searchlimit);

    /** go up and repeat until we've done pixlimit above or we hit a
    * block **/
    searchpos = 0;
    while (++searchpos <= searchlimit) {
	i = line - searchpos;
	if (i < bankbeg || next_boundry_average(i, col, searchlimit) == 0)
	    break;
    }

    /** go down and repeat until we've done pixlimit below or we hit a
    * block **/
    searchpos = 0;
    while (++searchpos <= searchlimit) {
	i = line + searchpos;
	if (i >= bankend || next_boundry_average(i, col, searchlimit) == 0)
	    break;
    }

    /** now average the colors we added if there are enough.  we must
    * have 1/2 of the specified search area.  we can't use a fixed
    * number for what is considered enough, or else the user increasing
    * his pixlimit has no effect on some pixels.  **/

    if (runningcount < (((searchlimit*2+1) * (searchlimit*2+1))/2))
	return 0;

    return 1;
}

/** this routine takes a row number in bx and sets up to call
* continue_boundry_average.  it doesn't pay attention to "good" or "bad"
* pixel markings in the low bit of the blue, but it does honor b&w
* boundries.  it returns a flag:  zero means no pixels were found on this
* line, non-zero means some were found.  zero implies a solid boundry in
* the line direction you were moving.  **/

int
next_boundry_average(int line, int col, int searchlimit)
{
    int counted;
    byte *traceoff;

    /** set up the trace buffer pointer **/
    traceoff = pointtrace(line);
    
    counted = continue_boundry_average(col, searchlimit, &traceoff[col],
		out_offs[line] + col*3);
    runningcount += counted;
    return counted != 0;
}

/** this routine takes a pointer to a pixel on an rgb scan line and a pixel
* number in col and averages searchlimit pixels to the left and
* searchlimit to the right, including the one we are on.  it does boundry
* tracing during that process, aborting at the end of a boundry.  it does
* not check for "bad" pixels.  don't call unless boundry tracing is on in
* the options bits. 
*
* entry:  brg_p points to the bgr pixel data, tr_off points to the b&w
* trace buffer entries for the pixel data, col is the number of the
* pixel on the scan line, centraltype and centpix have the original type
* of pixel we are trying to match and it's value.  centralbw has the black
* and white rendition of the cell you are on.  you have called clearaverage
* 1 time before starting your accumulated average. 
*
* returns no. of samples we've added to runningred, etc.
**/

int
continue_boundry_average(int col, int searchlimit, byte *tr_off, byte *brg_p)
{
    int counter;
    byte *p, *trp;
    int counted = 0;


    p = brg_p; /** starting position of rgb **/
    trp = tr_off; /** and of bw info **/

    /** look left on the same scan line for a pixel we can use. **/

    /** this is the count of pixels to the left **/
    counter = col;
    /** skip if only doing 1 we are on **/
    if (counter != 0 && searchlimit != 0) {
	if (counter > searchlimit)
	    counter = searchlimit;

	/** average the ones to the left **/
	do {
	    p -= 3;

	    /** get b&w for check of boundry, see if too
	    * different: if so, we hit a block **/
	    trp--;
	    if (abs(*trp - centralbw) > centraldev)
		break;

	    /** average the new color **/
	    runningblue += p[BLUE];
	    runninggreen += p[GREEN];
	    runningred += p[RED];

	    ++counted;

	} while (--counter > 0);

    }

    /** now average the colors to the right of this pixel, including
    * the one we are on.  **/

    p = brg_p;
    trp = tr_off;

    /** make count to right, including one we are on **/
    counter = bwidth - col;

    if (counter > searchlimit + 1)
	counter = searchlimit + 1;

    do { 
	/** get b&w for check of boundry, see if too
	* different: if so, we hit a block **/
	if (abs(*trp - centralbw) > centraldev)
	    break;

	/** average the new color **/
	runningblue += p[BLUE];
	runninggreen += p[GREEN];
	runningred += p[RED];

	++counted;

	trp++;
	p += 3;

    } while (--counter > 0);
    return counted;
}

/** this sub makes the average for the area specified in searchlimit.  it
* uses only pixels which have not been marked as having moire troubles, and
* which are not on or past a boundry with the current b&w info for the
* starting pixel.  since this might mean there aren't enough, this routine
* returns a flag:  non-zero means there were enough, zero means there
* weren't enough.  **/



int
goodaverage(int line, int col, int searchlimit)
{
    int i;
    int bankbeg = line & 0xff00;
    int bankend = bankbeg + bheight;
    int searchpos;

    /** clear the color averages **/
    clearaverage();

    /** do this row **/
    next_good_average(line, col, searchlimit);


    /** go up and repeat until we've done pixlimit above or we hit a
    * block **/
    searchpos = 0;
    while (++searchpos <= searchlimit) {
	i = line - searchpos;
	if (i < bankbeg || next_good_average(i, col, searchlimit) == 0)
	    break;
    }

    /** go down and repeat until we've done pixlimit below or we hit a
    * block **/
    searchpos = 0;
    while (++searchpos <= searchlimit) {
	i = line + searchpos;
	if (i >= bankend || next_good_average(i, col, searchlimit) == 0)
	    break;
    }

    /** now average the colors we added if there are enough.  we must
    * have 1/4 of the specified search area.  **/
    if (runningcount < (((searchlimit*2+1) * (searchlimit*2+1))/4))
	return 0;

    return 1;
}

/** this routine takes a row number in bx and sets up to call
* continue_good_average.  it does pay attention to "good" or "bad" pixel
* markings in the low bit of the blue, as well as honoring b&w boundries. 
* it returns a flag:  z means no pixels were found on this line, nz means
* some were found.  z implies a solid boundry in the line direction you
* were moving.	**/

int
next_good_average(int line, int col, int searchlimit)
{
    int counted;
    byte *traceoff;

    /** set up the trace buffer pointer **/
    traceoff = pointtrace(line);
    
    counted = continue_good_average(col, searchlimit,
	&traceoff[col], out_offs[line] + col*3);
    runningcount += counted;
    return counted != 0;
}

/** this routine takes a pointer to a pixel on an rgb scan line
* and a pixel number in col and averages searchlimit pixels
* to the left and searchlimit to the right, including the one we are on. 
* it does boundry tracing during that process, aborting at the end of a
* boundry.  it also checks for "bad" pixels.  you may call regardless of
* whether boundry tracing is on or off. 
*
* on entry: brg_p points to the bgr pixel data, tr_off points to 
* the b&w trace buffer entries for the pixel data, col is the number  
* of the pixel on the scan line, centraltype and centpix have the 
* original type of pixel we are trying to match and it's value.  you 
* have called clearaverage 1 time before starting your accumulated average. 
*
* returns no. of samples we've added to runningred, etc.
**/


int
continue_good_average(int col, int searchlimit, byte *tr_off, byte *brg_p)
{

    int counter;
    byte *p, *trp;
    int counted = 0;


    p = brg_p; /** starting position of rgb **/
    trp = tr_off; /** and of bw info **/

    /** look left on the same scan line for a pixel we can use. **/

    /** this is the count of pixels to the left **/
    counter = col;
    /** skip if only doing 1 we are on **/
    if (counter != 0 && searchlimit != 0) {
	if (counter > searchlimit)
	    counter = searchlimit;

	/** average the ones to the left **/
	if ((opts & OPT_COLOR_TRACING) == 0) {
	    do {
		p -= 3;

		/** get blue, see if "bad" **/
		if ((p[BLUE] & 1) == 0) {
		    /** average the new color **/
		    runningblue += p[BLUE];
		    runninggreen += p[GREEN];
		    runningred += p[RED];

		    ++counted;
		}
	    } while (--counter > 0);
	} else {
	    do {
		p -= 3;

		/** get b&w for check of boundry, see if too
		* different: end if boundry **/
		trp--;
		if (abs(*trp - centralbw) > centraldev)
		    break;

		/** get blue, see if "bad" **/
		if ((p[BLUE] & 1) == 0) {
		    /** average the new color **/
		    runningblue += p[BLUE];
		    runninggreen += p[GREEN];
		    runningred += p[RED];

		    ++counted;
		}
	    } while (--counter > 0);
	}

    }

    /** now average the colors to the right of this pixel, including
    * the one we are on.  **/

    p = brg_p;
    trp = tr_off;

    /** make count to right, including one we are on **/
    counter = bwidth - col;

    if (counter > searchlimit + 1)
	counter = searchlimit + 1;

    if ((opts & OPT_COLOR_TRACING) == 0) {
	do { 
	    /** get blue, see if "bad" **/
	    if ((p[BLUE] & 1) == 0) {
		/** average the new color **/
		runningblue += p[BLUE];
		runninggreen += p[GREEN];
		runningred += p[RED];

		++counted;
	    }
	    p += 3;
	} while (--counter > 0);
    } else {
	do {
	    /** get b&w for check of boundry, see if too
	    * different: if so, boundry and we stop moving right **/
	    if (abs(*trp - centralbw) > centraldev)
		break;

	    /** get blue, see if "bad" **/
	    if ((p[BLUE] & 1) == 0) {
		/** average the new color **/
		runningblue += p[BLUE];
		runninggreen += p[GREEN];
		runningred += p[RED];

		++counted;
	    }
	    trp++;
	    p += 3;
	} while (--counter > 0);
    }

    return counted;

}


/** this sub takes whatever is in runningred, green, and blue, and adjusts
* them to match the value read in centpix with filter curtype.	this
* routine uses math and the single pixel to make the intensity adjustment. 
* **/

void
fixintensity(int curpix, int curtype)
{
    int tmp;
    int redlevel, greenlevel, bluelevel;

#define SCAL(x) (x*128)/100;

    if (curtype == CYG) { /** cyg filter characteristics **/
	redlevel = SCAL(22);
	greenlevel = SCAL(178);
	bluelevel = SCAL(83);

    } else if (curtype == YEMG) { /** yemg filter characteristics **/
	redlevel = SCAL(131);
	greenlevel = SCAL(127);
	bluelevel = SCAL(59);

    } else if (curtype == CYMG) { /** cymg filter characteristics **/
	redlevel = SCAL(61);
	greenlevel = SCAL(121);
	bluelevel = SCAL(138);

    } else { /** yeg filter characteristics **/
	redlevel = SCAL(92);
	greenlevel = SCAL(184);
	bluelevel = SCAL(16);
    }

    tmp = (redlevel * runningred)/128;
    tmp += (bluelevel * runningblue)/128;
    tmp += (greenlevel * runninggreen)/128;

    tmp = rdiv(curpix*256,tmp);

    runningred = (runningred * tmp)/128;
    runninggreen = (runninggreen * tmp)/128;
    runningblue = (runningblue * tmp)/128;

}



/** common code for color calcs */
inline int
color_eqn(int A, int a, int B, int b, int C, int c)
{
    int t;
    t =     (A * 128)/100 * a +
	(B * 128)/100 * b +
	(C * 128)/100 * c;
    if (t < 0) {
	/** if underflow, it's a moire situation **/
	if (t < UNDERFLO * 128)
	    moirefound |= FLAGMOIRE;
	t = 0;
    }

    /** result*128 is in t.   **/
    return t / 128;
}


/* shorthand for common args for the color calc routines */
#define E entrycyg, entrycymg, entryyeg, entryyemg

/** these subs solve for green for any combination of three color filters. 
* the values should be in entrycymg, entryyeg, entryyemg, and entrycyg. 
* the names give the color filters in alphabetical order seperated by _ and
* all preceeded by g.  **/

/** cymg, yeg, and yemg.
     cymg = .61r +  1.21g + 1.38b 
     yeg  = .92r +  1.84g + .16b
     yemg = 1.31r + 1.27g + .59b     

    g = .21*cymg + .92*yeg - .74*yemg
**/

inline int
g_cymg_yeg_yemg(int entrycyg, int entrycymg, int entryyeg, int entryyemg )
{
    return color_eqn(21, entrycymg, 92, entryyeg, -74, entryyemg);
}


/** cyg, cymg and yeg 
      cymg = .61r + 1.21g + 1.38b 
      yeg  = .92r + 1.84g + .16b 
      cyg  = .22r + 1.78g + .83b 

    g = .74*cyg + .13*yeg - .46*cymg
**/

inline int
g_cyg_cymg_yeg(int entrycyg, int entrycymg, int entryyeg, int entryyemg )
{
    return color_eqn(74, entrycyg, 13, entryyeg, -46, entrycymg);
}


/** cyg, yeg, and yemg 
      yemg = 1.31r + 1.27g + .59b    
      yeg  = .92r + 1.84g + .16b 
      cyg = .22r +  1.78g + .83b 

    g = .23*cyg + .67*yeg - .51*yemg
**/

inline int
g_cyg_yeg_yemg(int entrycyg, int entrycymg, int entryyeg, int entryyemg )
{
    return color_eqn( 23, entrycyg, 67, entryyeg, -51, entryyemg );
}


/** cyg, cymg and yemg 
      yemg = 1.31r + 1.27g + .59b    
      cymg = .61r + 1.21g + 1.38b 
      cyg  = .22r + 1.78g + .83b 

    g = .86*cyg + .12*yemg - .57*cymg
**/

inline int
g_cyg_cymg_yemg(int entrycyg, int entrycymg, int entryyeg, int entryyemg )
{
    return color_eqn(86, entrycyg, 12, entryyemg, -57, entrycymg);
}

/** this sub does a mathmatical calculation to derive the green.  it is
* designed to be an equation based alternative to adding the 4 pixels used
* to make calculate the red and blue.  it takes 4 pixels to derive the red
* and blue, and they are on opposite scan lines.  but it only takes three
* pixels (with three different color filters) to solve for a given color. 
* this routine uses all 4 by solving opposite corners and checking for a
* deviation.  the deviation is range checked and a flag set if it's too
* high.  **/

/** the cells are arranged as follows:
	cyg  yemg cyg  yemg cyg  yemg
	cymg yeg  cymg yeg  cymg yeg
	cyg  yemg cyg  yemg cyg  yemg
	cymg yeg  cymg yeg  cymg yeg
**/


int
greensub(int entrycyg, int entrycymg, int entryyeg, int entryyemg, int centraltype )
{

    /**  average them but leave it as *2. **/

    if (centraltype == YEG) {
	/** solution for yeg cell.  add lower right, upper left **/
	return g_cymg_yeg_yemg(E) + g_cyg_cymg_yemg(E);
    } else if (centraltype == CYMG) {
	/** solution for cymg cell.  add lower left, upper right **/
	return g_cyg_cymg_yeg(E) + g_cyg_yeg_yemg(E);
    } else if (centraltype == YEMG) {
	/** solution for yemg cell.  add upper right, lower right **/
	return g_cyg_yeg_yemg(E) + g_cyg_cymg_yeg(E);
    } else {
	/** solution for cyg cell.  add upper left, lower right **/
	return g_cyg_cymg_yemg(E) + g_cymg_yeg_yemg(E);
    }
}


/** these subs solve for red for any combination of three color filters.  the 
values should be in entrycymg, entryyeg, entryyemg, and entrycyg.  on 
return, the solution is in ax.	only dx is changed.  the names give the 
color filters in alphabetical order seperated by _ and all preceeded by 
r. **/

/** cymg, yeg, and yemg. 
     cymg = .61r +  1.21g + 1.38b 
     yeg  = .92r +  1.84g + .16b 
     yemg = 1.31r + 1.27g + .59b     

    r = 1.49*yemg - .56*cymg - .66*yeg
**/

inline int
r_cymg_yeg_yemg(int entrycyg, int entrycymg, int entryyeg, int entryyemg )
{
    return color_eqn(149, entryyemg, -56, entrycymg, -66, entryyeg);
}


/** cyg, cymg and yeg 
      cymg = .61r + 1.21g + 1.38b 
      cyg  = .22r + 1.78g + .83b 
      yeg  = .92r + 1.84g + .16b 

    r = .92*yeg + .79*cymg - 1.49*cyg **/

inline int
r_cyg_cymg_yeg(int entrycyg, int entrycymg, int entryyeg, int entryyemg )
{
    return color_eqn(92, entryyeg, 79, entrycymg, -149, entrycyg);
}

/** cyg, yeg, and yemg 
      yemg = 1.31r + 1.27g + .59b    
      cyg = .22r +  1.78g + .83b 
      yeg  = .92r + 1.84g + .16b 

    sharp formula (assume redfactor=115) 
    r = .868*yemg -.0027*yeg - .617*cyg
**/

inline int
r_cyg_yeg_yemg(int entrycyg, int entrycymg, int entryyeg, int entryyemg )
{
    return color_eqn(87, entryyemg, 0, entryyeg, -62, entrycyg);
}

/** cyg, cymg and yemg 
      cymg = .61r + 1.21g + 1.38b 
      cyg  = .22r + 1.78g + .83b 
      yemg = 1.31r + 1.27g + .59b    

    same as sharp formula 
    r = .866*yemg + .0023*cymg - .619*cyg
**/

inline int
r_cyg_cymg_yemg(int entrycyg, int entrycymg, int entryyeg, int entryyemg )
{
    return color_eqn(87, entryyemg, 0, entrycymg, -62, entrycyg);
}

/** this sub does a mathmatical calculation to derive the red.	it is
* designed to be an equation based alternative to the sharp 2 pixel
* formula.  it works similar to greensub.  **/

int
redsub(int entrycyg, int entrycymg, int entryyeg, int entryyemg, int centraltype )
{

    /**  average them but leave it as *2. **/

    if (centraltype == YEG) {
	/** solution for yeg cell.  add lower right, upper left **/
	return r_cymg_yeg_yemg(E) + r_cyg_cymg_yemg(E);
    } else if (centraltype == CYMG) {
	/** solution for cymg cell.  add lower left, upper right **/
	return r_cyg_cymg_yeg(E) + r_cyg_yeg_yemg(E);
    } else if (centraltype == YEMG) {
	/** solution for yemg cell.  add upper right, lower right **/
	return r_cyg_yeg_yemg(E) + r_cyg_cymg_yeg(E);
    } else {
	/** solution for cyg cell.  add upper left, lower right **/
	return r_cyg_cymg_yemg(E) + r_cymg_yeg_yemg(E);
    }
}

/** these subs solve for blue for any combination of three color filters. 
* the values should be in entrycymg, entryyeg, entryyemg, and entrycyg.  on
* return, the solution is in ax.  only dx is changed.  the names give the
* color filters in alphabetical order seperated by _ and all preceeded by
* b.  **/

/** cymg, yeg, and yemg. 
     cymg = .61r +  1.21g + 1.38b 
     yeg  = .92r +  1.84g + .16b 
     yemg = 1.31r + 1.27g + .59b     

    b = .786*cymg - .513*yeg - .0058*yemg
**/

inline int
b_cymg_yeg_yemg(int entrycyg, int entrycymg, int entryyeg, int entryyemg )
{
    return color_eqn(79, entrycymg, -51, entryyeg, -1, entryyemg);
}


/** cyg, cymg and yeg 
      cymg = .61r + 1.21g + 1.38b 
      yeg  = .92r + 1.84g + .16b 
      cyg  = .22r + 1.78g + .83b 

    b = .781*cymg +.0058*cyg - .519*yeg
**/

inline int
b_cyg_cymg_yeg(int entrycyg, int entrycymg, int entryyeg, int entryyemg )
{
    return color_eqn(78, entrycymg, 1, entrycyg, -52, entryyeg);
}

/** cyg, yeg, and yemg 
      yemg = 1.31r + 1.27g + .59b    
      yeg  = .92r + 1.84g + .16b 
      cyg = .22r +  1.78g + .83b 

    b = .868*cyg + .862*yemg - 1.435*yeg
**/

inline int
b_cyg_yeg_yemg(int entrycyg, int entrycymg, int entryyeg, int entryyemg )
{
    return color_eqn(87, entrycyg, 86, entryyemg, -144, entryyeg);
}


/** cyg, cymg and yemg 
      yemg = 1.31r + 1.27g + .59b    
      cymg = .61r + 1.21g + 1.38b 
      cyg  = .22r + 1.78g + .83b 

    b = 1.224*cymg -.489*yemg - .483*cyg
**/

inline int
b_cyg_cymg_yemg(int entrycyg, int entrycymg, int entryyeg, int entryyemg )
{
    return color_eqn(122, entrycymg, -49, entryyemg, -48, entrycyg);
}

/** this sub does a mathmatical calculation to derive the blue.  it is
* designed to be an equation based alternative to the sharp 2 pixel
* formula.  it works similar to greensub.  **/

int
bluesub(int entrycyg, int entrycymg, int entryyeg, int entryyemg, int centraltype )
{

    /**  average them but leave it as *2. **/

    if (centraltype == YEG) {
	/** solution for yeg cell.  add lower right, upper left **/
	return b_cymg_yeg_yemg(E) + b_cyg_cymg_yemg(E);
    } else if (centraltype == CYMG) {
	/** solution for cymg cell.  add lower left, upper right **/
	return b_cyg_cymg_yeg(E) + b_cyg_yeg_yemg(E);
    } else if (centraltype == YEMG) {
	/** solution for yemg cell.  add upper right, lower right **/
	return b_cyg_yeg_yemg(E) + b_cyg_cymg_yeg(E);
    } else {
	/** solution for cyg cell.  add upper left, lower right **/
	return b_cyg_cymg_yemg(E) + b_cymg_yeg_yemg(E);
    }
}


#ifdef COLOR_FILTER_DOCUMENTATION
/* 

    ;	color filter documentation

    ;this table shows that each cell has a certain sensitivity to
    ;different frequencies of light.  if you assume that all frequencies
    ;can be represented by a red, green, and blue component, you can
    ;specify the percentage sensitivity of each photocell type to
    ;each color of light.  for instance, yellow registers 81% of the
    ;red light falling on it, 98% of the green, but only 8% of the blue.
    ;
    ;the sensitivities were designed by sharp so that when you combine
    ;cells in the correct manner, you can multiply adjacent combinations
    ;by a factor and subtract to get the pure color of red or blue.
    ;the cell combination happens when you scan in interleave mode.
    ;the cells are electrically combined.  then a given scan line
    ;will have the correct adjacent cells to use their formula to get
    ;a specific color.	on one scan line you can derive the red, but
    ;you have to derive the blue from the next scan line.  the result
    ;is a tricky resolution that is really closer to 1/4 what is specced.
    ;
    ;their explanation of how to derive green was fuzzy.  we do it
    ;by solving 3 adjacent cells (3 equations, 3 variables) for green.
    ;if you do that for red and blue you find yourself coming up with
    ;sharp's formula in 2 of the cases, so the reasoning is good.
    ;unfortunately, at the end green seems to be twice as bright as
    ;it should be for no known reason.	but it is proportionately correct.

    ;odd and even interlace fields have the same pixel combinations of:
    ;	cyg  yemg cyg  yemg cyg  yemg ... repeat for 512 total.
    ;	cymg yeg  cymg yeg  cymg yeg  ... repeat for 512 total.
    ;	cyg  yemg cyg  yemg cyg  yemg ... repeat for 512 total.
    ;	cymg yeg  cymg yeg  cymg yeg  ... repeat for 512 total.
    ;	|
    ;	|
    ;	repeat for 246 total
    ;
    ;non-interlace pixels are arranged like this:
    ;
    ;	cy ye cy ye cy ye ... repeat for 512 total.
    ;	g mg g mg g mg g  ... repeat for 512 total.
    ;	cy ye cy ye cy ye ... repeat for 512 total.
    ;	mg g mg g mg g mg ... repeat for 512 total.
    ;	|
    ;	|
    ;	repeat for 492 total
    ;
    ;a chart showing the relative percentage sensitivity of each cell type,
    ;plus their combinations and some formulas used to derive color information
    ;is present in the sharp book, "Designing with CCDs".  the important
    ;table is shown here:

    ;               red     green   blue
    ;---------------------------------------------------------
    ;   ye          81      98      8
    ;   cy          11      92      75
    ;   mg          50      29      51
    ;   g           11      86      8
    ;---------------------------------------------------------
    ;   ye+mg       131     127     59
    ;   cy+g        22      178     83
    ;   alpha       0.71    0.71    0.71
    ;   Subtraction 115     0.16    -0.15
    ;---------------------------------------------------------
    ;   cy+mg       61      121     138
    ;   ye+g        92      184     16
    ;   beta        0.66    0.66    0.66
    ;   Subtraction 0.33    -0.35   127
    ;---------------------------------------------------------
    ;

    ;here's the basic program that solves the 3 equation color combos.
    ;
    ;1 rem this program takes real world red, green, blue levels plus the value
    ;2 rem measured on a ccd cell and uses three of these combinations to come
    ;3 rem up with the factors for red, green, and blue for that type of cell.
    ;5 print
    ;6 print
    ;10 input "filter1 (cyg, cymg, yeg, yemg, etc.)";f1$
    ;15 f1=1
    ;20 input "red factor";rf1
    ;30 input "green factor";gf1
    ;40 input "blue factor";bf1
    ;60 print
    ;110 input "filter2 (cyg, cymg, yeg, yemg, etc.)";f2$
    ;115 f2=1
    ;120 input "red factor";rf2
    ;130 input "green factor";gf2
    ;140 input "blue factor";bf2
    ;150 print
    ;210 input "filter3 (cyg, cymg, yeg, yemg, etc.)";f3$
    ;215 f3=1
    ;220 input "red factor";rf3
    ;230 input "green factor";gf3
    ;240 input "blue factor";bf3
    ;250 print
    ;400 rem here's what we have inputted:
    ;402 rem f1*f1$ = rf1*r+gf1*g+bf1*b
    ;404 rem f2*f2$ = rf2*r+gf2*g+bf2*b
    ;406 rem f3*f3$ = rf3*r+gf3*g+bf3*b
    ;407 rem f1$, f2$, f3$ are the names of the filters read but in the final
    ;408 rem equation you will multiply the value read by the factors f1, f2, f3.
    ;409 rem
    ;410 rem now a normal solution to a 3 equation, 3 variable situation.
    ;415 rem
    ;420 rem first get rid of the blue and make 2 equations
    ;430 rf4=rf3*bf1/bf3
    ;431 gf4=gf3*bf1/bf3
    ;433 f4=f3*bf1/bf3
    ;435 rf4=rf1-rf4:gf4=gf1-gf4
    ;436 rem
    ;437 rem now: f1*f1$-f4*f3$ = rf4*r+gf4*g
    ;438 rem
    ;440 rf5=rf3*bf2/bf3
    ;441 gf5=gf3*bf2/bf3
    ;443 f5=f3*bf2/bf3
    ;445 rf5=rf2-rf5:gf5=gf2-gf5
    ;446 rem
    ;447 rem now: f2*f2$-f5*f3$ = rf5*r+gf5*g
    ;448 rem
    ;450 rem so now we have only 2 equations to worry about:
    ;451 rem f1*f1$-f4*f3$ = rf4*r+gf4*g
    ;452 rem f2*f2$-f5*f3$ = rf5*r+gf5*g
    ;453 rem
    ;454 rem solve them to get rid of green.
    ;460 rf6=rf4-rf5*gf4/gf5
    ;462 f6=f2*gf4/gf5
    ;464 f7=f4-f5*gf4/gf5
    ;468 rem now: f1*f1$-f6*f2$-f7*f3$ = rf6*r
    ;470 rem
    ;472 rem solve to get the three factors to deduce red
    ;474 redf1=f1/rf6:redf2=-f6/rf6:redf3=-f7/rf6
    ;476 rem
    ;480 rem now do the green. solve to get rid of red.
    ;481 gf6=gf4-gf5*rf4/rf5
    ;482 f6=f2*rf4/rf5
    ;484 f7=f4-f5*rf4/rf5
    ;488 rem now: f1*f1$-f6*f2$-f7*f3$ = gf6*g
    ;490 rem
    ;500 greenf1=f1/gf6:greenf2=-f6/gf6:greenf3=-f7/gf6
    ;501 rem
    ;1400 rem now back to the beginning to solve for blue
    ;1401 rem
    ;1402 rem f1*f1$ = rf1*r+gf1*g+bf1*b
    ;1404 rem f2*f2$ = rf2*r+gf2*g+bf2*b
    ;1406 rem f3*f3$ = rf3*r+gf3*g+bf3*b
    ;1420 rem first get rid of the green and make 2 equations
    ;1430 rf4=rf3*gf1/gf3
    ;1431 bf4=bf3*gf1/gf3
    ;1433 f4=f3*gf1/gf3
    ;1435 rf4=rf1-rf4:bf4=bf1-bf4
    ;1436 rem
    ;1437 rem now: f1*f1$-f4*f3$ = rf4*r+bf4*b
    ;1438 rem
    ;1440 rf5=rf3*gf2/gf3
    ;1441 bf5=bf3*gf2/gf3
    ;1443 f5=f3*gf2/gf3
    ;1445 rf5=rf2-rf5:bf5=bf2-bf5
    ;1446 rem
    ;1447 rem now: f2*f2$-f5*f3$ = rf5*r+bf5*b
    ;1448 rem
    ;1450 rem so now we have only 2 equations to worry about:
    ;1451 rem f1*f1$-f4*f3$ = rf4*r+bf4*b
    ;1452 rem f2*f2$-f5*f3$ = rf5*r+bf5*b
    ;1453 rem
    ;1454 rem solve them to get rid of red.
    ;1460 bf6=bf4-bf5*rf4/rf5
    ;1462 f6=f2*rf4/rf5
    ;1464 f7=f4-f5*rf4/rf5
    ;1468 rem now: f1*f1$-f6*f2$-f7*f3$ = bf6*b
    ;1470 rem
    ;1474 bluef1=f1/bf6:bluef2=-f6/bf6:bluef3=-f7/bf6
    ;10000 print
    ;10010 print "red = ";redf1;f1$;",";redf2;f2$;",";redf3;f3$
    ;10020 print "green = ";greenf1;f1$;",";greenf2;f2$;",";greenf3;f3$
    ;10030 print "blue = ";bluef1;f1$;",";bluef2;f2$;",";bluef3;f3$
    ;2000 goto 1

*/
#endif /* COLOR_FILTER_DOCUMENTATION */

