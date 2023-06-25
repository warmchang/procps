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
#define WATCH_DIFF	(1 << 0)
#define WATCH_CUMUL	(1 << 1)
#define WATCH_EXEC	(1 << 2)
#define WATCH_BEEP	(1 << 3)
#define WATCH_COLOR	(1 << 4)
#define WATCH_ERREXIT	(1 << 5)
#define WATCH_CHGEXIT	(1 << 6)
#define WATCH_EQUEXIT	(1 << 7)
#define WATCH_NORERUN	(1 << 8)

#ifdef WITH_WATCH8BIT
#define XL(c) L ## c
#define XEOF WEOF
#define Xint wint_t
#define Xgetc(stream) my_getwc(stream)
#else
#define XL(c) c
#define XEOF EOF
#define Xint int
#define Xgetc(stream) getc(stream)
#endif

static bool curses_started = false;
static long height = 24, width = 80;
static bool screen_size_changed = false;
static bool first_screen = true;
static uf8 show_title = 2;	/* number of lines used, 2 or 0 */
static bool precise_timekeeping = false;
static bool line_wrap = true;

#define min(x,y) ((x) > (y) ? (y) : (x))
#define MAX_ANSIBUF 100

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
static watch_usec_t get_time_usec(void)
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
/* read a multi-byte character from a byte-oriented stream */
static wint_t my_getwc(FILE *s)
{
	assert(MB_CUR_MAX < 256);
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
static void output_header(const wchar_t *wcommand, int wcommand_characters, double interval)
#else
static void output_header(const char *command, int command_characters, double interval)
#endif	/* WITH_WATCH8BIT */
{
	static uf8 max_host_name_len = 0;
	if (max_host_name_len == 0) {
		long n = sysconf(_SC_HOST_NAME_MAX);
		max_host_name_len = n>0 && n<256 ? n : 255;
	}
	char hostname[max_host_name_len + 1];
	if (gethostname(hostname, sizeof(hostname)))
		hostname[0] = '\0';
	hostname[max_host_name_len] = '\0';

	const time_t t = time(NULL);
	char *header;
	char *right_header;

	/*
	 * left justify interval and command, right justify hostname and time,
	 * clipping all to fit window width
	 */
	// TODO: ENOMEM not handled, just put on stack
	// TODO: left header never changes, make static
	int hlen = asprintf(&header, _("Every %.1fs: "), interval);
	// TODO: ctime() isn't locale-aware, replace with strftime()
	int rhlen = asprintf(&right_header, _("%s: %s"), hostname, ctime(&t));

	/*
	 * the rules:
	 *   width < rhlen : print nothing
	 *   width < rhlen + hlen + 1: print hostname, ts
	 *   width = rhlen + hlen + 1: print header, hostname, ts
	 *   width < rhlen + hlen + 4: print header, ..., hostname, ts
	 *   width < rhlen + hlen + wcommand_columns: print header,
	 *                           truncated wcommand, ..., hostname, ts
	 *   width > "": print header, wcomand, hostname, ts
	 * this is slightly different from how it used to be
	 */
	if (width < rhlen) {
		free(header);
		free(right_header);
		return;
	}

	if (rhlen + hlen + 1 <= width) {
		mvaddstr(0, 0, header);
		if (rhlen + hlen + 2 <= width) {
			if (width < rhlen + hlen + 4) {
				mvaddstr(0, width - rhlen - 4, "... ");
			} else {
#ifdef WITH_WATCH8BIT
	            int command_columns = wcswidth(wcommand, -1);
				if (width < rhlen + hlen + command_columns) {
					/* print truncated */
					int available = width - rhlen - hlen;
					int wcomm_len = wcommand_characters;
					while (available - 4 < command_columns) {
						wcomm_len--;
						command_columns = wcswidth(wcommand, wcomm_len);
					}
					mvaddnwstr(0, hlen, wcommand, wcomm_len);
					mvaddstr(0, width - rhlen - 4, "... ");
				} else {
					mvaddwstr(0, hlen, wcommand);
				}
#else
				if (width < rhlen + hlen + command_characters) {
					/* print truncated */
					mvaddnstr(0, hlen, command, width - rhlen - hlen - 4);
					mvaddstr(0, width - rhlen - 4, "... ");
				} else {
					mvaddnstr(0, hlen, command, width - rhlen - hlen);
                }
#endif	/* WITH_WATCH8BIT */
			}
		}
	}
	mvaddstr(0, width - rhlen + 1, right_header);
	free(header);
	free(right_header);
	return;
}

static void find_eol(FILE *p)
{
	Xint c;
	do c = Xgetc(p);
	while (c != XEOF
	    && c != XL('\n'));
}

static bool run_command(const char *command, char *const *restrict command_argv)
{
	int pipefd[2], status;
	pid_t child;

	/* allocate pipes */
	if (pipe(pipefd) < 0)
		xerr(7, _("unable to create IPC pipes"));

	/* flush stdout and stderr, since we're about to do fd stuff */
	fflush(stdout);
	fflush(stderr);

	/* fork to prepare to run command */
	child = fork();
	if (child < 0) {		/* fork error */
		xerr(2, _("unable to fork process"));
	} else if (child == 0) {	/* in child */
		close(pipefd[0]);		/* child doesn't need read side of pipe */
		close(1);			/* prepare to replace stdout with pipe */
		if (dup2(pipefd[1], 1) < 0) {	/* replace stdout with write side of pipe */
			xerr(3, _("dup2 failed"));
		}
		close(pipefd[1]);		/* once duped, the write fd isn't needed */
		dup2(1, 2);			/* stderr should default to stdout */

		// 0 untouched => application may think it's run interactively (see,
		// e.g., ps). Intentional?
		// TODO: using stdout/err (xerr(), exit()) is probably UB at this point
		if (flags & WATCH_EXEC) {	/* pass command to exec instead of system */
			if (execvp(command_argv[0], command_argv) == -1) {
				xerr(4, _("unable to execute '%s'"),
				     command_argv[0]);
			}
		} else {
			status = system(command);	/* watch manpage promises sh quoting */
			/* propagate command exit status as child exit status */
			if (!WIFEXITED(status)) {	/* child exits nonzero if command does */
				exit(EXIT_FAILURE);
			} else {
				exit(WEXITSTATUS(status));
			}
		}
	}
	/* otherwise, we're in parent */
	close(pipefd[1]);	/* close write side of pipe */

	FILE *p;
	if ((p = fdopen(pipefd[0], "r")) == NULL)
		xerr(5, _("fdopen"));
	// TODO: make sure p is buffered

	Xint c = XEOF, carry = XEOF;
	int x, y;
	int cwid;  // width in term columns
	bool inside_tab = false, inside_trailer = false;
	bool exit_early = false;
	bool diff = false;
	int buffer_size = 0, unchanged_buffer = 0;

	for (y = show_title; y < height; ++y) {
		// For x==width, only characters with wcwidth()==0 are allowed to be
		// output. Used, e.g., for codepoints which modify the preceding
		// character and swallowing a newline beyond last column of term.
		// TODO: bad cycle flow organization
		for (x = 0; x <= width; ++x) {
			// Inside_trailer overrides everything, preserves carry. Fill rest
			// of line with spaces. clrtoeol()/clrtobot() can't be used, because
			// diff detection in the blank space must be preserved (same idea
			// applies to the tab).
			if (inside_trailer) {
				if (x == width) {
					inside_trailer = false;
					break;
				}
				c = XL(' ');
			}
			else {
				// Carry first, then inside_tab, then new read. Carry can
				// contain a ' ' for a tab (when \t was beyond the end of the
				// previous term line) and that space must be consumed.
				if (carry != XEOF) {
					c = carry;
					carry = XEOF;
				}
				else if (inside_tab)
					c = XL(' ');
				else do c = Xgetc(p);
				while (c != XEOF
#ifdef WITH_WATCH8BIT
				    && ! iswprint(c)
				    && c < 128  // TODO: not portable (wint_t may be signed)
				    && wcwidth(c) <= 0
#else
				    && ! isprint(c)
#endif
				    && c != XL('\n')
				    && c != XL('\a')
				    && c != XL('\t')
				    && c != XL('\033'));

				if (c == XL('\n') || c == XEOF) {
					assert(carry == XEOF);
					assert(! inside_tab);
					--x;
					inside_trailer = true;
					continue;
				}
				else if (c == XL('\t')) {
					inside_tab = true;
					c = XL(' ');
					// diff of tabs only highlights the first space of it.
					// Intentional?
				}
				else if (c == XL('\033')) {
					process_ansi(p);
					--x;
					continue;
				}
				else if (c == XL('\a')) {
					beep();
					--x;
					continue;
				}
				assert(! inside_tab || c == XL(' '));
				// Here c is a non-special, printable char. It may have wcwidth
				// 0. Assumption: c doesn't change through the rest of the x-
				// and y-cycle.

#ifdef WITH_WATCH8BIT
				cwid = wcwidth(c);
				assert(cwid >= 0 && cwid <= 2);
#else
				cwid = 1;
#endif
				if (cwid > width-x) {
					assert(carry == XEOF);
					assert(inside_tab || ! inside_tab);
					carry = c;
					assert(cwid > 0);
					inside_trailer = true;
					--x;
					if (! line_wrap)
						find_eol(p);
					continue;
				}
			}

			move(y, x);

			// TODO: when cwid>1, check the diff of the second "slot" of the
			// char. Verify (watch -d) on COLUMNS=40:
			// echo -e 'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaZ日本\nb' > /tmp/del
			// echo -e 'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaZZ日本\nb' > /tmp/del
			if (!first_screen && !exit_early && (flags & WATCH_CHGEXIT)) {
#ifdef WITH_WATCH8BIT
				cchar_t oldc;
				in_wch(&oldc);
				exit_early = (wchar_t) c != oldc.chars[0];
#else
				chtype oldch = inch();
				unsigned char oldc = oldch & A_CHARTEXT;
				exit_early = (unsigned char)c != oldc;
#endif
			}

			if (!first_screen && !exit_early && (flags & WATCH_EQUEXIT)) {
				buffer_size++;
#ifdef WITH_WATCH8BIT
				cchar_t oldc;
				in_wch(&oldc);
				if ((wchar_t) c == oldc.chars[0])
					unchanged_buffer++;
#else
				chtype oldch = inch();
				unsigned char oldc = oldch & A_CHARTEXT;
				if ((unsigned char)c == oldc)
					unchanged_buffer++;
#endif
			}

			if (flags & WATCH_DIFF) {
#ifdef WITH_WATCH8BIT
				cchar_t oldc;
				in_wch(&oldc);
				diff = !first_screen
				    && ((wchar_t) c != oldc.chars[0]
				        ||
				        ((flags & WATCH_CUMUL)
				            && (oldc.attr & A_ATTRIBUTES)));
#else
				chtype oldch = inch();
				unsigned char oldc = oldch & A_CHARTEXT;
				diff = !first_screen
				    && ((unsigned char)c != oldc
				        ||
				        ((flags & WATCH_CUMUL)
				            && (oldch & A_ATTRIBUTES)));
#endif
			}

			if (diff)
				standout();
#ifdef WITH_WATCH8BIT
			wchar_t c2 = c;
			addnwstr(&c2, 1);
#else
			addch(c);
#endif
			if (diff)
				standend();

			if (! inside_trailer) {
				if (inside_tab && ((x+1)%8 == 0 || x == width-1))
					inside_tab = false;

				x += cwid-1;
			}
		}

		if (! line_wrap) {
			carry = XEOF;
			inside_tab = false;
			reset_ansi();
			set_ansi_attribute(-1, NULL);
		}
	}
	// TODO: lines past max(last line with output, last line with output in
	// previous command run) ca be done with clrtobot()

	fclose(p);

	// TODO: waitpid() getting interrupted by window size changes looks like
	// a good potential source of children that'll never be waited for. Confirm.
	/* harvest child process and get status, propagated from command */
	if (waitpid(child, &status, 0) < 0)
		xerr(8, _("waitpid"));

	/* if child process exited in error, beep if option_beep is set */
	if ((!WIFEXITED(status) || WEXITSTATUS(status))) {
		if (flags & WATCH_BEEP)
			beep();
		if (flags & WATCH_ERREXIT) {
			// TODO: add a few spaces to the end of the string to separate it
			// from the cmd output that may be on that line
			mvaddstr(height - 1, 0,
			    _("command exit with a non-zero status, press a key to exit"));
			refresh();
			getchar();
			do_exit(8);
		}
	}

	if (unchanged_buffer == buffer_size && (flags & WATCH_EQUEXIT))
		exit_early = true;

	first_screen = false;
	refresh();
	return exit_early;
}

int main(int argc, char *argv[])
{
	int optc;
	double interval = 2;
	int max_cycles = 1;
	int cycle_count = 0;
	char *interval_string;
	char *command;
	char **command_argv;
	int command_length = 0;	/* not including final \0 */
	int command_exit = 0;
	watch_usec_t last_run = 0;
	watch_usec_t next_loop = 0;	/* next loop time in us, used for precise time
	                           	 * keeping only */
#ifdef WITH_WATCH8BIT
	wchar_t *wcommand = NULL;
	int wcommand_characters = 0;	/* not including final \0 */
#endif	/* WITH_WATCH8BIT */

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
	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
	atexit(close_stdout);

	interval_string = getenv("WATCH_INTERVAL");
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
			flags |= WATCH_EQUEXIT;
			max_cycles = strtod_nol_or_err(optarg, _("failed to parse argument"));
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
	command_argv = &(argv[optind]);

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
	/* convert to wide for printing purposes */
	/*mbstowcs(NULL, NULL, 0); */
	wcommand_characters = mbstowcs(NULL, command, 0);
	if (wcommand_characters < 0) {
		fprintf(stderr, _("unicode handling error\n"));
		return EXIT_FAILURE;
	}
	wcommand =
	    (wchar_t *) malloc((wcommand_characters + 1) * sizeof(wcommand));
	if (wcommand == NULL) {
		fprintf(stderr, _("unicode handling error (malloc)\n"));
		return EXIT_FAILURE;
	}
	mbstowcs(wcommand, command, wcommand_characters + 1);
#endif	/* WITH_WATCH8BIT */

	get_terminal_size();

	/* Catch keyboard interrupts so we can put tty back in a sane
	 * state.  */
	signal(SIGINT, die);
	signal(SIGTERM, die);
	signal(SIGHUP, die);
	signal(SIGWINCH, winch_handler);

	/* Set up tty for curses use.  */
	curses_started = true;
	initscr();
	if (flags & WATCH_COLOR) {
		if (has_colors()) {
			start_color();
			use_default_colors();
			init_ansi_colors();
		} else {
			flags &= ~WATCH_COLOR;
		}
	}
	nonl();
	noecho();
	cbreak();
	curs_set(0);

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
			get_terminal_size();
			resizeterm(height, width);
			clear();
			/* redrawwin(stdscr); */
			screen_size_changed = false;
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
			command_exit = run_command(command, command_argv);

			if (flags & WATCH_EQUEXIT) {
				if (cycle_count == max_cycles && command_exit)
					break;
				else if (command_exit)
					cycle_count++;
				else
					cycle_count = 0;
			}
		} else if (command_exit)
			break;
		else
			refresh();

		if (precise_timekeeping) {
			watch_usec_t cur_time = get_time_usec();
			next_loop += USECS_PER_SEC * interval;
			if (cur_time < next_loop)
				usleep(next_loop - cur_time);
		} else if (interval < UINT_MAX / USECS_PER_SEC)
			usleep(interval * USECS_PER_SEC);
		else
			sleep(interval);
	}

	do_exit(EXIT_SUCCESS);
}
