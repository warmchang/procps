/*
 * watch - execute a program repeatedly, displaying output fullscreen
 *
 * Copyright © 2010-2023 Jim Warner <james.warner@comcast.net>
 * Copyright © 2015-2023 Craig Small <csmall@dropbear.xyz>
 * Copyright © 2011-2012 Sami Kerola <kerolasa@iki.fi>
 * Copyright © 2002-2007 Albert Cahalan
 * Copyright © 1999      Mike Coleman <mkc@acm.org>.
 *
 * Based on the original 1991 'watch' by Tony Rems <rembo@unisoft.com>
 * (with mods and corrections by Francois Pinard).
 *
 * stderr handling, exec, and beep option added by Morty Abzug, 2008
 * Unicode Support added by Jarrod Lowe <procps@rrod.net> in 2009.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "c.h"
#include "config.h"
#include "fileutils.h"
#include "nls.h"
#include "strutils.h"
#include "xalloc.h"
#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <locale.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
#include <assert.h>
#ifdef WITH_WATCH8BIT
# define _XOPEN_SOURCE_EXTENDED 1
# include <wchar.h>
# include <wctype.h>
# include <ncursesw/ncurses.h>
#else
# include <ncurses.h>
#endif	/* WITH_WATCH8BIT */

#ifdef FORCE_8BIT
# undef isprint
# define isprint(x) ( (x>=' '&&x<='~') || (x>=0xa0) )
#endif

/* Boolean command line options */
static int flags;
#define WATCH_DIFF     (1 << 0)
#define WATCH_CUMUL    (1 << 1)
#define WATCH_EXEC     (1 << 2)
#define WATCH_BEEP     (1 << 3)
#define WATCH_COLOR    (1 << 4)
#define WATCH_ERREXIT  (1 << 5)
#define WATCH_CHGEXIT  (1 << 6)
#define WATCH_EQUEXIT  (1 << 7)
#define WATCH_NORERUN  (1 << 8)
// Do we care about screen contents changes at all?
#define WATCH_ALL_DIFF (WATCH_DIFF | WATCH_CHGEXIT | WATCH_EQUEXIT)

#define TAB_WIDTH 8
#define MAX_ANSIBUF 100

#ifdef WITH_WATCH8BIT
#define XL(c) L ## c
#define XEOF WEOF
#define Xint wint_t
#define Xgetc(stream) getmb(stream)
#else
#define XL(c) c
#define XEOF EOF
#define Xint int
#define Xgetc(stream) getc(stream)
#endif

// TODO: bools into flags above
static bool curses_started = false;
static long height = 24, width = 80;  // TODO: long?
static bool screen_size_changed = false;
static bool first_screen = true;
static uf8 show_title = 2;	/* number of lines used, 2 or 0 */
static bool precise_timekeeping = false;
static bool line_wrap = true;

static void __attribute__ ((__noreturn__)) usage(FILE * out)
{
	fputs(USAGE_HEADER, out);
	fprintf(out, _(" %s [options] command\n"), program_invocation_short_name);
	fputs(USAGE_OPTIONS, out);
	// TODO: other tools in src/ use one leading blank
	fputs(_("  -b, --beep             beep if command has a non-zero exit\n"), out);
	fputs(_("  -c, --color            interpret ANSI color and style sequences\n"), out);
	fputs(_("  -C, --no-color         do not interpret ANSI color and style sequences\n"), out);
	fputs(_("  -d, --differences[=<permanent>]\n"
	        "                         highlight changes between updates\n"), out);
	fputs(_("  -e, --errexit          exit if command has a non-zero exit\n"), out);
	fputs(_("  -g, --chgexit          exit when output from command changes\n"), out);
	fputs(_("  -q, --equexit <cycles>\n"
	        "                         exit when output from command does not change\n"), out);
	fputs(_("  -n, --interval <secs>  seconds to wait between updates\n"), out);
	fputs(_("  -p, --precise          attempt run command in precise intervals\n"), out);
	fputs(_("  -r, --no-rerun         do not rerun program on window resize\n"), out);
	fputs(_("  -t, --no-title         turn off header\n"), out);
	fputs(_("  -w, --no-wrap          turn off line wrapping\n"), out);
	fputs(_("  -x, --exec             pass command to exec instead of \"sh -c\"\n"), out);
	fputs(USAGE_SEPARATOR, out);
	fputs(USAGE_HELP, out);
	fputs(_(" -v, --version  output version information and exit\n"), out);
	fprintf(out, USAGE_MAN_TAIL("watch(1)"));

	exit(out == stderr ? EXIT_FAILURE : EXIT_SUCCESS);
}

static int nr_of_colors;
static int attributes;
static int fg_col;
static int bg_col;
static int more_colors;


static void reset_ansi(void)
{
	attributes = A_NORMAL;
	fg_col = 0;
	bg_col = 0;
}

static void init_ansi_colors(void)
{
	const short ncurses_colors[] = {
		-1, COLOR_BLACK, COLOR_RED, COLOR_GREEN, COLOR_YELLOW,
		COLOR_BLUE, COLOR_MAGENTA, COLOR_CYAN, COLOR_WHITE
	};
	nr_of_colors = sizeof(ncurses_colors) / sizeof(*ncurses_colors);

	more_colors = (COLORS >= 16) && (COLOR_PAIRS >= 16 * 16);

	// Initialize ncurses colors. -1 is terminal default
	// 0-7 are auto created standard colors initialized by ncurses
	if (more_colors) {
		// Initialize using ANSI SGR 8-bit specified colors
		// 8-15 are bright colors
		init_color(8, 333, 333, 333);  // Bright black
		init_color(9, 1000, 333, 333);  // Bright red
		init_color(10, 333, 1000, 333);  // Bright green
		init_color(11, 1000, 1000, 333);  // Bright yellow
		init_color(12, 333, 333, 1000);  // Bright blue
		init_color(13, 1000, 333, 1000);  // Bright magenta
		init_color(14, 333, 1000, 1000);  // Bright cyan
		// Often ncurses is built with only 256 colors, so lets
		// stop here - so we can support the -1 terminal default
		//init_color(15, 1000, 1000, 1000);  // Bright white
		nr_of_colors += 7;
	}

	// Initialize all color pairs with ncurses
	for (bg_col = 0; bg_col < nr_of_colors; bg_col++)
		for (fg_col = 0; fg_col < nr_of_colors; fg_col++)
			init_pair(bg_col * nr_of_colors + fg_col + 1, fg_col - 1, bg_col - 1);

	reset_ansi();
}


static int process_ansi_color_escape_sequence(char** escape_sequence) {
	// process SGR ANSI color escape sequence
	// Eg 8-bit
	// 38;5;⟨n⟩  (set fg color to n)
	// 48;5;⟨n⟩  (set bg color to n)
	//
	// Eg 24-bit (not yet implemented)
	// ESC[ 38;2;⟨r⟩;⟨g⟩;⟨b⟩ m Select RGB foreground color
	// ESC[ 48;2;⟨r⟩;⟨g⟩;⟨b⟩ m Select RGB background color
	long num;

	if (!escape_sequence || !*escape_sequence)
		return 0; /* avoid NULLPTR dereference, return "not understood" */

	if ((*escape_sequence)[0] != ';')
		return 0; /* not understood */

	if ((*escape_sequence)[1] == '5') {
		// 8 bit! ANSI specifies a predefined set of 256 colors here.
		if ((*escape_sequence)[2] != ';')
			return 0; /* not understood */
		// TODO: not guaranteed to work with the same array
		num = strtol((*escape_sequence) + 3, escape_sequence, 10);
		if (num >= 0 && num <= 7) {
			// 0-7 are standard colors  same as SGR 30-37
			return num + 1;
		}
		if (num >= 8 && num <= 15) {
			// 8-15 are standard colors  same as SGR 90-97
			 return more_colors ? num + 1 : num - 8 + 1;
		}

		// Remainder aren't yet implemented
		// 16-231:  6 × 6 × 6 cube (216 colors): 16 + 36 × r + 6 × g + b (0 ≤ r, g, b ≤ 5)
		// 232-255:  grayscale from black to white in 24 steps
	}

	return 0; /* not understood */
}


static int set_ansi_attribute(const int attrib, char** escape_sequence)
{
	if (!(flags & WATCH_COLOR))
		return 1;

	switch (attrib) {
	case -1:	/* restore last settings */
		break;
	case 0:		/* restore default settings */
		reset_ansi();
		break;
	case 1:		/* set bold / increased intensity */
		attributes |= A_BOLD;
		break;
	case 2:		/* set decreased intensity (if supported) */
		attributes |= A_DIM;
		break;
#ifdef A_ITALIC
	case 3:		/* set italic (if supported) */
		attributes |= A_ITALIC;
		break;
#endif
	case 4:		/* set underline */
		attributes |= A_UNDERLINE;
		break;
	case 5:		/* set blinking */
		attributes |= A_BLINK;
		break;
	case 7:		/* set inversed */
		attributes |= A_REVERSE;
		break;
	case 21:	/* unset bold / increased intensity */
		attributes &= ~A_BOLD;
		break;
	case 22:	/* unset bold / any intensity modifier */
		attributes &= ~(A_BOLD | A_DIM);
		break;
#ifdef A_ITALIC
	case 23:	/* unset italic */
		attributes &= ~A_ITALIC;
		break;
#endif
	case 24:	/* unset underline */
		attributes &= ~A_UNDERLINE;
		break;
	case 25:	/* unset blinking */
		attributes &= ~A_BLINK;
		break;
	case 27:	/* unset inversed */
		attributes &= ~A_REVERSE;
		break;
    case 38:
        fg_col = process_ansi_color_escape_sequence(escape_sequence);
        if (fg_col == 0) {
            return 0; /* not understood */
        }
        break;
	case 39:
		fg_col = 0;
		break;
    case 48:
        bg_col = process_ansi_color_escape_sequence(escape_sequence);
        if (bg_col == 0) {
            return 0; /* not understood */
        }
        break;
    case 49:
        bg_col = 0;
        break;
	default:
		if (attrib >= 30 && attrib <= 37) {	/* set foreground color */
			fg_col = attrib - 30 + 1;
		} else if (attrib >= 40 && attrib <= 47) { /* set background color */
			bg_col = attrib - 40 + 1;
		} else if (attrib >= 90 && attrib <= 97) { /* set bright fg color */
			fg_col = more_colors ? attrib - 90 + 9 : attrib - 90 + 1;
		} else if (attrib >= 100 && attrib <= 107) { /* set bright bg color */
			bg_col = more_colors ? attrib - 100 + 9 : attrib - 100 + 1;
		} else {
			return 0; /* Not understood */
		}
	}
    attr_set(attributes, bg_col * nr_of_colors + fg_col + 1, NULL);
    return 1;
}

static void process_ansi(FILE * fp)
{
	if (!(flags & WATCH_COLOR))
		return;

	int i, c;
	char buf[MAX_ANSIBUF];
	char *numstart, *endptr;
	int ansi_attribute;

	c = getc(fp);

	if (c == '(') {
		c = getc(fp);
		c = getc(fp);
	}
	if (c != '[') {
		ungetc(c, fp);
		return;
	}
	for (i = 0; i < MAX_ANSIBUF; i++) {
		c = getc(fp);
		/* COLOUR SEQUENCE ENDS in 'm' */
		if (c == 'm') {
			buf[i] = '\0';
			break;
		}
		if ((c < '0' || c > '9') && c != ';') {
			return;
		}
		assert(c >= 0 && c <= SCHAR_MAX);
		buf[i] = c;
	}
	/*
	 * buf now contains a semicolon-separated list of decimal integers,
	 * each indicating an attribute to apply.
	 * For example, buf might contain "0;1;31", derived from the color
	 * escape sequence "<ESC>[0;1;31m". There can be 1 or more
	 * attributes to apply, but typically there are between 1 and 3.
	 */

    /* Special case of <ESC>[m */
    if (buf[0] == '\0')
        set_ansi_attribute(0, NULL);

    for (endptr = numstart = buf; *endptr != '\0'; numstart = endptr + 1) {
        ansi_attribute = strtol(numstart, &endptr, 10);
        if (!set_ansi_attribute(ansi_attribute, &endptr))
            break;
        if (numstart == endptr)
            set_ansi_attribute(0, NULL); /* [m treated as [0m */
    }
}

static void __attribute__ ((__noreturn__)) do_exit(int status)
{
	if (curses_started) {
		endwin();
		curs_set(1);
	}
	exit(status);
}

/* signal handler */
static void die(int notused __attribute__ ((__unused__)))
{
	do_exit(EXIT_SUCCESS);
}

static void winch_handler(int notused __attribute__ ((__unused__)))
{
	screen_size_changed = true;
}

static char env_col_buf[24];
static char env_row_buf[24];
static int incoming_cols;
static int incoming_rows;

static void get_terminal_size(void)
{
	struct winsize w;
	if (!incoming_cols) {
		/* have we checked COLUMNS? */
		const char *s = getenv("COLUMNS");
		incoming_cols = -1;
		if (s && *s) {
			long t;
			char *endptr;
			t = strtol(s, &endptr, 0);
			if (!*endptr && 0 < t)
				incoming_cols = t;
			width = incoming_cols;
			snprintf(env_col_buf, sizeof env_col_buf, "COLUMNS=%ld",
				 width);
			putenv(env_col_buf);
		}
	}
	if (!incoming_rows) {
		/* have we checked LINES? */
		const char *s = getenv("LINES");
		incoming_rows = -1;
		if (s && *s) {
			long t;
			char *endptr;
			t = strtol(s, &endptr, 0);
			if (!*endptr && 0 < t)
				incoming_rows = t;
			height = incoming_rows;
			snprintf(env_row_buf, sizeof env_row_buf, "LINES=%ld",
				 height);
			putenv(env_row_buf);
		}
	}
	if (ioctl(STDERR_FILENO, TIOCGWINSZ, &w) == 0) {
		if (incoming_cols < 0 || incoming_rows < 0) {
			if (incoming_rows < 0 && w.ws_row > 0) {
				height = w.ws_row;
				snprintf(env_row_buf, sizeof env_row_buf,
					 "LINES=%ld", height);
				putenv(env_row_buf);
			}
			if (incoming_cols < 0 && w.ws_col > 0) {
				width = w.ws_col;
				snprintf(env_col_buf, sizeof env_col_buf,
					 "COLUMNS=%ld", width);
				putenv(env_col_buf);
			}
		}
	}
}

/* get current time in usec */
typedef unsigned long long watch_usec_t;
#define USECS_PER_SEC (1000000ull)
static inline watch_usec_t get_time_usec(void)
{
	struct timeval now;
#if defined(HAVE_CLOCK_GETTIME) && defined(_POSIX_TIMERS)
	struct timespec ts;
	if (0 > clock_gettime(CLOCK_MONOTONIC, &ts))
		xerr(EXIT_FAILURE, "Cannot get monotonic clock");
	TIMESPEC_TO_TIMEVAL(&now, &ts);
#else
	gettimeofday(&now, NULL);
#endif /* HAVE_CLOCK_GETTIME */
	return USECS_PER_SEC * now.tv_sec + now.tv_usec;
}

#ifdef WITH_WATCH8BIT
// Read a multi-byte character from a byte-oriented stream. libc only provides
// getc() and getwc().
// TODO: as long as process_ansi() is ok with wchar, this can be dropped
// completely
// TODO: -> strutils
static wint_t getmb(FILE *s)
{
	unsigned char i[MB_CUR_MAX];
	uf8 byte = 0;
	int c;
	wchar_t rval;
	mbstate_t mbstate;

	memset(&mbstate, 0, sizeof(mbstate));

	while ((c = getc(s)) != EOF) {
		i[byte] = c;
		if (mbrtowc(&rval, (char *)i+byte, 1, &mbstate) <= 1)
			/* legal conversion, may be L'\0' */
			return rval;
		if (++byte == MB_CUR_MAX) {
			while (byte)
				/* at least *try* to fix up */
				ungetc(i[--byte], s);
			errno = EILSEQ;
			break;
		}
	}

	return WEOF;
}
#endif	/* WITH_WATCH8BIT */

#ifdef WITH_WATCH8BIT
// Returns column width of a multi-byte string. libc only provides strlen() and
// wcswidth().
// s is valid, '\0'-term.
// pwcs is optional (NULL or valid) output param., will be L'\0'-term., no
// additional cost in receiving it, caller free()s.
// Value returned doesn't count the L'\0', can be 0. Error => -1 and nothing is
// returned in *pwcs.
// TODO -> strutils
static int mbswidth(const char *restrict s, wchar_t *restrict *const restrict pwcs)
{
	size_t wclen = mbstowcs(NULL, s, 0);  // wclen doesn't incl. L'\0'
	if (wclen == (size_t)-1) {
		errno = EILSEQ;
		return -1;
	}
	++wclen;  // ok, it's !=SIZE_MAX
	if ((uintmax_t)wclen * sizeof(wchar_t) / sizeof(wchar_t) != wclen) {
		errno = EOVERFLOW;
		return -1;
	}

	wchar_t *wcs = malloc(wclen * sizeof(*wcs));
	if (! wcs)
		return -1;
	mbstowcs(wcs, s, wclen);

	// wcswidth() is useless. POSIX 2001 doesn't say what it does when the width
	// is > INT_MAX. May be some UB.
	int ret = 0;
	int curwid;
	for (--wclen; wclen; --wclen) {
		curwid = wcwidth(wcs[wclen-1]);
		if (curwid == -1) {
			free(wcs);
			errno = EILSEQ;
			return -1;
		}
		if (INT_MAX - ret < curwid) {
			free(wcs);
			errno = EOVERFLOW;
			return -1;
		}
		ret += curwid;
	}

	if (pwcs) {
		*pwcs = wcs;
	} else free(wcs);
	return ret;
}
#endif

#ifdef WITH_WATCH8BIT
static void output_header(const wchar_t *wcommand, int wcommand_characters, double interval)
#else
static void output_header(const char *command, int command_characters, double interval)
#endif
{
	static char *lheader;
	static int lheader_len = 0;  // tell-tale
#ifdef WITH_WATCH8BIT
	static wchar_t *wlheader;
	static int wlheader_wid;
#endif

	static char rheader[256+128];  // hostname and timestamp
	static int rheader_lenmid = 0;  // just before timestamp
	int rheader_len;
#ifdef WITH_WATCH8BIT
	wchar_t *wrheader;
	int wrheader_wid;

	static int wcommandwid;
	static sf8 ellipsiswid;
#endif

	if (! lheader_len) {
		// my glibc says HOST_NAME_MAX is 64, but as soon as it updates it to be
		// at least POSIX.1-2001-compliant, it will be one of: 255, very large,
		// unspecified
		if (gethostname(rheader, 256))
			rheader[0] = '\0';
		rheader[255] = '\0';
		rheader_lenmid = strlen(rheader);
		rheader[rheader_lenmid++] = ':';
		rheader[rheader_lenmid++] = ' ';

		// never freed for !WATCH8BIT
		lheader_len = asprintf(&lheader, _("Every %.1fs: "), interval);
		if (lheader_len == -1)
			xerr(EXIT_FAILURE, "cannot allocate memory");  // TODO: gettext
#ifdef WITH_WATCH8BIT
		// never freed
		wlheader_wid = mbswidth(lheader, &wlheader);
		if (wlheader_wid == -1) {
			wlheader = L"";
			wlheader_wid = 0;
		}
		free(lheader);

		wcommandwid = wcswidth(wcommand, wcommand_characters);
		ellipsiswid = wcwidth(L'\u2026');
#endif
	}

	// TODO: a gettext string for rheader no longer used
	const time_t t = time(NULL);
	rheader_len = rheader_lenmid + strftime(rheader+rheader_lenmid, sizeof(rheader)-rheader_lenmid, "%X", localtime(&t));
	if (rheader_len == rheader_lenmid)
		rheader[rheader_len] = '\0';
#ifdef WITH_WATCH8BIT
	wrheader_wid = mbswidth(rheader, &wrheader);
	if (wrheader_wid == -1) {
		wrheader = xmalloc(sizeof(*wrheader));
		wrheader[0] = L'\0';
		wrheader_wid = 0;
	}
#endif

	/* left justify interval and command, right justify hostname and time,
	 * clipping all to fit window width
	 *
	 * the rules:
	 *   width < rhlen : print nothing
	 *   width < rhlen + hlen + 1: print hostname, ts
	 *   width = rhlen + hlen + 1: print header, hostname, ts
	 *   width < rhlen + hlen + 4: print header, ..., hostname, ts
	 *   width < rhlen + hlen + wcommand_columns: print header,
	 *                           truncated wcommand, ..., hostname, ts
	 *   width > "": print header, wcomand, hostname, ts
	 * this is slightly different from how it used to be */

#ifdef WITH_WATCH8BIT
	if (width >= wrheader_wid) {
		mvaddwstr(0, width - wrheader_wid, wrheader);
		const int avail4cmd = width - wlheader_wid - wrheader_wid;
		if (avail4cmd >= 0) {
			mvaddwstr(0, 0, wlheader);
			int cmdwid = wcommandwid;
			if (cmdwid == -1) {
				wcommand = L"";
				cmdwid = 0;
			}
			// All of cmd fits, 1 = delimiting space
			if (avail4cmd >= cmdwid + 1)
				addwstr(wcommand);
			// Else print truncated cmd (to 0 chars, possibly) + ellipsis. If
			// there's too little space even for the ellipsis, print nothing.
			else if (avail4cmd >= ellipsiswid + 1) {
				assert(wcommand_characters);
				// from the back
				do cmdwid = wcswidth(wcommand, --wcommand_characters);
				while (cmdwid == -1 || cmdwid > avail4cmd-ellipsiswid-1);
					addnwstr(wcommand, wcommand_characters);
				addwstr(L"\u2026");
			}
		}
	}
	free(wrheader);
#else
	if (width >= rheader_len) {
		mvaddstr(0, width - rheader_len, rheader);
		const int avail4cmd = width - lheader_len - rheader_len;
		if (avail4cmd >= 0) {
			mvaddstr(0, 0, lheader);
			if (avail4cmd >= command_characters + 1)
				addstr(command);
			else if (avail4cmd >= 3 + 1) {
				addnstr(command, avail4cmd - 3 - 1);
				addstr("...");
			}
		}
	}
#endif
}

// When first_screen, returns false. Otherwise, when WATCH_ALL_DIFF is false,
// return value is unspecified. Otherwise, returns true <==> the character at
// (y, x) changed.
// In the future, to do things like #233, this may be well extended with an attr
// parameter and an option to only change that, not the base char. The change
// detection routine may have its uses as a separate function as well.
static bool display_char(int y, int x, Xint c) {
	assert(c != XEOF);
	bool changed, diff;
	move(y, x);

	if (first_screen || ! (flags & WATCH_ALL_DIFF))
		diff = changed = false;
	else {
		// TODO: when cwid>1, check the diff of the second and other "slots" of
		// the char. With COLUMNS=40:
		// echo -e 'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaZ日本\nb' > /tmp/del
		// echo -e 'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaZZ日本\nb' > /tmp/del
#ifdef WITH_WATCH8BIT
		cchar_t oldc;
		in_wch(&oldc);
		changed = (wchar_t)c != oldc.chars[0];
		if (flags & WATCH_DIFF)
			diff = changed || (flags&WATCH_CUMUL && oldc.attr&A_STANDOUT);
		else diff = false;
#else
		chtype oldc = inch();
		changed = (unsigned char)c != (oldc & A_CHARTEXT);
		if (flags & WATCH_DIFF)
			diff = changed || (flags&WATCH_CUMUL && oldc&A_STANDOUT);
		else diff = false;
#endif
	}

	if (diff)
		attron(A_STANDOUT);
#ifdef WITH_WATCH8BIT
	wchar_t c2 = c;
	addnwstr(&c2, 1);
#else
	addch(c);
#endif
	if (diff)
		attroff(A_STANDOUT);

	return changed;
}

static inline void find_eol(FILE *p)
{
	Xint c;
	do c = Xgetc(p);
	while (c != XEOF && c != XL('\n'));
}

static inline bool my_clrtoeol(int y, int x)
{
	if (flags & WATCH_ALL_DIFF) {
		bool screen_changed = false;
		while (x < width)
			screen_changed = display_char(y, x++, XL(' ')) || screen_changed;
		return screen_changed;
	}

	// make sure color is preserved
	move(y, x);
	clrtoeol();  // faster, presumably
	return false;
}

static inline bool my_clrtobot(int y, int x)
{
	if (flags & WATCH_ALL_DIFF) {
		bool screen_changed = false;
		while (y < height) {
			while (x < width)
				screen_changed = display_char(y, x++, XL(' ')) || screen_changed;
			x = 0;
			++y;
		}
		return screen_changed;
	}

	// make sure color is preserved
	move(y, x);
	clrtobot();  // faster, presumably
	return false;
}

// When first_screen initially, returns false. Otherwise, when WATCH_ALL_DIFF is
// false, return value is unspecified. Otherwise, returns true <==> the screen
// changed.
// Make sure not to leak system resources (incl. fds, processes). Suggesting
// -D_XOPEN_SOURCE=600 and an EINTR loop around every fclose() as well.
static bool run_command(const char *command, char *const *restrict command_argv)
{
	int pipefd[2], status;
	pid_t child;

	if (pipe(pipefd) < 0)
		xerr(7, _("unable to create IPC pipes"));

	/* flush stdout and stderr, since we're about to do fd stuff */
	fflush(stdout);
	fflush(stderr);

	child = fork();
	if (child < 0) {		/* fork error */
		xerr(2, _("unable to fork process"));
	} else if (child == 0) {	/* in child */
		// stdout/err can't be used here. Avoid xerr(), close_stdout(), ...
		fclose(stdout);  // so as not to confuse _Exit()
		fclose(stderr);  // so as not to confuse _Exit()
		/* child doesn't need output side of pipe */
		while (close(pipefd[0]) == -1 && errno == EINTR) ;
		/* replace stdout with pipe input */
		while (dup2(pipefd[1], 1) == -1 && errno == EINTR) ;
		/* once duped, the write fd isn't needed */
		while (close(pipefd[1]) == -1 && errno == EINTR) ;
		/* stderr should default to stdout */
		while (dup2(1, 2) == -1 && errno == EINTR) ;
		// TODO: 0 untouched. Is that intentional? I suppose the application
		// might conclude it's run interactively (see ps). And hang if it
		// should wait for input (watch 'read A; echo $A').

		if (flags & WATCH_EXEC) {	/* pass command to exec instead of system */
			execvp(command_argv[0], command_argv);
			const char *const errmsg = strerror(errno);
			(void)!write(2, command_argv[0], strlen(command_argv[0]));
			// TODO: gettext?
			(void)!write(2, ": ", 2);
			(void)!write(2, errmsg, strlen(errmsg));
			_Exit(4);
		}
		status = system(command);
		/* propagate command exit status as child exit status */
		/* child exits nonzero if command does */
		// error msg is provided by sh
		_Exit(WIFEXITED(status) ? WEXITSTATUS(status) : EXIT_FAILURE);
	}
	/* otherwise, we're in parent */

	/* close write side of pipe */
	while (close(pipefd[1]) == -1 && errno == EINTR) ;
	FILE *p;
	if ((p = fdopen(pipefd[0], "r")) == NULL)
		xerr(5, _("fdopen"));
	setvbuf(p, NULL, _IOFBF, BUFSIZ);  // We'll getc() from it. A lot.

	Xint c, carry = XEOF;
	int cwid, y, x;  // cwid = character width in terminal columns
	bool screen_changed = false;

	for (y = show_title; y < height; ++y) {
		x = 0;
		while (true) {
			// x is where the next char will be put. When x==width only
			// characters with wcwidth()==0 are output. Used, e.g., for
			// codepoints which modify the preceding character and swallowing a
			// newline / a color sequence / ... after a printable character in
			// the rightmost column.
			assert(x <= width);
			assert(x == 0 || carry == XEOF);

			if (carry != XEOF) {
				c = carry;
				carry = XEOF;
			}
			else c = Xgetc(p);
			assert(carry == XEOF);

			if (c == XEOF) {
				screen_changed = my_clrtobot(y, x) || screen_changed;
				y = height - 1;
				break;
			}
			if (c == XL('\n')) {
				screen_changed = my_clrtoeol(y, x) || screen_changed;
				break;
			}
			if (c == XL('\033')) {
				process_ansi(p);
				continue;
			}
			if (c == XL('\a')) {
				beep();
				continue;
			}
			if (c == XL('\t'))  // not is(w)print()
				// one space is enough to consider a \t printed, if there're no
				// more columns
				cwid = 1;
			else {
#ifdef WITH_WATCH8BIT
				// There used to be (! iswprint(c) && c < 128) because of Debian
				// #240989. Presumably because glibc of the time didn't
				// recognize ä, ö, ü, Π, ά, λ, ς, ... as printable. Today,
				// iswprint() in glibc works as expected and the "c<128" is
				// letting all non-printables >=128 get through.
				if (! iswprint(c))
					continue;
				cwid = wcwidth(c);
				assert(cwid >= 0 && cwid <= 2);
#else
				if (! isprint(c))
					continue;
				cwid = 1;
#endif
			}

			// now c is something printable
			// if it doesn't fit
			if (cwid > width-x) {
				assert(cwid > 0 && cwid <= 2);
				assert(width-x <= 1);
				if (line_wrap)
					carry = c;
				else {
					find_eol(p);
					reset_ansi();
					set_ansi_attribute(-1, NULL);
				}
				screen_changed = my_clrtoeol(y, x) || screen_changed;
				break;
			}

			// it fits, print it
			if (c == XL('\t')) {
				// TODO: diff of tabs only highlights the first space
				do screen_changed = display_char(y, x++, XL(' ')) || screen_changed;
				while (x % TAB_WIDTH && x < width);
			}
			else {
				screen_changed = display_char(y, x, c) || screen_changed;
				x += cwid;
			}
		}
	}

	fclose(p);

	/* harvest child process and get status, propagated from command */
	while (waitpid(child, &status, 0) == -1) {
		if (errno != EINTR)
			xerr(8, _("waitpid"));
	}

	/* if child process exited in error, beep if option_beep is set */
	if ((!WIFEXITED(status) || WEXITSTATUS(status))) {
		if (flags & WATCH_BEEP)
			beep();
		if (flags & WATCH_ERREXIT) {
			int stdinfl = fcntl(0, F_GETFL);
			if (stdinfl >= 0 && fcntl(0, F_SETFL, stdinfl|O_NONBLOCK) >= 0) {
				while (getchar() != EOF) ;
				fcntl(0, F_SETFL, stdinfl);
			}

			// TODO: add a few spaces to the end of the string to separate it
			// from the cmd output that may already be on the line
			mvaddstr(height - 1, 0,
			    _("command exit with a non-zero status, press a key to exit"));
			refresh();
			getchar();
			do_exit(8);
		}
	}

	return screen_changed;
}

int main(int argc, char *argv[])
{
	int optc;
	char *command;
	int command_length;	/* not including final \0 */
	watch_usec_t last_run = 0;
	/* next loop time in us, used for precise time keeping only */
	watch_usec_t next_loop = 0;
	double interval = 2;
	int max_cycles = 0;
	int cycle_count = 1;
	bool scr_contents_chg;

#ifdef WITH_COLORWATCH
        flags |= WATCH_COLOR;
#endif /* WITH_COLORWATCH */

	static const struct option longopts[] = {
		{"color", no_argument, 0, 'c'},
		{"no-color", no_argument, 0, 'C'},
		{"differences", optional_argument, 0, 'd'},
		{"help", no_argument, 0, 'h'},
		{"interval", required_argument, 0, 'n'},
		{"beep", no_argument, 0, 'b'},
		{"errexit", no_argument, 0, 'e'},
		{"chgexit", no_argument, 0, 'g'},
		{"equexit", required_argument, 0, 'q'},
		{"exec", no_argument, 0, 'x'},
		{"precise", no_argument, 0, 'p'},
		{"no-rerun", no_argument, 0, 'r'},
		{"no-title", no_argument, 0, 't'},
		{"no-wrap", no_argument, 0, 'w'},
		{"version", no_argument, 0, 'v'},
		{0, 0, 0, 0}
	};

#ifdef HAVE_PROGRAM_INVOCATION_NAME
	program_invocation_name = program_invocation_short_name;
#endif
	// TODO: when !WATCH8BIT, setlocale() should be omitted or initd as "C".
	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
	atexit(close_stdout);

	const char *const interval_string = getenv("WATCH_INTERVAL");
	if(interval_string != NULL)
		interval = strtod_nol_or_err(interval_string, _("Could not parse interval from WATCH_INTERVAL"));

	while ((optc =
		getopt_long(argc, argv, "+bCced::ghq:n:prtwvx", longopts, (int *)0))
	       != EOF) {
		switch (optc) {
		case 'b':
			flags |= WATCH_BEEP;
			break;
		case 'c':
			flags |= WATCH_COLOR;
			break;
		case 'C':
			flags &= ~WATCH_COLOR;
			break;
		case 'd':
			flags |= WATCH_DIFF;
			if (optarg)
				flags |= WATCH_CUMUL;
			break;
		case 'e':
			flags |= WATCH_ERREXIT;
			break;
		case 'g':
			flags |= WATCH_CHGEXIT;
			break;
		case 'q':
			// TODO: strtod isn't int
			max_cycles = strtod_nol_or_err(optarg, _("failed to parse argument"));
			if (max_cycles >= 1)
				flags |= WATCH_EQUEXIT;
			break;
		case 'r':
			flags |= WATCH_NORERUN;
			break;
		case 't':
			show_title = 0;
			break;
		case 'w':
			line_wrap = false;
			break;
		case 'x':
			flags |= WATCH_EXEC;
			break;
		case 'n':
			interval = strtod_nol_or_err(optarg, _("failed to parse argument"));
			break;
		case 'p':
			precise_timekeeping = true;
			break;
		case 'h':
			usage(stdout);
			break;
		case 'v':
			printf(PROCPS_NG_VERSION);
			return EXIT_SUCCESS;
		default:
			usage(stderr);
			break;
		}
	}

	if (optind >= argc)
		usage(stderr);

	/* save for later */
	char *const *const command_argv = argv + optind;

	command_length = strlen(argv[optind]);
	command = xmalloc(command_length+1);
	memcpy(command, argv[optind++], command_length+1);
	for (; optind < argc; optind++) {
		int s = strlen(argv[optind]);
		/* space and \0 */
		command = xrealloc(command, command_length + s + 2);
		command[command_length] = ' ';
		memcpy(command+command_length+1, argv[optind], s);
		/* space then string length */
		command_length += 1 + s;
		command[command_length] = '\0';
	}

#ifdef WITH_WATCH8BIT
	wchar_t *wcommand;
	/* convert to wide for printing purposes */
	/*mbstowcs(NULL, NULL, 0); */
	/* not including final \0 */
	int wcommand_characters = mbstowcs(NULL, command, 0);
	if (wcommand_characters < 0) {
		fprintf(stderr, _("unicode handling error\n"));
		return EXIT_FAILURE;
	}
	wcommand = (wchar_t *)malloc((wcommand_characters + 1) * sizeof(*wcommand));
	if (wcommand == NULL) {
		fprintf(stderr, _("unicode handling error (malloc)\n"));
		return EXIT_FAILURE;
	}
	mbstowcs(wcommand, command, wcommand_characters + 1);
#endif	/* WITH_WATCH8BIT */

	get_terminal_size();

	/* Catch keyboard interrupts so we can put tty back in a sane state.  */
	signal(SIGINT, die);
	signal(SIGTERM, die);
	signal(SIGHUP, die);
	signal(SIGWINCH, winch_handler);

	/* Set up tty for curses use.  */
	curses_started = true;
	initscr();
	// TODO: we want color by default and an option to turn it off
	if (flags & WATCH_COLOR) {
		if (has_colors()) {
			start_color();
			use_default_colors();
			init_ansi_colors();
		}
		else flags &= ~WATCH_COLOR;
	}
	nonl();
	noecho();
	cbreak();
	curs_set(0);

	tzset();
	if (interval < 0.1)
		interval = 0.1;
	if (interval > UINT_MAX)
		interval = UINT_MAX;
	if (precise_timekeeping)
		next_loop = get_time_usec();

	while (1) {
		reset_ansi();
		set_ansi_attribute(-1, NULL);

		if (screen_size_changed) {
			screen_size_changed = false;  // "atomic" test-and-set
			get_terminal_size();
			resizeterm(height, width);
			clear();
			/* redrawwin(stdscr); */
			first_screen = true;
		}

		if (show_title)
#ifdef WITH_WATCH8BIT
			output_header(wcommand, wcommand_characters, interval);
#else
			output_header(command, command_length, interval);
#endif	/* WITH_WATCH8BIT */

		if (!(flags & WATCH_NORERUN) ||
		        get_time_usec() - last_run > interval * USECS_PER_SEC) {
			last_run = get_time_usec();
			scr_contents_chg = run_command(command, command_argv);

			// [BUG] When screen resizes, its contents change, but not
			// necessarily because cmd output's changed. It may have, but that
			// event is lost. Prevents cycle_count from soaring while resizing.
			if (! first_screen) {
				if (flags & WATCH_CHGEXIT && scr_contents_chg)
					break;
				if (flags & WATCH_EQUEXIT) {
					if (scr_contents_chg)
						cycle_count = 1;
					else {
						if (cycle_count == max_cycles)
							break;
						++cycle_count;
					}
				}
			}
			first_screen = false;
		}

		refresh();

		if (precise_timekeeping) {
			watch_usec_t cur_time = get_time_usec();
			next_loop += USECS_PER_SEC * interval;
			if (cur_time < next_loop)
				usleep(next_loop - cur_time);
		}
		else if (interval < UINT_MAX / USECS_PER_SEC)
			usleep(interval * USECS_PER_SEC);
		else sleep(interval);
	}

	do_exit(EXIT_SUCCESS);
}
