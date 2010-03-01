/* watch -- execute a program repeatedly, displaying output fullscreen
 *
 * Based on the original 1991 'watch' by Tony Rems <rembo@unisoft.com>
 * (with mods and corrections by Francois Pinard).
 *
 * Substantially reworked, new features (differences option, SIGWINCH
 * handling, unlimited command length, long line handling) added Apr 1999 by
 * Mike Coleman <mkc@acm.org>.
 *
 * Changes by Albert Cahalan, 2002-2003.
 * stderr handling, exec, and beep option added by Morty Abzug, 2008
 */

#include <ctype.h>
#include <getopt.h>
#include <signal.h>
#include <ncurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>
#include <termios.h>
#include <locale.h>
#include "proc/procps.h"
#include "config.h"

#ifdef FORCE_8BIT
#undef isprint
#define isprint(x) ( (x>=' '&&x<='~') || (x>=0xa0) )
#endif

static struct option longopts[] = {
  {"color", no_argument, 0, 'c' },
	{"differences", optional_argument, 0, 'd'},
	{"help", no_argument, 0, 'h'},
	{"interval", required_argument, 0, 'n'},
	{"beep", no_argument, 0, 'b'},
	{"errexit", no_argument, 0, 'e'},
	{"exec", no_argument, 0, 'x'},
	{"no-title", no_argument, 0, 't'},
	{"version", no_argument, 0, 'v'},
	{0, 0, 0, 0}
};

static char usage[] =
    "Usage: %s [-bdhntvx] [--beep] [--color] [--differences[=cumulative]] [--exec] [--help] [--interval=<n>] [--no-title] [--version] <command>\n";

static char *progname;

static int curses_started = 0;
static int height = 24, width = 80;
static int screen_size_changed = 0;
static int first_screen = 1;
static int show_title = 2;  // number of lines used, 2 or 0

#define min(x,y) ((x) > (y) ? (y) : (x))
#define MAX_ANSIBUF 10

static void init_ansi_colors(void)
{
  int i;
  short ncurses_colors[] = {
    COLOR_BLACK, COLOR_RED, COLOR_GREEN, COLOR_YELLOW, COLOR_BLUE,
    COLOR_MAGENTA, COLOR_CYAN, COLOR_WHITE };

  for (i=0; i< 8; i++)
    init_pair(i+1, ncurses_colors[i], -1);
}

static void set_ansi_attribute(const int attrib)
{
  switch (attrib)
  {
    case -1:
      return;
    case 0:
      standend();
      return;
    case 1:
      attrset(A_BOLD);
      return;
  }
  if (attrib >= 30 && attrib <= 37) {
    color_set(attrib-29,NULL);
    return;
  }
}

static void process_ansi(FILE *fp)
{
  int i,c, num1, num2;
  char buf[MAX_ANSIBUF];
  char *nextnum;


  c= getc(fp);
  if (c != '[') {
    ungetc(c, fp);
    return;
  }
  for(i=0; i<MAX_ANSIBUF; i++)
  {
    c = getc(fp);
    if (c == 'm') //COLOUR SEQUENCE ENDS in 'm'
    {
      buf[i] = '\0';
      break;
    }
    if (c < '0' && c > '9' && c != ';')
    {
      while(--i >= 0)
        ungetc(buf[i],fp);
      return;
    }
    buf[i] = (char)c;
  }
  num1 = strtol(buf, &nextnum, 10);
  if (nextnum != buf && nextnum[0] != '\0')
    num2 = strtol(nextnum+1, NULL, 10);
  else
    num2 = -1;
  set_ansi_attribute(num1);
  set_ansi_attribute(num2);
}

static void do_usage(void) NORETURN;
static void do_usage(void)
{
	fprintf(stderr, usage, progname);
	exit(1);
}

static void do_exit(int status) NORETURN;
static void do_exit(int status)
{
	if (curses_started)
		endwin();
	exit(status);
}

/* signal handler */
static void die(int notused) NORETURN;
static void die(int notused)
{
	(void) notused;
	do_exit(0);
}

static void
winch_handler(int notused)
{
	(void) notused;
	screen_size_changed = 1;
}

static char env_col_buf[24];
static char env_row_buf[24];
static int incoming_cols;
static int incoming_rows;

static void
get_terminal_size(void)
{
	struct winsize w;
	if(!incoming_cols){  // have we checked COLUMNS?
		const char *s = getenv("COLUMNS");
		incoming_cols = -1;
		if(s && *s){
			long t;
			char *endptr;
			t = strtol(s, &endptr, 0);
			if(!*endptr && (t>0) && (t<(long)666)) incoming_cols = (int)t;
			width = incoming_cols;
			snprintf(env_col_buf, sizeof env_col_buf, "COLUMNS=%d", width);
			putenv(env_col_buf);
		}
	}
	if(!incoming_rows){  // have we checked LINES?
		const char *s = getenv("LINES");
		incoming_rows = -1;
		if(s && *s){
			long t;
			char *endptr;
			t = strtol(s, &endptr, 0);
			if(!*endptr && (t>0) && (t<(long)666)) incoming_rows = (int)t;
			height = incoming_rows;
			snprintf(env_row_buf, sizeof env_row_buf, "LINES=%d", height);
			putenv(env_row_buf);
		}
	}
	if (incoming_cols<0 || incoming_rows<0){
		if (ioctl(2, TIOCGWINSZ, &w) == 0) {
			if (incoming_rows<0 && w.ws_row > 0){
				height = w.ws_row;
				snprintf(env_row_buf, sizeof env_row_buf, "LINES=%d", height);
				putenv(env_row_buf);
			}
			if (incoming_cols<0 && w.ws_col > 0){
				width = w.ws_col;
				snprintf(env_col_buf, sizeof env_col_buf, "COLUMNS=%d", width);
				putenv(env_col_buf);
			}
		}
	}
}

int
main(int argc, char *argv[])
{
	int optc;
	int option_differences = 0,
	    option_differences_cumulative = 0,
			option_exec = 0,
			option_beep = 0,
      option_color = 0,
        option_errexit = 0,
	    option_help = 0, option_version = 0;
	double interval = 2;
	char *command;
	char **command_argv;
	int command_length = 0;	/* not including final \0 */
	int pipefd[2];
	int status;
	pid_t child;

	setlocale(LC_ALL, "");
	progname = argv[0];

	while ((optc = getopt_long(argc, argv, "+bced::hn:vtx", longopts, (int *) 0))
	       != EOF) {
		switch (optc) {
		case 'b':
			option_beep = 1;
			break;
    case 'c':
      option_color = 1;
      break;
		case 'd':
			option_differences = 1;
			if (optarg)
				option_differences_cumulative = 1;
			break;
        case 'e':
            option_errexit = 1;
            break;
		case 'h':
			option_help = 1;
			break;
		case 't':
			show_title = 0;
			break;
		case 'x':
		  option_exec = 1;
			break;
		case 'n':
			{
				char *str;
				interval = strtod(optarg, &str);
				if (!*optarg || *str)
					do_usage();
				if(interval < 0.1)
					interval = 0.1;
				if(interval > ~0u/1000000)
					interval = ~0u/1000000;
			}
			break;
		case 'v':
			option_version = 1;
			break;
		default:
			do_usage();
			break;
		}
	}

	if (option_version) {
		fprintf(stderr, "%s\n", PACKAGE_NAME " version " PACKAGE_VERSION);
		if (!option_help)
			exit(0);
	}

	if (option_help) {
		fprintf(stderr, usage, progname);
		fputs("  -b, --beep\t\t\t\tbeep if the command has a non-zero exit\n", stderr);
		fputs("  -d, --differences[=cumulative]\thighlight changes between updates\n", stderr);
		fputs("\t\t(cumulative means highlighting is cumulative)\n", stderr);
		fputs("  -e, --errexit\t\t\t\texit watch if the command has a non-zero exit\n", stderr);
		fputs("  -h, --help\t\t\t\tprint a summary of the options\n", stderr);
		fputs("  -n, --interval=<seconds>\t\tseconds to wait between updates\n", stderr);
		fputs("  -v, --version\t\t\t\tprint the version number\n", stderr);
		fputs("  -t, --no-title\t\t\tturns off showing the header\n", stderr);
		fputs("  -x, --exec\t\t\t\tpass command to exec instead of sh\n", stderr);
		exit(0);
	}

	if (optind >= argc)
		do_usage();

	command_argv=&(argv[optind]); /* save for later */

	command = strdup(argv[optind++]);
	command_length = strlen(command);
	for (; optind < argc; optind++) {
		char *endp;
		int s = strlen(argv[optind]);
		command = realloc(command, command_length + s + 2);	/* space and \0 */
		endp = command + command_length;
		*endp = ' ';
		memcpy(endp + 1, argv[optind], s);
		command_length += 1 + s;	/* space then string length */
		command[command_length] = '\0';
	}

	get_terminal_size();

	/* Catch keyboard interrupts so we can put tty back in a sane state.  */
	signal(SIGINT, die);
	signal(SIGTERM, die);
	signal(SIGHUP, die);
	signal(SIGWINCH, winch_handler);

	/* Set up tty for curses use.  */
	curses_started = 1;
	initscr();
  if (option_color) {
    if (has_colors()) {
      start_color();
      use_default_colors();
      init_ansi_colors();
    } else
      option_color = 0;
  }
	nonl();
	noecho();
	cbreak();

	for (;;) {
		time_t t = time(NULL);
		char *ts = ctime(&t);
		int tsl = strlen(ts);
		char *header;
		FILE *p;
		int x, y;
		int oldeolseen = 1;

		if (screen_size_changed) {
			get_terminal_size();
			resizeterm(height, width);
			clear();
			/* redrawwin(stdscr); */
			screen_size_changed = 0;
			first_screen = 1;
		}

		if (show_title) {
			// left justify interval and command,
			// right justify time, clipping all to fit window width
			asprintf(&header, "Every %.1fs: %.*s",
				interval, min(width - 1, command_length), command);
			mvaddstr(0, 0, header);
			if (strlen(header) > (size_t) (width - tsl - 1))
				mvaddstr(0, width - tsl - 4, "...  ");
			mvaddstr(0, width - tsl + 1, ts);
			free(header);
		}

		/* allocate pipes */
		if (pipe(pipefd)<0) {
		  perror("pipe");
			do_exit(7);
	  }

		/* flush stdout and stderr, since we're about to do fd stuff */
		fflush(stdout);
		fflush(stderr);

		/* fork to prepare to run command */
		child=fork();

		if (child<0) { /* fork error */
		  perror("fork");
			do_exit(2);
		} else if (child==0) { /* in child */
			close (pipefd[0]); /* child doesn't need read side of pipe */
			close (1); /* prepare to replace stdout with pipe */
			if (dup2 (pipefd[1], 1)<0) { /* replace stdout with write side of pipe */
			  perror("dup2");
				exit(3);
			}
			dup2(1, 2); /* stderr should default to stdout */

			if (option_exec) { /* pass command to exec instead of system */
			  if (execvp(command_argv[0], command_argv)==-1) {
				  perror("exec");
				  exit(4);
				}
			} else {
		    status=system(command); /* watch manpage promises sh quoting */

			  /* propagate command exit status as child exit status */
			  if (!WIFEXITED(status)) { /* child exits nonzero if command does */
			    exit(1);
			  } else {
			    exit(WEXITSTATUS(status));
		    }
			}

		}

		/* otherwise, we're in parent */
		close(pipefd[1]); /* close write side of pipe */
		if ((p=fdopen(pipefd[0], "r"))==NULL) {
		  perror("fdopen");
			do_exit(5);
		}


		for (y = show_title; y < height; y++) {
			int eolseen = 0, tabpending = 0;
			for (x = 0; x < width; x++) {
				int c = ' ';
				int attr = 0;

				if (!eolseen) {
					/* if there is a tab pending, just spit spaces until the
					   next stop instead of reading characters */
					if (!tabpending)
						do
							c = getc(p);
						while (c != EOF && !isprint(c)
						       && c != '\n'
                                                      && c != '\t'
                   && (c != L'\033' || option_color != 1));
          if (c == L'\033' && option_color == 1) {
            x--;
            process_ansi(p);
            continue;
          }
					if (c == '\n')
						if (!oldeolseen && x == 0) {
							x = -1;
							continue;
						} else
							eolseen = 1;
					else if (c == '\t')
						tabpending = 1;
					if (c == EOF || c == '\n' || c == '\t')
						c = ' ';
					if (tabpending && (((x + 1) % 8) == 0))
						tabpending = 0;
				}
				move(y, x);
				if (option_differences) {
					chtype oldch = inch();
					unsigned char oldc = oldch & A_CHARTEXT;
					attr = !first_screen
					    && ((unsigned char)c != oldc
						||
						(option_differences_cumulative
						 && (oldch & A_ATTRIBUTES)));
				}
				if (attr)
					standout();
				addch(c);
				if (attr)
					standend();
			}
			oldeolseen = eolseen;
		}

		fclose(p);

		/* harvest child process and get status, propagated from command */
		if (waitpid(child, &status, 0)<0) {
		  perror("waitpid");
			do_exit(8);
		};

		/* if child process exited in error, beep if option_beep is set */
		if ((!WIFEXITED(status) || WEXITSTATUS(status))) {
          if (option_beep) beep();
          if (option_errexit) do_exit(8);
		}

		first_screen = 0;
		refresh();
		usleep(interval * 1000000);
	}

	endwin();

	return 0;
}
